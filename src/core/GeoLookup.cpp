#include "core/GeoLookup.h"

#include <QFileInfo>
#include <QHostAddress>

#include <iostream>

#ifdef HAVE_MAXMINDDB
#include <maxminddb.h>

struct GeoLookup::Impl {
    MMDB_s mmdb{};
};

namespace {
void logErr(const QString &s) {
    std::cerr << ("geo: " + s).toStdString() << std::endl;
}

// libmaxminddb resolves "::ffff:1.2.3.4" as IPv6 and misses IPv4 trees,
// so map IPv4-mapped addresses back to dotted quads first.
QString normalizeIp(const QString &ip) {
    if (ip.startsWith(QStringLiteral("::ffff:"))) {
        const QString tail = ip.mid(7);
        QHostAddress v4(tail);
        if (v4.protocol() == QAbstractSocket::IPv4Protocol)
            return tail;
    }
    // Strip IPv6 zone id ("%eth0") which getaddrinfo would reject.
    const int pct = ip.indexOf(QLatin1Char('%'));
    return pct > 0 ? ip.left(pct) : ip;
}

QString entryString(const MMDB_entry_s &entry, const char *field) {
    MMDB_entry_data_s data{};
    // Path: country/city -> names -> en
    const int status = MMDB_get_value(
        const_cast<MMDB_entry_s *>(&entry), &data, field, "names", "en", nullptr);
    if (status != MMDB_SUCCESS || !data.has_data)
        return {};
    if (data.type != MMDB_DATA_TYPE_UTF8_STRING)
        return {};
    return QString::fromUtf8(reinterpret_cast<const char *>(data.utf8_string),
                             static_cast<int>(data.data_size));
}
}
#else
struct GeoLookup::Impl {};
#endif

GeoLookup::GeoLookup()
    : m_impl(new Impl())
{
}

GeoLookup::~GeoLookup() {
#ifdef HAVE_MAXMINDDB
    if (m_loaded)
        MMDB_close(&m_impl->mmdb);
#endif
    delete m_impl;
}

bool GeoLookup::load(const QString &path) {
    m_path = path;
    m_loaded = false;
#ifdef HAVE_MAXMINDDB
    if (!QFileInfo::exists(path)) {
        logErr(QStringLiteral("file not found: %1 (geo fields will stay empty)").arg(path));
        return false;
    }
    const int status = MMDB_open(path.toLocal8Bit().constData(), MMDB_MODE_MMAP,
                                 &m_impl->mmdb);
    if (status != MMDB_SUCCESS) {
        logErr(QStringLiteral("cannot open %1: %2 (geo fields will stay empty)")
               .arg(path, QString::fromUtf8(MMDB_strerror(status))));
        return false;
    }
    m_loaded = true;
    std::cout << ("geo: loaded " + path).toStdString() << std::endl;
    return true;
#else
    logErr(QStringLiteral("built without libmaxminddb (geo fields will stay empty)"));
    return false;
#endif
}

std::pair<QString, QString> GeoLookup::lookup(const QString &ip) const {
    if (!m_loaded)
        return {};
#ifdef HAVE_MAXMINDDB
    const QString norm = normalizeIp(ip);
    const QByteArray ascii = norm.toLatin1();
    int gaiError = 0, mmdbError = MMDB_SUCCESS;
    MMDB_lookup_result_s result =
        MMDB_lookup_string(&m_impl->mmdb, ascii.constData(), &gaiError, &mmdbError);
    if (gaiError != 0 || mmdbError != MMDB_SUCCESS || !result.found_entry)
        return {};
    return {entryString(result.entry, "country"), entryString(result.entry, "city")};
#else
    Q_UNUSED(ip);
    return {};
#endif
}

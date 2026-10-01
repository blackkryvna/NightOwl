#include "core/Config.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

namespace {
constexpr quint16 kDefaultPort = 2222;
constexpr int kDefaultMaxSessions = 50;
constexpr int kDefaultTimeoutSec = 60;
constexpr int kDefaultMaxLine = 256;

int clampedInt(QSettings &s, const QString &key, int def, int lo, int hi) {
    bool ok = false;
    const int v = s.value(key, def).toInt(&ok);
    if (!ok || v < lo || v > hi)
        return def;
    return v;
}
}

Config::Config(const QString &filePath) {
    QString candidate = filePath;

    if (candidate.isEmpty()) {
        // 1) config.ini in current working directory
        if (QFileInfo::exists(QStringLiteral("config.ini"))) {
            candidate = QStringLiteral("config.ini");
        } else {
            // 2) config.ini next to the binary
            const QString appDir = QCoreApplication::applicationDirPath();
            const QString nextToBin = QDir(appDir).filePath(QStringLiteral("config.ini"));
            if (QFileInfo::exists(nextToBin))
                candidate = nextToBin;
        }
    }

    if (candidate.isEmpty() || !QFileInfo::exists(candidate)) {
        m_usingDefaults = true;
        return;
    }

    m_filePath = candidate;
    QSettings settings(candidate, QSettings::IniFormat);

    bool ok = false;
    const uint rawPort = settings.value(QStringLiteral("honeypot/port"), kDefaultPort).toUInt(&ok);
    // Unprivileged ports only: app must never require root.
    if (!ok || rawPort < 1025 || rawPort > 65535) {
        m_usingDefaults = true;
        return;
    }
    m_port = static_cast<quint16>(rawPort);
    m_maxSessions = clampedInt(settings, QStringLiteral("honeypot/max_sessions"),
                               kDefaultMaxSessions, 1, 1000);
    m_sessionTimeoutSec = clampedInt(settings, QStringLiteral("honeypot/session_timeout_sec"),
                                     kDefaultTimeoutSec, 5, 3600);
    m_maxLineLength = clampedInt(settings, QStringLiteral("honeypot/max_line_length"),
                                 kDefaultMaxLine, 32, 4096);

    m_dbHost = settings.value(QStringLiteral("database/host"), m_dbHost).toString();
    m_dbPort = clampedInt(settings, QStringLiteral("database/port"), 5432, 1, 65535);
    m_dbName = settings.value(QStringLiteral("database/name"), m_dbName).toString();
    m_dbUser = settings.value(QStringLiteral("database/user"), m_dbUser).toString();
    m_dbPassword = settings.value(QStringLiteral("database/password"), QString()).toString();

    m_geoipPath = settings.value(QStringLiteral("geoip/path"), m_geoipPath).toString();

    m_usingDefaults = false;
}

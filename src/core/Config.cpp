#include "core/Config.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

namespace {
constexpr quint16 kDefaultPort = 2222;
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
        m_port = kDefaultPort;
        return;
    }

    m_filePath = candidate;
    QSettings settings(candidate, QSettings::IniFormat);
    bool ok = false;
    const uint raw = settings.value(QStringLiteral("honeypot/port"), kDefaultPort).toUInt(&ok);
    // Unprivileged ports only: app must never require root.
    if (!ok || raw < 1025 || raw > 65535) {
        m_port = kDefaultPort;
        m_usingDefaults = true;
        return;
    }
    m_port = static_cast<quint16>(raw);
    m_usingDefaults = false;
}

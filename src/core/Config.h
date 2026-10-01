#pragma once

#include <QString>
#include <QtGlobal>

// Loads honeypot settings from an INI file.
// [honeypot] is used from stage 1-2, [database]/[geoip] from stages 3-4.
class Config {
public:
    // If filePath is empty, tries ./config.ini then <appDir>/config.ini.
    explicit Config(const QString &filePath = QString());

    quint16 port() const { return m_port; }
    int maxSessions() const { return m_maxSessions; }
    int sessionTimeoutSec() const { return m_sessionTimeoutSec; }
    int maxLineLength() const { return m_maxLineLength; }

    QString dbHost() const { return m_dbHost; }
    int dbPort() const { return m_dbPort; }
    QString dbName() const { return m_dbName; }
    QString dbUser() const { return m_dbUser; }
    QString dbPassword() const { return m_dbPassword; }

    QString geoipPath() const { return m_geoipPath; }

    QString filePath() const { return m_filePath; }
    bool usingDefaults() const { return m_usingDefaults; }

private:
    quint16 m_port = 2222;
    int m_maxSessions = 50;
    int m_sessionTimeoutSec = 60;
    int m_maxLineLength = 256;

    QString m_dbHost = QStringLiteral("127.0.0.1");
    int m_dbPort = 5432;
    QString m_dbName = QStringLiteral("honeyden");
    QString m_dbUser = QStringLiteral("honeyden");
    QString m_dbPassword;

    QString m_geoipPath = QStringLiteral("GeoLite2-City.mmdb");

    QString m_filePath;
    bool m_usingDefaults = true;
};

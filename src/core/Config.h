#pragma once

#include <QString>
#include <QtGlobal>

// Loads honeypot settings from an INI file.
// Stage 1 uses only [honeypot]/port, rest is for later stages.
class Config {
public:
    // If filePath is empty, tries ./config.ini then <appDir>/config.ini.
    explicit Config(const QString &filePath = QString());

    quint16 port() const { return m_port; }
    QString filePath() const { return m_filePath; }
    bool usingDefaults() const { return m_usingDefaults; }

private:
    quint16 m_port = 2222;
    QString m_filePath;
    bool m_usingDefaults = true;
};

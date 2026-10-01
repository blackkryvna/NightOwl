#pragma once

#include <QString>
#include <utility>

// Country/city lookup backed by a local GeoLite2-City.mmdb file.
// If the file is missing or unreadable the app keeps working,
// lookup() just returns empty strings.
class GeoLookup {
public:
    GeoLookup();
    ~GeoLookup();

    GeoLookup(const GeoLookup &) = delete;
    GeoLookup &operator=(const GeoLookup &) = delete;

    // Returns false when the file is missing/broken (fields stay empty).
    bool load(const QString &path);
    bool isLoaded() const { return m_loaded; }
    QString path() const { return m_path; }

    // English country/city names, or {"",""} when unknown.
    // Never throws; safe to call when not loaded.
    std::pair<QString, QString> lookup(const QString &ip) const;

private:
    struct Impl;
    Impl *m_impl = nullptr;
    QString m_path;
    bool m_loaded = false;
};

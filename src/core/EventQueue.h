#pragma once

#include <QDateTime>
#include <QString>
#include <QMetaType>
#include <QtGlobal>

// Events flowing from Session -> Listener -> (live table, DbWriter).
// All client strings are untrusted: already truncated to max line length
// by Session before being put here. Never concatenate into SQL.
namespace Events {

struct SessionStarted {
    quint64 token = 0;      // in-memory session id (not the DB id)
    QString ip;
    quint16 port = 0;
    QString country;        // filled by GeoLookup in stage 4, empty until then
    QString city;
    QDateTime startedAt;
};

struct AuthAttempt {
    quint64 token = 0;
    QString ip;
    quint16 port = 0;
    QString country;
    QString city;
    QDateTime ts;
    QString username;
    QString password;
    int attemptNo = 0;      // 1-based counter within this TCP session
};

struct SessionFinished {
    quint64 token = 0;
    QDateTime endedAt;
};

void registerMetaTypes();

} // namespace Events

Q_DECLARE_METATYPE(Events::SessionStarted)
Q_DECLARE_METATYPE(Events::AuthAttempt)
Q_DECLARE_METATYPE(Events::SessionFinished)

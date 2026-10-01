#pragma once

#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QObject>
#include <QSqlDatabase>
#include <QString>

#include "db/DbWriter.h" // reuses DbConfig

// One row of the run list (left History panel).
struct RunInfo {
    int id = -1;
    QDateTime startedAt;
    QDateTime endedAt; // null while the run is active
    int attackCount = 0;
    qint64 durationSec() const {
        const QDateTime end = endedAt.isValid() ? endedAt : QDateTime::currentDateTimeUtc();
        return startedAt.isValid() ? startedAt.secsTo(end) : 0;
    }
};

// One stored attempt of a past run (for the shared table/chart widgets).
struct RunAttempt {
    QString ip;
    QString country;
    QString city;
    int port = 0;
    QDateTime ts;
    QString username;
    QString password;
    int attemptNo = 0; // per-session counter, restored in ts order
};

// Read-only DB access for the History panel. Lives in its own QThread;
// the GUI talks to it only via signals/slots. SELECT-only, prepare+bind.
class DbReader : public QObject {
    Q_OBJECT
public:
    explicit DbReader(const DbConfig &cfg, QObject *parent = nullptr);

    static void registerMetaTypes();

public slots:
    void open();
    void close();
    void loadRuns();
    void loadRunDetails(int runId);

signals:
    void runsLoaded(const QList<RunInfo> &runs);
    void runDetailsLoaded(int runId, const QList<RunAttempt> &attempts);
    void errorOccurred(const QString &message);

private:
    void fail(const QString &what, const QString &dbText);

    DbConfig m_cfg;
    QSqlDatabase m_db;
    bool m_open = false;
};

Q_DECLARE_METATYPE(RunInfo)
Q_DECLARE_METATYPE(RunAttempt)

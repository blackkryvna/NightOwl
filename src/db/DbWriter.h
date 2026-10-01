#pragma once

#include "core/EventQueue.h"

#include <QObject>
#include <QMap>
#include <QSqlDatabase>
#include <QString>

// Persistent event sink. Lives in its own QThread; all slots run there.
// QSqlDatabase: one connection per thread, used only inside the worker.
// All SQL uses prepare() + bindValue(), never string concatenation.
struct DbConfig {
    QString host = QStringLiteral("127.0.0.1");
    int port = 5432;
    QString name = QStringLiteral("honeyden");
    QString user = QStringLiteral("honeyden");
    QString password;
};

class DbWriter : public QObject {
    Q_OBJECT
public:
    explicit DbWriter(const DbConfig &cfg, QObject *parent = nullptr);

public slots:
    void open();
    void close();
    void startRun();
    void finishRun();
    void onSessionStarted(const Events::SessionStarted &e);
    void onAuthAttempt(const Events::AuthAttempt &e);
    void onSessionFinished(const Events::SessionFinished &e);

signals:
    void runStarted(int runId);
    void errorOccurred(const QString &message);

private:
    bool ensureOpen();
    void logError(const QString &msg);
    int ensureSession(const Events::AuthAttempt &e);

    DbConfig m_cfg;
    QSqlDatabase m_db;
    QMap<quint64, int> m_sessions; // in-memory token -> DB sessions.id
    int m_runId = -1;
    bool m_open = false;
};

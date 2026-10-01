#include "db/DbReader.h"

#include <QMap>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QVariant>

#include <iostream>

void DbReader::registerMetaTypes() {
    qRegisterMetaType<RunInfo>("RunInfo");
    qRegisterMetaType<RunAttempt>("RunAttempt");
    qRegisterMetaType<QList<RunInfo>>("QList<RunInfo>");
    qRegisterMetaType<QList<RunAttempt>>("QList<RunAttempt>");
}

DbReader::DbReader(const DbConfig &cfg, QObject *parent)
    : QObject(parent)
    , m_cfg(cfg)
{
}

void DbReader::open() {
    if (m_open && m_db.isOpen())
        return;
    const QString connName = QStringLiteral("honeyden_reader_%1")
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    if (QSqlDatabase::contains(connName))
        m_db = QSqlDatabase::database(connName);
    else
        m_db = QSqlDatabase::addDatabase(QStringLiteral("QPSQL"), connName);
    m_db.setHostName(m_cfg.host);
    m_db.setPort(m_cfg.port);
    m_db.setDatabaseName(m_cfg.name);
    m_db.setUserName(m_cfg.user);
    m_db.setPassword(m_cfg.password);
    m_db.setConnectOptions(QStringLiteral("connect_timeout=5"));
    if (!m_db.open()) {
        m_open = false;
        fail(QStringLiteral("connect"), m_db.lastError().text());
        return;
    }
    m_open = true;
}

void DbReader::close() {
    if (m_db.isOpen())
        m_db.close();
    m_open = false;
}

void DbReader::fail(const QString &what, const QString &dbText) {
    const QString msg = what + QStringLiteral(": ") + dbText;
    std::cerr << ("db-reader: " + msg).toStdString() << std::endl;
    emit errorOccurred(msg);
}

void DbReader::loadRuns() {
    if (!m_open || !m_db.isOpen())
        open();
    if (!m_open || !m_db.isOpen()) {
        emit runsLoaded({});
        return;
    }
    QSqlQuery q(m_db);
    // One row per run with its attack total; newest first.
    q.prepare(QStringLiteral(
        "SELECT r.id, r.started_at, r.ended_at, COUNT(a.id) AS attacks "
        "FROM runs r "
        "LEFT JOIN sessions s ON s.run_id = r.id "
        "LEFT JOIN auth_attempts a ON a.session_id = s.id "
        "GROUP BY r.id ORDER BY r.id DESC"));
    QList<RunInfo> out;
    if (!q.exec()) {
        fail(QStringLiteral("loadRuns"), q.lastError().text());
        emit runsLoaded(out);
        return;
    }
    while (q.next()) {
        RunInfo r;
        r.id = q.value(0).toInt();
        r.startedAt = q.value(1).toDateTime();
        r.endedAt = q.value(2).toDateTime();
        r.attackCount = q.value(3).toInt();
        out.append(r);
    }
    emit runsLoaded(out);
}

void DbReader::loadRunDetails(int runId) {
    QList<RunAttempt> out;
    if (!m_open || !m_db.isOpen())
        open();
    if (!m_open || !m_db.isOpen()) {
        emit runDetailsLoaded(runId, out);
        return;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT s.id, s.ip, s.port, s.country, s.city, "
        "       a.username, a.password, a.ts "
        "FROM auth_attempts a "
        "JOIN sessions s ON s.id = a.session_id "
        "WHERE s.run_id = :run ORDER BY a.ts, a.id"));
    q.bindValue(QStringLiteral(":run"), runId);
    if (!q.exec()) {
        fail(QStringLiteral("loadRunDetails"), q.lastError().text());
        emit runDetailsLoaded(runId, out);
        return;
    }
    // Restore per-session attempt numbers in chronological order.
    QMap<int, int> counters; // sessions.id -> attempts so far
    while (q.next()) {
        RunAttempt a;
        const int sid = q.value(0).toInt();
        a.ip = q.value(1).toString();
        a.port = q.value(2).toInt();
        a.country = q.value(3).toString();
        a.city = q.value(4).toString();
        a.username = q.value(5).toString();
        a.password = q.value(6).toString();
        a.ts = q.value(7).toDateTime();
        a.attemptNo = ++counters[sid];
        out.append(a);
    }
    emit runDetailsLoaded(runId, out);
}

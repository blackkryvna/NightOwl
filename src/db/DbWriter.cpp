#include "db/DbWriter.h"

#include <QDateTime>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QVariant>

#include <iostream>

namespace {
void logLine(const QString &s) {
    std::cout << ("db: " + s).toStdString() << std::endl;
}
void logErr(const QString &s) {
    std::cerr << ("db: " + s).toStdString() << std::endl;
}
// Postgres INET rejects garbage; store NULL instead of failing the row.
bool looksLikeIp(const QString &ip) {
    return !ip.isEmpty() && ip.size() < 64;
}
}

DbWriter::DbWriter(const DbConfig &cfg, QObject *parent)
    : QObject(parent)
    , m_cfg(cfg)
{
}

void DbWriter::open() {
    if (m_open && m_db.isOpen())
        return;
    const QString connName = QStringLiteral("honeyden_writer_%1")
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
    // Short timeout: a stuck DB must not freeze the honeypot.
    m_db.setConnectOptions(QStringLiteral("connect_timeout=5"));
    if (!m_db.open()) {
        m_open = false;
        const QString err = m_db.lastError().text();
        logErr(QStringLiteral("cannot connect to %1:%2/%3 as %4: %5")
               .arg(m_cfg.host).arg(m_cfg.port).arg(m_cfg.name).arg(m_cfg.user).arg(err));
        emit errorOccurred(err);
        return;
    }
    m_open = true;
    logLine(QStringLiteral("connected to %1:%2/%3").arg(m_cfg.host).arg(m_cfg.port).arg(m_cfg.name));
}

void DbWriter::close() {
    if (m_runId > 0)
        finishRun();
    m_sessions.clear();
    if (m_db.isOpen())
        m_db.close();
    m_open = false;
}

bool DbWriter::ensureOpen() {
    if (m_open && m_db.isOpen())
        return true;
    open();
    return m_open && m_db.isOpen();
}

void DbWriter::logError(const QString &msg) {
    logErr(msg);
    emit errorOccurred(msg);
}

void DbWriter::startRun() {
    if (!ensureOpen()) {
        logErr(QStringLiteral("startRun skipped: no DB connection"));
        return;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO runs (started_at) VALUES (now()) RETURNING id"));
    if (!q.exec() || !q.next()) {
        logError(QStringLiteral("startRun failed: ") + q.lastError().text());
        return;
    }
    m_runId = q.value(0).toInt();
    m_sessions.clear();
    logLine(QStringLiteral("run #%1 started").arg(m_runId));
    emit runStarted(m_runId);
}

void DbWriter::finishRun() {
    if (m_runId <= 0)
        return;
    if (!ensureOpen()) {
        logErr(QStringLiteral("finishRun skipped: no DB connection"));
        return;
    }
    const int finishedId = m_runId;
    m_runId = -1; // reset first: close() calls us again, and GUI reuses the writer per run
    m_sessions.clear();
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE runs SET ended_at = now() WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), finishedId);
    if (!q.exec()) {
        logError(QStringLiteral("finishRun failed: ") + q.lastError().text());
        return;
    }
    // Close sessions that were still open when the run stopped.
    QSqlQuery qs(m_db);
    qs.prepare(QStringLiteral(
        "UPDATE sessions SET ended_at = now() WHERE run_id = :id AND ended_at IS NULL"));
    qs.bindValue(QStringLiteral(":id"), finishedId);
    if (!qs.exec())
        logError(QStringLiteral("finishRun sessions cleanup failed: ") + qs.lastError().text());
    else
        logLine(QStringLiteral("run #%1 finished").arg(finishedId));
}

void DbWriter::onSessionStarted(const Events::SessionStarted &e) {
    if (m_runId <= 0 || !ensureOpen())
        return;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO sessions (run_id, ip, port, country, city, started_at) "
        "VALUES (:run, :ip, :port, :country, :city, :started) RETURNING id"));
    q.bindValue(QStringLiteral(":run"), m_runId);
    if (looksLikeIp(e.ip))
        q.bindValue(QStringLiteral(":ip"), e.ip);
    else
        q.bindValue(QStringLiteral(":ip"), QVariant());
    q.bindValue(QStringLiteral(":port"), e.port);
    q.bindValue(QStringLiteral(":country"), e.country.isEmpty() ? QVariant() : QVariant(e.country));
    q.bindValue(QStringLiteral(":city"), e.city.isEmpty() ? QVariant() : QVariant(e.city));
    q.bindValue(QStringLiteral(":started"), e.startedAt.isValid() ? e.startedAt : QDateTime::currentDateTimeUtc());
    if (!q.exec() || !q.next()) {
        logError(QStringLiteral("session insert failed: ") + q.lastError().text());
        return;
    }
    m_sessions[e.token] = q.value(0).toInt();
}

int DbWriter::ensureSession(const Events::AuthAttempt &e) {
    auto it = m_sessions.find(e.token);
    if (it != m_sessions.end())
        return it.value();
    // Attempt arrived without a recorded start (e.g. writer restarted):
    // create the session row on demand so the attempt is not lost.
    Events::SessionStarted s;
    s.token = e.token;
    s.ip = e.ip;
    s.port = e.port;
    s.country = e.country;
    s.city = e.city;
    s.startedAt = e.ts.isValid() ? e.ts : QDateTime::currentDateTimeUtc();
    onSessionStarted(s);
    it = m_sessions.find(e.token);
    return it != m_sessions.end() ? it.value() : -1;
}

void DbWriter::onAuthAttempt(const Events::AuthAttempt &e) {
    if (m_runId <= 0 || !ensureOpen())
        return;
    const int sessionId = ensureSession(e);
    if (sessionId <= 0)
        return;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO auth_attempts (session_id, username, password, ts) "
        "VALUES (:sid, :user, :pass, :ts)"));
    q.bindValue(QStringLiteral(":sid"), sessionId);
    q.bindValue(QStringLiteral(":user"), e.username);
    q.bindValue(QStringLiteral(":pass"), e.password);
    q.bindValue(QStringLiteral(":ts"), e.ts.isValid() ? e.ts : QDateTime::currentDateTimeUtc());
    if (!q.exec())
        logError(QStringLiteral("auth insert failed: ") + q.lastError().text());
}

void DbWriter::onSessionFinished(const Events::SessionFinished &e) {
    auto it = m_sessions.find(e.token);
    if (it == m_sessions.end() || !ensureOpen())
        return;
    const int sessionId = it.value();
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE sessions SET ended_at = :ts WHERE id = :id"));
    q.bindValue(QStringLiteral(":ts"), e.endedAt.isValid() ? e.endedAt : QDateTime::currentDateTimeUtc());
    q.bindValue(QStringLiteral(":id"), sessionId);
    if (!q.exec())
        logError(QStringLiteral("session finish failed: ") + q.lastError().text());
    m_sessions.remove(e.token); // bound the in-memory map
}

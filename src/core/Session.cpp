#include "core/Session.h"

#include <QTcpSocket>
#include <QHostAddress>
#include <QTimer>
#include <QDateTime>

#include "core/GeoLookup.h"

#include <atomic>
#include <iostream>

namespace {
// qInfo() is avoided: default Qt handler is silent in some environments.
void logLine(const QString &line) {
    std::cout << line.toStdString() << std::endl;
}
void logError(const QString &line) {
    std::cerr << line.toStdString() << std::endl;
}
quint64 nextToken() {
    static std::atomic<quint64> s_next{1};
    return s_next.fetch_add(1);
}
}

Session::Session(qintptr socketDescriptor, int maxLineLength, int timeoutSec,
                 GeoLookup *geo, QObject *parent)
    : QObject(parent)
    , m_maxLineLength(maxLineLength)
{
    m_token = nextToken();
    m_socket = new QTcpSocket(this);
    if (!m_socket->setSocketDescriptor(socketDescriptor)) {
        logError(QStringLiteral("session #%1: cannot set socket descriptor: %2")
                 .arg(m_token).arg(m_socket->errorString()));
        m_socket->deleteLater();
        deleteLater();
        return;
    }

    m_ip = m_socket->peerAddress().toString();
    m_peerPort = m_socket->peerPort();
    m_startedAt = QDateTime::currentDateTimeUtc();

    // GeoIP is best-effort: missing DB or unknown IP -> empty fields.
    if (geo && geo->isLoaded()) {
        const auto loc = geo->lookup(m_ip);
        m_country = loc.first;
        m_city = loc.second;
    }

    // Idle timeout: reset on every received chunk.
    m_idleTimer = new QTimer(this);
    m_idleTimer->setSingleShot(true);
    m_idleTimer->setInterval(timeoutSec * 1000);
    connect(m_idleTimer, &QTimer::timeout, this, &Session::onTimeout);
    m_idleTimer->start();

    connect(m_socket, &QTcpSocket::readyRead, this, &Session::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &Session::onDisconnected);

    QString where = QStringLiteral("session #%1: new connection from %2:%3")
            .arg(m_token).arg(m_ip).arg(m_peerPort);
    if (!m_country.isEmpty() || !m_city.isEmpty())
        where += QStringLiteral(" [%1/%2]").arg(m_country, m_city);
    logLine(where);

    // Fake banner + first login prompt.
    sendLine("Welcome to Ubuntu 20.04.3 LTS\r\n");

    Events::SessionStarted ev;
    ev.token = m_token;
    ev.ip = m_ip;
    ev.port = m_peerPort;
    ev.country = m_country;
    ev.city = m_city;
    ev.startedAt = m_startedAt;
    emit sessionStarted(ev);

    promptLogin();
}

void Session::sendLine(const QByteArray &data) {
    if (m_socket && m_socket->isOpen())
        m_socket->write(data);
}

void Session::promptLogin() {
    m_state = State::AwaitLogin;
    m_pendingUser.clear();
    sendLine("login: ");
}

void Session::promptPassword() {
    m_state = State::AwaitPassword;
    sendLine("Password: ");
}

void Session::onReadyRead() {
    if (m_idleTimer)
        m_idleTimer->start(); // activity resets the idle timeout

    m_buffer += m_socket->readAll();

    // Enforce max buffered bytes without a newline (slowloris protection).
    if (m_buffer.size() > m_maxLineLength + 2) {
        const int nl = m_buffer.indexOf('\n');
        if (nl < 0 || nl > m_maxLineLength + 1) {
            sendLine("Line too long, disconnecting.\r\n");
            m_socket->disconnectFromHost();
            return;
        }
    }

    // Extract complete lines.
    int nl = -1;
    while ((nl = m_buffer.indexOf('\n')) >= 0) {
        QByteArray raw = m_buffer.left(nl + 1);
        m_buffer.remove(0, nl + 1);
        // Strip trailing CR/LF, keep the rest as-is.
        while (!raw.isEmpty() && (raw.back() == '\n' || raw.back() == '\r'))
            raw.chop(1);
        if (raw.size() > m_maxLineLength) {
            sendLine("Line too long, disconnecting.\r\n");
            m_socket->disconnectFromHost();
            return;
        }
        // Client bytes are untrusted: lossy UTF-8, control chars kept
        // for the DB as plain text (UI shows them via Qt::PlainText).
        handleLine(QString::fromUtf8(raw));
        if (m_socket->state() != QAbstractSocket::ConnectedState)
            return;
    }
}

void Session::handleLine(const QString &line) {
    if (m_state == State::AwaitLogin) {
        // Empty login: just re-prompt, don't count as an attempt.
        if (line.isEmpty()) {
            promptLogin();
            return;
        }
        m_pendingUser = line.left(m_maxLineLength);
        promptPassword();
    } else {
        ++m_attempts;
        Events::AuthAttempt ev;
        ev.token = m_token;
        ev.ip = m_ip;
        ev.port = m_peerPort;
        ev.country = m_country;
        ev.city = m_city;
        ev.ts = QDateTime::currentDateTimeUtc();
        ev.username = m_pendingUser;
        ev.password = line.left(m_maxLineLength);
        ev.attemptNo = m_attempts;
        emit authAttempt(ev);

        logLine(QStringLiteral("session #%1: [%2] attempt %3 user='%4'")
                .arg(m_token).arg(m_ip).arg(m_attempts).arg(m_pendingUser));

        // Always deny, then allow the next attempt on the same connection.
        sendLine("Access denied\r\n");
        promptLogin();
    }
}

void Session::onTimeout() {
    sendLine("Timed out.\r\n");
    if (m_socket)
        m_socket->disconnectFromHost();
}

void Session::onDisconnected() {
    logLine(QStringLiteral("session #%1: disconnected %2:%3 after %4 attempt(s)")
            .arg(m_token).arg(m_ip).arg(m_peerPort).arg(m_attempts));
    finish();
}

void Session::finish() {
    if (m_finished)
        return;
    m_finished = true;
    if (m_idleTimer)
        m_idleTimer->stop();
    Events::SessionFinished ev;
    ev.token = m_token;
    ev.endedAt = QDateTime::currentDateTimeUtc();
    emit sessionFinished(ev);
    deleteLater();
}

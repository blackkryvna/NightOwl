#include "core/Listener.h"
#include "core/Session.h"

#include <QTcpSocket>
#include <QHostAddress>

#include <iostream>

namespace {
void logLine(const QString &line) {
    std::cout << line.toStdString() << std::endl;
}
void logError(const QString &line) {
    std::cerr << line.toStdString() << std::endl;
}
}

Listener::Listener(int maxSessions, int maxLineLength, int timeoutSec, QObject *parent)
    : QTcpServer(parent)
    , m_maxSessions(maxSessions)
    , m_maxLineLength(maxLineLength)
    , m_timeoutSec(timeoutSec)
{
}

bool Listener::start(quint16 port) {
    if (!listen(QHostAddress::Any, port)) {
        logError(QStringLiteral("listener: cannot listen on port %1: %2")
                 .arg(port).arg(errorString()));
        return false;
    }
    logLine(QStringLiteral("listener: listening on port %1 (max %2 sessions)")
            .arg(port).arg(m_maxSessions));
    return true;
}

void Listener::incomingConnection(qintptr socketDescriptor) {
    if (m_active >= m_maxSessions) {
        // Reject gracefully so scanners see a clean refusal, not a hang.
        QTcpSocket *tmp = new QTcpSocket(this);
        if (tmp->setSocketDescriptor(socketDescriptor)) {
            tmp->write("Too many connections, try later.\r\n");
            tmp->disconnectFromHost();
        }
        tmp->deleteLater();
        logError(QStringLiteral("listener: rejecting connection, %1 active (max %2)")
                 .arg(m_active).arg(m_maxSessions));
        return;
    }
    ++m_active;
    Session *s = new Session(socketDescriptor, m_maxLineLength, m_timeoutSec, this);
    connect(s, &Session::sessionStarted, this, &Listener::sessionStarted);
    connect(s, &Session::authAttempt, this, &Listener::authAttempt);
    connect(s, &Session::sessionFinished, this, &Listener::sessionFinished);
    // Count active sessions by object lifetime (finish() ends with deleteLater).
    connect(s, &QObject::destroyed, this, &Listener::onSessionFinished);
}

void Listener::onSessionFinished() {
    if (m_active > 0)
        --m_active;
}

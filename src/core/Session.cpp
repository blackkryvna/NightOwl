#include "core/Session.h"

#include <QTcpSocket>
#include <QHostAddress>
#include <QDebug>

#include <iostream>

namespace {
// NOTE: qInfo()/qDebug() are intentionally not used here.
// The default Qt message handler is suppressed in some environments
// (verified: even qCritical produces no output), so we log via
// std::cout/std::cerr directly to guarantee the trap output is visible.
void logLine(const QString &line) {
    std::cout << line.toStdString() << std::endl; // endl flushes
}
void logError(const QString &line) {
    std::cerr << line.toStdString() << std::endl;
}
}

Session::Session(qintptr socketDescriptor, QObject *parent)
    : QObject(parent)
{
    m_socket = new QTcpSocket(this);
    if (!m_socket->setSocketDescriptor(socketDescriptor)) {
        logError(QStringLiteral("session: cannot set socket descriptor: %1")
                 .arg(m_socket->errorString()));
        m_socket->deleteLater();
        deleteLater();
        return;
    }

    const QString peer = QStringLiteral("%1:%2")
        .arg(m_socket->peerAddress().toString())
        .arg(m_socket->peerPort());
    logLine(QStringLiteral("session: new connection from %1").arg(peer));

    connect(m_socket, &QTcpSocket::readyRead, this, &Session::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &Session::onDisconnected);
}

void Session::onReadyRead() {
    const QByteArray data = m_socket->readAll();
    if (data.isEmpty())
        return;

    const QString peer = QStringLiteral("%1:%2")
        .arg(m_socket->peerAddress().toString())
        .arg(m_socket->peerPort());

    // Stage 1: dump everything. Convert lossy to text for console,
    // non-printable bytes stay visible as replacement chars.
    // Full limits/timeouts/dialog come in stage 2.
    const QString text = QString::fromUtf8(data, data.size());
    logLine(QStringLiteral("session: [%1] got %2 bytes: %3")
            .arg(peer).arg(data.size()).arg(text));
}

void Session::onDisconnected() {
    const QString peer = m_socket
        ? QStringLiteral("%1:%2").arg(m_socket->peerAddress().toString()).arg(m_socket->peerPort())
        : QStringLiteral("?");
    logLine(QStringLiteral("session: disconnected %1").arg(peer));
    deleteLater(); // RAII: socket is a child, deleted with us
}

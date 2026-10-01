#include "core/Listener.h"
#include "core/Session.h"

#include <iostream>

namespace {
void logLine(const QString &line) {
    // See Session.cpp: avoid qInfo(), it is swallowed in some environments.
    std::cout << line.toStdString() << std::endl;
}
void logError(const QString &line) {
    std::cerr << line.toStdString() << std::endl;
}
}

Listener::Listener(QObject *parent)
    : QTcpServer(parent)
{
}

bool Listener::start(quint16 port) {
    if (!listen(QHostAddress::Any, port)) {
        logError(QStringLiteral("listener: cannot listen on port %1: %2")
                 .arg(port).arg(errorString()));
        return false;
    }
    logLine(QStringLiteral("listener: listening on port %1").arg(port));
    return true;
}

void Listener::incomingConnection(qintptr socketDescriptor) {
    // Session self-manages lifetime (deleteLater on disconnect).
    // Parented to the listener so orphan sockets die with it.
    new Session(socketDescriptor, this);
}

#pragma once

#include <QTcpServer>

// Listens for inbound TCP connections and spawns a Session per client.
// QTcpServer: Qt event-driven TCP server, emits signals on new connections.
class Listener : public QTcpServer {
    Q_OBJECT
public:
    explicit Listener(QObject *parent = nullptr);

    bool start(quint16 port);

protected:
    void incomingConnection(qintptr socketDescriptor) override;
};

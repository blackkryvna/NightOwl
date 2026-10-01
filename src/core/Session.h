#pragma once

#include <QObject>

class QTcpSocket;

// One inbound client connection.
// Stage 1: just dumps everything the client sends to stdout.
class Session : public QObject {
    Q_OBJECT
public:
    explicit Session(qintptr socketDescriptor, QObject *parent = nullptr);

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    QTcpSocket *m_socket = nullptr;
};

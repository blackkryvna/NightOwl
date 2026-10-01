#pragma once

#include "core/EventQueue.h"

#include <QObject>

class QTcpSocket;
class QTimer;
class GeoLookup;

// One inbound client connection with a fake login dialog.
// QTcpSocket: event-driven TCP socket, readyRead/disconnected signals.
// Always denies access but records every attempt via signals.
class Session : public QObject {
    Q_OBJECT
public:
    explicit Session(qintptr socketDescriptor,
                     int maxLineLength = 256,
                     int timeoutSec = 60,
                     GeoLookup *geo = nullptr, // not owned, may be null
                     QObject *parent = nullptr);

    QString peerIp() const { return m_ip; }
    quint16 peerPort() const { return m_peerPort; }
    quint64 token() const { return m_token; }

public slots:
    // Asks the client connection to close (used on honeypot stop).
    void shutdown();

signals:
    void sessionStarted(const Events::SessionStarted &e);
    void authAttempt(const Events::AuthAttempt &e);
    void sessionFinished(const Events::SessionFinished &e);

private slots:
    void onReadyRead();
    void onDisconnected();
    void onTimeout();

private:
    void sendLine(const QByteArray &data);
    void promptLogin();
    void promptPassword();
    void handleLine(const QString &line);
    void finish();

    enum class State { AwaitLogin, AwaitPassword };

    QTcpSocket *m_socket = nullptr;
    QTimer *m_idleTimer = nullptr;
    QByteArray m_buffer;
    QString m_ip;
    quint16 m_peerPort = 0;
    quint64 m_token = 0;
    QString m_country;
    QString m_city;
    QString m_pendingUser;
    QDateTime m_startedAt;
    State m_state = State::AwaitLogin;
    int m_attempts = 0;
    int m_maxLineLength = 256;
    bool m_finished = false;
};

#pragma once

#include "core/EventQueue.h"

#include <QTcpServer>

class GeoLookup;

// Listens for inbound TCP connections and spawns a Session per client.
// QTcpServer: event-driven server. Enforces a cap on simultaneous sessions
// and re-emits per-session signals so UI and DB only talk to the Listener.
class Listener : public QTcpServer {
    Q_OBJECT
public:
    explicit Listener(int maxSessions = 50,
                      int maxLineLength = 256,
                      int timeoutSec = 60,
                      GeoLookup *geo = nullptr,
                      QObject *parent = nullptr);

public slots:
    // Slot so it can be invoked across threads (BlockingQueuedConnection).
    bool start(quint16 port);
    // Stops accepting; existing sessions are asked to disconnect.
    void stop();

signals:
    void sessionStarted(const Events::SessionStarted &e);
    void authAttempt(const Events::AuthAttempt &e);
    void sessionFinished(const Events::SessionFinished &e);

public:
    int activeSessions() const { return m_active; }

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onSessionFinished();

private:
    int m_maxSessions;
    int m_maxLineLength;
    int m_timeoutSec;
    GeoLookup *m_geo = nullptr; // not owned, outlives the listener
    int m_active = 0;
};

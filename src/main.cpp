#include "core/Config.h"
#include "core/EventQueue.h"
#include "core/Listener.h"

#include <QCoreApplication>

#include <iostream>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("honeyden"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    Events::registerMetaTypes();

    Config config;
    if (config.usingDefaults())
        std::cout << "config: config.ini not found or invalid, using defaults (port 2222)"
                  << std::endl;
    else
        std::cout << "config: loaded from " << config.filePath().toStdString()
                  << " port " << config.port() << std::endl;

    Listener listener(config.maxSessions(), config.maxLineLength(), config.sessionTimeoutSec());
    if (!listener.start(config.port()))
        return 1;

    // Live feed comes straight from Session signals (not via DB).
    QObject::connect(&listener, &Listener::authAttempt,
                     [](const Events::AuthAttempt &e) {
        std::cout << "auth: [" << e.ip.toStdString() << "]"
                  << " user='" << e.username.toStdString() << "'"
                  << " pass='" << e.password.toStdString() << "'"
                  << " attempt=" << e.attemptNo << std::endl;
    });

    std::cout << "honeyden: trap running. Test with: nc 127.0.0.1 "
              << config.port() << std::endl;
    return app.exec();
}

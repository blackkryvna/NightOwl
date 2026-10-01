#include "core/Config.h"
#include "core/EventQueue.h"
#include "core/GeoLookup.h"
#include "core/Listener.h"
#include "db/DbWriter.h"

#include <QCoreApplication>
#include <QThread>

#include <csignal>
#include <iostream>

namespace {
void requestQuit(int) {
    // Async-safe enough for this console trap: ask the event loop to stop
    // so main() can flush the DB queue (finishRun) before exiting.
    if (auto *app = QCoreApplication::instance())
        app->quit();
}
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("honeyden"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    Events::registerMetaTypes();
    std::signal(SIGINT, requestQuit);
    std::signal(SIGTERM, requestQuit);

    Config config;
    if (config.usingDefaults())
        std::cout << "config: config.ini not found or invalid, using defaults (port 2222)"
                  << std::endl;
    else
        std::cout << "config: loaded from " << config.filePath().toStdString()
                  << " port " << config.port() << std::endl;

    // GeoIP is optional: no file -> empty country/city, trap still works.
    GeoLookup geo;
    geo.load(config.geoipPath());

    // DB lives in its own thread; trap talks to it only via signals/slots.
    DbConfig dbCfg;
    dbCfg.host = config.dbHost();
    dbCfg.port = config.dbPort();
    dbCfg.name = config.dbName();
    dbCfg.user = config.dbUser();
    dbCfg.password = config.dbPassword();

    QThread dbThread;
    dbThread.setObjectName(QStringLiteral("db-writer"));
    DbWriter *writer = new DbWriter(dbCfg);
    writer->moveToThread(&dbThread);
    QObject::connect(&dbThread, &QThread::started, writer, &DbWriter::open);
    QObject::connect(&dbThread, &QThread::started, writer, &DbWriter::startRun);
    QObject::connect(writer, &DbWriter::errorOccurred, [](const QString &m) {
        std::cerr << "db error: " << m.toStdString() << std::endl;
    });
    dbThread.start();

    Listener listener(config.maxSessions(), config.maxLineLength(),
                      config.sessionTimeoutSec(), &geo);
    if (!listener.start(config.port())) {
        dbThread.quit();
        dbThread.wait();
        delete writer;
        return 1;
    }

    // Live feed comes straight from Session signals (not via DB).
    QObject::connect(&listener, &Listener::authAttempt,
                     [](const Events::AuthAttempt &e) {
        QString loc;
        if (!e.country.isEmpty() || !e.city.isEmpty())
            loc = QStringLiteral(" [%1/%2]").arg(e.country, e.city);
        std::cout << "auth: [" << e.ip.toStdString() << "]"
                  << loc.toStdString()
                  << " user='" << e.username.toStdString() << "'"
                  << " pass='" << e.password.toStdString() << "'"
                  << " attempt=" << e.attemptNo << std::endl;
    });
    // Persist everything (queued into the writer thread).
    QObject::connect(&listener, &Listener::sessionStarted,
                     writer, &DbWriter::onSessionStarted);
    QObject::connect(&listener, &Listener::authAttempt,
                     writer, &DbWriter::onAuthAttempt);
    QObject::connect(&listener, &Listener::sessionFinished,
                     writer, &DbWriter::onSessionFinished);

    std::cout << "honeyden: trap running. Test with: nc 127.0.0.1 "
              << config.port() << std::endl;

    const int rc = app.exec();

    // Flush the queue: finishRun + close run in the worker, blocking,
    // so ended_at is stored even on Ctrl+C / SIGTERM.
    QMetaObject::invokeMethod(writer, "finishRun", Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(writer, "close", Qt::BlockingQueuedConnection);
    dbThread.quit();
    dbThread.wait();
    delete writer;
    return rc;
}

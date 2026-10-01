#include "ui/MonitorWindow.h"
#include "ui/AttackTableModel.h"

#include "core/GeoLookup.h"
#include "core/Listener.h"
#include "db/DbWriter.h"

#include <QCloseEvent>
#include <QHeaderView>
#include <QLabel>
#include <QMetaObject>
#include <QScrollArea>
#include <QTableView>
#include <QVBoxLayout>

#include <iostream>

MonitorWindow::MonitorWindow(const Config &config, QWidget *parent)
    : QWidget(parent)
    , m_config(config)
{
    setWindowTitle(QStringLiteral("HoneyDen — monitoring"));
    resize(900, 650);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    // Vertical scroll area holding all monitoring widgets.
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    outer->addWidget(scroll);

    auto *content = new QWidget(scroll);
    scroll->setWidget(content);
    auto *layout = new QVBoxLayout(content);

    m_statusLabel = new QLabel(QStringLiteral("Starting…"), content);
    layout->addWidget(m_statusLabel);

    auto *tableLabel = new QLabel(QStringLiteral("Attacks (live, max 1000 rows)"), content);
    layout->addWidget(tableLabel);

    m_model = new AttackTableModel(this);
    m_table = new QTableView(content);
    m_table->setModel(m_model);
    m_table->setMinimumHeight(300);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table);

    // Stage 7 adds the live chart here, stage 8 the stop button.
    layout->addStretch(1);

    startTrap();
}

MonitorWindow::~MonitorWindow() {
    stopTrap();
}

void MonitorWindow::closeEvent(QCloseEvent *event) {
    QWidget::closeEvent(event);
    emit closed();
}

void MonitorWindow::startTrap() {
    // GeoIP loads before threads start, so net-thread reads are safe.
    m_geo = new GeoLookup();
    const bool geoOk = m_geo->load(m_config.geoipPath());

    DbConfig dbCfg;
    dbCfg.host = m_config.dbHost();
    dbCfg.port = m_config.dbPort();
    dbCfg.name = m_config.dbName();
    dbCfg.user = m_config.dbUser();
    dbCfg.password = m_config.dbPassword();

    m_writer = new DbWriter(dbCfg);
    m_writer->moveToThread(&m_dbThread);
    connect(&m_dbThread, &QThread::started, m_writer, &DbWriter::open);
    connect(&m_dbThread, &QThread::started, m_writer, &DbWriter::startRun);
    connect(m_writer, &DbWriter::runStarted, this, [this](int runId) {
        m_statusLabel->setText(
            QStringLiteral("Listening on %1, run #%2").arg(m_config.port()).arg(runId));
    });
    connect(m_writer, &DbWriter::errorOccurred, this, [](const QString &m) {
        std::cerr << "db error: " << m.toStdString() << std::endl;
    });
    m_dbThread.start();

    m_listener = new Listener(m_config.maxSessions(), m_config.maxLineLength(),
                              m_config.sessionTimeoutSec(), m_geo);
    m_listener->moveToThread(&m_netThread);
    // GUI table is fed straight from Session signals, not via the DB.
    connect(m_listener, &Listener::authAttempt,
            m_model, &AttackTableModel::addAttempt);
    connect(m_listener, &Listener::sessionStarted,
            m_writer, &DbWriter::onSessionStarted);
    connect(m_listener, &Listener::authAttempt,
            m_writer, &DbWriter::onAuthAttempt);
    connect(m_listener, &Listener::sessionFinished,
            m_writer, &DbWriter::onSessionFinished);
    m_netThread.start();

    bool ok = false;
    QMetaObject::invokeMethod(m_listener, "start", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, ok),
                              Q_ARG(quint16, m_config.port()));
    m_running = ok;
    if (ok) {
        QString status = QStringLiteral("Listening on %1").arg(m_config.port());
        if (!geoOk)
            status += QStringLiteral(" (no GeoIP)");
        m_statusLabel->setText(status);
    } else {
        m_statusLabel->setText(
            QStringLiteral("Cannot listen on port %1").arg(m_config.port()));
    }
}

void MonitorWindow::stopTrap() {
    if (m_listener) {
        // Runs Listener::stop() inside the net thread, blocks until done.
        QMetaObject::invokeMethod(m_listener, "stop", Qt::BlockingQueuedConnection);
    }
    m_netThread.quit();
    m_netThread.wait();
    delete m_listener;
    m_listener = nullptr;

    if (m_writer) {
        // Flush the queue: finishRun + close run in the worker, blocking.
        QMetaObject::invokeMethod(m_writer, "finishRun", Qt::BlockingQueuedConnection);
        QMetaObject::invokeMethod(m_writer, "close", Qt::BlockingQueuedConnection);
    }
    m_dbThread.quit();
    m_dbThread.wait();
    delete m_writer;
    m_writer = nullptr;

    delete m_geo;
    m_geo = nullptr;
    m_running = false;
}

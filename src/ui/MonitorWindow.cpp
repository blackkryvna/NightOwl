#include "ui/MonitorWindow.h"
#include "ui/AttackChart.h"
#include "ui/AttackTableModel.h"

#include "core/GeoLookup.h"
#include "core/Listener.h"
#include "db/DbWriter.h"

#include <QCloseEvent>
#include <QHeaderView>
#include <QLabel>
#include <QMetaObject>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTableView>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <iostream>

MonitorWindow::MonitorWindow(const Config &config, QWidget *parent)
    : QWidget(parent)
    , m_config(config)
{
    setWindowTitle(QStringLiteral("HoneyDen — monitoring"));
    resize(1100, 650);

    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    // Left side: History panel, appears after the scan stops (stage 9 fills it).
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    outer->addWidget(splitter);

    m_historySide = new QWidget(splitter);
    auto *histLayout = new QVBoxLayout(m_historySide);
    auto *histTitle = new QLabel(QStringLiteral("История"), m_historySide);
    histTitle->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold;"));
    histLayout->addWidget(histTitle);
    auto *histHint = new QLabel(QStringLiteral("Run list arrives in stage 9."), m_historySide);
    histHint->setWordWrap(true);
    histLayout->addWidget(histHint);
    histLayout->addStretch(1);
    m_historySide->setVisible(false);
    splitter->addWidget(m_historySide);

    // Vertical scroll area holding all monitoring widgets.
    auto *scroll = new QScrollArea(splitter);
    scroll->setWidgetResizable(true);
    splitter->addWidget(scroll);
    splitter->setStretchFactor(1, 1);

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

    auto *chartLabel = new QLabel(QStringLiteral("Logins over time"), content);
    layout->addWidget(chartLabel);
    m_chart = new AttackChart(120, content);
    layout->addWidget(m_chart);

    // Bottom-right: stop button under the graph.
    auto *stopRow = new QHBoxLayout();
    stopRow->addStretch(1);
    m_stopButton = new QPushButton(QStringLiteral("Остановить сканирование"), content);
    m_stopButton->setMinimumHeight(40);
    m_stopButton->setCursor(Qt::PointingHandCursor);
    connect(m_stopButton, &QPushButton::clicked, this, &MonitorWindow::onStopClicked);
    stopRow->addWidget(m_stopButton);
    layout->addLayout(stopRow);

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

void MonitorWindow::onStopClicked() {
    if (m_stopped)
        return;
    m_stopped = true;
    m_stopButton->setEnabled(false);
    m_stopButton->setText(QStringLiteral("Останавливается…"));
    // Close the listener, flush the writer queue, store runs.ended_at.
    stopTrap();
    if (m_chart)
        m_chart->setLive(false);
    m_statusLabel->setText(QStringLiteral("Stopped. Run saved to the database."));
    m_stopButton->setText(QStringLiteral("Остановлено"));
    // Open the History tab on the left (populated with real runs in stage 9).
    m_historySide->setVisible(true);
    emit scanStopped();
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
    // Same for the live chart: one bucket increment per attempt.
    connect(m_listener, &Listener::authAttempt, this, [this](const Events::AuthAttempt &e) {
        if (m_chart)
            m_chart->addEvent(e.ts);
    });
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

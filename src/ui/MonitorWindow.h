#pragma once

#include "core/Config.h"
#include "db/DbReader.h" // RunAttempt for the history slot
#include "db/DbWriter.h" // DbConfig

#include <QThread>
#include <QWidget>

class AttackTableModel;
class AttackChart;
class DbWriter;
class DbReader;
class GeoLookup;
class HistoryPanel;
class Listener;
class QLabel;
class QPushButton;
class QTableView;

// Live monitoring screen. QScrollArea gives the vertical scroll;
// on top sits the attack table (QTableView + AttackTableModel),
// below it the live chart, bottom-right the stop button.
// The trap (Listener) runs in its own thread, the DB writer in another;
// the GUI only receives queued signals and never touches sockets or SQL.
//
// After "Stop", the listener closes, the writer queue flushes, runs.ended_at
// is stored, and the History side panel appears on the left.
class MonitorWindow : public QWidget {
    Q_OBJECT
public:
    explicit MonitorWindow(const Config &config, QWidget *parent = nullptr);
    ~MonitorWindow() override;

    AttackTableModel *model() const { return m_model; }
    bool trapRunning() const { return m_running; }

signals:
    void closed();
    void scanStopped();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onStopClicked();
    void showRunDetails(int runId, const QList<RunAttempt> &attempts);

private:
    void startTrap();
    void stopTrap();
    void startHistory(); // reader thread + run list after the scan stops
    DbConfig dbConfig() const;

    Config m_config;
    AttackTableModel *m_model = nullptr;
    AttackChart *m_chart = nullptr;
    QTableView *m_table = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_stopButton = nullptr;
    QWidget *m_historySide = nullptr; // shown after stop (filled in stage 9)

    QThread m_netThread;
    QThread m_dbThread;
    QThread m_readerThread; // history SELECTs, never the GUI thread
    Listener *m_listener = nullptr;
    DbWriter *m_writer = nullptr;
    DbReader *m_reader = nullptr;
    HistoryPanel *m_history = nullptr;
    GeoLookup *m_geo = nullptr; // read from the net thread after load()

    bool m_running = false;
    bool m_stopped = false;
};

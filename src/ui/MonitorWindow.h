#pragma once

#include "core/Config.h"

#include <QThread>
#include <QWidget>

class AttackTableModel;
class DbWriter;
class GeoLookup;
class Listener;
class QLabel;
class QTableView;

// Live monitoring screen. QScrollArea gives the vertical scroll;
// on top sits the attack table (QTableView + AttackTableModel).
// The trap (Listener) runs in its own thread, the DB writer in another;
// the GUI only receives queued signals and never touches sockets or SQL.
class MonitorWindow : public QWidget {
    Q_OBJECT
public:
    explicit MonitorWindow(const Config &config, QWidget *parent = nullptr);
    ~MonitorWindow() override;

    AttackTableModel *model() const { return m_model; }
    bool trapRunning() const { return m_running; }

signals:
    void closed();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void startTrap();
    void stopTrap();

    Config m_config;
    AttackTableModel *m_model = nullptr;
    QTableView *m_table = nullptr;
    QLabel *m_statusLabel = nullptr;

    QThread m_netThread;
    QThread m_dbThread;
    Listener *m_listener = nullptr;
    DbWriter *m_writer = nullptr;
    GeoLookup *m_geo = nullptr; // read from the net thread after load()

    bool m_running = false;
};

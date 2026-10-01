#pragma once

#include <QWidget>

#include "db/DbReader.h" // RunInfo

class QListWidget;
class QListWidgetItem;

// Left History tab: list of past runs (date, duration, attack count).
// Filled by the owning MonitorWindow after the scan stops.
class HistoryPanel : public QWidget {
    Q_OBJECT
public:
    explicit HistoryPanel(QWidget *parent = nullptr);

    int runCount() const;

signals:
    void runSelected(int runId);

public slots:
    void setRuns(const QList<RunInfo> &runs);
    void showError(const QString &message);

private slots:
    void onItemClicked(QListWidgetItem *item);

private:
    static QString formatDuration(qint64 secs);

    QListWidget *m_list = nullptr;
};

#pragma once

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QWidget>

#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

QT_CHARTS_USE_NAMESPACE

class QTimer;

// Reusable attacks-per-second chart (live + history share this code).
// QChartView/QLineSeries: QtCharts view + polyline series;
// QDateTimeAxis formats X as clock time; the window slides every second.
class AttackChart : public QWidget {
    Q_OBJECT
public:
    // windowSec: how many recent seconds stay visible in live mode.
    explicit AttackChart(int windowSec = 120, QWidget *parent = nullptr);

    // Live mode: call per incoming attempt; the 1s ticker flushes buckets.
    void setLive(bool live);
    void addEvent(QDateTime ts);

    // History mode: rebuild the whole series from stored timestamps.
    void setEvents(const QList<QDateTime> &timestamps);

    int pointCount() const;

private slots:
    void onTick();

private:
    void rebuildSeries();
    void updateAxes();

    QChartView *m_view = nullptr;
    QLineSeries *m_series = nullptr;
    QDateTimeAxis *m_axisX = nullptr;
    QValueAxis *m_axisY = nullptr;
    QTimer *m_timer = nullptr;
    QMap<qint64, int> m_buckets; // unix second -> attempts
    int m_windowSec;
    bool m_live = true;
};

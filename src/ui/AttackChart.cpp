#include "ui/AttackChart.h"

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QTimer>
#include <QVBoxLayout>

AttackChart::AttackChart(int windowSec, QWidget *parent)
    : QWidget(parent)
    , m_windowSec(windowSec)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_series = new QLineSeries(this);
    m_series->setName(QStringLiteral("attempts/sec"));

    auto *chart = new QChart();
    chart->addSeries(m_series);
    chart->setTitle(QStringLiteral("Logins per second (live)"));
    chart->legend()->hide();

    m_axisX = new QDateTimeAxis(this);
    m_axisX->setFormat(QStringLiteral("HH:mm:ss"));
    m_axisX->setTitleText(QStringLiteral("time"));
    chart->addAxis(m_axisX, Qt::AlignBottom);
    m_series->attachAxis(m_axisX);

    m_axisY = new QValueAxis(this);
    m_axisY->setTitleText(QStringLiteral("attempts"));
    m_axisY->setLabelFormat(QStringLiteral("%d"));
    m_axisY->setMin(0);
    m_axisY->setMax(5);
    chart->addAxis(m_axisY, Qt::AlignLeft);
    m_series->attachAxis(m_axisY);

    m_view = new QChartView(chart, this);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setMinimumHeight(260);
    layout->addWidget(m_view);

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &AttackChart::onTick);
    m_timer->start();

    updateAxes();
}

void AttackChart::setLive(bool live) {
    m_live = live;
    if (live && !m_timer->isActive())
        m_timer->start();
    if (!live)
        m_timer->stop();
}

void AttackChart::addEvent(QDateTime ts) {
    if (!ts.isValid())
        ts = QDateTime::currentDateTimeUtc();
    const qint64 sec = ts.toSecsSinceEpoch();
    m_buckets[sec] += 1;
    if (m_live)
        rebuildSeries(); // show the spike immediately, tick slides the window
}

void AttackChart::setEvents(const QList<QDateTime> &timestamps) {
    setLive(false);
    m_buckets.clear();
    for (const QDateTime &ts : timestamps) {
        if (!ts.isValid())
            continue;
        m_buckets[ts.toSecsSinceEpoch()] += 1;
    }
    rebuildSeries();
}

int AttackChart::pointCount() const {
    return m_series ? m_series->count() : 0;
}

void AttackChart::onTick() {
    if (!m_live)
        return;
    // Drop buckets outside the sliding window so the series stays bounded.
    const qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    auto it = m_buckets.begin();
    while (it != m_buckets.end() && it.key() < now - m_windowSec)
        it = m_buckets.erase(it);
    rebuildSeries();
}

void AttackChart::rebuildSeries() {
    if (!m_live) {
        // History: one point per non-empty second across the whole run.
        m_series->clear();
        int peak = 0;
        for (auto it = m_buckets.begin(); it != m_buckets.end(); ++it) {
            const qint64 ms = it.key() * 1000;
            m_series->append(static_cast<double>(ms), static_cast<double>(it.value()));
            peak = qMax(peak, it.value());
        }
        if (!m_buckets.isEmpty()) {
            m_axisX->setRange(QDateTime::fromMSecsSinceEpoch(m_buckets.firstKey() * 1000),
                              QDateTime::fromMSecsSinceEpoch((m_buckets.lastKey() + 1) * 1000));
        }
        m_axisY->setMax(qMax(5, peak + 1));
        return;
    }
    // Live: dense points for every second of the window (zeros included).
    const qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    m_series->clear();
    int peak = 0;
    for (qint64 s = now - m_windowSec + 1; s <= now; ++s) {
        const int v = m_buckets.value(s, 0);
        m_series->append(static_cast<double>(s * 1000), static_cast<double>(v));
        peak = qMax(peak, v);
    }
    updateAxes();
    m_axisY->setMax(qMax(5, peak + 1));
}

void AttackChart::updateAxes() {
    const QDateTime now = QDateTime::currentDateTime();
    m_axisX->setRange(now.addSecs(-m_windowSec), now);
}

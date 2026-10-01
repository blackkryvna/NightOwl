#include "ui/HistoryPanel.h"

#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

HistoryPanel::HistoryPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *title = new QLabel(QStringLiteral("История"), this);
    title->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold;"));
    layout->addWidget(title);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_list, &QListWidget::itemClicked, this, &HistoryPanel::onItemClicked);
    layout->addWidget(m_list, 1);
}

int HistoryPanel::runCount() const {
    return m_list ? m_list->count() : 0;
}

QString HistoryPanel::formatDuration(qint64 secs) {
    if (secs < 0)
        secs = 0;
    if (secs < 60)
        return QStringLiteral("%1 s").arg(secs);
    if (secs < 3600)
        return QStringLiteral("%1 min").arg(secs / 60);
    return QStringLiteral("%1 h %2 min").arg(secs / 3600).arg((secs % 3600) / 60);
}

void HistoryPanel::setRuns(const QList<RunInfo> &runs) {
    m_list->clear();
    for (const RunInfo &r : runs) {
        const QString when = r.startedAt.toLocalTime()
            .toString(QStringLiteral("yyyy-MM-dd HH:mm"));
        const QString line = QStringLiteral("Run #%1 — %2, %3, %4 attacks")
            .arg(r.id).arg(when).arg(formatDuration(r.durationSec())).arg(r.attackCount);
        auto *item = new QListWidgetItem(line, m_list);
        item->setData(Qt::UserRole, r.id);
    }
    if (m_list->count() == 0)
        m_list->addItem(QStringLiteral("No runs yet."));
}

void HistoryPanel::showError(const QString &message) {
    m_list->clear();
    m_list->addItem(QStringLiteral("History unavailable: %1").arg(message));
}

void HistoryPanel::onItemClicked(QListWidgetItem *item) {
    if (!item)
        return;
    const int runId = item->data(Qt::UserRole).toInt();
    if (runId > 0)
        emit runSelected(runId);
}

#include "ui/MainMenu.h"

#include <QLabel>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QVBoxLayout>

MainMenu::MainMenu(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("HoneyDen"));
    setMinimumSize(420, 320);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(48, 48, 48, 48);
    layout->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("HoneyDen"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("font-size: 32px; font-weight: bold;"));
    layout->addWidget(title);

    auto *sub = new QLabel(QStringLiteral("SSH/Telnet honeypot monitor"), this);
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet(QStringLiteral("color: gray;"));
    layout->addWidget(sub);

    layout->addStretch(1);

    m_startButton = new QPushButton(QStringLiteral("Запустить ханипот"), this);
    m_startButton->setMinimumHeight(56);
    m_startButton->setCursor(Qt::PointingHandCursor);
    m_startButton->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 18px; border-radius: 10px; "
        "background: #2d7d46; color: white; padding: 12px; }"
        "QPushButton:hover { background: #359055; }"
        "QPushButton:disabled { background: #555; }"));
    connect(m_startButton, &QPushButton::clicked, this, &MainMenu::onStartClicked);
    layout->addWidget(m_startButton);

    m_hintLabel = new QLabel(
        QStringLiteral("Слушает порт из config.ini. Не требует root."), this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setStyleSheet(QStringLiteral("color: gray; font-size: 12px;"));
    layout->addWidget(m_hintLabel);
}

void MainMenu::onStartClicked() {
    m_startButton->setEnabled(false);
    // QPropertyAnimation: interpolates a QObject property over time.
    // Here "geometry" shrinks ~8x4 px in 90 ms, then springs back in 120 ms.
    const QRect r0 = m_startButton->geometry();
    const QPoint c = r0.center();
    const QRect r1(c.x() - r0.width() / 2 + 4, c.y() - r0.height() / 2 + 2,
                   r0.width() - 8, r0.height() - 4);

    auto *shrink = new QPropertyAnimation(m_startButton, "geometry", this);
    shrink->setDuration(90);
    shrink->setStartValue(r0);
    shrink->setEndValue(r1);
    auto *grow = new QPropertyAnimation(m_startButton, "geometry", this);
    grow->setDuration(120);
    grow->setStartValue(r1);
    grow->setEndValue(r0);

    connect(shrink, &QAbstractAnimation::finished,
            grow, [grow]() { grow->start(QAbstractAnimation::DeleteWhenStopped); });
    connect(grow, &QAbstractAnimation::finished, this, [this]() {
        m_startButton->setEnabled(true);
        emit startRequested();
    });
    shrink->start(QAbstractAnimation::DeleteWhenStopped);
}

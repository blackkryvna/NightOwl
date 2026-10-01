#pragma once

#include <QWidget>

class QPushButton;
class QLabel;

// First screen: title + a big "launch" button.
// The button plays a press animation (QPropertyAnimation over "geometry":
// shrinks a few pixels and springs back) and then emits startRequested().
class MainMenu : public QWidget {
    Q_OBJECT
public:
    explicit MainMenu(QWidget *parent = nullptr);

signals:
    void startRequested();

private slots:
    void onStartClicked();

private:
    QPushButton *m_startButton = nullptr;
    QLabel *m_hintLabel = nullptr;
};

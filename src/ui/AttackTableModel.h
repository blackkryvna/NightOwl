#pragma once

#include "core/EventQueue.h"

#include <QAbstractTableModel>
#include <QDateTime>
#include <QList>
#include <QString>

// Table model for attack attempts.
// QAbstractTableModel: data source behind QTableView; the view queries
// rowCount/columnCount/data and we notify it via begin/endInsertRows.
// Fed directly by Session signals (via Listener), NOT via the DB.
// Keeps at most 1000 latest rows in memory; everything persists in PostgreSQL.
class AttackTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    static constexpr int kMaxRows = 1000;

    enum Column {
        ColIp = 0,
        ColCountry,
        ColCity,
        ColPort,
        ColTime,
        ColLogin,
        ColPassword,
        ColAttempts,
        ColCount
    };

    struct Row {
        QString ip;
        QString country;
        QString city;
        quint16 port = 0;
        QDateTime ts;
        QString username;
        QString password;
        int attemptNo = 0;
    };

    explicit AttackTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void clear();

public slots:
    void addAttempt(const Events::AuthAttempt &e);

private:
    QList<Row> m_rows;
};

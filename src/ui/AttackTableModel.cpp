#include "ui/AttackTableModel.h"

AttackTableModel::AttackTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int AttackTableModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_rows.size();
}

int AttackTableModel::columnCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : ColCount;
}

QVariant AttackTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case ColIp:       return QStringLiteral("IP");
    case ColCountry:  return QStringLiteral("Country");
    case ColCity:     return QStringLiteral("City");
    case ColPort:     return QStringLiteral("Port");
    case ColTime:     return QStringLiteral("Time");
    case ColLogin:    return QStringLiteral("Login");
    case ColPassword: return QStringLiteral("Password");
    case ColAttempts: return QStringLiteral("Attempts");
    default:          return {};
    }
}

QVariant AttackTableModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    // Plain strings only: the default view delegate renders DisplayRole
    // as plain text (no HTML), so stolen credentials can't inject markup.
    if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
        return {};
    const Row &r = m_rows.at(index.row());
    switch (index.column()) {
    case ColIp:       return r.ip;
    case ColCountry:  return r.country;
    case ColCity:     return r.city;
    case ColPort:     return r.port;
    case ColTime:     return r.ts.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    case ColLogin:    return r.username;
    case ColPassword: return r.password;
    case ColAttempts: return r.attemptNo;
    default:          return {};
    }
}

void AttackTableModel::clear() {
    if (m_rows.isEmpty())
        return;
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

void AttackTableModel::addAttempt(const Events::AuthAttempt &e) {
    // Drop overflow from the front (oldest), keep the newest kMaxRows.
    if (m_rows.size() >= kMaxRows) {
        const int drop = m_rows.size() - kMaxRows + 1;
        beginRemoveRows(QModelIndex(), 0, drop - 1);
        m_rows.erase(m_rows.begin(), m_rows.begin() + drop);
        endRemoveRows();
    }
    const int row = m_rows.size();
    beginInsertRows(QModelIndex(), row, row);
    Row r;
    r.ip = e.ip;
    r.country = e.country;
    r.city = e.city;
    r.port = e.port;
    r.ts = e.ts;
    r.username = e.username;
    r.password = e.password;
    r.attemptNo = e.attemptNo;
    m_rows.append(r);
    endInsertRows();
}

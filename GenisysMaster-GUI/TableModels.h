#pragma once
#include "MasterSession.h"
#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <deque>

class SlaveTableModel : public QAbstractTableModel
{
public:
    explicit SlaveTableModel(MasterSession& session, QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void refresh();
private:
    MasterSession& session_;
};

class ByteTableModel : public QAbstractTableModel
{
public:
    ByteTableModel(MasterSession& session, bool controls, QObject* parent = nullptr);
    void selectSlave(int id);
    void refresh();
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    void toggle(const QModelIndex& index);
private:
    MasterSession& session_;
    bool controls_;
    int id_ = 0;
    std::vector<uint8_t> previous_;
};

class HexByteDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
};

class EventTableModel : public QAbstractTableModel
{
public:
    enum { CategoryRole = Qt::UserRole + 1, SeverityRole };
    using QAbstractTableModel::QAbstractTableModel;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void append(int slaveId, const QString& text, const QString& category, int severity);
    void clear();
private:
    struct Event { QDateTime time; int slaveId; QString text, category; int severity; };
    std::deque<Event> events_;
};

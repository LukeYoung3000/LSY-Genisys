#include "TableModels.h"
#include <QColor>
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QSpinBox>

namespace
{
    using Comm = LSY::NetworkGenisysMaster::COMMUNICATIONSTATUS;
    QString StatusName(const MasterSession& session, Comm status)
    {
        if (!session.running() && session.lastError().isEmpty()) return "STOPPED";
        return status == Comm::ONLINE ? "ONLINE" : status == Comm::OFFLINE ? "OFFLINE" : "UNKNOWN";
    }
    QIcon Dot(const QColor& color)
    {
        QPixmap image(12, 12); image.fill(Qt::transparent);
        QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen); painter.setBrush(color); painter.drawEllipse(1, 1, 10, 10);
        return QIcon(image);
    }
}
SlaveTableModel::SlaveTableModel(MasterSession& session, QObject* parent) : QAbstractTableModel(parent), session_(session)
{
    connect(&session_, &MasterSession::configurationChanged, this, [this]() { beginResetModel(); endResetModel(); });
}
int SlaveTableModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(session_.slaves().size()); }
int SlaveTableModel::columnCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : 3; }
QVariant SlaveTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount()) return {};
    const auto& state = session_.slaves()[index.row()];
    if (role == Qt::DisplayRole)
    {
        if (index.column() == 0) return int(state.configuration.id);
        if (index.column() == 1) return session_.destination(state.configuration.id);
        if (index.column() == 2) return StatusName(session_, state.status.communication);
    }
    if (role == Qt::DecorationRole && index.column() == 2)
    {
        static const QIcon online = Dot(QColor("#24a148")), offline = Dot(QColor("#da4141")), unknown = Dot(QColor("#8c98a5"));
        return session_.running() && state.status.communication == Comm::ONLINE ? online :
            state.status.communication == Comm::OFFLINE && (session_.running() || !session_.lastError().isEmpty()) ? offline : unknown;
    }
    if (role == Qt::TextAlignmentRole && index.column() == 0) return int(Qt::AlignCenter);
    if (role == Qt::ToolTipRole) return QString("%1 indications; %2 completed; %3 timeouts%4")
        .arg(state.status.indications_current ? "Current" : "Stale/unconfirmed").arg(state.status.completed_count).arg(state.status.timeout_count)
        .arg(state.status.busy ? "; request in progress" : "");
    return {};
}
QVariant SlaveTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) return QStringList{"ID", session_.serial() ? "Bus" : "Endpoint", "Status"}.value(section);
    return {};
}
void SlaveTableModel::refresh() { emit headerDataChanged(Qt::Horizontal, 1, 1); if (rowCount()) emit dataChanged(index(0, 0), index(rowCount() - 1, 2)); }

ByteTableModel::ByteTableModel(MasterSession& session, bool controls, QObject* parent)
    : QAbstractTableModel(parent), session_(session), controls_(controls) {}
void ByteTableModel::selectSlave(int id)
{
    if (id_ == id && previous_.size() == size_t(rowCount())) return;
    beginResetModel(); id_ = id; previous_.clear(); endResetModel();
    refresh();
}
int ByteTableModel::rowCount(const QModelIndex& parent) const
{
    const auto* state = session_.slave(id_);
    return parent.isValid() || !state ? 0 : static_cast<int>(state->configuration.frame_bytes);
}
int ByteTableModel::columnCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : 10; }
QVariant ByteTableModel::data(const QModelIndex& index, int role) const
{
    const auto* state = session_.slave(id_);
    if (!state || !index.isValid() || index.row() >= rowCount()) return {};
    const auto frame = controls_ ? state->controls.frame : state->indications;
    const uint8_t value = (*frame)[index.row()];
    const int bit = 9 - index.column();
    if (role == Qt::DisplayRole)
    {
        if (index.column() == 0) return index.row();
        if (index.column() == 1) return QString("0x%1").arg(value, 2, 16, QChar('0')).toUpper().replace("0X", "0x");
        return (value >> bit) & 1;
    }
    if (role == Qt::EditRole && index.column() == 1) return int(value);
    if (role == Qt::TextAlignmentRole) return int(Qt::AlignCenter);
    if (role == Qt::ForegroundRole && !controls_ && !state->status.indications_current) return QColor("#7b8490");
    if (role == Qt::BackgroundRole && index.column() >= 1)
    {
        if (controls_ && state->controls.ByteChanged(index.row())) return QColor("#fff0c2");
        if (!controls_ && state->status.indications_current && bit >= 0 && bit < 8 &&
            session_.elapsed() - state->changedAt[index.row()][bit] < 1200) return QColor("#ffe5a1");
        if ((controls_ || state->status.indications_current) && index.column() >= 2 && ((value >> bit) & 1)) return QColor("#d9f2d7");
    }
    if (role == Qt::ToolTipRole)
    {
        if (controls_) return "Local desired control, not device readback. Double-click Hex to edit; click a bit to toggle. Edits are not sent automatically.";
        return state->status.indications_current ? "Live indication. Amber highlights a recent bit change." : "Cached/unconfirmed indication. A completed recall is required for current data.";
    }
    return {};
}
QVariant ByteTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    if (section == 0) return "Byte";
    if (section == 1) return "Hex";
    return 9 - section;
}
Qt::ItemFlags ByteTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    auto result = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (controls_ && index.column() == 1) result |= Qt::ItemIsEditable;
    return result;
}
bool ByteTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!controls_ || role != Qt::EditRole || index.column() != 1) return false;
    bool ok = false; const int byte = value.toInt(&ok);
    if (!ok || !session_.stageByte(id_, index.row(), byte)) return false;
    refresh(); return true;
}
void ByteTableModel::toggle(const QModelIndex& index)
{
    if (controls_ && index.column() >= 2 && index.column() < 10 && session_.toggleBit(id_, index.row(), 9 - index.column())) refresh();
}
void ByteTableModel::refresh()
{
    const auto* state = session_.slave(id_);
    if (!state) return;
    const auto frame = controls_ ? state->controls.frame : state->indications;
    if (previous_.size() != size_t(rowCount())) previous_.assign(rowCount(), 0xFF);
    for (int row = 0; row < rowCount(); ++row)
    {
        const uint8_t value = (*frame)[row];
        if (value == previous_[row]) continue;
        previous_[row] = value;
        emit dataChanged(index(row, 0), index(row, 9), {Qt::DisplayRole, Qt::EditRole});
    }
    // Views repaint backgrounds directly; no edit-role refresh on unchanged
    // control bytes, so live polling never overwrites a user's open editor.
}
QWidget* HexByteDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const
{
    auto* editor = new QSpinBox(parent); editor->setObjectName("controlByteEditor");
    editor->setRange(0, 255); editor->setDisplayIntegerBase(16); editor->setPrefix("0x"); editor->setFrame(false);
    return editor;
}
void HexByteDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const { qobject_cast<QSpinBox*>(editor)->setValue(index.data(Qt::EditRole).toInt()); }
void HexByteDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    auto* spin = qobject_cast<QSpinBox*>(editor); spin->interpretText(); model->setData(index, spin->value(), Qt::EditRole);
}

int EventTableModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(events_.size()); }
int EventTableModel::columnCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : 3; }
QVariant EventTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount()) return {};
    const auto& event = events_[index.row()];
    if (role == CategoryRole) return event.category;
    if (role == SeverityRole) return event.severity;
    if (role == Qt::DisplayRole)
    {
        if (index.column() == 0) return event.time.toString("HH:mm:ss");
        if (index.column() == 1) return event.slaveId ? QString("Slave %1").arg(event.slaveId) : "System";
        return event.text;
    }
    if (role == Qt::ForegroundRole && event.severity) return QColor(event.severity == 2 ? "#b32929" : "#8c6100");
    if (role == Qt::ToolTipRole) return event.time.toString(Qt::ISODate) + " | " + event.category + " | " + event.text;
    return {};
}
QVariant EventTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) return QStringList{"Time", "Slave", "Event"}.value(section);
    return {};
}
void EventTableModel::append(int id, const QString& text, const QString& category, int severity)
{
    if (events_.size() >= 1000) { beginRemoveRows({}, 0, 0); events_.pop_front(); endRemoveRows(); }
    const int row = rowCount(); beginInsertRows({}, row, row);
    events_.push_back({QDateTime::currentDateTime(), id, text, category, severity}); endInsertRows();
}
void EventTableModel::clear() { beginResetModel(); events_.clear(); endResetModel(); }

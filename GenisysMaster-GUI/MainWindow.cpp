#include "MainWindow.h"
#include "TableModels.h"
#include "ui_MainWindow.h"
#include "GenisysMaster-Lib-Version.h"
#include "third_party/pugixml/pugixml.hpp"
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTableView>
#include <QToolBar>
#include <QVBoxLayout>
#include <QStyle>
#include <QStackedWidget>
#include <Windows.h>
#include <QCollator>
#pragma comment(lib, "Advapi32.lib")
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace
{
    using Comm = LSY::NetworkGenisysMaster::COMMUNICATIONSTATUS;
    ConsoleConfiguration Defaults() { ConsoleConfiguration result; result.slaves.push_back({34, {"127.0.0.1", 7169}, 125}); return result; }
    QSpinBox* Spin(QWidget* parent, const char* name, int minimum, int maximum, const QString& suffix = {})
    {
        auto* spin = new QSpinBox(parent); spin->setObjectName(name); spin->setRange(minimum, maximum); spin->setSuffix(suffix); return spin;
    }
    QPushButton* Button(const QString& text, const char* name, QWidget* parent)
    { auto* button = new QPushButton(text, parent); button->setObjectName(name); return button; }
    void PrepareTable(QTableView* table)
    {
        table->verticalHeader()->hide(); table->verticalHeader()->setDefaultSectionSize(28);
        table->setAlternatingRowColors(true); table->setShowGrid(true); table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->horizontalHeader()->setHighlightSections(false);
    }
    QString Hex(int value) { return QString("0x%1").arg(value, 2, 16, QChar('0')).toUpper().replace("0X", "0x"); }
}
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui_(std::make_unique<Ui::MainWindow>())
{
    ui_->setupUi(this); setMinimumSize(1040, 720); resize(1440, 940); setWindowTitle("Genisys Master[*]");
    buildInterface(); connectInterface(); session_.applyConfiguration(Defaults());
    refreshTimer_.setInterval(100); connect(&refreshTimer_, &QTimer::timeout, this, &MainWindow::refresh); refreshTimer_.start(); refresh();
}
MainWindow::~MainWindow() { refreshTimer_.stop(); disconnect(&session_, nullptr, this, nullptr); session_.stop(); }
void MainWindow::buildInterface()
{
    setStyleSheet(
        "QMainWindow { background:#f3f5f7; }"
        "QGroupBox { background:white; border:1px solid #d4dae1; border-radius:4px; margin-top:12px; padding:8px; }"
        "QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 4px; color:#263342; font-weight:600; }"
        "QTableView { background:white; alternate-background-color:#fafbfc; gridline-color:#e0e4e9; border:1px solid #d5dbe2; }"
        "QHeaderView::section { background:#edf0f4; padding:6px; border:0; border-right:1px solid #d5dbe2; border-bottom:1px solid #d5dbe2; font-weight:600; }"
        "QPushButton { padding:5px 10px; }"
        "QPushButton#sendChangesButton { background:#0869c8; color:white; border:1px solid #0759aa; border-radius:3px; }"
        "QPushButton#sendChangesButton:disabled { background:#d8dfe7; color:#7e8793; border-color:#d8dfe7; }"
        "QToolBar { spacing:8px; padding:6px; background:#fafbfc; border-bottom:1px solid #d5dbe2; }"
    );
    auto* toolbar = addToolBar("Connection"); toolbar->setObjectName("mainToolbar"); toolbar->setMovable(false); toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    newAction_ = new QAction(style()->standardIcon(QStyle::SP_FileIcon), "New configuration", this); newAction_->setShortcut(QKeySequence::New);
    openAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton), "Open XML", this); openAction_->setShortcut(QKeySequence::Open); openAction_->setObjectName("openConfigAction");
    saveAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), "Save XML", this); saveAction_->setShortcut(QKeySequence::Save);
    saveAsAction_ = new QAction("Save XML as...", this); saveAsAction_->setShortcut(QKeySequence::SaveAs);
    for (auto* action : {newAction_, openAction_, saveAction_, saveAsAction_}) ui_->menuFile->insertAction(ui_->actionExit, action);
    toolbar->addAction(openAction_); toolbar->addAction(saveAction_); toolbar->addSeparator();
    runAction_ = toolbar->addAction(style()->standardIcon(QStyle::SP_MediaPlay), "Start"); runAction_->setObjectName("runAction");
    connectionBadge_ = new QLabel("Stopped", toolbar); connectionBadge_->setObjectName("connectionBadge"); toolbar->addWidget(connectionBadge_);
    auto* connectionMenu = new QMenu("&Connection", this); menuBar()->insertMenu(ui_->menuHelp->menuAction(), connectionMenu); connectionMenu->addAction(runAction_);
    recallAction_ = connectionMenu->addAction("Recall selected"); recallAllAction_ = connectionMenu->addAction("Recall all");
    auto* root = new QVBoxLayout(ui_->centralWidget); root->setContentsMargins(8, 4, 8, 8); root->setSpacing(6);
    transportPanel_ = new QWidget(ui_->centralWidget); auto* settings = new QGridLayout(transportPanel_); settings->setContentsMargins(2, 0, 2, 0);
    transport_ = new QComboBox(transportPanel_); transport_->setObjectName("transportCombo"); transport_->addItems({"UDP", "Serial (COM)"});
    settings->addWidget(new QLabel("Transport", transportPanel_), 0, 0); settings->addWidget(transport_, 0, 1);
    transportPages_ = new QStackedWidget(transportPanel_);
    auto* udpPage = new QWidget(transportPages_); auto* udpSettings = new QHBoxLayout(udpPage); udpSettings->setContentsMargins(4, 0, 0, 0);
    localIp_ = new QLineEdit(udpPage); localIp_->setObjectName("localIpEdit"); localIp_->setMaximumWidth(150);
    localPort_ = Spin(udpPage, "localPortSpin", 0, 65535); localPort_->setToolTip("Zero asks Windows to choose a receive port.");
    udpSettings->addWidget(new QLabel("Local IPv4", udpPage)); udpSettings->addWidget(localIp_);
    udpSettings->addWidget(new QLabel("Receive port", udpPage)); udpSettings->addWidget(localPort_); udpSettings->addStretch();
    transportPages_->addWidget(udpPage);
    auto* serialPage = new QWidget(transportPages_); auto* serialSettings = new QGridLayout(serialPage); serialSettings->setContentsMargins(4, 0, 0, 0);
    comPort_ = new QComboBox(serialPage); comPort_->setObjectName("comPortCombo"); comPort_->setEditable(true); comPort_->setInsertPolicy(QComboBox::NoInsert); comPort_->setMinimumContentsLength(7);
    comPort_->setToolTip("Select a detected port or type a COM name. Refresh preserves the current value.");
    auto* refresh = Button("Refresh", "refreshPortsButton", serialPage); refresh->setToolTip("Refresh the COM ports reported by Windows.");
    baud_ = new QComboBox(serialPage); baud_->setObjectName("baudCombo"); baud_->setEditable(true); baud_->setInsertPolicy(QComboBox::NoInsert);
    baud_->addItems({"300", "600", "1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"});
    parity_ = new QComboBox(serialPage); parity_->setObjectName("parityCombo"); parity_->addItem("None", "N"); parity_->addItem("Even", "E"); parity_->addItem("Odd", "O");
    stopBits_ = new QComboBox(serialPage); stopBits_->setObjectName("stopBitsCombo"); stopBits_->addItem("1", 1); stopBits_->addItem("2", 2);
    writeTimeout_ = Spin(serialPage, "writeTimeoutSpin", 1, 600000, " ms");
    dtr_ = new QCheckBox("DTR", serialPage); dtr_->setObjectName("dtrCheck"); rts_ = new QCheckBox("RTS", serialPage); rts_->setObjectName("rtsCheck");
    for (auto* check : {dtr_, rts_}) check->setToolTip("Assert a fixed line level. Flow control and manual RS-485 direction switching are disabled.");
    serialSettings->addWidget(new QLabel("COM port", serialPage), 0, 0); serialSettings->addWidget(comPort_, 0, 1); serialSettings->addWidget(refresh, 0, 2);
    serialSettings->addWidget(new QLabel("Baud", serialPage), 0, 3); serialSettings->addWidget(baud_, 0, 4);
    serialSettings->addWidget(new QLabel("Parity", serialPage), 0, 5); serialSettings->addWidget(parity_, 0, 6);
    serialSettings->addWidget(new QLabel("Stop bits", serialPage), 0, 7); serialSettings->addWidget(stopBits_, 0, 8);
    serialSettings->addWidget(new QLabel("Write timeout", serialPage), 0, 9); serialSettings->addWidget(writeTimeout_, 0, 10);
    auto* serialNote = new QLabel("8 data bits  |  RS-485: use an adapter with automatic direction control.", serialPage); serialNote->setStyleSheet("color:#647184;");
    serialSettings->addWidget(dtr_, 1, 0); serialSettings->addWidget(rts_, 1, 1); serialSettings->addWidget(serialNote, 1, 2, 1, 9);
    transportPages_->addWidget(serialPage); settings->addWidget(transportPages_, 0, 2, 1, 10);
    pollInterval_ = Spin(transportPanel_, "pollIntervalSpin", 1, 600000, " ms"); timeout_ = Spin(transportPanel_, "timeoutSpin", 1, 600000, " ms");
    retries_ = Spin(transportPanel_, "retriesSpin", 0, 100); recovery_ = Spin(transportPanel_, "recoverySpin", 1, 600000, " ms");
    automatic_ = new QCheckBox("Automatic polling", transportPanel_); automatic_->setObjectName("automaticPollingCheck");
    secure_ = new QCheckBox("Secure polls (CRC)", transportPanel_); secure_->setObjectName("securePollingCheck");
    settings->addWidget(new QLabel("Poll", transportPanel_), 1, 0); settings->addWidget(pollInterval_, 1, 1);
    settings->addWidget(new QLabel("Response timeout", transportPanel_), 1, 2); settings->addWidget(timeout_, 1, 3);
    settings->addWidget(new QLabel("Retries", transportPanel_), 1, 4); settings->addWidget(retries_, 1, 5);
    settings->addWidget(new QLabel("Recovery", transportPanel_), 1, 6); settings->addWidget(recovery_, 1, 7);
    settings->addWidget(automatic_, 1, 8, 1, 2); settings->addWidget(secure_, 1, 10, 1, 2); root->addWidget(transportPanel_);
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshPorts); refreshPorts();
    auto* vertical = new QSplitter(Qt::Vertical, ui_->centralWidget); vertical->setObjectName("verticalSplitter"); vertical->setChildrenCollapsible(false); root->addWidget(vertical, 1);
    auto* horizontal = new QSplitter(Qt::Horizontal, vertical); horizontal->setObjectName("mainSplitter"); horizontal->setChildrenCollapsible(false);
    auto* left = new QWidget(horizontal); left->setMinimumWidth(310); auto* leftLayout = new QVBoxLayout(left); leftLayout->setContentsMargins(0, 0, 0, 0);
    auto* slaveGroup = new QGroupBox("Slaves", left); auto* slaveLayout = new QVBoxLayout(slaveGroup);
    slaveTable_ = new QTableView(slaveGroup); slaveTable_->setObjectName("slaveTable"); PrepareTable(slaveTable_);
    slaveModel_ = new SlaveTableModel(session_, this); slaveTable_->setModel(slaveModel_); slaveTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    slaveTable_->setEditTriggers(QAbstractItemView::NoEditTriggers); slaveTable_->setMinimumHeight(125);
    slaveTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents); slaveTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch); slaveTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    slaveLayout->addWidget(slaveTable_, 1); onlineCount_ = new QLabel(slaveGroup); onlineCount_->setObjectName("onlineCountLabel"); slaveLayout->addWidget(onlineCount_);
    auto* actions = new QHBoxLayout; add_ = Button("Add", "addSlaveButton", slaveGroup); edit_ = Button("Edit", "editSlaveButton", slaveGroup); remove_ = Button("Remove", "removeSlaveButton", slaveGroup);
    add_->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder)); remove_->setIcon(style()->standardIcon(QStyle::SP_DialogDiscardButton));
    actions->addWidget(add_); actions->addWidget(edit_); actions->addWidget(remove_); slaveLayout->addLayout(actions);
    auto* recalls = new QHBoxLayout; recall_ = Button("Recall selected", "recallSelectedButton", slaveGroup); recallAll_ = Button("Recall all", "recallAllButton", slaveGroup);
    recalls->addWidget(recall_); recalls->addWidget(recallAll_); slaveLayout->addLayout(recalls); leftLayout->addWidget(slaveGroup, 1);
    auto* detailGroup = new QGroupBox("Selected slave", left); auto* detail = new QFormLayout(detailGroup);
    detailsTitle_ = new QLabel(detailGroup); detailsTitle_->setStyleSheet("font-weight:600; font-size:14px;"); detailsTitle_->setObjectName("selectedSlaveLabel");
    frameCounts_ = new QLabel(detailGroup); dataState_ = new QLabel(detailGroup); completed_ = new QLabel(detailGroup); timeouts_ = new QLabel(detailGroup);
    detail->addRow(detailsTitle_); detail->addRow(frameCounts_); detail->addRow("Indications:", dataState_); detail->addRow("Completed:", completed_); detail->addRow("Timeouts:", timeouts_); leftLayout->addWidget(detailGroup);
    auto* right = new QSplitter(Qt::Vertical, horizontal); right->setChildrenCollapsible(false);
    auto* indicationGroup = new QGroupBox("Live indications", right); auto* indicationLayout = new QVBoxLayout(indicationGroup); indicationGroup->setMinimumHeight(210);
    indicationState_ = new QLabel(indicationGroup); indicationState_->setObjectName("indicationStateLabel"); indicationLayout->addWidget(indicationState_);
    indicationTable_ = new QTableView(indicationGroup); indicationTable_->setObjectName("indicationTable"); PrepareTable(indicationTable_);
    indicationModel_ = new ByteTableModel(session_, false, this); indicationTable_->setModel(indicationModel_); indicationTable_->setEditTriggers(QAbstractItemView::NoEditTriggers); indicationLayout->addWidget(indicationTable_, 1);
    auto* indicationFooter = new QHBoxLayout; indicationMode_ = new QLabel(indicationGroup); updated_ = new QLabel(indicationGroup);
    indicationFooter->addWidget(indicationMode_); indicationFooter->addStretch(); indicationFooter->addWidget(updated_); indicationLayout->addLayout(indicationFooter);
    auto* controlGroup = new QGroupBox("Control editor", right); controlGroup->setMinimumHeight(265); auto* controlLayout = new QVBoxLayout(controlGroup);
    controlLayout->addWidget(new QLabel("Local draft - edit Hex or click bits. Nothing is sent until you click Send.", controlGroup));
    controlTable_ = new QTableView(controlGroup); controlTable_->setObjectName("controlTable"); PrepareTable(controlTable_);
    controlModel_ = new ByteTableModel(session_, true, this); controlTable_->setModel(controlModel_); controlTable_->setItemDelegateForColumn(1, new HexByteDelegate(controlTable_));
    controlTable_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed); controlTable_->installEventFilter(this); controlLayout->addWidget(controlTable_, 1);
    for (auto* table : {indicationTable_, controlTable_})
    {
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed); table->setColumnWidth(0, 58);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed); table->setColumnWidth(1, 80);
    }
    auto* sending = new QHBoxLayout; includeMode_ = new QCheckBox("Include mode", controlGroup); includeMode_->setObjectName("includeModeCheck");
    mode_ = Spin(controlGroup, "controlModeSpin", 0, 255); mode_->setDisplayIntegerBase(16); mode_->setPrefix("0x"); mode_->setMaximumWidth(90);
    sendFull_ = Button("Send full frame", "sendFullButton", controlGroup); send_ = Button("Send changes", "sendChangesButton", controlGroup);
    sendFull_->setToolTip("Explicitly send EVERY local control byte, including zeros. These are desired values, not readback.");
    sending->addWidget(includeMode_); sending->addWidget(mode_); sending->addStretch(); sending->addWidget(sendFull_); sending->addWidget(send_); controlLayout->addLayout(sending);
    controlResult_ = new QLabel(controlGroup); controlResult_->setObjectName("controlResultLabel"); controlResult_->setTextInteractionFlags(Qt::TextSelectableByMouse); controlResult_->setWordWrap(true); controlLayout->addWidget(controlResult_);
    auto* events = new QGroupBox("Events", vertical); auto* eventLayout = new QVBoxLayout(events); auto* eventActions = new QHBoxLayout;
    auto* filter = new QComboBox(events); filter->setObjectName("eventFilterCombo"); filter->addItems({"All events", "Indications", "Controls", "Status", "Warnings / errors"}); filter->setMaximumWidth(190);
    followEvents_ = new QCheckBox("Follow", events); followEvents_->setChecked(true); auto* clear = Button("Clear", "clearEventsButton", events);
    eventActions->addWidget(filter); eventActions->addWidget(followEvents_); eventActions->addStretch(); eventActions->addWidget(clear); eventLayout->addLayout(eventActions);
    eventTable_ = new QTableView(events); eventTable_->setObjectName("eventTable"); PrepareTable(eventTable_); eventTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    eventModel_ = new EventTableModel(this); eventFilter_ = new QSortFilterProxyModel(this); eventFilter_->setSourceModel(eventModel_); eventFilter_->setFilterKeyColumn(0); eventTable_->setModel(eventFilter_);
    eventTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed); eventTable_->setColumnWidth(0, 100); eventTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed); eventTable_->setColumnWidth(1, 100);
    eventTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch); eventLayout->addWidget(eventTable_);
    connect(clear, &QPushButton::clicked, eventModel_, &EventTableModel::clear);
    connect(filter, &QComboBox::currentIndexChanged, this, [this, filter](int index)
    {
        eventFilter_->setFilterRole(index == 4 ? EventTableModel::SeverityRole : EventTableModel::CategoryRole);
        eventFilter_->setFilterRegularExpression(index == 0 ? "" : index == 4 ? "[12]" : "^" + filter->currentText() + "$");
    });
    horizontal->setSizes({360, 1000}); right->setSizes({285, 300}); vertical->setSizes({610, 180});
}
void MainWindow::connectInterface()
{
    connect(ui_->actionExit, &QAction::triggered, this, &QWidget::close);
    connect(ui_->actionAbout, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "Genisys Master", "Genisys Master test application\nQt Widgets / native C++ master library\n\nControls are desired values, not physical output readback.");
    });
    connect(newAction_, &QAction::triggered, this, [this]() { session_.applyConfiguration(Defaults()); path_.clear(); setWindowModified(false); });
    connect(openAction_, &QAction::triggered, this, &MainWindow::chooseOpen);
    connect(saveAction_, &QAction::triggered, this, [this]() { chooseSave(false); });
    connect(saveAsAction_, &QAction::triggered, this, [this]() { chooseSave(true); });
    connect(runAction_, &QAction::triggered, this, &MainWindow::toggleRunning);
    connect(add_, &QPushButton::clicked, this, [this]() { editSlave(true); });
    connect(edit_, &QPushButton::clicked, this, [this]() { editSlave(false); });
    connect(remove_, &QPushButton::clicked, this, &MainWindow::removeSlave);
    connect(recall_, &QPushButton::clicked, this, [this]() { session_.recall(selectedId_); });
    connect(recallAction_, &QAction::triggered, this, [this]() { session_.recall(selectedId_); });
    connect(recallAll_, &QPushButton::clicked, &session_, &MasterSession::recallAll);
    connect(recallAllAction_, &QAction::triggered, &session_, &MasterSession::recallAll);
    connect(send_, &QPushButton::clicked, this, [this]() { session_.sendControl(selectedId_, false); });
    connect(sendFull_, &QPushButton::clicked, this, [this]() { session_.sendControl(selectedId_, true); });
    connect(controlTable_, &QTableView::clicked, controlModel_, &ByteTableModel::toggle);
    connect(includeMode_, &QCheckBox::toggled, this, [this]() { if (!loading_) session_.stageMode(selectedId_, includeMode_->isChecked(), mode_->value()); });
    connect(mode_, &QSpinBox::valueChanged, this, [this]() { if (!loading_) session_.stageMode(selectedId_, includeMode_->isChecked(), mode_->value()); });
    connect(slaveTable_->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &MainWindow::selectionChanged);
    connect(&session_, &MasterSession::configurationChanged, this, &MainWindow::syncConfiguration);
    connect(&session_, &MasterSession::changed, this, [this]() { pendingRefresh_ = true; });
    connect(&session_, &MasterSession::eventAdded, this, [this](int id, const QString& text, const QString& category, int severity) {
        eventModel_->append(id, text, category, severity);
        if (followEvents_->isChecked()) eventTable_->scrollToBottom();
    });
    connect(localIp_, &QLineEdit::textEdited, this, &MainWindow::setDirty);
    connect(transport_, &QComboBox::currentIndexChanged, this, [this](int index) { transportPages_->setCurrentIndex(index); setDirty(); });
    for (auto* combo : {comPort_, baud_, parity_, stopBits_}) connect(combo, &QComboBox::currentTextChanged, this, &MainWindow::setDirty);
    for (auto* spin : {localPort_, pollInterval_, timeout_, retries_, recovery_, writeTimeout_}) connect(spin, &QSpinBox::valueChanged, this, &MainWindow::setDirty);
    for (auto* check : {automatic_, secure_, dtr_, rts_}) connect(check, &QCheckBox::toggled, this, &MainWindow::setDirty);
}
LSY::NetworkGenisysMaster::Config MainWindow::transportConfiguration() const
{
    auto config = session_.configuration().network;
    config.connection_type_ = transport_->currentIndex() == 1 ? LSY::NetworkGenisysMaster::Config::CONNECTIONTYPE::SERIAL : LSY::NetworkGenisysMaster::Config::CONNECTIONTYPE::UDP;
    config.com_port_ = comPort_->currentText().trimmed().toStdString();
    bool baudOk = false; config.baud_rate_ = baud_->currentText().trimmed().toUInt(&baudOk);
    if (!baudOk || !config.baud_rate_) throw std::runtime_error("Baud rate must be a positive whole number (up to 4294967295).");
    config.data_bits_ = 8; config.parity_ = parity_->currentData().toString().toLatin1().at(0); config.stop_bits_ = static_cast<uint8_t>(stopBits_->currentData().toUInt());
    config.write_timeout_ms_ = writeTimeout_->value(); config.dtr_enable_ = dtr_->isChecked(); config.rts_enable_ = rts_->isChecked();
    config.local_ip_ = localIp_->text().trimmed().toStdString();
    config.receive_port_ = static_cast<uint16_t>(localPort_->value());
    config.poll_interval_ms_ = pollInterval_->value(); config.response_timeout_ms_ = timeout_->value();
    config.retry_count_ = retries_->value(); config.recovery_interval_ms_ = recovery_->value();
    config.automatic_polling_ = automatic_->isChecked(); config.secure_poll_ = secure_->isChecked();
    return config;
}
void MainWindow::setDirty()
{
    if (loading_) return;
    setWindowModified(true); pendingRefresh_ = true;
    if (!session_.running())
        try { session_.setTransportConfiguration(transportConfiguration()); }
        catch (const std::exception&) { /* Keep the last valid preview; Start/Save report incomplete fields. */ }
}
void MainWindow::refreshPorts()
{
    const QString current = comPort_->currentText(); const bool wasLoading = loading_; loading_ = true;
    QStringList names; HKEY ports = nullptr;
    // Enumerate values directly: SERIALCOMM value names contain backslashes.
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_QUERY_VALUE, &ports) == ERROR_SUCCESS)
    {
        for (DWORD index = 0; ; ++index)
        {
            wchar_t valueName[256]{}, value[256]{}; DWORD nameSize = 256, bytes = sizeof(value), type = 0;
            const auto result = RegEnumValueW(ports, index, valueName, &nameSize, nullptr, &type, reinterpret_cast<BYTE*>(value), &bytes);
            if (result == ERROR_NO_MORE_ITEMS) break;
            if (result != ERROR_SUCCESS || type != REG_SZ) continue;
            auto name = QString::fromWCharArray(value, static_cast<int>(bytes / sizeof(wchar_t)));
            if (name.endsWith(QChar::Null)) name.chop(1);
            if (name.startsWith("COM", Qt::CaseInsensitive)) names.append(name);
        }
        RegCloseKey(ports);
    }
    names.removeDuplicates(); QCollator collator; collator.setNumericMode(true);
    std::sort(names.begin(), names.end(), [&collator](const QString& left, const QString& right) { return collator.compare(left, right) < 0; });
    comPort_->clear(); comPort_->addItems(names); comPort_->setEditText(current.isEmpty() ? "COM1" : current); loading_ = wasLoading;
}
void MainWindow::syncConfiguration()
{
    loading_ = true;
    const auto& config = session_.configuration().network;
    transport_->setCurrentIndex(session_.serial() ? 1 : 0); transportPages_->setCurrentIndex(transport_->currentIndex());
    comPort_->setEditText(QString::fromStdString(config.com_port_)); baud_->setEditText(QString::number(config.baud_rate_));
    parity_->setCurrentIndex(parity_->findData(QString(QChar(config.parity_)))); stopBits_->setCurrentIndex(stopBits_->findData(int(config.stop_bits_)));
    writeTimeout_->setValue(config.write_timeout_ms_); dtr_->setChecked(config.dtr_enable_); rts_->setChecked(config.rts_enable_);
    localIp_->setText(QString::fromStdString(config.local_ip_)); localPort_->setValue(config.receive_port_);
    pollInterval_->setValue(config.poll_interval_ms_); timeout_->setValue(config.response_timeout_ms_);
    retries_->setValue(config.retry_count_); recovery_->setValue(config.recovery_interval_ms_);
    automatic_->setChecked(config.automatic_polling_); secure_->setChecked(config.secure_poll_);
    indicationModel_->selectSlave(0); controlModel_->selectSlave(0); selectedId_ = 0;
    loading_ = false;
    if (!session_.slaves().empty()) selectSlave(session_.slaves().front().configuration.id);
    else selectionChanged();
    pendingRefresh_ = true;
}
void MainWindow::selectSlave(int id)
{
    for (int row = 0; row < slaveModel_->rowCount(); ++row)
        if (slaveModel_->data(slaveModel_->index(row, 0)).toInt() == id) {
            slaveTable_->setCurrentIndex(slaveModel_->index(row, 0)); slaveTable_->selectRow(row); selectionChanged(); return;
        }
}
void MainWindow::selectionChanged()
{
    selectedId_ = slaveTable_->currentIndex().isValid() ? slaveModel_->data(slaveModel_->index(slaveTable_->currentIndex().row(), 0)).toInt() : 0;
    indicationModel_->selectSlave(selectedId_); controlModel_->selectSlave(selectedId_);
    loading_ = true;
    const auto* state = session_.slave(selectedId_);
    includeMode_->setChecked(state && state->controls.include_mode); mode_->setValue(state ? state->controls.mode : 5);
    loading_ = false; pendingRefresh_ = true;
}
void MainWindow::refresh()
{
    indicationTable_->viewport()->update(); controlTable_->viewport()->update();
    if (!pendingRefresh_) return;
    pendingRefresh_ = false;
    slaveModel_->refresh(); indicationModel_->refresh(); controlModel_->refresh();
    const bool running = session_.running(); const auto* state = session_.slave(selectedId_);
    transportPanel_->setEnabled(!running); openAction_->setEnabled(!running); newAction_->setEnabled(!running);
    runAction_->setText(running ? "Stop" : "Start"); runAction_->setIcon(style()->standardIcon(running ? QStyle::SP_MediaStop : QStyle::SP_MediaPlay));
    runAction_->setEnabled(running || !session_.slaves().empty());
    add_->setEnabled(!running); edit_->setEnabled(!running && state); remove_->setEnabled(!running && state);
    connectionBadge_->setText(running ? (session_.serial() ? "  Serial running  " : "  UDP running  ") : session_.lastError().isEmpty() ? "  Stopped  " : "  Connection error  ");
    connectionBadge_->setToolTip(session_.lastError());
    connectionBadge_->setStyleSheet(running ? "color:#147344; font-weight:600;" : session_.lastError().isEmpty() ? "color:#647184; font-weight:600;" : "color:#b32929; font-weight:600;");
    recall_->setEnabled(running && state); recallAction_->setEnabled(recall_->isEnabled());
    recallAll_->setEnabled(running); recallAllAction_->setEnabled(running);
    sendFull_->setEnabled(running && state && !session_.busyForControl(selectedId_));
    send_->setEnabled(sendFull_->isEnabled() && (state->controls.Dirty() || state->controls.include_mode));
    controlTable_->setEnabled(state != nullptr); includeMode_->setEnabled(state != nullptr); mode_->setEnabled(state && includeMode_->isChecked());
    unsigned online = 0; for (const auto& item : session_.slaves()) if (item.status.communication == Comm::ONLINE) ++online;
    onlineCount_->setText(QString("%1 / %2 online").arg(online).arg(session_.slaves().size()));
    statusBar()->showMessage(QString("%1  |  %2  |  %3")
        .arg(running ? "Running" : session_.lastError().isEmpty() ? "Stopped" : session_.lastError())
        .arg(QString::fromStdString(ConsoleTransportDescription(session_.configuration().network, session_.localPort())))
        .arg(path_.isEmpty() ? "Unsaved configuration" : path_));
    if (!state) {
        detailsTitle_->setText("No slave selected"); frameCounts_->clear(); dataState_->clear(); completed_->clear(); timeouts_->clear();
        indicationState_->setText("No data"); indicationMode_->clear(); updated_->clear(); controlResult_->clear(); return;
    }
    detailsTitle_->setText(QString("Slave %1  |  %2").arg(selectedId_).arg(session_.destination(selectedId_)));
    frameCounts_->setText(QString("%1 bytes / %2 bits").arg(state->configuration.frame_bytes).arg(state->configuration.frame_bytes * 8));
    const bool current = running && state->status.indications_current;
    dataState_->setText(current ? "Current" : "Stale cache");
    completed_->setText(QString("%1").arg(state->status.completed_count)); timeouts_->setText(QString("%1").arg(state->status.timeout_count));
    indicationState_->setText(current ? "LIVE" : "STALE / NOT RECEIVED");
    indicationState_->setStyleSheet(current ? "color:#147344; font-weight:600;" : "color:#946116; font-weight:600;");
    indicationMode_->setText("Mode: " + Hex(state->indications->GetModeByte()));
    updated_->setText("Last data: " + (state->lastUpdate.isValid() ? state->lastUpdate.toString("HH:mm:ss.zzz") : "never"));
    const QString result = QString::fromStdString(state->controls.Result());
    controlResult_->setText(result + (state->controls.Dirty() ? "  |  Modified draft" : ""));
    controlResult_->setStyleSheet(result.startsWith("ACKNOWLEDGED") ? "color:#147344;" : result.contains("unknown") ? "color:#946116;" : "color:#526173;");
}
void MainWindow::toggleRunning()
{
    try {
        if (session_.running()) session_.stop();
        else { session_.setTransportConfiguration(transportConfiguration()); session_.start(); }
    } catch (const std::exception& error) { QMessageBox::warning(this, "Cannot start communications", QString::fromUtf8(error.what())); }
    pendingRefresh_ = true; refresh();
}
void MainWindow::editSlave(bool adding)
{
    if (session_.running()) return;
    const auto* state = session_.slave(selectedId_); if (!adding && !state) return;
    ConsoleSlaveConfiguration item = state && !adding ? state->configuration : ConsoleSlaveConfiguration{};
    if (adding) { for (int id = 1; id <= 255; ++id) if (!session_.slave(id)) { item.id = static_cast<uint8_t>(id); break; } }
    QDialog dialog(this); dialog.setObjectName("slaveDialog"); dialog.setWindowTitle(adding ? "Add slave" : "Edit slave");
    auto* layout = new QFormLayout(&dialog);
    auto* id = Spin(&dialog, "slaveIdSpin", 1, 255); id->setValue(item.id);
    const bool serial = transport_->currentIndex() == 1;
    auto* ip = new QLineEdit(item.has_endpoint ? QString::fromStdString(item.endpoint.ip_address_) : QString(), &dialog); ip->setObjectName("slaveIpEdit");
    auto* port = Spin(&dialog, "slavePortSpin", 1, 65535); port->setValue(item.endpoint.port_);
    auto* bytes = Spin(&dialog, "slaveBytesSpin", 1, 224); bytes->setValue(item.frame_bytes);
    layout->addRow("Slave ID", id);
    if (!serial) { layout->addRow("Remote IPv4", ip); layout->addRow("Remote UDP port", port); } else { ip->hide(); port->hide(); }
    layout->addRow("Frame bytes", bytes);
    auto* note = new QLabel(serial ? "All IDs share the selected COM port.\nApplying changes resets local drafts and cached indications." : "Multiple IDs may share the same IP and port.\nApplying changes resets local drafts and cached indications.", &dialog); layout->addRow(note);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog); buttons->setObjectName("slaveDialogButtons"); layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    while (dialog.exec() == QDialog::Accepted) {
        auto config = session_.configuration(); config.network = transportConfiguration();
        item.id = static_cast<uint8_t>(id->value()); item.frame_bytes = bytes->value();
        if (!serial) { item.endpoint = {ip->text().trimmed().toStdString(), static_cast<uint16_t>(port->value())}; item.has_endpoint = true; }
        else if (adding) item.has_endpoint = false;
        if (adding) config.slaves.push_back(item);
        else for (auto& candidate : config.slaves) if (candidate.id == selectedId_) { candidate = item; break; }
        try { session_.applyConfiguration(config); selectSlave(item.id); setDirty(); break; }
        catch (const std::exception& error) { QMessageBox::warning(&dialog, "Invalid slave configuration", QString::fromUtf8(error.what())); }
    }
}
void MainWindow::removeSlave()
{
    if (session_.running() || !session_.slave(selectedId_)) return;
    auto config = session_.configuration(); config.network = transportConfiguration();
    config.slaves.erase(std::remove_if(config.slaves.begin(), config.slaves.end(), [this](const auto& item) { return item.id == selectedId_; }), config.slaves.end());
    try { session_.applyConfiguration(config); setDirty(); }
    catch (const std::exception& error) { QMessageBox::warning(this, "Invalid configuration", QString::fromUtf8(error.what())); }
}
bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == controlTable_ && event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Space) {
        controlModel_->toggle(controlTable_->currentIndex()); return true;
    }
    return QMainWindow::eventFilter(watched, event);
}
void MainWindow::chooseOpen()
{
    const auto path = QFileDialog::getOpenFileName(this, "Open master configuration", path_, "XML configuration (*.xml);;All files (*)");
    if (path.isEmpty()) return;
    QString error; if (!loadConfiguration(path, &error)) QMessageBox::warning(this, "Cannot open configuration", error);
}
void MainWindow::chooseSave(bool saveAs)
{
    QString path = path_;
    if (saveAs || path.isEmpty()) path = QFileDialog::getSaveFileName(this, "Save master configuration", path.isEmpty() ? "GenisysMaster.xml" : path, "XML configuration (*.xml)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".xml", Qt::CaseInsensitive)) path += ".xml";
    QString error; if (!saveConfiguration(path, &error)) QMessageBox::warning(this, "Cannot save configuration", error);
}
bool MainWindow::loadConfiguration(const QString& path, QString* error)
{
    try {
        ConsoleConfiguration config; LoadConsoleConfiguration(path.toStdWString(), config);
        session_.applyConfiguration(config); path_ = path; setWindowModified(false);
        eventModel_->append(0, "Loaded " + path, "System", 0); pendingRefresh_ = true; return true;
    } catch (const std::exception& exception) { if (error) *error = QString::fromUtf8(exception.what()); return false; }
}
bool MainWindow::saveConfiguration(const QString& path, QString* error)
{
    try {
        if (!session_.running()) session_.setTransportConfiguration(transportConfiguration());
        const auto& config = session_.configuration();
        ValidateConsoleConfiguration(config);
        pugi::xml_document document;
        document.append_child(pugi::node_declaration).append_attribute("version") = "1.0";
        document.append_child(pugi::node_comment).set_value(" Genisys Master configuration. Control drafts and cached indications are not saved. ");
        auto root = document.append_child("GenisysMaster"); root.append_attribute("version") = "1"; root.append_attribute("transport") = session_.serial() ? "serial" : "udp";
        if (session_.serial())
        {
            auto serial = root.append_child("Serial"); serial.append_attribute("port") = config.network.com_port_.c_str(); serial.append_attribute("baud-rate") = config.network.baud_rate_;
            serial.append_attribute("data-bits") = config.network.data_bits_; serial.append_attribute("parity") = std::string(1, config.network.parity_).c_str();
            serial.append_attribute("stop-bits") = config.network.stop_bits_; serial.append_attribute("write-timeout-ms") = config.network.write_timeout_ms_;
            serial.append_attribute("dtr") = config.network.dtr_enable_; serial.append_attribute("rts") = config.network.rts_enable_;
        }
        else
        {
            auto udp = root.append_child("Udp"); udp.append_attribute("local-ip") = config.network.local_ip_.c_str(); udp.append_attribute("receive-port") = config.network.receive_port_;
        }
        auto polling = root.append_child("Polling");
        polling.append_attribute("enabled") = config.network.automatic_polling_; polling.append_attribute("secure") = config.network.secure_poll_;
        polling.append_attribute("interval-ms") = config.network.poll_interval_ms_; polling.append_attribute("response-timeout-ms") = config.network.response_timeout_ms_;
        polling.append_attribute("retries") = config.network.retry_count_; polling.append_attribute("recovery-interval-ms") = config.network.recovery_interval_ms_;
        auto console = root.append_child("Console");
        console.append_attribute("plain") = config.plain; console.append_attribute("recall-all") = config.recall_all;
        console.append_attribute("wait-ms") = config.wait_ms; console.append_attribute("run-ms") = config.run_ms;
        auto slaves = root.append_child("Slaves");
        for (const auto& item : config.slaves) {
            auto node = slaves.append_child("Slave"); node.append_attribute("id") = item.id;
            if (item.has_endpoint) { node.append_attribute("ip") = item.endpoint.ip_address_.c_str(); node.append_attribute("port") = item.endpoint.port_; }
            node.append_attribute("frame-bytes") = item.frame_bytes;
        }
        std::ostringstream stream; document.save(stream, "  "); const auto bytes = stream.str();
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes.data(), static_cast<qint64>(bytes.size())) != static_cast<qint64>(bytes.size()) || !file.commit())
            throw std::runtime_error("Cannot save XML: " + file.errorString().toStdString());
        path_ = path; setWindowModified(false); pendingRefresh_ = true;
        eventModel_->append(0, "Saved " + path, "System", 0); return true;
    } catch (const std::exception& exception) { if (error) *error = QString::fromUtf8(exception.what()); return false; }
}

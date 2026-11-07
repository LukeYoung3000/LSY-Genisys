#include "MainWindow.h"
#include "TableModels.h"
#include "ProtocolGenisysSlave.h"
#include <Ws2tcpip.h>
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFont>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStyleFactory>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>
#include <stdexcept>
#include <functional>
#include <map>
using Bytes = std::vector<uint8_t>;
void Check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
#include "../../GenisysMaster-Lib/tests/UdpTestPeer.h"
#include "../../GenisysMaster-Lib/tests/SerialTestPeer.h"
namespace LSY
{
    struct NetworkGenisysMasterTestAccess
    {
        static std::unique_ptr<NetworkGenisysMaster> Create(NetworkGenisysMaster::Config config, std::unique_ptr<MasterTransport> io)
        { return std::unique_ptr<NetworkGenisysMaster>(new NetworkGenisysMaster(config, std::move(io))); }
    };
}
struct MasterSessionTestAccess
{
    static void Factory(MasterSession& session, std::function<std::unique_ptr<LSY::NetworkGenisysMaster>(LSY::NetworkGenisysMaster::Config)> factory)
    { session.networkFactory_ = std::move(factory); }
};
void CheckSerialGui(MainWindow& window, QTemporaryDir& temporary, const QString& screenshot, QStringList& passed);

template<class T> T* Widget(MainWindow& window, const char* name)
{
    auto* widget = window.findChild<T*>(name); Check(widget != nullptr, name); return widget;
}
void Until(const std::function<bool()>& condition, const char* text)
{
    QElapsedTimer timer; timer.start();
    while (!condition() && timer.elapsed() < 4000) QTest::qWait(10);
    Check(condition(), text);
}
void ClickBit(QTableView* table, int row, int column)
{
    const auto index = table->model()->index(row, column); table->scrollTo(index);
    QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier, table->visualRect(index).center());
}
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setStyle(QStyleFactory::create("Fusion")); QApplication::setFont(QFont("Segoe UI", 10));
    QString report = argc > 1 ? QString::fromLocal8Bit(argv[1]) : "GuiChecks.txt";
    QString screenshot = argc > 2 ? QString::fromLocal8Bit(argv[2]) : QString();
    QStringList passed;
    int result = 0;
    WSADATA wsa{}; WSAStartup(MAKEWORD(2, 2), &wsa);
    try {
        MainWindow window; window.show(); QTest::qWait(150);
        auto& session = window.session();
        Check(!session.running(), "GUI must start stopped");
        Peer peer; peer.Add(3, 0x81); peer.Add(4, 0x25); peer.Add(5, 0x0F); peer.offline_ids[5] = true;
        ConsoleConfiguration config; config.network.local_ip_ = "127.0.0.1"; config.network.receive_port_ = 0;
        config.network.poll_interval_ms_ = 150; config.network.response_timeout_ms_ = 60;
        config.network.retry_count_ = 1; config.network.recovery_interval_ms_ = 200;
        config.slaves = {{3, {"127.0.0.1", peer.port}, 8}, {4, {"127.0.0.1", peer.port}, 8}, {5, {"127.0.0.1", peer.port}, 8}};
        session.applyConfiguration(config); QTest::qWait(120);
        auto* run = Widget<QAction>(window, "runAction");
        auto* table = Widget<QTableView>(window, "controlTable");
        auto* indications = Widget<QTableView>(window, "indicationTable");
        auto* slaves = Widget<QTableView>(window, "slaveTable");
        auto* send = Widget<QPushButton>(window, "sendChangesButton");
        ClickBit(table, 0, 2);
        Check((*session.slave(3)->controls.frame)[0] == 0x80 && peer.requests[0xFC] == 0, "Stopped edits stay local");
        QTemporaryDir temporary; Check(temporary.isValid(), "Temporary configuration folder");
        const auto xml = temporary.path() + QString::fromUtf8("/master-\xE6\xB5\x8B\xE8\xAF\x95.xml");
        QString error;
        Check(window.saveConfiguration(xml, &error), "Save XML including Unicode path");
        ConsoleConfiguration loaded; LoadConsoleConfiguration(xml.toStdWString(), loaded);
        Check(loaded.slaves.size() == 3 && loaded.slaves[1].endpoint.port_ == peer.port, "XML round-trip shared endpoint");
        Check(window.loadConfiguration(xml, &error) && !session.running() && (*session.slave(3)->controls.frame)[0] == 0, "Load stays stopped and discards transient control drafts");
        QFile invalid(temporary.path() + "/bad.xml"); Check(invalid.open(QIODevice::WriteOnly), "Create invalid XML"); invalid.write("<bad>"); invalid.close();
        Check(!window.loadConfiguration(invalid.fileName(), &error) && session.slaves().size() == 3, "Bad XML preserves existing configuration");
        QFile serialFile(temporary.path() + "/serial.xml"); Check(serialFile.open(QIODevice::WriteOnly), "Create serial XML");
        serialFile.write("<GenisysMaster version='1' transport='serial'><Slaves><Slave id='3' frame-bytes='8'/></Slaves></GenisysMaster>"); serialFile.close();
        Check(window.loadConfiguration(serialFile.fileName(), &error) && session.serial() && !session.running(), "GUI loads serial XML while stopped");
        Check(Widget<QComboBox>(window, "transportCombo")->currentIndex() == 1 &&
            !Widget<QLineEdit>(window, "localIpEdit")->isVisible(), "Serial selection hides the UDP page");
        Check(window.loadConfiguration(xml, &error) && !session.serial(), "UDP configuration can be loaded after serial");
        passed << "PASS: XML round-trip, Unicode paths, validation and local-only drafts";
        QTimer peerTimer; peerTimer.setInterval(2);
        QObject::connect(&peerTimer, &QTimer::timeout, &window, [&]() { if (session.running()) peer.Process(session.localPort()); }); peerTimer.start();
        run->trigger();
        Until([&]() { return session.slave(3)->status.indications_current && session.slave(4)->status.indications_current && session.slave(5)->status.communication == LSY::NetworkGenisysMaster::COMMUNICATIONSTATUS::OFFLINE; }, "Multiple IDs poll on one socket; missing ID becomes offline");
        Until([&]() { return indications->model()->index(0, 1).data().toString() == "0x81"; }, "Indication model updates");
        Check(!Widget<QPushButton>(window, "addSlaveButton")->isEnabled(), "Configuration locked while running");
        peer.frames[3]->WriteByte(0, 0x42);
        Until([&]() { return indications->model()->index(0, 1).data().toString() == "0x42"; }, "Changed live indications refresh");
        const auto otherBefore = peer.station_requests[4][0xFB];
        peer.offline_ids[3] = true;
        Until([&]() { return session.slave(3)->status.communication == LSY::NetworkGenisysMaster::COMMUNICATIONSTATUS::OFFLINE; }, "Offline status detected");
        Check(!session.slave(3)->status.indications_current && peer.station_requests[4][0xFB] > otherBefore, "Offline data stale while other slave continues");
        peer.offline_ids[3] = false;
        Until([&]() { return session.slave(3)->status.indications_current; }, "Recovery recall restores current indications");
        passed << "PASS: UDP shared endpoint, polling, live indications, offline detection and recovery";
        ClickBit(table, 0, 2); Check((*session.slave(3)->controls.frame)[0] == 0x80, "Click toggles bit7");
        const auto hexIndex = table->model()->index(0, 1); table->setCurrentIndex(hexIndex); table->edit(hexIndex);
        auto* editor = Widget<QSpinBox>(window, "controlByteEditor"); editor->setValue(0x83); QTest::qWait(350);
        Check(editor->value() == 0x83 && peer.requests[0xFC] == 0, "Live polling preserves an uncommitted editor; no automatic sends");
        QTest::keyClick(editor, Qt::Key_Return); QTest::qWait(20);
        Check((*session.slave(3)->controls.frame)[0] == 0x83, "Hex editor commits staged value");
        Widget<QCheckBox>(window, "includeModeCheck")->setChecked(true); Widget<QSpinBox>(window, "controlModeSpin")->setValue(5);
        Until([&]() { return send->isEnabled(); }, "Send enabled when draft exists and idle"); QTest::mouseClick(send, Qt::LeftButton);
        Until([&]() { return session.slave(3)->controls.Result() == "ACKNOWLEDGED"; }, "Explicit control acknowledged");
        Check((*peer.control_frames[3])[0] == 0x83 && peer.last_controls[3][4] == 0xE0 && peer.last_controls[3][5] == 5 && !session.slave(3)->controls.Dirty(), "Slave receives edited controls and mode");
        const unsigned sent = peer.requests[0xFC];
        peer.offline_ids[3] = true; session.stageByte(3, 0, 0x84);
        Until([&]() { return send->isEnabled(); }, "Can send new draft"); QTest::mouseClick(send, Qt::LeftButton);
        Until([&]() { return session.slave(3)->controls.Result().find("TIMEOUT") == 0; }, "Lost control response shows uncertain outcome");
        Check(peer.requests[0xFC] == sent + 1 && session.slave(3)->controls.Dirty(), "Controls never automatically retransmit after timeout");
        peer.offline_ids[3] = false;
        Until([&]() { return session.slave(3)->status.indications_current; }, "Recover after lost control");
        Until([&]() { return send->isEnabled(); }, "Explicit resend available"); QTest::mouseClick(send, Qt::LeftButton);
        Until([&]() { return session.slave(3)->controls.Result() == "ACKNOWLEDGED"; }, "Explicit new control acknowledged");
        // Switch selection and return: per-slave drafts are retained.
        slaves->setCurrentIndex(slaves->model()->index(1, 0)); ClickBit(table, 1, 9);
        Check((*session.slave(4)->controls.frame)[1] == 1 && (*session.slave(3)->controls.frame)[1] == 0, "Drafts are per slave");
        slaves->setCurrentIndex(slaves->model()->index(0, 0)); QTest::qWait(120);
        passed << "PASS: bit/hex editing, retained editor, explicit controls/mode, ACK, uncertain timeout and per-slave drafts";
        auto* events = Widget<QTableView>(window, "eventTable"); auto* filter = Widget<QComboBox>(window, "eventFilterCombo");
        filter->setCurrentIndex(2); Check(events->model()->rowCount() > 0, "Controls event filter"); filter->setCurrentIndex(4); Check(events->model()->rowCount() > 0, "Warnings event filter"); filter->setCurrentIndex(0);
        if (!screenshot.isEmpty()) Check(window.grab().save(screenshot), "Save running GUI screenshot");
        // Stop in flight: do not report WAITING forever or silently assume success.
        Until([&]() { return !session.slave(3)->status.busy; }, "Wait idle before stop test");
        peer.offline_ids[3] = true; session.stageByte(3, 0, 0x85); Check(session.sendControl(3, false), "Start control before stopping");
        run->trigger(); QTest::qWait(120);
        Check(!session.running() && !session.slave(3)->status.indications_current && session.slave(3)->controls.Result().find("STOPPED") == 0 && !send->isEnabled(), "Stop retains draft and marks pending outcome unknown");
        Check(Widget<QPushButton>(window, "addSlaveButton")->isEnabled(), "Configuration unlocked after stop");
        passed << "PASS: event filtering and Stop with pending control";
        auto dialogResponse = [&]() {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()); Check(dialog != nullptr, "Slave modal editor");
            dialog->findChild<QSpinBox*>("slaveIdSpin")->setValue(9);
            dialog->findChild<QSpinBox*>("slavePortSpin")->setValue(peer.port);
            dialog->findChild<QSpinBox*>("slaveBytesSpin")->setValue(8); dialog->accept();
        };
        QTimer::singleShot(50, &window, dialogResponse); QTest::mouseClick(Widget<QPushButton>(window, "addSlaveButton"), Qt::LeftButton);
        Check(session.slave(9) && session.slaves().size() == 4, "Add slave dialog applies"); QTest::qWait(120);
        QTimer::singleShot(50, &window, [&]() {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()); Check(dialog != nullptr, "Edit modal");
            dialog->findChild<QSpinBox*>("slaveIdSpin")->setValue(10); dialog->accept();
        });
        QTest::mouseClick(Widget<QPushButton>(window, "editSlaveButton"), Qt::LeftButton);
        Check(session.slave(10) && !session.slave(9), "Edit slave ID applies"); QTest::qWait(120);
        QTest::mouseClick(Widget<QPushButton>(window, "removeSlaveButton"), Qt::LeftButton);
        Check(!session.slave(10) && session.slaves().size() == 3, "Remove selected slave");
        QTest::mouseClick(Widget<QPushButton>(window, "clearEventsButton"), Qt::LeftButton); Check(events->model()->rowCount() == 0, "Clear events");
        EventTableModel bounded; for (int i = 0; i < 1010; ++i) bounded.append(3, QString::number(i), "Status", 0);
        Check(bounded.rowCount() == 1000 && bounded.index(0, 2).data().toString() == "10", "Event buffer bounded to newest 1000 entries");
        passed << "PASS: Add/Edit/Remove dialogs and bounded event buffer";
        peerTimer.stop();
        CheckSerialGui(window, temporary, screenshot, passed);
        passed << "All GUI integration checks passed.";
    } catch (const std::exception& error) { passed << "FAIL: " + QString::fromUtf8(error.what()); result = 1; }
    WSACleanup();
    QFile output(report); if (output.open(QIODevice::WriteOnly | QIODevice::Text)) { QTextStream text(&output); text << passed.join('\n') << '\n'; }
    return result;
}

void CheckSerialGui(MainWindow& window, QTemporaryDir& temporary, const QString& screenshot, QStringList& passed)
{
    auto& session = window.session();
    ConsoleConfiguration config;
    config.network.connection_type_ = LSY::NetworkGenisysMaster::Config::CONNECTIONTYPE::SERIAL;
    config.network.com_port_ = "COM10"; config.network.baud_rate_ = 19200; config.network.parity_ = 'E'; config.network.stop_bits_ = 2;
    config.network.write_timeout_ms_ = 2000; config.network.dtr_enable_ = true;
    config.network.poll_interval_ms_ = 500; config.network.response_timeout_ms_ = 60; config.network.retry_count_ = 1; config.network.recovery_interval_ms_ = 1000;
    config.slaves = {{3, {}, 8, false}, {4, {}, 8, false}, {5, {}, 8, false}};
    session.applyConfiguration(config); QTest::qWait(120);
    auto* transport = Widget<QComboBox>(window, "transportCombo");
    auto* com = Widget<QComboBox>(window, "comPortCombo");
    auto* baud = Widget<QComboBox>(window, "baudCombo");
    auto* parity = Widget<QComboBox>(window, "parityCombo");
    auto* stops = Widget<QComboBox>(window, "stopBitsCombo");
    auto* dtr = Widget<QCheckBox>(window, "dtrCheck"); auto* rts = Widget<QCheckBox>(window, "rtsCheck");
    auto* writeTimeout = Widget<QSpinBox>(window, "writeTimeoutSpin");
    auto* slaves = Widget<QTableView>(window, "slaveTable"); auto* controls = Widget<QTableView>(window, "controlTable");
    auto* indications = Widget<QTableView>(window, "indicationTable");
    auto* send = Widget<QPushButton>(window, "sendChangesButton"); auto* run = Widget<QAction>(window, "runAction");
    Check(transport->currentIndex() == 1 && com->currentText() == "COM10" && baud->currentText() == "19200" && parity->currentData().toString() == "E" &&
        stops->currentData().toInt() == 2 && dtr->isChecked() && !rts->isChecked(), "Serial settings populate transport controls");
    Check(!Widget<QLineEdit>(window, "localIpEdit")->isVisible() && slaves->model()->index(0, 1).data().toString() == "COM10", "Serial mode hides IP fields and displays the shared COM bus");
    com->setEditText("COM42"); baud->setEditText("38400"); parity->setCurrentIndex(2); stops->setCurrentIndex(0); dtr->setChecked(false); rts->setChecked(true); writeTimeout->setValue(3000);
    QTest::mouseClick(Widget<QPushButton>(window, "refreshPortsButton"), Qt::LeftButton);
    Check(com->currentText() == "COM42", "Port refresh preserves a manually entered port");
    const auto serialPath = temporary.path() + QString::fromUtf8("/serial-\xE6\xB5\x8B\xE8\xAF\x95.xml"); QString error;
    Check(window.saveConfiguration(serialPath, &error), "Save serial settings");
    ConsoleConfiguration loaded; LoadConsoleConfiguration(serialPath.toStdWString(), loaded);
    Check(loaded.network.connection_type_ == LSY::NetworkGenisysMaster::Config::CONNECTIONTYPE::SERIAL && loaded.network.com_port_ == "COM42" && loaded.network.baud_rate_ == 38400 &&
        loaded.network.parity_ == 'O' && loaded.network.stop_bits_ == 1 && !loaded.network.dtr_enable_ && loaded.network.rts_enable_ && loaded.network.write_timeout_ms_ == 3000 && !loaded.slaves[0].has_endpoint,
        "Serial XML round-trip preserves all line settings and ID-only slaves");
    QFile saved(serialPath); Check(saved.open(QIODevice::ReadOnly), "Read saved serial XML"); const auto savedBytes = saved.readAll(); saved.close();
    Check(!savedBytes.contains("<Udp") && !savedBytes.contains(" ip="), "Serial XML does not invent IP endpoints");
    // Invalid edits fail atomically, leaving the previous valid file untouched.
    baud->setEditText("bad"); Check(!window.saveConfiguration(serialPath, &error), "Invalid baud rejected");
    Check(saved.open(QIODevice::ReadOnly) && saved.readAll() == savedBytes, "Failed serial save leaves existing XML intact"); saved.close(); baud->setEditText("38400");
    transport->setCurrentIndex(0); QTest::qWait(120);
    Check(slaves->model()->index(0, 1).data().toString() == "Set UDP endpoint" && !window.saveConfiguration(temporary.path() + "/incomplete.xml", &error), "Switching ID-only slaves to UDP requires real endpoints");
    Check(window.loadConfiguration(serialPath, &error) && session.serial() && !session.running(), "Serial XML reload remains stopped");
    // The serial slave editor only needs address and frame size.
    QTimer::singleShot(50, &window, [&]() {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()); Check(dialog != nullptr, "Serial slave editor");
        Check(!dialog->findChild<QLineEdit*>("slaveIpEdit")->isVisible() && !dialog->findChild<QSpinBox*>("slavePortSpin")->isVisible(), "Serial editor hides remote endpoints");
        dialog->findChild<QSpinBox*>("slaveIdSpin")->setValue(9); dialog->findChild<QSpinBox*>("slaveBytesSpin")->setValue(8); dialog->accept();
    });
    QTest::mouseClick(Widget<QPushButton>(window, "addSlaveButton"), Qt::LeftButton);
    Check(session.slave(9) && !session.slave(9)->configuration.has_endpoint, "Serial ID can be added without an endpoint");
    QTest::qWait(120); QTest::mouseClick(Widget<QPushButton>(window, "removeSlaveButton"), Qt::LeftButton);
    Check(!session.slave(9) && session.slaves().size() == 3, "Remove a serial slave");
    Check(window.loadConfiguration(serialPath, &error), "Restore serial settings after editing");
    passed << "PASS: serial transport widgets, port refresh, ID-only editing, XML round-trips and atomic validation";

    StreamPeer* peer = nullptr;
    MasterSessionTestAccess::Factory(session, [&](LSY::NetworkGenisysMaster::Config configuration) {
        auto stream = std::make_unique<StreamPeer>(); peer = stream.get();
        stream->Add(3, 0x81); stream->Add(4, 0x25); stream->Add(5, 0x0F); stream->offline[5] = true;
        return LSY::NetworkGenisysMasterTestAccess::Create(configuration, std::move(stream));
    });
    run->trigger();
    Until([&]() { return session.slave(3)->status.indications_current && session.slave(4)->status.indications_current && session.slave(5)->status.communication == LSY::NetworkGenisysMaster::COMMUNICATIONSTATUS::OFFLINE; }, "GUI session polls multiple serial IDs and detects an offline ID");
    Check(!transport->isEnabled() && !com->isEnabled() && Widget<QLabel>(window, "connectionBadge")->text().contains("Serial"), "Serial settings locked with correct running badge");
    const auto recalls3 = peer->byId[3][0xFD], recalls4 = peer->byId[4][0xFD], recalls5 = peer->byId[5][0xFD];
    QTest::mouseClick(Widget<QPushButton>(window, "recallAllButton"), Qt::LeftButton);
    Until([&]() { return peer->byId[3][0xFD] > recalls3 && peer->byId[4][0xFD] > recalls4 && peer->byId[5][0xFD] > recalls5; }, "GUI recall-all reaches every serial ID in sequence");
    // Select another ID while a queued recall owns the wire: controls must wait.
    Check(session.recall(4), "Queue serial recall while bus active");
    Until([&]() { return session.slave(4)->status.busy; }, "Slave 4 owns serial bus"); peer->holdWrite = true;
    Check(session.busyForControl(3), "Other IDs cannot send while the serial bus is busy"); QTest::qWait(120);
    Check(!Widget<QPushButton>(window, "sendFullButton")->isEnabled(), "GUI disables sends when another ID owns the bus"); peer->holdWrite = false;
    Until([&]() { return !session.busyForControl(3); }, "Serial bus becomes available");
    ClickBit(controls, 0, 2);
    Check((*session.slave(3)->controls.frame)[0] == 0x80 && peer->requests[0xFC] == 0, "Serial control edit stays local");
    Widget<QCheckBox>(window, "includeModeCheck")->setChecked(true); Widget<QSpinBox>(window, "controlModeSpin")->setValue(5);
    Until([&]() { return send->isEnabled(); }, "Serial control send enabled while idle"); QTest::mouseClick(send, Qt::LeftButton);
    Until([&]() { return session.slave(3)->controls.Result() == "ACKNOWLEDGED"; }, "Serial control acknowledged");
    Check((*peer->controls[3])[0] == 0x80 && !session.slave(3)->controls.Dirty(), "Serial control reaches the unchanged slave protocol");
    const auto sent = peer->requests[0xFC]; peer->offline[3] = true; session.stageByte(3, 0, 0x82);
    Until([&]() { return send->isEnabled(); }, "Stage explicit serial control with missing response"); QTest::mouseClick(send, Qt::LeftButton);
    Until([&]() { return session.slave(3)->controls.Result().find("TIMEOUT") == 0; }, "Serial control timeout is uncertain");
    Check(peer->requests[0xFC] == sent + 1 && session.slave(3)->controls.Dirty(), "Serial FC controls are never automatically retransmitted");
    peer->offline[3] = false; Until([&]() { return session.slave(3)->status.indications_current; }, "Offline serial slave recovers");
    session.stageByte(3, 0, 0x83); Until([&]() { return send->isEnabled(); }, "New serial draft can explicitly send"); QTest::mouseClick(send, Qt::LeftButton);
    Until([&]() { return session.slave(3)->controls.Result() == "ACKNOWLEDGED"; }, "Explicit serial resend acknowledged");
    peer->frames[3]->WriteByte(0, 0xF6);
    Until([&]() { return indications->model()->index(0, 1).data().toString() == "0xF6"; }, "Live serial table receives escaped indication changes"); QTest::qWait(120);
    if (!screenshot.isEmpty()) Check(window.grab().save(QFileInfo(screenshot).absolutePath() + "/GuiSerialChecks.png"), "Save serial GUI screenshot");
    passed << "PASS: serial GUI polling, shared-bus recalls, busy sends, indications, control ACK and uncertain timeout";
    // A fatal port error must retire I/O after reporting the active control outcome.
    Until([&]() { return !session.busyForControl(3); }, "Idle before serial disconnect"); session.stageByte(3, 0, 0x84);
    Check(session.sendControl(3, false), "Serial control before disconnect"); peer->offline[3] = true; peer->failRead = true;
    Until([&]() { return !session.running(); }, "Serial port failure stops session");
    Until([&]() { return transport->isEnabled() && !send->isEnabled() && Widget<QLabel>(window, "connectionBadge")->text().contains("error"); }, "Failure unlocks configuration and disables sends");
    Check(!session.lastError().isEmpty() && session.slave(3)->status.communication == LSY::NetworkGenisysMaster::COMMUNICATIONSTATUS::OFFLINE && !session.slave(3)->status.indications_current &&
        (*session.slave(3)->indications)[0] == 0xF6 && session.slave(3)->controls.Result().find("SEND FAILED") == 0 && session.slave(3)->controls.Dirty(), "Port failure preserves stale data and reports pending control outcome");
    run->trigger(); Until([&]() { return session.slave(3)->status.indications_current; }, "Serial GUI reconnects with Start");
    Check(peer->requests[0xFC] == 0 && (*session.slave(3)->controls.frame)[0] == 0x84, "Reconnect retains drafts without sending them automatically");
    run->trigger(); QTest::qWait(120);
    Check(!session.running() && session.lastError().isEmpty() && !session.slave(3)->status.indications_current && !send->isEnabled(), "Normal serial Stop retains stale data and drafts");
    MasterSessionTestAccess::Factory(session, {});
    com->setEditText("COM999999999"); bool unavailable = false;
    try { session.start(); } catch (const std::exception& failure) { unavailable = std::string(failure.what()).find("serial port") != std::string::npos; }
    Check(unavailable && !session.running(), "Actual Windows backend reports unavailable serial port without starting");
    passed << "PASS: serial disconnect outcomes, stopped controls, reconnect and native unavailable-port diagnostics";
    passed << "SKIP: native connected-COM traffic (serial GUI traffic above uses an injected byte-stream peer)";
}

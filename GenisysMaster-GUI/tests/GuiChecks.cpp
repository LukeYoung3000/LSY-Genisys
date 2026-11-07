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
        passed << "All GUI integration checks passed.";
    } catch (const std::exception& error) { passed << "FAIL: " + QString::fromUtf8(error.what()); result = 1; }
    WSACleanup();
    QFile output(report); if (output.open(QIODevice::WriteOnly | QIODevice::Text)) { QTextStream text(&output); text << passed.join('\n') << '\n'; }
    return result;
}

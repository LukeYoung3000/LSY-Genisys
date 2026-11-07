#include "MasterSession.h"
#include "GenisysMaster-Lib-Logging.h"
#include <Ws2tcpip.h>
#include <set>
#include <stdexcept>

namespace
{
    void ValidateAddress(const std::string& text)
    {
        in_addr address{};
        if (InetPtonA(AF_INET, text.c_str(), &address) != 1) throw std::runtime_error("Enter a numeric IPv4 address: " + text);
    }
    void ValidateTransport(const LSY::NetworkGenisysMaster::Config& config)
    {
        ValidateAddress(config.local_ip_);
        if (!config.poll_interval_ms_ || config.poll_interval_ms_ > 600000 ||
            !config.response_timeout_ms_ || config.response_timeout_ms_ > 600000 ||
            !config.recovery_interval_ms_ || config.recovery_interval_ms_ > 600000 || config.retry_count_ > 100)
            throw std::runtime_error("Intervals must be 1..600000 ms; retries must be 0..100.");
    }
}

MasterSession::MasterSession(QObject* parent) : QObject(parent)
{
    clock_.start();
    processTimer_.setInterval(10);
    processTimer_.setTimerType(Qt::PreciseTimer);
    connect(&processTimer_, &QTimer::timeout, this, &MasterSession::process);
    LSY::MasterLogging::AddLogWarningFunc([this](std::string text) { log(0, QString::fromStdString(text), "System", 1); return true; });
    LSY::MasterLogging::AddLogErrorFunc([this](std::string text) { log(0, QString::fromStdString(text), "System", 2); return true; });
}
MasterSession::~MasterSession()
{
    processTimer_.stop();
    if (network_) network_->Shutdown();
    network_.reset();
    // The library's log registrations are process-global. Do not leave dangling captures.
    LSY::MasterLogging::AddLogWarningFunc([](std::string) { return false; });
    LSY::MasterLogging::AddLogErrorFunc([](std::string) { return false; });
}
const MasterSession::Slave* MasterSession::slave(int id) const
{
    for (const auto& item : slaves_) if (item.configuration.id == id) return &item;
    return nullptr;
}
MasterSession::Slave* MasterSession::mutableSlave(int id)
{
    for (auto& item : slaves_) if (item.configuration.id == id) return &item;
    return nullptr;
}
uint16_t MasterSession::localPort() const { return network_ ? network_->GetLocalPort() : configuration_.network.receive_port_; }
void MasterSession::log(int id, const QString& text, const QString& category, int severity)
{
    emit eventAdded(id, text, category, severity);
}
void MasterSession::applyConfiguration(const ConsoleConfiguration& config)
{
    if (running()) throw std::runtime_error("Stop communications before changing configuration.");
    ValidateTransport(config.network);
    std::set<uint8_t> ids;
    std::vector<Slave> replacement;
    for (const auto& item : config.slaves)
    {
        if (!item.id || !item.endpoint.port_ || !item.frame_bytes || item.frame_bytes > 224 || !ids.insert(item.id).second)
            throw std::runtime_error("Slave IDs must be unique (1..255), ports 1..65535, and frames 1..224 bytes.");
        ValidateAddress(item.endpoint.ip_address_);
        Slave state;
        state.configuration = item;
        state.indications = std::make_shared<LSY::MasterDataFrame>("Indications " + std::to_string(item.id), item.frame_bytes);
        state.controls.Initialize(item.id, item.frame_bytes);
        state.controls.mode = 5; // Optional draft value; not sent unless Include mode is selected.
        state.previous.assign(item.frame_bytes, 0);
        state.changedAt.resize(item.frame_bytes);
        for (auto& bits : state.changedAt) bits.fill(-10000);
        replacement.push_back(std::move(state));
    }
    configuration_ = config;
    slaves_ = std::move(replacement);
    emit configurationChanged();
    emit changed();
}
void MasterSession::setTransportConfiguration(const LSY::NetworkGenisysMaster::Config& config)
{
    if (running()) throw std::runtime_error("Stop communications before changing transport settings.");
    ValidateTransport(config);
    configuration_.network = config;
    emit changed();
}
void MasterSession::start()
{
    if (running()) return;
    if (slaves_.empty()) throw std::runtime_error("Add at least one slave before starting.");
    ValidateTransport(configuration_.network);
    auto protocol = std::make_shared<LSY::ProtocolGenisysMaster>();
    auto network = std::make_unique<LSY::NetworkGenisysMaster>(configuration_.network);
    if (!network->AddProtocol(protocol)) throw std::runtime_error("Cannot attach master protocol.");
    for (auto& state : slaves_)
    {
        const auto id = state.configuration.id;
        if (!protocol->AddDataFrame(id, false, state.indications) || !protocol->AddDataFrame(id, true, state.controls.frame) ||
            !network->AddSlave(id, state.configuration.endpoint)) throw std::runtime_error("Cannot register slave configuration.");
        std::function<void(std::string, LSY::MasterDataFrame::CALLBACKEVENT)> callback =
            [this, id](std::string, LSY::MasterDataFrame::CALLBACKEVENT event)
            {
                auto* current = mutableSlave(id);
                if (!current) return;
                if (event == LSY::MasterDataFrame::CALLBACKEVENT::INDICATION_CHANGE)
                {
                    for (size_t offset = 0; offset < current->previous.size(); ++offset)
                    {
                        const uint8_t value = (*current->indications)[offset];
                        const uint8_t difference = current->previous[offset] ^ value;
                        for (int bit = 0; bit < 8; ++bit) if (difference & (1 << bit)) current->changedAt[offset][bit] = clock_.elapsed();
                        current->previous[offset] = value;
                    }
                    log(id, "Indications changed", "Indications");
                }
                else log(id, QString("Mode updated to 0x%1").arg(current->indications->GetModeByte(), 2, 16, QChar('0')).toUpper(), "Indications");
            };
        state.indications->AddEventCallback(callback);
    }
    std::function<void(uint8_t, LSY::NetworkGenisysMaster::COMMUNICATIONSTATUS)> callback =
        [this](uint8_t id, LSY::NetworkGenisysMaster::COMMUNICATIONSTATUS status)
        {
            const bool online = status == LSY::NetworkGenisysMaster::COMMUNICATIONSTATUS::ONLINE;
            log(id, online ? "Online" : "Offline - cached indications are stale", "Status", online ? 0 : 1);
        };
    network->AddStatusCallback(callback);
    if (!network->Start()) throw std::runtime_error("Cannot bind UDP socket. Check the local IP/port and whether another application is using it.");
    protocol_ = std::move(protocol);
    network_ = std::move(network);
    for (auto& state : slaves_) state.status = {};
    processTimer_.start();
    log(0, QString("UDP started on %1:%2").arg(QString::fromStdString(configuration_.network.local_ip_)).arg(localPort()), "Status");
    emit changed();
}
void MasterSession::stop()
{
    if (!network_) return;
    processTimer_.stop();
    network_->Shutdown();
    network_.reset(); protocol_.reset();
    for (auto& state : slaves_)
    {
        state.controls.OnStopped();
        state.status.busy = false;
        state.status.indications_current = false;
        state.status.communication = LSY::NetworkGenisysMaster::COMMUNICATIONSTATUS::UNKNOWN;
    }
    log(0, "UDP stopped; cached indications are stale", "Status");
    emit changed();
}
void MasterSession::process()
{
    if (!network_) return;
    network_->Process(); // Library logging reports rejected datagrams/socket errors.
    for (const auto& response : network_->GetResponses())
    {
        if (auto* state = mutableSlave(response.slave_id))
        {
            state->controls.OnResponse(response);
            if (response.type == LSY::ProtocolGenisysMaster::RESPONSETYPE::INDICATION_DATA) state->lastUpdate = QDateTime::currentDateTime();
        }
    }
    for (const auto& event : network_->GetTransactionEvents())
    {
        using Event = LSY::NetworkGenisysMaster::TRANSACTIONEVENT;
        auto* state = mutableSlave(event.slave_id);
        if (!state) continue;
        state->controls.OnTransaction(event);
        if (event.control_request && event.type != Event::RETRY)
            log(event.slave_id, QString::fromStdString(state->controls.Result()), "Controls", event.type == Event::COMPLETE ? 0 : 1);
        else if (event.type == Event::RETRY) log(event.slave_id, QString("Response retry %1").arg(event.retries), event.control_request ? "Controls" : "Status", 1);
        else if (event.type == Event::TIMEOUT) log(event.slave_id, "Response timeout", "Status", 1);
        else if (event.type == Event::SEND_FAILED) log(event.slave_id, "Request send failed", "Status", 2);
    }
    for (auto& state : slaves_) network_->GetSlaveStatus(state.configuration.id, state.status);
    emit changed();
}
bool MasterSession::recall(int id)
{
    if (!network_ || !network_->RequestRecall(static_cast<uint8_t>(id)))
    {
        log(id, "Recall not sent: stopped, unknown slave, or busy", "Status", 1); return false;
    }
    log(id, "Recall sent", "Status");
    return true;
}
void MasterSession::recallAll() { for (const auto& state : slaves_) recall(state.configuration.id); }
bool MasterSession::stageByte(int id, int offset, int value)
{
    auto* state = mutableSlave(id);
    if (!state || offset < 0 || value < 0 || value > 255 || !state->controls.frame->WriteByte(offset, static_cast<uint8_t>(value))) return false;
    emit changed(); return true;
}
bool MasterSession::toggleBit(int id, int offset, int bit)
{
    auto* state = mutableSlave(id); bool value = false;
    if (!state || offset < 0 || bit < 0 || bit > 7 || !state->controls.frame->ReadBit(offset, static_cast<uint8_t>(bit), value)) return false;
    state->controls.frame->WriteBit(offset, static_cast<uint8_t>(bit), !value);
    emit changed(); return true;
}
void MasterSession::stageMode(int id, bool include, int value)
{
    if (auto* state = mutableSlave(id)) { state->controls.include_mode = include; state->controls.mode = static_cast<uint8_t>(value); emit changed(); }
}
bool MasterSession::sendControl(int id, bool full)
{
    auto* state = mutableSlave(id);
    if (!state || !network_ || !state->controls.Send(*network_, static_cast<uint8_t>(id), full))
    {
        log(id, "Control NOT sent: stopped, busy, no changes/mode, or send failed", "Controls", 1); return false;
    }
    log(id, full ? "Full control frame explicitly sent" : "Changed controls explicitly sent", "Controls");
    emit changed(); return true;
}

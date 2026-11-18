#include "SimulatorSession.h"
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
        ConsoleConfiguration candidate; candidate.network = config;
        candidate.slaves.push_back(ConsoleSlaveConfiguration{});
        ValidateConsoleConfiguration(candidate);
    }
}

SimulatorSession::SimulatorSession()
{
    LSY::MasterLogging::AddLogWarningFunc([this](std::string text) { log(0, text, "System", 1); return true; });
    LSY::MasterLogging::AddLogErrorFunc([this](std::string text) { log(0, text, "System", 2); return true; });
}
SimulatorSession::~SimulatorSession()
{
    if (network_) network_->Shutdown();
    network_.reset();
    // The library's log registrations are process-global. Do not leave dangling captures.
    LSY::MasterLogging::AddLogWarningFunc([](std::string) { return false; });
    LSY::MasterLogging::AddLogErrorFunc([](std::string) { return false; });
}
const SimulatorSession::Slave* SimulatorSession::slave(int id) const
{
    for (const auto& item : slaves_) if (item.configuration.id == id) return &item;
    return nullptr;
}
SimulatorSession::Slave* SimulatorSession::mutableSlave(int id)
{
    for (auto& item : slaves_) if (item.configuration.id == id) return &item;
    return nullptr;
}
uint16_t SimulatorSession::localPort() const { return serial() ? 0 : network_ ? network_->GetLocalPort() : configuration_.network.receive_port_; }
void SimulatorSession::log(int id, const std::string& text, const std::string& category, int severity)
{
    if (eventAdded) eventAdded(id, text, category, severity);
}
void SimulatorSession::applyConfiguration(const ConsoleConfiguration& config)
{
    if (running()) throw std::runtime_error("Stop communications before changing configuration.");
    ValidateTransport(config.network);
    std::set<uint8_t> ids;
    std::vector<Slave> replacement;
    for (const auto& item : config.slaves)
    {
        if (!item.id || !item.frame_bytes || item.frame_bytes > 224 || !ids.insert(item.id).second)
            throw std::runtime_error("Slave IDs must be unique (1..255), ports 1..65535, and frames 1..224 bytes.");
        if (item.has_endpoint) {
            if (!item.endpoint.port_) throw std::runtime_error("Slave UDP port must be 1..65535.");
            ValidateAddress(item.endpoint.ip_address_);
        }
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
    lastError_.clear();
    configuration_ = config;
    slaves_ = std::move(replacement);
}
void SimulatorSession::setTransportConfiguration(const LSY::NetworkGenisysMaster::Config& config)
{
    if (running()) throw std::runtime_error("Stop communications before changing transport settings.");
    ValidateTransport(config);
    lastError_.clear();
    configuration_.network = config;
}
void SimulatorSession::start()
{
    if (running()) return;
    if (slaves_.empty()) throw std::runtime_error("Add at least one slave before starting.");
    ValidateConsoleConfiguration(configuration_);
    lastError_.clear();
    auto protocol = std::make_shared<LSY::ProtocolGenisysMaster>();
    auto network = std::make_unique<LSY::NetworkGenisysMaster>(configuration_.network);
    if (!network->AddProtocol(protocol)) throw std::runtime_error("Cannot attach master protocol.");
    for (auto& state : slaves_)
    {
        const auto id = state.configuration.id;
        if (!protocol->AddDataFrame(id, false, state.indications) || !protocol->AddDataFrame(id, true, state.controls.frame) ||
            !(serial() ? network->AddSlave(id) : network->AddSlave(id, state.configuration.endpoint))) throw std::runtime_error("Cannot register slave configuration.");
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
                        for (int bit = 0; bit < 8; ++bit) if (difference & (1 << bit)) current->changedAt[offset][bit] = elapsed();
                        current->previous[offset] = value;
                    }
                    log(id, "Indications changed", "Indications");
                }
                else log(id, "Mode updated", "Indications");
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
    if (!network->Start())
    {
        lastError_ = serial() ? "Cannot open serial port. Check the COM name, line settings and whether another application is using it." :
            "Cannot bind UDP socket. Check the local IP/port and whether another application is using it.";
        throw std::runtime_error(lastError_);
    }
    std::vector<uint8_t> ids; for (const auto& state : slaves_) ids.push_back(state.configuration.id);
    recalls_ = std::make_unique<ConsoleRecalls>(ids, serial());
    protocol_ = std::move(protocol);
    network_ = std::move(network);
    for (auto& state : slaves_) state.status = {};
    log(0, ConsoleTransportDescription(configuration_.network, localPort()) + " started", "Status");
}
void SimulatorSession::stop()
{
    if (!network_) return;
    network_->Shutdown();
    network_.reset(); protocol_.reset(); recalls_.reset(); lastError_.clear();
    for (auto& state : slaves_)
    {
        state.controls.OnStopped();
        state.status.busy = false;
        state.status.indications_current = false;
        state.status.communication = LSY::NetworkGenisysMaster::COMMUNICATIONSTATUS::UNKNOWN;
    }
    log(0, serial() ? "Serial stopped; cached indications are stale" : "UDP stopped; cached indications are stale", "Status");
}
void SimulatorSession::process()
{
    if (!network_) return;
    network_->Process(!recalls_->Pending()); // Explicit recalls take priority over new scheduled polls.
    if (network_->IsRunning())
        for (const auto& request : recalls_->Process(*network_))
            log(request.id, request.sent ? "Recall sent" : "Recall send failed", "Status", request.sent ? 0 : 1);
    for (const auto& response : network_->GetResponses())
    {
        if (auto* state = mutableSlave(response.slave_id))
        {
            state->controls.OnResponse(response);
            if (response.type == LSY::ProtocolGenisysMaster::RESPONSETYPE::INDICATION_DATA) state->lastUpdate = std::chrono::system_clock::now();
        }
    }
    for (const auto& event : network_->GetTransactionEvents())
    {
        using Event = LSY::NetworkGenisysMaster::TRANSACTIONEVENT;
        auto* state = mutableSlave(event.slave_id);
        if (!state) continue;
        state->controls.OnTransaction(event);
        if (event.control_request && event.type != Event::RETRY)
            log(event.slave_id, state->controls.Result(), "Controls", event.type == Event::COMPLETE ? 0 : 1);
        else if (event.type == Event::RETRY) log(event.slave_id, "Response retry " + std::to_string(event.retries), event.control_request ? "Controls" : "Status", 1);
        else if (event.type == Event::TIMEOUT) log(event.slave_id, "Response timeout", "Status", 1);
        else if (event.type == Event::SEND_FAILED) log(event.slave_id, "Request send failed", "Status", 2);
    }
    for (auto& state : slaves_) network_->GetSlaveStatus(state.configuration.id, state.status);
    if (!network_->IsRunning())
    {
        // Consume control response/transaction events above before retiring I/O.
        for (auto& state : slaves_) state.controls.OnStopped();
        network_->Shutdown(); network_.reset(); protocol_.reset(); recalls_.reset();
        lastError_ = "Serial connection lost. Check the port and press Start to reconnect.";
        log(0, lastError_, "Status", 2);
    }
}
bool SimulatorSession::recall(int id)
{
    if (!network_ || !recalls_->Queue(static_cast<uint8_t>(id)))
    {
        log(id, "Recall not queued: stopped or unknown slave", "Status", 1); return false;
    }
    log(id, "Recall queued", "Status");
    return true;
}
void SimulatorSession::recallAll() { for (const auto& state : slaves_) recall(state.configuration.id); }
bool SimulatorSession::stageByte(int id, int offset, int value)
{
    auto* state = mutableSlave(id);
    if (!state || offset < 0 || value < 0 || value > 255 || !state->controls.frame->WriteByte(offset, static_cast<uint8_t>(value))) return false;
    return true;
}
bool SimulatorSession::toggleBit(int id, int offset, int bit)
{
    auto* state = mutableSlave(id); bool value = false;
    if (!state || offset < 0 || bit < 0 || bit > 7 || !state->controls.frame->ReadBit(offset, static_cast<uint8_t>(bit), value)) return false;
    state->controls.frame->WriteBit(offset, static_cast<uint8_t>(bit), !value);
    return true;
}
void SimulatorSession::stageMode(int id, bool include, int value)
{
    if (auto* state = mutableSlave(id)) { state->controls.include_mode = include; state->controls.mode = static_cast<uint8_t>(value); }
}
bool SimulatorSession::sendControl(int id, bool full)
{
    auto* state = mutableSlave(id);
    if (!state || !network_ || busyForControl(id) || !state->controls.Send(*network_, static_cast<uint8_t>(id), full))
    {
        log(id, "Control NOT sent: stopped, busy, no changes/mode, or send failed", "Controls", 1); return false;
    }
    log(id, full ? "Full control frame explicitly sent" : "Changed controls explicitly sent", "Controls");
    return true;
}

std::string SimulatorSession::destination(int id) const
{
    const auto* state = slave(id);
    if (!state) return {};
    if (!serial() && !state->configuration.has_endpoint) return "Set UDP endpoint";
    return ConsoleSlaveDestination(configuration_.network, state->configuration.endpoint);
}
bool SimulatorSession::busyForControl(int id) const
{
    const auto* state = slave(id);
    if (!running() || !state || state->status.busy) return true;
    if (serial())
    {
        if (recalls_ && recalls_->Pending()) return true;
        for (const auto& item : slaves_) if (item.status.busy) return true;
    }
    return false;
}

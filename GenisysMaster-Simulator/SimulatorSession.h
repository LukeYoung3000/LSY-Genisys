#pragma once
#include "ConsoleConfiguration.h"
#include "ConsoleControls.h"
#include "ConsoleRecalls.h"
#include <array>
#include <chrono>

class SimulatorSession
{

public:
    struct Slave
    {
        ConsoleSlaveConfiguration configuration;
        std::shared_ptr<LSY::MasterDataFrame> indications;
        ConsoleControls controls;
        LSY::NetworkGenisysMaster::SlaveStatus status;
        bool flushPending = false;
        std::chrono::system_clock::time_point lastUpdate;
        std::vector<uint8_t> previous;
        std::vector<std::array<int64_t, 8>> changedAt;
    };
    explicit SimulatorSession();
    ~SimulatorSession();
    const ConsoleConfiguration& configuration() const { return configuration_; }
    const std::vector<Slave>& slaves() const { return slaves_; }
    const Slave* slave(int id) const;
    bool running() const { return network_ != nullptr; }
    bool serial() const { return configuration_.network.connection_type_ == LSY::NetworkGenisysMaster::Config::CONNECTIONTYPE::SERIAL; }
    bool busyForControl(int id) const;
    const std::string& lastError() const { return lastError_; }
    std::string destination(int id) const;
    uint16_t localPort() const;
    int64_t elapsed() const { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - clock_).count(); }
    void applyConfiguration(const ConsoleConfiguration& configuration);
    void setTransportConfiguration(const LSY::NetworkGenisysMaster::Config& configuration);
    void setFlushControlsOnConnect(bool enabled);
    void start();
    void stop();
    void process();
    bool recall(int id);
    void recallAll();
    bool stageByte(int id, int offset, int value);
    bool toggleBit(int id, int offset, int bit);
    void stageMode(int id, bool include, int value);
    bool sendControl(int id, bool full);
    std::function<void(int, const std::string&, const std::string&, int)> eventAdded;
private:
    ConsoleConfiguration configuration_;
    std::vector<Slave> slaves_;
    std::shared_ptr<LSY::ProtocolGenisysMaster> protocol_;
    std::unique_ptr<LSY::NetworkGenisysMaster> network_;
    std::unique_ptr<ConsoleRecalls> recalls_;
    std::string lastError_;
    const std::chrono::steady_clock::time_point clock_ = std::chrono::steady_clock::now();
    Slave* mutableSlave(int id);
    bool controlsPending() const;
    void dispatchControls();
    void log(int id, const std::string& message, const std::string& category = "System", int severity = 0);
};

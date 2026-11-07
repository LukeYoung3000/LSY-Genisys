#pragma once
#include "ConsoleConfiguration.h"
#include "ConsoleControls.h"
#include <QObject>
#include <QDateTime>
#include <QElapsedTimer>
#include <QTimer>
#include <array>

class MasterSession : public QObject
{
    Q_OBJECT
public:
    struct Slave
    {
        ConsoleSlaveConfiguration configuration;
        std::shared_ptr<LSY::MasterDataFrame> indications;
        ConsoleControls controls;
        LSY::NetworkGenisysMaster::SlaveStatus status;
        QDateTime lastUpdate;
        std::vector<uint8_t> previous;
        std::vector<std::array<qint64, 8>> changedAt;
    };
    explicit MasterSession(QObject* parent = nullptr);
    ~MasterSession() override;
    const ConsoleConfiguration& configuration() const { return configuration_; }
    const std::vector<Slave>& slaves() const { return slaves_; }
    const Slave* slave(int id) const;
    bool running() const { return network_ != nullptr; }
    uint16_t localPort() const;
    qint64 elapsed() const { return clock_.elapsed(); }
    void applyConfiguration(const ConsoleConfiguration& configuration);
    void setTransportConfiguration(const LSY::NetworkGenisysMaster::Config& configuration);
    void start();
    void stop();
    bool recall(int id);
    void recallAll();
    bool stageByte(int id, int offset, int value);
    bool toggleBit(int id, int offset, int bit);
    void stageMode(int id, bool include, int value);
    bool sendControl(int id, bool full);
signals:
    void configurationChanged();
    void changed();
    void eventAdded(int slaveId, const QString& message, const QString& category, int severity);
private:
    ConsoleConfiguration configuration_;
    std::vector<Slave> slaves_;
    std::shared_ptr<LSY::ProtocolGenisysMaster> protocol_;
    std::unique_ptr<LSY::NetworkGenisysMaster> network_;
    QTimer processTimer_;
    QElapsedTimer clock_;
    Slave* mutableSlave(int id);
    void process();
    void log(int id, const QString& message, const QString& category = "System", int severity = 0);
};

#pragma once
#include "MasterSession.h"
#include <QMainWindow>
#include <memory>
namespace Ui { class MainWindow; }
class SlaveTableModel; class ByteTableModel; class EventTableModel;
class QSortFilterProxyModel; class QTableView; class QLineEdit; class QSpinBox;
class QCheckBox; class QLabel; class QPushButton; class QAction;
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    MasterSession& session() { return session_; }
    bool loadConfiguration(const QString& path, QString* error = nullptr);
    bool saveConfiguration(const QString& path, QString* error = nullptr);
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    std::unique_ptr<Ui::MainWindow> ui_;
    MasterSession session_;
    QTimer refreshTimer_;
    SlaveTableModel* slaveModel_ = nullptr;
    ByteTableModel* indicationModel_ = nullptr;
    ByteTableModel* controlModel_ = nullptr;
    EventTableModel* eventModel_ = nullptr;
    QSortFilterProxyModel* eventFilter_ = nullptr;
    QTableView *slaveTable_ = nullptr, *indicationTable_ = nullptr, *controlTable_ = nullptr, *eventTable_ = nullptr;
    QLineEdit* localIp_ = nullptr;
    QSpinBox *localPort_ = nullptr, *pollInterval_ = nullptr, *timeout_ = nullptr, *retries_ = nullptr, *recovery_ = nullptr, *mode_ = nullptr;
    QCheckBox *automatic_ = nullptr, *secure_ = nullptr, *includeMode_ = nullptr, *followEvents_ = nullptr;
    QLabel *connectionBadge_ = nullptr, *onlineCount_ = nullptr, *detailsTitle_ = nullptr,
        *frameCounts_ = nullptr, *dataState_ = nullptr, *completed_ = nullptr, *timeouts_ = nullptr,
        *indicationState_ = nullptr, *indicationMode_ = nullptr, *updated_ = nullptr, *controlResult_ = nullptr;
    QPushButton *add_ = nullptr, *edit_ = nullptr, *remove_ = nullptr, *recall_ = nullptr,
        *recallAll_ = nullptr, *send_ = nullptr, *sendFull_ = nullptr;
    QAction *openAction_ = nullptr, *saveAction_ = nullptr, *saveAsAction_ = nullptr, *newAction_ = nullptr,
        *runAction_ = nullptr, *recallAction_ = nullptr, *recallAllAction_ = nullptr;
    QWidget* transportPanel_ = nullptr;
    QString path_;
    bool loading_ = false;
    bool pendingRefresh_ = true;
    int selectedId_ = 0;
    void buildInterface(); void connectInterface(); void syncConfiguration(); void refresh();
    void selectSlave(int id); void selectionChanged(); void editSlave(bool adding); void removeSlave();
    void toggleRunning(); void chooseOpen(); void chooseSave(bool saveAs); void setDirty();
    LSY::NetworkGenisysMaster::Config transportConfiguration() const;
};

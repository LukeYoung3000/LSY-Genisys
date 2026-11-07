#include "NetworkGenisysMaster.h"
#include "MainWindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QTimer>
#include <QFont>
#include <QStyleFactory>
#include <stdexcept>

namespace
{
    // Exercise real library symbols before displaying the window. No socket is opened.
    void VerifyMasterLibrary()
    {
        auto frame = std::make_shared<LSY::MasterDataFrame>("GUI startup check", 1);
        auto protocol = std::make_shared<LSY::ProtocolGenisysMaster>();
        std::vector<uint8_t> recall;
        LSY::NetworkGenisysMaster::Config configuration;
        configuration.receive_port_ = 0;
        LSY::NetworkGenisysMaster network(configuration);
        if (!protocol->AddDataFrame(3, false, frame) || !network.AddProtocol(protocol) ||
            !protocol->BuildRecall(3, recall) || recall != std::vector<uint8_t>({0xFD, 3, 0, 0x91, 0xF6}))
            throw std::runtime_error("Master library startup check failed");
        protocol->CancelRequest(3);
    }
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication::setFont(QFont("Segoe UI", 10));
    app.setApplicationName("GenisysMaster-GUI");
    app.setOrganizationName("LSY");
    app.setApplicationVersion("0.2.0");
    QCommandLineParser parser;
    parser.setApplicationDescription("Genisys master UDP test application");
    parser.addOption({"config", "Load an XML configuration (starts stopped).", "path"});
    parser.addOption({"start", "Start UDP communications after loading configuration."});
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"smoke-test", "Verify library linkage, render the window and exit automatically."});
    parser.addOption({"screenshot", "Save a window image during --smoke-test.", "path"});
    parser.process(app);
    try
    {
        VerifyMasterLibrary();
        MainWindow window;
        QString error;
        if (parser.isSet("config") && !window.loadConfiguration(parser.value("config"), &error)) throw std::runtime_error(error.toStdString());
        if (parser.isSet("start")) window.session().start();
        window.show();
        if (parser.isSet("smoke-test"))
        {
            QTimer::singleShot(250, &app, [&]()
            {
                const QString path = parser.value("screenshot");
                const bool saved = path.isEmpty() || window.grab().save(path);
                app.exit(saved ? 0 : 1);
            });
        }
        return app.exec();
    }
    catch (const std::exception& error)
    {
        if (!parser.isSet("smoke-test")) QMessageBox::critical(nullptr, "Genisys Master", QString::fromLocal8Bit(error.what()));
        return 1;
    }
}

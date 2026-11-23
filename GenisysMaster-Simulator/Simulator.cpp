#include "SimulatorSession.h"
#include "Version.h"
#include "GenisysMaster-Lib-Version.h"
#include "third_party/pugixml/pugixml.hpp"

#include <wx/wx.h>
#include <wx/grid.h>
#include <wx/spinctrl.h>
#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/notebook.h>
#include <sstream>

namespace
{
using Network = LSY::NetworkGenisysMaster;
using Transport = Network::Config::CONNECTIONTYPE;

wxString Text(const std::string& value) { return wxString::FromUTF8(value); }

class SimulatorFrame final : public wxFrame
{
public:
    SimulatorFrame() : wxFrame(nullptr, wxID_ANY, "Genisys Master Simulator", wxDefaultPosition, wxSize(1170, 820)), timer_(this)
    {
        SetMinSize(wxSize(1040, 740));
        auto* menu = new wxMenu;
        menu->Append(wxID_OPEN, "&Open XML...\tCtrl+O");
        menu->Append(wxID_SAVEAS, "&Save XML...\tCtrl+Shift+S");
        menu->AppendSeparator();
        menu->Append(wxID_EXIT, "E&xit");
        auto* help = new wxMenu;
        help->Append(wxID_ABOUT, "&About");
        auto* menus = new wxMenuBar;
        menus->Append(menu, "&File");
        menus->Append(help, "&Help");
        SetMenuBar(menus);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) { Open(); }, wxID_OPEN);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) { Save(); }, wxID_SAVEAS);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(); }, wxID_EXIT);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) {
            wxMessageBox(wxString::Format(
                "About - Genisys Master Simulator\n\n"
                "Author: Luke Young\nGitHub: LukeYoung3000\n"
                "Version - Lib: %s\nVersion - App: %s",
                wxString::FromUTF8(genisys_master_lib_version),
                wxString::FromUTF8(genisys_master_simulator_version)),
                "About", wxOK | wxICON_INFORMATION, this);
        }, wxID_ABOUT);

        auto* panel = new wxPanel(this);
        panel->SetBackgroundColour(wxColour(234, 240, 245));
        auto* root = new wxBoxSizer(wxVERTICAL);
        panel->SetSizer(root);
        auto* header = new wxPanel(panel);
        header->SetBackgroundColour(wxColour(22, 39, 57));
        auto* banner = new wxBoxSizer(wxHORIZONTAL);
        header->SetSizer(banner);
        auto* title = new wxStaticText(header, wxID_ANY, "Genisys Master Simulator");
        title->SetForegroundColour(*wxWHITE);
        title->SetFont(title->GetFont().Bold());
        banner->Add(title, 0, wxALIGN_CENTER_VERTICAL | wxALL, 8);
        banner->AddStretchSpacer();
        Action(header, banner, "Open XML", [this] { Open(); });
        Action(header, banner, "Save XML", [this] { Save(); });
        start_ = Action(header, banner, "Start", [this] { Toggle(); });
        connection_ = new wxStaticText(header, wxID_ANY, "Stopped");
        connection_->SetForegroundColour(*wxWHITE);
        connection_->SetMinSize(wxSize(140, -1));
        banner->Add(connection_, 0, wxALIGN_CENTER_VERTICAL | wxALL, 8);
        root->Add(header, 0, wxEXPAND);

        auto* body = new wxBoxSizer(wxHORIZONTAL);
        root->Add(body, 1, wxEXPAND | wxALL, 8);
        setup_ = new wxPanel(panel);
        setup_->SetBackgroundColour(*wxWHITE);
        setup_->SetMinSize(wxSize(280, -1));
        auto* side = new wxBoxSizer(wxVERTICAL);
        setup_->SetSizer(side);
        Heading(setup_, side, "Connection setup");
        auto* tabs = new wxNotebook(setup_, wxID_ANY);
        auto* udp = new wxPanel(tabs);
        auto* serial = new wxPanel(tabs);
        tabs->AddPage(udp, "UDP");
        tabs->AddPage(serial, "Serial");
        transport_ = tabs;
        side->Add(tabs, 0, wxEXPAND | wxALL, 5);
        auto* udpFields = Fields(udp);
        ip_ = Entry(udp, udpFields, "Local IPv4");
        port_ = Spin(udp, udpFields, "Receive port", 0, 65535, 7170);
        auto* serialFields = Fields(serial);
        com_ = new wxComboBox(serial, wxID_ANY);
        serialFields->Add(new wxStaticText(serial, wxID_ANY, "COM port"), 0, wxALIGN_CENTER_VERTICAL);
        serialFields->Add(com_, 1, wxEXPAND);
        baud_ = Entry(serial, serialFields, "Baud");
        parity_ = new wxChoice(serial, wxID_ANY);
        parity_->Append("None"); parity_->Append("Even"); parity_->Append("Odd");
        serialFields->Add(new wxStaticText(serial, wxID_ANY, "Parity"), 0, wxALIGN_CENTER_VERTICAL);
        serialFields->Add(parity_, 1, wxEXPAND);
        stopBits_ = Spin(serial, serialFields, "Stop bits", 1, 2, 1);
        writeTimeout_ = Spin(serial, serialFields, "Write timeout (ms)", 1, 600000, 1000);
        dtr_ = new wxCheckBox(serial, wxID_ANY, "DTR");
        rts_ = new wxCheckBox(serial, wxID_ANY, "RTS");
        serialFields->Add(dtr_); serialFields->Add(rts_);
        serialFields->Add(new wxStaticText(serial, wxID_ANY, "8 data bits"), 0, wxALIGN_CENTER_VERTICAL);
        Action(serial, serialFields, "Refresh", [this] { RefreshPorts(); });
        RefreshPorts();

        auto* polling = new wxFlexGridSizer(2, 4, 7);
        polling->AddGrowableCol(1);
        side->Add(polling, 0, wxEXPAND | wxALL, 7);
        interval_ = Spin(setup_, polling, "Poll interval (ms)", 1, 600000, 500);
        timeout_ = Spin(setup_, polling, "Response timeout (ms)", 1, 600000, 500);
        retries_ = Spin(setup_, polling, "Retries", 0, 100, 2);
        recovery_ = Spin(setup_, polling, "Recovery (ms)", 1, 600000, 1000);
        automatic_ = new wxCheckBox(setup_, wxID_ANY, "Automatic polling");
        secure_ = new wxCheckBox(setup_, wxID_ANY, "Secure polls");
        polling->Add(automatic_); polling->Add(secure_);
        Heading(setup_, side, "Slaves");
        slaves_ = new wxListBox(setup_, wxID_ANY, wxDefaultPosition, wxSize(-1, 90));
        side->Add(slaves_, 1, wxEXPAND | wxALL, 5);
        slaves_->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) { PaintState(); });
        auto* slaveActions = new wxBoxSizer(wxHORIZONTAL);
        side->Add(slaveActions, 0, wxEXPAND);
        add_ = Action(setup_, slaveActions, "Add", [this] { EditSlave(true); });
        edit_ = Action(setup_, slaveActions, "Edit", [this] { EditSlave(false); });
        remove_ = Action(setup_, slaveActions, "Remove", [this] { RemoveSlave(); });
        details_ = new wxStaticText(setup_, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE);
        details_->SetMinSize(wxSize(-1, 48));
        side->Add(details_, 0, wxEXPAND | wxALL, 7);
        body->Add(setup_, 0, wxEXPAND | wxRIGHT, 8);

        auto* work = new wxBoxSizer(wxVERTICAL);
        body->Add(work, 1, wxEXPAND);
        auto* tables = new wxBoxSizer(wxHORIZONTAL);
        work->Add(tables, 1, wxEXPAND);
        auto* left = new wxBoxSizer(wxVERTICAL);
        auto* right = new wxBoxSizer(wxVERTICAL);
        tables->Add(left, 1, wxEXPAND | wxRIGHT, 8);
        tables->Add(right, 1, wxEXPAND);
        Heading(panel, left, "Indications (Read Only)");
        Heading(panel, right, "Controls (Local Draft)");
        indications_ = Grid(panel, left);
        controls_ = Grid(panel, right);
        controls_->Bind(wxEVT_GRID_CELL_LEFT_CLICK, [this](wxGridEvent& event) {
            if (event.GetCol() > 0) session_.toggleBit(Selected(), event.GetRow(), 8 - event.GetCol());
            PaintState();
            event.Skip();
        });
        controls_->Bind(wxEVT_GRID_CELL_LEFT_DCLICK, [this](wxGridEvent& event) {
            if (event.GetCol() != 0) return;
            const auto* slave = session_.slave(Selected());
            if (!slave) return;
            wxTextEntryDialog dialog(this, "Enter a byte in hexadecimal (00–FF)", "Edit control byte",
                wxString::Format("%02X", unsigned((*slave->controls.frame)[event.GetRow()])));
            if (dialog.ShowModal() != wxID_OK) return;
            unsigned long value = 0;
            if (dialog.GetValue().ToULong(&value, 16) && value <= 255)
                session_.stageByte(Selected(), event.GetRow(), static_cast<int>(value));
            else wxMessageBox("Enter a hexadecimal byte from 00 to FF.", "Invalid byte", wxOK | wxICON_WARNING, this);
            PaintState();
        });
        auto* recalls = new wxBoxSizer(wxHORIZONTAL);
        left->Add(recalls, 0, wxEXPAND);
        recall_ = Action(panel, recalls, "Recall selected", [this] { session_.recall(Selected()); });
        recallAll_ = Action(panel, recalls, "Recall all", [this] { session_.recallAll(); });
        auto* modeRow = new wxBoxSizer(wxHORIZONTAL);
        right->Add(modeRow, 0, wxEXPAND | wxTOP, 4);
        flushOnConnect_ = new wxCheckBox(panel, wxID_ANY, "Flush controls when slave comes online");
        flushOnConnect_->SetValue(true);
        right->Add(flushOnConnect_, 0, wxEXPAND | wxTOP | wxBOTTOM, 4);
        includeMode_ = new wxCheckBox(panel, wxID_ANY, "Include mode");
        mode_ = new wxSpinCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxSize(65, -1), wxSP_ARROW_KEYS, 0, 255, 5);
        modeRow->Add(includeMode_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
        modeRow->Add(mode_, 0, wxALIGN_CENTER_VERTICAL);
        auto stageMode = [this](wxCommandEvent&) { session_.stageMode(Selected(), includeMode_->GetValue(), mode_->GetValue()); };
        includeMode_->Bind(wxEVT_CHECKBOX, stageMode);
        mode_->Bind(wxEVT_TEXT, stageMode);
        auto* sends = new wxBoxSizer(wxHORIZONTAL);
        right->Add(sends, 0, wxEXPAND);
        send_ = Action(panel, sends, "Send changes", [this] { Send(false); });
        sendFull_ = Action(panel, sends, "Send full frame", [this] { Send(true); });
        result_ = new wxStaticText(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE);
        result_->SetMinSize(wxSize(-1, 24));
        right->Add(result_, 0, wxEXPAND | wxLEFT, 5);
        auto* events = new wxBoxSizer(wxHORIZONTAL);
        work->Add(events, 0, wxEXPAND | wxTOP, 4);
        Heading(panel, events, "Events");
        filter_ = new wxChoice(panel, wxID_ANY);
        for (auto label : {"All events", "Indications", "Controls", "Status", "Warnings / errors"}) filter_->Append(label);
        filter_->SetSelection(0);
        filter_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { RenderEvents(); });
        events->Add(filter_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
        follow_ = new wxCheckBox(panel, wxID_ANY, "Follow");
        follow_->SetValue(true);
        events->Add(follow_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
        Action(panel, events, "Clear", [this] { events_.clear(); RenderEvents(); });
        log_ = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 90), wxTE_MULTILINE | wxTE_READONLY);
        work->Add(log_, 0, wxEXPAND);
        CreateStatusBar();

        session_.eventAdded = [this](int id, const std::string& message, const std::string& category, int severity) {
            Record(id, Text(message), Text(category), severity);
        };
        ConsoleConfiguration defaults;
        defaults.slaves.push_back({});
        Apply(defaults);
        Bind(wxEVT_TIMER, [this](wxTimerEvent&) {
            session_.process();
            if (++ticks_ == 5) { ticks_ = 0; PaintState(); }
        });
        timer_.Start(10);
    }

    ~SimulatorFrame() override
    {
        timer_.Stop();
        session_.eventAdded = {};
        session_.stop();
    }

    void Start()
    {
        session_.setTransportConfiguration(Settings());
        session_.setFlushControlsOnConnect(flushOnConnect_->GetValue());
        session_.start();
        PaintState();
    }

    void Load(const wxString& path)
    {
        ConsoleConfiguration config;
        LoadConsoleConfiguration(path.ToStdWstring(), config);
        Apply(config);
        path_ = path;
        Record(0, "Loaded " + path);
    }

private:
    struct Event { wxString text, category; int severity; };
    SimulatorSession session_;
    wxTimer timer_;
    unsigned ticks_ = 0;
    wxString path_;
    std::vector<Event> events_;
    int modeSlave_ = 0;
    wxPanel* setup_;
    wxNotebook* transport_;
    wxComboBox* com_;
    wxChoice *parity_, *filter_;
    wxCheckBox *automatic_, *secure_, *dtr_, *rts_, *flushOnConnect_, *includeMode_, *follow_;
    wxSpinCtrl *port_, *stopBits_, *writeTimeout_, *interval_, *timeout_, *retries_, *recovery_, *mode_;
    wxTextCtrl *ip_, *log_, *baud_;
    wxListBox* slaves_;
    wxGrid *indications_, *controls_;
    wxStaticText *connection_, *details_, *result_;
    wxButton *start_, *add_, *edit_, *remove_, *recall_, *recallAll_, *send_, *sendFull_;

    static void Heading(wxWindow* parent, wxSizer* sizer, const wxString& text)
    {
        auto* label = new wxStaticText(parent, wxID_ANY, text);
        label->SetFont(label->GetFont().Bold());
        sizer->Add(label, 0, wxALL, 6);
    }

    static wxButton* Action(wxWindow* parent, wxSizer* sizer, const wxString& label, std::function<void()> action)
    {
        auto* button = new wxButton(parent, wxID_ANY, label);
        button->Bind(wxEVT_BUTTON, [action = std::move(action)](wxCommandEvent&) { action(); });
        sizer->Add(button, 0, wxALL, 3);
        return button;
    }

    static wxFlexGridSizer* Fields(wxPanel* parent)
    {
        auto* fields = new wxFlexGridSizer(2, 4, 6);
        fields->AddGrowableCol(1);
        auto* outer = new wxBoxSizer(wxVERTICAL);
        outer->Add(fields, 0, wxEXPAND | wxALL, 6);
        parent->SetSizer(outer);
        return fields;
    }

    static wxTextCtrl* Entry(wxWindow* parent, wxSizer* fields, const wxString& label)
    {
        fields->Add(new wxStaticText(parent, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
        auto* entry = new wxTextCtrl(parent, wxID_ANY);
        fields->Add(entry, 1, wxEXPAND);
        return entry;
    }

    static wxSpinCtrl* Spin(wxWindow* parent, wxSizer* fields, const wxString& label, int low, int high, int initial)
    {
        fields->Add(new wxStaticText(parent, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
        auto* spin = new wxSpinCtrl(parent, wxID_ANY, "", wxDefaultPosition, wxSize(92, -1), wxSP_ARROW_KEYS, low, high, initial);
        fields->Add(spin, 1, wxEXPAND);
        return spin;
    }

    static wxGrid* Grid(wxWindow* parent, wxSizer* sizer)
    {
        auto* grid = new wxGrid(parent, wxID_ANY);
        grid->CreateGrid(0, 9);
        grid->EnableEditing(false);
        grid->DisableDragGridSize();
        grid->SetDefaultRowSize(21);
        grid->SetRowLabelSize(63);
        grid->SetDefaultCellAlignment(wxALIGN_CENTER, wxALIGN_CENTER);
        grid->SetColLabelValue(0, "Hex");
        grid->SetColSize(0, 42);
        for (int col = 1; col < 9; ++col)
        {
            grid->SetColLabelValue(col, wxString::Format("Bit %d", 8 - col));
            grid->SetColSize(col, 36);
        }
        sizer->Add(grid, 1, wxEXPAND);
        return grid;
    }

    void RefreshPorts()
    {
        const auto current = com_->GetValue();
        com_->Clear();
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
        {
            for (DWORD index = 0; ; ++index)
            {
                wchar_t name[256]{}, value[256]{};
                DWORD nameSize = 256, bytes = sizeof(value), type = 0;
                const auto status = RegEnumValueW(key, index, name, &nameSize, nullptr, &type, reinterpret_cast<BYTE*>(value), &bytes);
                if (status != ERROR_SUCCESS && status != ERROR_MORE_DATA) break;
                if (status == ERROR_SUCCESS && type == REG_SZ && bytes >= sizeof(wchar_t))
                {
                    value[255] = L'\0';
                    com_->Append(wxString(value));
                }
            }
            RegCloseKey(key);
        }
        com_->SetValue(current);
    }

    int Selected() const
    {
        const int row = slaves_->GetSelection();
        if (row < 0 || static_cast<size_t>(row) >= session_.slaves().size()) return 0;
        return session_.slaves()[row].configuration.id;
    }

    Network::Config Settings() const
    {
        auto config = session_.configuration().network;
        config.connection_type_ = transport_->GetSelection() == 1 ? Transport::SERIAL : Transport::UDP;
        config.local_ip_ = ip_->GetValue().ToStdString();
        config.receive_port_ = static_cast<uint16_t>(port_->GetValue());
        config.com_port_ = com_->GetValue().ToStdString();
        unsigned long baud = 0;
        const auto baudText = baud_->GetValue();
        if (!baudText.IsEmpty() && baudText.find_first_not_of("0123456789") == wxString::npos &&
            baudText.ToULong(&baud) && baud != 0)
            config.baud_rate_ = baud;
        else
            config.baud_rate_ = 0;
        config.parity_ = "NEO"[parity_->GetSelection()];
        config.stop_bits_ = static_cast<uint8_t>(stopBits_->GetValue());
        config.write_timeout_ms_ = writeTimeout_->GetValue();
        config.dtr_enable_ = dtr_->GetValue();
        config.rts_enable_ = rts_->GetValue();
        config.automatic_polling_ = automatic_->GetValue();
        config.secure_poll_ = secure_->GetValue();
        config.poll_interval_ms_ = interval_->GetValue();
        config.response_timeout_ms_ = timeout_->GetValue();
        config.retry_count_ = retries_->GetValue();
        config.recovery_interval_ms_ = recovery_->GetValue();
        return config;
    }

    void Apply(const ConsoleConfiguration& config, int select = 0)
    {
        session_.applyConfiguration(config);
        const auto& net = config.network;
        transport_->ChangeSelection(net.connection_type_ == Transport::SERIAL ? 1 : 0);
        ip_->ChangeValue(Text(net.local_ip_)); port_->SetValue(net.receive_port_);
        com_->SetValue(Text(net.com_port_)); baud_->ChangeValue(wxString::Format("%u", net.baud_rate_));
        parity_->SetSelection(net.parity_ == 'E' ? 1 : net.parity_ == 'O' ? 2 : 0);
        stopBits_->SetValue(net.stop_bits_); writeTimeout_->SetValue(net.write_timeout_ms_);
        dtr_->SetValue(net.dtr_enable_); rts_->SetValue(net.rts_enable_);
        automatic_->SetValue(net.automatic_polling_); secure_->SetValue(net.secure_poll_);
        flushOnConnect_->SetValue(config.flush_controls_on_connect);
        interval_->SetValue(net.poll_interval_ms_); timeout_->SetValue(net.response_timeout_ms_);
        retries_->SetValue(net.retry_count_); recovery_->SetValue(net.recovery_interval_ms_);
        slaves_->Clear();
        int row = 0;
        for (const auto& slave : config.slaves)
        {
            const int index = slaves_->Append(wxString::Format("Slave %u", unsigned(slave.id)));
            if (slave.id == select) row = index;
        }
        if (!config.slaves.empty()) slaves_->SetSelection(row);
        modeSlave_ = -1;
        PaintState();
    }

    void Toggle()
    {
        try
        {
            if (session_.running()) session_.stop();
            else Start();
        }
        catch (const std::exception& error) { ShowError(error); }
        PaintState();
    }

    void EditSlave(bool adding)
    {
        const auto* selected = session_.slave(Selected());
        if (!adding && !selected) return;
        auto config = session_.configuration();
        config.network = Settings();
        config.flush_controls_on_connect = flushOnConnect_->GetValue();
        const bool serial = config.network.connection_type_ == Transport::SERIAL;
        ConsoleSlaveConfiguration item = adding ? ConsoleSlaveConfiguration{} : selected->configuration;
        if (adding)
        {
            int available = 1;
            while (available <= 255 && session_.slave(available)) ++available;
            if (available > 255) return;
            item.id = static_cast<uint8_t>(available);
            item.has_endpoint = !serial;
        }

        wxDialog dialog(this, wxID_ANY, adding ? "Add slave" : "Edit slave");
        auto* root = new wxBoxSizer(wxVERTICAL);
        auto* fields = new wxFlexGridSizer(2, 6, 8);
        fields->AddGrowableCol(1);
        root->Add(fields, 0, wxEXPAND | wxALL, 12);
        auto* id = Spin(&dialog, fields, "Slave ID", 1, 255, item.id);
        auto* bytes = Spin(&dialog, fields, "Frame size (bytes)", 1, 224, item.frame_bytes);
        wxTextCtrl* remote = nullptr;
        wxSpinCtrl* remotePort = nullptr;
        if (!serial)
        {
            remote = Entry(&dialog, fields, "Remote IPv4");
            if (item.has_endpoint) remote->SetValue(Text(item.endpoint.ip_address_));
            remotePort = Spin(&dialog, fields, "Remote port", 1, 65535, item.endpoint.port_);
        }
        root->Add(dialog.CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 8);
        dialog.SetSizerAndFit(root);
        if (dialog.ShowModal() != wxID_OK) return;
        item.id = static_cast<uint8_t>(id->GetValue());
        item.frame_bytes = bytes->GetValue();
        if (!serial)
        {
            item.has_endpoint = true;
            item.endpoint = {remote->GetValue().ToStdString(), static_cast<uint16_t>(remotePort->GetValue())};
        }
        if (adding) config.slaves.push_back(item);
        else config.slaves[slaves_->GetSelection()] = item;
        try { Apply(config, item.id); }
        catch (const std::exception& error) { ShowError(error); }
    }

    void RemoveSlave()
    {
        const int row = slaves_->GetSelection();
        if (row < 0) return;
        auto config = session_.configuration();
        config.network = Settings();
        config.slaves.erase(config.slaves.begin() + row);
        try { Apply(config); }
        catch (const std::exception& error) { ShowError(error); }
    }

    void Send(bool full)
    {
        session_.stageMode(Selected(), includeMode_->GetValue(), mode_->GetValue());
        session_.sendControl(Selected(), full);
        PaintState();
    }

    void PaintGrid(wxGrid* grid, const SimulatorSession::Slave* slave, bool draft)
    {
        const int rows = slave ? static_cast<int>(slave->configuration.frame_bytes) : 0;
        grid->BeginBatch();
        if (grid->GetNumberRows() > rows) grid->DeleteRows(rows, grid->GetNumberRows() - rows);
        if (grid->GetNumberRows() < rows) grid->AppendRows(rows - grid->GetNumberRows());
        for (int row = 0; row < rows; ++row)
        {
            const auto value = (*(draft ? slave->controls.frame : slave->indications))[row];
            const auto label = wxString::Format("Byte %d", row);
            const auto hex = wxString::Format("%02X", unsigned(value));
            const wxColour hexColour = draft && slave->controls.ByteChanged(row) ? wxColour(255, 221, 127) : *wxWHITE;
            if (grid->GetRowLabelValue(row) != label) grid->SetRowLabelValue(row, label);
            if (grid->GetCellValue(row, 0) != hex) grid->SetCellValue(row, 0, hex);
            if (grid->GetCellBackgroundColour(row, 0) != hexColour) grid->SetCellBackgroundColour(row, 0, hexColour);
            for (int col = 1; col < 9; ++col)
            {
                const int bit = 8 - col;
                const bool on = (value & (1 << bit)) != 0;
                wxColour colour = on ? wxColour(44, 232, 40) : wxColour(255, 70, 70);
                if (!draft && !slave->status.indications_current) colour = wxColour(223, 226, 230);
                else if (!draft && session_.elapsed() - slave->changedAt[row][bit] < 1200) colour = wxColour(255, 221, 127);
                const wxString text = on ? "1" : "0";
                if (grid->GetCellValue(row, col) != text) grid->SetCellValue(row, col, text);
                if (grid->GetCellBackgroundColour(row, col) != colour) grid->SetCellBackgroundColour(row, col, colour);
            }
        }
        grid->EndBatch();
    }

    void PaintState()
    {
        using State = Network::COMMUNICATIONSTATUS;
        const bool running = session_.running();
        const int id = Selected();
        const auto* slave = session_.slave(id);
        transport_->Enable(!running);
        for (auto* field : {interval_, timeout_, retries_, recovery_}) field->Enable(!running);
        automatic_->Enable(!running); secure_->Enable(!running);
        flushOnConnect_->Enable(!running);
        add_->Enable(!running); edit_->Enable(!running && slave); remove_->Enable(!running && slave);
        recall_->Enable(running && slave); recallAll_->Enable(running);
        start_->SetLabel(running ? "Stop" : "Start");
        start_->Enable(running || !session_.slaves().empty());
        GetMenuBar()->Enable(wxID_OPEN, !running);
        connection_->SetLabel(running ? (session_.serial() ? "Serial running" : "UDP running") :
            session_.lastError().empty() ? "Stopped" : "Connection error");
        bool canSend = running && slave && !session_.busyForControl(id);
        send_->Enable(canSend && (slave->controls.Dirty() || slave->controls.include_mode));
        sendFull_->Enable(canSend);
        includeMode_->Enable(slave != nullptr);
        if (id != modeSlave_)
        {
            modeSlave_ = id;
            includeMode_->SetValue(slave && slave->controls.include_mode);
            mode_->SetValue(slave ? slave->controls.mode : 5);
        }
        mode_->Enable(slave && includeMode_->GetValue());

        for (size_t row = 0; row < session_.slaves().size(); ++row)
        {
            const auto& entry = session_.slaves()[row];
            const char* status = entry.status.communication == State::ONLINE ? "Online" :
                entry.status.communication == State::OFFLINE ? "Offline" : running ? "Waiting" : "Stopped";
            const auto label = wxString::Format("Slave %u | %s | %s", unsigned(entry.configuration.id),
                Text(session_.destination(entry.configuration.id)), wxString::FromUTF8(status));
            if (slaves_->GetString(static_cast<unsigned>(row)) != label) slaves_->SetString(static_cast<unsigned>(row), label);
        }
        if (slave)
        {
            const auto& status = slave->status;
            details_->SetLabel(wxString::Format("%s | Mode 0x%02X\nCompleted %llu | Timeouts %llu",
                status.indications_current ? wxString("Current") : wxString("Stale"),
                unsigned(slave->indications->GetModeByte()), static_cast<unsigned long long>(status.completed_count),
                static_cast<unsigned long long>(status.timeout_count)));
            result_->SetLabel(Text(slave->controls.Result()) + (slave->controls.Dirty() ? " | Modified draft" : ""));
        }
        else { details_->SetLabel(""); result_->SetLabel(""); }
        PaintGrid(indications_, slave, false);
        PaintGrid(controls_, slave, true);
        SetStatusText(Text(ConsoleTransportDescription(session_.configuration().network, session_.localPort())) +
            (session_.lastError().empty() ? wxString() : " | " + Text(session_.lastError())));
    }

    bool Visible(const Event& event) const
    {
        const int filter = filter_->GetSelection();
        if (filter == 0) return true;
        if (filter == 4) return event.severity != 0;
        return event.category == filter_->GetStringSelection();
    }

    void Record(int id, const wxString& message, const wxString& category = "System", int severity = 0)
    {
        Event entry{wxDateTime::Now().FormatISOTime() + "   " + (id ? wxString::Format("Slave %d   ", id) : wxString()) + message + "\n", category, severity};
        events_.push_back(entry);
        if (events_.size() > 1000)
        {
            events_.erase(events_.begin());
            RenderEvents();
        }
        else if (Visible(entry))
        {
            const auto position = log_->GetInsertionPoint();
            log_->AppendText(entry.text);
            if (!follow_->GetValue()) log_->SetInsertionPoint(position);
        }
    }

    void RenderEvents()
    {
        const auto position = log_->GetInsertionPoint();
        wxString content;
        for (const auto& event : events_) if (Visible(event)) content += event.text;
        log_->ChangeValue(content);
        if (follow_->GetValue()) log_->SetInsertionPointEnd();
        else log_->SetInsertionPoint((std::min)(position, log_->GetLastPosition()));
    }

    void ShowError(const std::exception& error)
    {
        wxMessageBox(Text(error.what()), "Genisys Master Simulator", wxOK | wxICON_ERROR, this);
    }

    void Open()
    {
        if (session_.running()) return;
        wxFileDialog dialog(this, "Open master configuration", "", "", "XML configuration (*.xml)|*.xml", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dialog.ShowModal() != wxID_OK) return;
        try { Load(dialog.GetPath()); }
        catch (const std::exception& error) { ShowError(error); }
    }

    void Save()
    {
        wxFileDialog dialog(this, "Save master configuration", "", "GenisysMaster.xml", "XML configuration (*.xml)|*.xml", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dialog.ShowModal() != wxID_OK) return;
        try
        {
            auto config = session_.configuration();
            if (!session_.running())
            {
                config.network = Settings();
                config.flush_controls_on_connect = flushOnConnect_->GetValue();
            }
            ValidateConsoleConfiguration(config);
            const auto& net = config.network;
            pugi::xml_document document;
            auto root = document.append_child("GenisysMaster");
            root.append_attribute("version") = "1";
            root.append_attribute("transport") = net.connection_type_ == Transport::SERIAL ? "serial" : "udp";
            if (net.connection_type_ == Transport::SERIAL)
            {
                auto node = root.append_child("Serial");
                node.append_attribute("port") = net.com_port_.c_str();
                node.append_attribute("baud-rate") = net.baud_rate_;
                node.append_attribute("parity") = std::string(1, net.parity_).c_str();
                node.append_attribute("stop-bits") = net.stop_bits_;
                node.append_attribute("write-timeout-ms") = net.write_timeout_ms_;
                node.append_attribute("dtr") = net.dtr_enable_;
                node.append_attribute("rts") = net.rts_enable_;
            }
            else
            {
                auto node = root.append_child("Udp");
                node.append_attribute("local-ip") = net.local_ip_.c_str();
                node.append_attribute("receive-port") = net.receive_port_;
            }
            auto polling = root.append_child("Polling");
            polling.append_attribute("enabled") = net.automatic_polling_;
            polling.append_attribute("secure") = net.secure_poll_;
            polling.append_attribute("interval-ms") = net.poll_interval_ms_;
            polling.append_attribute("response-timeout-ms") = net.response_timeout_ms_;
            polling.append_attribute("retries") = net.retry_count_;
            polling.append_attribute("recovery-interval-ms") = net.recovery_interval_ms_;
            auto controls = root.append_child("Controls");
            controls.append_attribute("flush-on-connect") = config.flush_controls_on_connect;
            auto console = root.append_child("Console");
            console.append_attribute("plain") = config.plain;
            console.append_attribute("recall-all") = config.recall_all;
            console.append_attribute("wait-ms") = config.wait_ms;
            console.append_attribute("run-ms") = config.run_ms;
            auto slaves = root.append_child("Slaves");
            for (const auto& slave : config.slaves)
            {
                auto node = slaves.append_child("Slave");
                node.append_attribute("id") = slave.id;
                node.append_attribute("frame-bytes") = slave.frame_bytes;
                if (slave.has_endpoint)
                {
                    node.append_attribute("ip") = slave.endpoint.ip_address_.c_str();
                    node.append_attribute("port") = slave.endpoint.port_;
                }
            }
            std::ostringstream output;
            document.save(output, "  ");
            const auto bytes = output.str();
            wxTempFile file(dialog.GetPath());
            if (!file.IsOpened() || !file.Write(bytes.data(), bytes.size()) || !file.Commit())
                throw std::runtime_error("Could not save the configuration.");
            path_ = dialog.GetPath();
            Record(0, "Saved " + path_);
        }
        catch (const std::exception& error) { ShowError(error); }
    }
};

class SimulatorApp final : public wxApp
{
    wxTimer smokeTimer_;
public:
    bool OnInit() override
    {
        bool start = false, smoke = false;
        auto* frame = new SimulatorFrame;
        try
        {
            wxString configPath;
            for (int index = 1; index < argc; ++index)
            {
                const wxString option = argv[index];
                if (option == "--config" && configPath.empty() && index + 1 < argc) configPath = argv[++index];
                else if (option == "--start") start = true;
                else if (option == "--smoke-test") smoke = true;
                else throw std::runtime_error("Usage: GenisysMaster-Simulator [--config file.xml] [--start]");
            }
            if (!configPath.empty()) frame->Load(configPath);
            if (start) frame->Start();
        }
        catch (const std::exception& error)
        {
            wxMessageBox(Text(error.what()), "Configuration error", wxOK | wxICON_ERROR);
            frame->Destroy();
            return false;
        }
        frame->Show();
        if (smoke)
        {
            Bind(wxEVT_TIMER, [frame](wxTimerEvent&) { frame->Close(); });
            smokeTimer_.SetOwner(this);
            smokeTimer_.StartOnce(750);
        }
        return true;
    }
};
}

wxIMPLEMENT_APP(SimulatorApp);

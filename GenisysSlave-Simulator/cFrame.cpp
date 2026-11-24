#include "cFrame.h"

#ifdef __WXMSW__
	#include <windows.h>
#endif

///////////////////////////////////////////////////////////////////////////

cFrame::cFrame( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxFrame( parent, id, title, pos, size, style )
{
	SetMinSize(FromDIP(wxSize(1040, 620)));
	m_panel1 = new wxPanel(this);
	m_panel1->SetBackgroundColour(wxColour(237, 242, 247));
	const int gap = FromDIP(8);
	auto* layout = new wxBoxSizer(wxVERTICAL);
	m_panel1->SetSizer(layout);

	// Keep the server action and master session status visible above the workspace.
	auto* header = new wxPanel(m_panel1);
	header->SetBackgroundColour(wxColour(23, 40, 61));
	auto* headerRow = new wxBoxSizer(wxHORIZONTAL);
	header->SetSizer(headerRow);
	auto* titleLabel = new wxStaticText(header, wxID_ANY, "Genisys Slave Simulator");
	titleLabel->SetFont(titleLabel->GetFont().Bold().Larger());
	titleLabel->SetForegroundColour(*wxWHITE);
	headerRow->Add(titleLabel, 1, wxALIGN_CENTER_VERTICAL | wxALL, gap);
	m_toggleStartStop = new wxToggleButton(header, wxID_ANY, "Start");
	headerRow->Add(m_toggleStartStop, 0, wxALIGN_CENTER_VERTICAL | wxALL, gap);
	m_staticTextMasterStatus = new wxStaticText(header, wxID_ANY, "Master disconnected");
	m_staticTextMasterStatus->SetForegroundColour(*wxWHITE);
	m_staticTextMasterStatus->SetMinSize(FromDIP(wxSize(175, -1)));
	headerRow->Add(m_staticTextMasterStatus, 0, wxALIGN_CENTER_VERTICAL | wxALL, gap);
	layout->Add(header, 0, wxEXPAND);

	auto* workspace = new wxBoxSizer(wxHORIZONTAL);
	layout->Add(workspace, 1, wxEXPAND | wxALL, gap);
	auto* sidebar = new wxPanel(m_panel1);
	sidebar->SetBackgroundColour(*wxWHITE);
	auto* sidebarLayout = new wxBoxSizer(wxVERTICAL);
	sidebar->SetSizer(sidebarLayout);
	auto* setup = new wxBoxSizer(wxVERTICAL);
	sidebarLayout->Add(setup, 0, wxEXPAND | wxALL, gap);
	workspace->Add(sidebar, 0, wxEXPAND | wxRIGHT, gap);

	auto addHeading = [sidebar, setup, gap](const wxString& text)
	{
		auto* heading = new wxStaticText(sidebar, wxID_ANY, text);
		heading->SetFont(heading->GetFont().Bold());
		setup->Add(heading, 0, wxTOP | wxBOTTOM, gap);
	};
	auto addField = [sidebar, gap](wxFlexGridSizer* fields, const wxString& name, wxWindow* input)
	{
		fields->Add(new wxStaticText(sidebar, wxID_ANY, name), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap);
		fields->Add(input, 0, wxEXPAND);
	};

	addHeading("Connection setup");
	const wxString protocols[] = { "UDP", "TCP", "Serial" };
	m_radioBox1 = new wxRadioBox(sidebar, wxID_ANY, "Protocol", wxDefaultPosition, wxDefaultSize,
		WXSIZEOF(protocols), protocols, WXSIZEOF(protocols), wxRA_SPECIFY_COLS);
	setup->Add(m_radioBox1, 0, wxEXPAND | wxBOTTOM, gap);

	m_transportFields = new wxBoxSizer(wxVERTICAL);
	auto* listenRow = new wxFlexGridSizer(2, gap / 2, gap);
	listenRow->AddGrowableCol(1);
	m_textUDPrx = new wxTextCtrl(sidebar, wxID_ANY);
	addField(listenRow, "Listen port", m_textUDPrx);
	m_transportFields->Add(listenRow, 0, wxEXPAND | wxBOTTOM, gap / 2);

	auto* replyRow = new wxFlexGridSizer(2, gap / 2, gap);
	replyRow->AddGrowableCol(1);
	m_textUDPtx = new wxTextCtrl(sidebar, wxID_ANY);
	addField(replyRow, "Reply port", m_textUDPtx);
	m_transportFields->Add(replyRow, 0, wxEXPAND);

	auto* serialFields = new wxFlexGridSizer(2, gap / 2, gap);
	serialFields->AddGrowableCol(1);
	const wxString baudRates[] = { "9600", "19200", "38400", "57600", "115200" };
	const wxString dataBits[] = { "5", "6", "7", "8" };
	const wxString stopBits[] = { "1", "1.5", "2" };
	const wxString parity[] = { "None (N)", "Even (E)", "Odd (O)" };
	m_comboComPort = new wxComboBox(sidebar, wxID_ANY, wxEmptyString,
		wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
	m_comboBaudRate = new wxComboBox(sidebar, wxID_ANY, wxEmptyString,
		wxDefaultPosition, wxDefaultSize, WXSIZEOF(baudRates), baudRates, wxCB_READONLY);
	m_comboDataBits = new wxComboBox(sidebar, wxID_ANY, wxEmptyString,
		wxDefaultPosition, wxDefaultSize, WXSIZEOF(dataBits), dataBits, wxCB_READONLY);
	m_comboStopBits = new wxComboBox(sidebar, wxID_ANY, wxEmptyString,
		wxDefaultPosition, wxDefaultSize, WXSIZEOF(stopBits), stopBits, wxCB_READONLY);
	m_comboParity = new wxComboBox(sidebar, wxID_ANY, wxEmptyString,
		wxDefaultPosition, wxDefaultSize, WXSIZEOF(parity), parity, wxCB_READONLY);
	m_comboBaudRate->SetSelection(0);
	m_comboDataBits->SetSelection(3);
	m_comboStopBits->SetSelection(0);
	m_comboParity->SetSelection(0);
	addField(serialFields, "COM port", m_comboComPort);
	addField(serialFields, "Baud rate", m_comboBaudRate);
	addField(serialFields, "Data bits", m_comboDataBits);
	addField(serialFields, "Stop bits", m_comboStopBits);
	addField(serialFields, "Parity", m_comboParity);
	m_transportFields->Add(serialFields, 0, wxEXPAND);
	setup->Add(m_transportFields, 0, wxEXPAND | wxBOTTOM, gap);
	auto* timeoutRow = new wxFlexGridSizer(2, gap / 2, gap);
	timeoutRow->AddGrowableCol(1);
	m_textPollTimeout = new wxTextCtrl(sidebar, wxID_ANY, "5000");
	m_textPollTimeout->SetToolTip("Shared maximum interval between Genisys polls. Enter 0 to disable.");
	addField(timeoutRow, "Poll timeout (ms)", m_textPollTimeout);
	setup->Add(timeoutRow, 0, wxEXPAND | wxBOTTOM, gap);

	addHeading("Slaves");
	m_slaveList = new wxListBox(sidebar, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(215, 84)));
	setup->Add(m_slaveList, 0, wxEXPAND | wxBOTTOM, gap);
	auto* slaveActions = new wxBoxSizer(wxHORIZONTAL);
	m_addSlave = new wxButton(sidebar, wxID_ANY, "Add");
	m_removeSlave = new wxButton(sidebar, wxID_ANY, "Remove");
	slaveActions->Add(m_addSlave, 0, wxRIGHT, gap);
	slaveActions->Add(m_removeSlave);
	setup->Add(slaveActions, 0, wxBOTTOM, gap);

	addHeading("Selected slave");
	auto* genisysFields = new wxFlexGridSizer(2, gap / 2, gap);
	genisysFields->AddGrowableCol(1);
	m_textSlaveID = new wxTextCtrl(sidebar, wxID_ANY);
	m_textFrameSize = new wxTextCtrl(sidebar, wxID_ANY);


	addField(genisysFields, "Slave ID", m_textSlaveID);
	addField(genisysFields, "Frame size (bytes)", m_textFrameSize);

	setup->Add(genisysFields, 0, wxEXPAND | wxBOTTOM, gap);
	m_alwaysRecallResponse = new wxCheckBox(sidebar, wxID_ANY, "Always send recall response");
	m_includeRecallMode = new wxCheckBox(sidebar, wxID_ANY, "Include mode byte in recall");
	m_includeRecallMode->SetValue(true);
	setup->Add(m_alwaysRecallResponse, 0, wxBOTTOM, gap / 2);
	setup->Add(m_includeRecallMode, 0, wxBOTTOM, gap);
	m_saveSlave = new wxButton(sidebar, wxID_ANY, "Save slave");
	setup->Add(m_saveSlave, 0, wxBOTTOM, gap);

	addHeading("Response mode byte");
	auto* modeReturn = new wxBoxSizer(wxHORIZONTAL);
	m_textModeByteReturn = new wxTextCtrl(sidebar, wxID_ANY, "0x04");
	m_buttonApplyModeByte = new wxButton(sidebar, wxID_ANY, "Apply");
	modeReturn->Add(m_textModeByteReturn, 1, wxRIGHT, gap);
	modeReturn->Add(m_buttonApplyModeByte);
	setup->Add(modeReturn, 0, wxEXPAND);

	// Let the grids fill the workspace while the diagnostics stay pinned below them.
	auto* footer = new wxPanel(m_panel1);
	footer->SetBackgroundColour(*wxWHITE);
	auto* diagnostics = new wxBoxSizer(wxHORIZONTAL);
	footer->SetSizer(diagnostics);
	auto addDiagnostic = [footer, diagnostics, gap](const wxString& label, const wxString& initial, int width)
	{
		diagnostics->Add(new wxStaticText(footer, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL | wxALL, gap);
		auto* value = new wxStaticText(footer, wxID_ANY, initial);
		value->SetMinSize(footer->FromDIP(wxSize(width, -1)));
		diagnostics->Add(value, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap);
		return value;
	};
	m_staticTextLastMasterIp = addDiagnostic("Last master", "--", 126);
	m_staticTextLastModeByte = addDiagnostic("Last received mode byte", "--", 44);
	m_staticTextResponceCounter = addDiagnostic("Responses", "0", 150);
	layout->Add(footer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, gap);

	auto* frameLayout = new wxBoxSizer(wxVERTICAL);
	frameLayout->Add(m_panel1, 1, wxEXPAND);
	SetSizer(frameLayout);

	wxBoxSizer* bSizer4;
	bSizer4 = new wxBoxSizer( wxVERTICAL );

	m_staticText11 = new wxStaticText( m_panel1, wxID_ANY, wxT("Indications (Read/Write)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText11->Wrap( -1 );
	bSizer4->Add( m_staticText11, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

	m_grid3 = new wxGrid( m_panel1, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(390, 240)), wxVSCROLL );

	// Grid
	m_grid3->CreateGrid( 125, 9 );
	m_grid3->EnableEditing( false );
	m_grid3->EnableGridLines( true );
	m_grid3->SetGridLineColour( wxColour(223, 231, 240) );
	m_grid3->EnableDragGridSize( false );
	m_grid3->SetMargins( 0, 0 );

	// Columns
	m_grid3->SetColSize( 0, 42 );
	m_grid3->SetColSize( 1, 30 );
	m_grid3->SetColSize( 2, 30 );
	m_grid3->SetColSize( 3, 30 );
	m_grid3->SetColSize( 4, 30 );
	m_grid3->SetColSize( 5, 30 );
	m_grid3->SetColSize( 6, 30 );
	m_grid3->SetColSize( 7, 30 );
	m_grid3->SetColSize( 8, 30 );
	m_grid3->EnableDragColMove( false );
	m_grid3->EnableDragColSize( false );
	m_grid3->SetColLabelValue( 0, wxT("Hex") );
	m_grid3->SetColLabelValue( 1, wxT("Bit 7") );
	m_grid3->SetColLabelValue( 2, wxT("Bit 6") );
	m_grid3->SetColLabelValue( 3, wxT("Bit 5") );
	m_grid3->SetColLabelValue( 4, wxT("Bit 4") );
	m_grid3->SetColLabelValue( 5, wxT("Bit 3") );
	m_grid3->SetColLabelValue( 6, wxT("Bit 2") );
	m_grid3->SetColLabelValue( 7, wxT("Bit 1") );
	m_grid3->SetColLabelValue( 8, wxT("Bit 0") );
	m_grid3->SetColLabelAlignment( wxALIGN_CENTER, wxALIGN_CENTER );

	// Rows
	m_grid3->AutoSizeRows();
	m_grid3->EnableDragRowSize( false );
	m_grid3->SetRowLabelValue( 0, wxT("Byte 0") );
	m_grid3->SetRowLabelValue( 1, wxT("Byte 1") );
	m_grid3->SetRowLabelValue( 2, wxT("Byte 2") );
	m_grid3->SetRowLabelValue( 3, wxT("Byte 3") );
	m_grid3->SetRowLabelValue( 4, wxT("Byte 4") );
	m_grid3->SetRowLabelValue( 5, wxT("Byte 5") );
	m_grid3->SetRowLabelValue( 6, wxT("Byte 6") );
	m_grid3->SetRowLabelValue( 7, wxT("Byte 7") );
	m_grid3->SetRowLabelValue( 8, wxT("Byte 8") );
	m_grid3->SetRowLabelValue( 9, wxT("Byte 9") );
	m_grid3->SetRowLabelValue( 10, wxT("Byte 10") );
	m_grid3->SetRowLabelValue( 11, wxT("Byte 11") );
	m_grid3->SetRowLabelValue( 12, wxT("Byte 12") );
	m_grid3->SetRowLabelValue( 13, wxT("Byte 13") );
	m_grid3->SetRowLabelValue( 14, wxT("Byte 14") );
	m_grid3->SetRowLabelValue( 15, wxT("Byte 15") );
	m_grid3->SetRowLabelValue( 16, wxT("Byte 16") );
	m_grid3->SetRowLabelValue( 17, wxT("Byte 17") );
	m_grid3->SetRowLabelValue( 18, wxT("Byte 18") );
	m_grid3->SetRowLabelValue( 19, wxT("Byte 19") );
	m_grid3->SetRowLabelValue( 20, wxT("Byte 20") );
	m_grid3->SetRowLabelValue( 21, wxT("Byte 21") );
	m_grid3->SetRowLabelValue( 22, wxT("Byte 22") );
	m_grid3->SetRowLabelValue( 23, wxT("Byte 23") );
	m_grid3->SetRowLabelValue( 24, wxT("Byte 24") );
	m_grid3->SetRowLabelValue( 25, wxT("Byte 25") );
	m_grid3->SetRowLabelValue( 26, wxT("Byte 26") );
	m_grid3->SetRowLabelValue( 27, wxT("Byte 27") );
	m_grid3->SetRowLabelValue( 28, wxT("Byte 28") );
	m_grid3->SetRowLabelValue( 29, wxT("Byte 29") );
	m_grid3->SetRowLabelValue( 30, wxT("Byte 30") );
	m_grid3->SetRowLabelValue( 31, wxT("Byte 31") );
	m_grid3->SetRowLabelValue( 32, wxT("Byte 32") );
	m_grid3->SetRowLabelValue( 33, wxT("Byte 33") );
	m_grid3->SetRowLabelValue( 34, wxT("Byte 34") );
	m_grid3->SetRowLabelValue( 35, wxT("Byte 35") );
	m_grid3->SetRowLabelValue( 36, wxT("Byte 36") );
	m_grid3->SetRowLabelValue( 37, wxT("Byte 37") );
	m_grid3->SetRowLabelValue( 38, wxT("Byte 38") );
	m_grid3->SetRowLabelValue( 39, wxT("Byte 39") );
	m_grid3->SetRowLabelValue( 40, wxT("Byte 40") );
	m_grid3->SetRowLabelValue( 41, wxT("Byte 41") );
	m_grid3->SetRowLabelValue( 42, wxT("Byte 42") );
	m_grid3->SetRowLabelValue( 43, wxT("Byte 43") );
	m_grid3->SetRowLabelValue( 44, wxT("Byte 44") );
	m_grid3->SetRowLabelValue( 45, wxT("Byte 45") );
	m_grid3->SetRowLabelValue( 46, wxT("Byte 46") );
	m_grid3->SetRowLabelValue( 47, wxT("Byte 47") );
	m_grid3->SetRowLabelValue( 48, wxT("Byte 48") );
	m_grid3->SetRowLabelValue( 49, wxT("Byte 49") );
	m_grid3->SetRowLabelValue( 50, wxT("Byte 50") );
	m_grid3->SetRowLabelValue( 51, wxT("Byte 51") );
	m_grid3->SetRowLabelValue( 52, wxT("Byte 52") );
	m_grid3->SetRowLabelValue( 53, wxT("Byte 53") );
	m_grid3->SetRowLabelValue( 54, wxT("Byte 54") );
	m_grid3->SetRowLabelValue( 55, wxT("Byte 55") );
	m_grid3->SetRowLabelValue( 56, wxT("Byte 56") );
	m_grid3->SetRowLabelValue( 57, wxT("Byte 57") );
	m_grid3->SetRowLabelValue( 58, wxT("Byte 58") );
	m_grid3->SetRowLabelValue( 59, wxT("Byte 59") );
	m_grid3->SetRowLabelValue( 60, wxT("Byte 60") );
	m_grid3->SetRowLabelValue( 61, wxT("Byte 61") );
	m_grid3->SetRowLabelValue( 62, wxT("Byte 62") );
	m_grid3->SetRowLabelValue( 63, wxT("Byte 63") );
	m_grid3->SetRowLabelValue( 64, wxT("Byte 64") );
	m_grid3->SetRowLabelValue( 65, wxT("Byte 65") );
	m_grid3->SetRowLabelValue( 66, wxT("Byte 66") );
	m_grid3->SetRowLabelValue( 67, wxT("Byte 67") );
	m_grid3->SetRowLabelValue( 68, wxT("Byte 68") );
	m_grid3->SetRowLabelValue( 69, wxT("Byte 69") );
	m_grid3->SetRowLabelValue( 70, wxT("Byte 70") );
	m_grid3->SetRowLabelValue( 71, wxT("Byte 71") );
	m_grid3->SetRowLabelValue( 72, wxT("Byte 72") );
	m_grid3->SetRowLabelValue( 73, wxT("Byte 73") );
	m_grid3->SetRowLabelValue( 74, wxT("Byte 74") );
	m_grid3->SetRowLabelValue( 75, wxT("Byte 75") );
	m_grid3->SetRowLabelValue( 76, wxT("Byte 76") );
	m_grid3->SetRowLabelValue( 77, wxT("Byte 77") );
	m_grid3->SetRowLabelValue( 78, wxT("Byte 78") );
	m_grid3->SetRowLabelValue( 79, wxT("Byte 79") );
	m_grid3->SetRowLabelValue( 80, wxT("Byte 80") );
	m_grid3->SetRowLabelValue( 81, wxT("Byte 81") );
	m_grid3->SetRowLabelValue( 82, wxT("Byte 82") );
	m_grid3->SetRowLabelValue( 83, wxT("Byte 83") );
	m_grid3->SetRowLabelValue( 84, wxT("Byte 84") );
	m_grid3->SetRowLabelValue( 85, wxT("Byte 85") );
	m_grid3->SetRowLabelValue( 86, wxT("Byte 86") );
	m_grid3->SetRowLabelValue( 87, wxT("Byte 87") );
	m_grid3->SetRowLabelValue( 88, wxT("Byte 88") );
	m_grid3->SetRowLabelValue( 89, wxT("Byte 89") );
	m_grid3->SetRowLabelValue( 90, wxT("Byte 90") );
	m_grid3->SetRowLabelValue( 91, wxT("Byte 91") );
	m_grid3->SetRowLabelValue( 92, wxT("Byte 92") );
	m_grid3->SetRowLabelValue( 93, wxT("Byte 93") );
	m_grid3->SetRowLabelValue( 94, wxT("Byte 94") );
	m_grid3->SetRowLabelValue( 95, wxT("Byte 95") );
	m_grid3->SetRowLabelValue( 96, wxT("Byte 96") );
	m_grid3->SetRowLabelValue( 97, wxT("Byte 97") );
	m_grid3->SetRowLabelValue( 98, wxT("Byte 98") );
	m_grid3->SetRowLabelValue( 99, wxT("Byte 99") );
	m_grid3->SetRowLabelValue( 100, wxT("Byte 100") );
	m_grid3->SetRowLabelValue( 101, wxT("Byte 101") );
	m_grid3->SetRowLabelValue( 102, wxT("Byte 102") );
	m_grid3->SetRowLabelValue( 103, wxT("Byte 103") );
	m_grid3->SetRowLabelValue( 104, wxT("Byte 104") );
	m_grid3->SetRowLabelValue( 105, wxT("Byte 105") );
	m_grid3->SetRowLabelValue( 106, wxT("Byte 106") );
	m_grid3->SetRowLabelValue( 107, wxT("Byte 107") );
	m_grid3->SetRowLabelValue( 108, wxT("Byte 108") );
	m_grid3->SetRowLabelValue( 109, wxT("Byte 109") );
	m_grid3->SetRowLabelValue( 110, wxT("Byte 110") );
	m_grid3->SetRowLabelValue( 111, wxT("Byte 111") );
	m_grid3->SetRowLabelValue( 112, wxT("Byte 112") );
	m_grid3->SetRowLabelValue( 113, wxT("Byte 113") );
	m_grid3->SetRowLabelValue( 114, wxT("Byte 114") );
	m_grid3->SetRowLabelValue( 115, wxT("Byte 115") );
	m_grid3->SetRowLabelValue( 116, wxT("Byte 116") );
	m_grid3->SetRowLabelValue( 117, wxT("Byte 117") );
	m_grid3->SetRowLabelValue( 118, wxT("Byte 118") );
	m_grid3->SetRowLabelValue( 119, wxT("Byte 119") );
	m_grid3->SetRowLabelValue( 120, wxT("Byte 120") );
	m_grid3->SetRowLabelValue( 121, wxT("Byte 121") );
	m_grid3->SetRowLabelValue( 122, wxT("Byte 122") );
	m_grid3->SetRowLabelValue( 123, wxT("Byte 123") );
	m_grid3->SetRowLabelValue( 124, wxT("Byte 124") );
	m_grid3->SetRowLabelAlignment( wxALIGN_CENTER, wxALIGN_CENTER );

	// Label Appearance

	// Cell Defaults
	m_grid3->SetDefaultCellAlignment( wxALIGN_CENTER, wxALIGN_CENTER );
	m_grid3->Enable( false );

	bSizer4->Add( m_grid3, 1, wxEXPAND | wxALL, 5 );


	workspace->Add( bSizer4, 1, wxEXPAND | wxRIGHT, gap );

	wxBoxSizer* bSizer81;
	bSizer81 = new wxBoxSizer( wxVERTICAL );

	m_staticText111 = new wxStaticText( m_panel1, wxID_ANY, wxT("Controls (Read Only)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText111->Wrap( -1 );
	bSizer81->Add( m_staticText111, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

	m_grid31 = new wxGrid( m_panel1, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(390, 240)), wxVSCROLL );

	// Grid
	m_grid31->CreateGrid( 125, 9 );
	m_grid31->EnableEditing( false );
	m_grid31->EnableGridLines( true );
	m_grid31->SetGridLineColour( wxColour(223, 231, 240) );
	m_grid31->EnableDragGridSize( false );
	m_grid31->SetMargins( 0, 0 );

	// Columns
	m_grid31->SetColSize( 0, 42 );
	m_grid31->SetColSize( 1, 30 );
	m_grid31->SetColSize( 2, 30 );
	m_grid31->SetColSize( 3, 30 );
	m_grid31->SetColSize( 4, 30 );
	m_grid31->SetColSize( 5, 30 );
	m_grid31->SetColSize( 6, 30 );
	m_grid31->SetColSize( 7, 30 );
	m_grid31->SetColSize( 8, 30 );
	m_grid31->EnableDragColMove( false );
	m_grid31->EnableDragColSize( false );
	m_grid31->SetColLabelValue( 0, wxT("Hex") );
	m_grid31->SetColLabelValue( 1, wxT("Bit 7") );
	m_grid31->SetColLabelValue( 2, wxT("Bit 6") );
	m_grid31->SetColLabelValue( 3, wxT("Bit 5") );
	m_grid31->SetColLabelValue( 4, wxT("Bit 4") );
	m_grid31->SetColLabelValue( 5, wxT("Bit 3") );
	m_grid31->SetColLabelValue( 6, wxT("Bit 2") );
	m_grid31->SetColLabelValue( 7, wxT("Bit 1") );
	m_grid31->SetColLabelValue( 8, wxT("Bit 0") );
	m_grid31->SetColLabelAlignment( wxALIGN_CENTER, wxALIGN_CENTER );

	// Rows
	m_grid31->AutoSizeRows();
	m_grid31->EnableDragRowSize( false );
	m_grid31->SetRowLabelValue( 0, wxT("Byte 0") );
	m_grid31->SetRowLabelValue( 1, wxT("Byte 1") );
	m_grid31->SetRowLabelValue( 2, wxT("Byte 2") );
	m_grid31->SetRowLabelValue( 3, wxT("Byte 3") );
	m_grid31->SetRowLabelValue( 4, wxT("Byte 4") );
	m_grid31->SetRowLabelValue( 5, wxT("Byte 5") );
	m_grid31->SetRowLabelValue( 6, wxT("Byte 6") );
	m_grid31->SetRowLabelValue( 7, wxT("Byte 7") );
	m_grid31->SetRowLabelValue( 8, wxT("Byte 8") );
	m_grid31->SetRowLabelValue( 9, wxT("Byte 9") );
	m_grid31->SetRowLabelValue( 10, wxT("Byte 10") );
	m_grid31->SetRowLabelValue( 11, wxT("Byte 11") );
	m_grid31->SetRowLabelValue( 12, wxT("Byte 12") );
	m_grid31->SetRowLabelValue( 13, wxT("Byte 13") );
	m_grid31->SetRowLabelValue( 14, wxT("Byte 14") );
	m_grid31->SetRowLabelValue( 15, wxT("Byte 15") );
	m_grid31->SetRowLabelValue( 16, wxT("Byte 16") );
	m_grid31->SetRowLabelValue( 17, wxT("Byte 17") );
	m_grid31->SetRowLabelValue( 18, wxT("Byte 18") );
	m_grid31->SetRowLabelValue( 19, wxT("Byte 19") );
	m_grid31->SetRowLabelValue( 20, wxT("Byte 20") );
	m_grid31->SetRowLabelValue( 21, wxT("Byte 21") );
	m_grid31->SetRowLabelValue( 22, wxT("Byte 22") );
	m_grid31->SetRowLabelValue( 23, wxT("Byte 23") );
	m_grid31->SetRowLabelValue( 24, wxT("Byte 24") );
	m_grid31->SetRowLabelValue( 25, wxT("Byte 25") );
	m_grid31->SetRowLabelValue( 26, wxT("Byte 26") );
	m_grid31->SetRowLabelValue( 27, wxT("Byte 27") );
	m_grid31->SetRowLabelValue( 28, wxT("Byte 28") );
	m_grid31->SetRowLabelValue( 29, wxT("Byte 29") );
	m_grid31->SetRowLabelValue( 30, wxT("Byte 30") );
	m_grid31->SetRowLabelValue( 31, wxT("Byte 31") );
	m_grid31->SetRowLabelValue( 32, wxT("Byte 32") );
	m_grid31->SetRowLabelValue( 33, wxT("Byte 33") );
	m_grid31->SetRowLabelValue( 34, wxT("Byte 34") );
	m_grid31->SetRowLabelValue( 35, wxT("Byte 35") );
	m_grid31->SetRowLabelValue( 36, wxT("Byte 36") );
	m_grid31->SetRowLabelValue( 37, wxT("Byte 37") );
	m_grid31->SetRowLabelValue( 38, wxT("Byte 38") );
	m_grid31->SetRowLabelValue( 39, wxT("Byte 39") );
	m_grid31->SetRowLabelValue( 40, wxT("Byte 40") );
	m_grid31->SetRowLabelValue( 41, wxT("Byte 41") );
	m_grid31->SetRowLabelValue( 42, wxT("Byte 42") );
	m_grid31->SetRowLabelValue( 43, wxT("Byte 43") );
	m_grid31->SetRowLabelValue( 44, wxT("Byte 44") );
	m_grid31->SetRowLabelValue( 45, wxT("Byte 45") );
	m_grid31->SetRowLabelValue( 46, wxT("Byte 46") );
	m_grid31->SetRowLabelValue( 47, wxT("Byte 47") );
	m_grid31->SetRowLabelValue( 48, wxT("Byte 48") );
	m_grid31->SetRowLabelValue( 49, wxT("Byte 49") );
	m_grid31->SetRowLabelValue( 50, wxT("Byte 50") );
	m_grid31->SetRowLabelValue( 51, wxT("Byte 51") );
	m_grid31->SetRowLabelValue( 52, wxT("Byte 52") );
	m_grid31->SetRowLabelValue( 53, wxT("Byte 53") );
	m_grid31->SetRowLabelValue( 54, wxT("Byte 54") );
	m_grid31->SetRowLabelValue( 55, wxT("Byte 55") );
	m_grid31->SetRowLabelValue( 56, wxT("Byte 56") );
	m_grid31->SetRowLabelValue( 57, wxT("Byte 57") );
	m_grid31->SetRowLabelValue( 58, wxT("Byte 58") );
	m_grid31->SetRowLabelValue( 59, wxT("Byte 59") );
	m_grid31->SetRowLabelValue( 60, wxT("Byte 60") );
	m_grid31->SetRowLabelValue( 61, wxT("Byte 61") );
	m_grid31->SetRowLabelValue( 62, wxT("Byte 62") );
	m_grid31->SetRowLabelValue( 63, wxT("Byte 63") );
	m_grid31->SetRowLabelValue( 64, wxT("Byte 64") );
	m_grid31->SetRowLabelValue( 65, wxT("Byte 65") );
	m_grid31->SetRowLabelValue( 66, wxT("Byte 66") );
	m_grid31->SetRowLabelValue( 67, wxT("Byte 67") );
	m_grid31->SetRowLabelValue( 68, wxT("Byte 68") );
	m_grid31->SetRowLabelValue( 69, wxT("Byte 69") );
	m_grid31->SetRowLabelValue( 70, wxT("Byte 70") );
	m_grid31->SetRowLabelValue( 71, wxT("Byte 71") );
	m_grid31->SetRowLabelValue( 72, wxT("Byte 72") );
	m_grid31->SetRowLabelValue( 73, wxT("Byte 73") );
	m_grid31->SetRowLabelValue( 74, wxT("Byte 74") );
	m_grid31->SetRowLabelValue( 75, wxT("Byte 75") );
	m_grid31->SetRowLabelValue( 76, wxT("Byte 76") );
	m_grid31->SetRowLabelValue( 77, wxT("Byte 77") );
	m_grid31->SetRowLabelValue( 78, wxT("Byte 78") );
	m_grid31->SetRowLabelValue( 79, wxT("Byte 79") );
	m_grid31->SetRowLabelValue( 80, wxT("Byte 80") );
	m_grid31->SetRowLabelValue( 81, wxT("Byte 81") );
	m_grid31->SetRowLabelValue( 82, wxT("Byte 82") );
	m_grid31->SetRowLabelValue( 83, wxT("Byte 83") );
	m_grid31->SetRowLabelValue( 84, wxT("Byte 84") );
	m_grid31->SetRowLabelValue( 85, wxT("Byte 85") );
	m_grid31->SetRowLabelValue( 86, wxT("Byte 86") );
	m_grid31->SetRowLabelValue( 87, wxT("Byte 87") );
	m_grid31->SetRowLabelValue( 88, wxT("Byte 88") );
	m_grid31->SetRowLabelValue( 89, wxT("Byte 89") );
	m_grid31->SetRowLabelValue( 90, wxT("Byte 90") );
	m_grid31->SetRowLabelValue( 91, wxT("Byte 91") );
	m_grid31->SetRowLabelValue( 92, wxT("Byte 92") );
	m_grid31->SetRowLabelValue( 93, wxT("Byte 93") );
	m_grid31->SetRowLabelValue( 94, wxT("Byte 94") );
	m_grid31->SetRowLabelValue( 95, wxT("Byte 95") );
	m_grid31->SetRowLabelValue( 96, wxT("Byte 96") );
	m_grid31->SetRowLabelValue( 97, wxT("Byte 97") );
	m_grid31->SetRowLabelValue( 98, wxT("Byte 98") );
	m_grid31->SetRowLabelValue( 99, wxT("Byte 99") );
	m_grid31->SetRowLabelValue( 100, wxT("Byte 100") );
	m_grid31->SetRowLabelValue( 101, wxT("Byte 101") );
	m_grid31->SetRowLabelValue( 102, wxT("Byte 102") );
	m_grid31->SetRowLabelValue( 103, wxT("Byte 103") );
	m_grid31->SetRowLabelValue( 104, wxT("Byte 104") );
	m_grid31->SetRowLabelValue( 105, wxT("Byte 105") );
	m_grid31->SetRowLabelValue( 106, wxT("Byte 106") );
	m_grid31->SetRowLabelValue( 107, wxT("Byte 107") );
	m_grid31->SetRowLabelValue( 108, wxT("Byte 108") );
	m_grid31->SetRowLabelValue( 109, wxT("Byte 109") );
	m_grid31->SetRowLabelValue( 110, wxT("Byte 110") );
	m_grid31->SetRowLabelValue( 111, wxT("Byte 111") );
	m_grid31->SetRowLabelValue( 112, wxT("Byte 112") );
	m_grid31->SetRowLabelValue( 113, wxT("Byte 113") );
	m_grid31->SetRowLabelValue( 114, wxT("Byte 114") );
	m_grid31->SetRowLabelValue( 115, wxT("Byte 115") );
	m_grid31->SetRowLabelValue( 116, wxT("Byte 116") );
	m_grid31->SetRowLabelValue( 117, wxT("Byte 117") );
	m_grid31->SetRowLabelValue( 118, wxT("Byte 118") );
	m_grid31->SetRowLabelValue( 119, wxT("Byte 119") );
	m_grid31->SetRowLabelValue( 120, wxT("Byte 120") );
	m_grid31->SetRowLabelValue( 121, wxT("Byte 121") );
	m_grid31->SetRowLabelValue( 122, wxT("Byte 122") );
	m_grid31->SetRowLabelValue( 123, wxT("Byte 123") );
	m_grid31->SetRowLabelValue( 124, wxT("Byte 124") );
	m_grid31->SetRowLabelAlignment( wxALIGN_CENTER, wxALIGN_CENTER );

	// Label Appearance

	// Cell Defaults
	m_grid31->SetDefaultCellAlignment( wxALIGN_CENTER, wxALIGN_CENTER );
	m_grid31->Enable( false );

	bSizer81->Add( m_grid31, 1, wxEXPAND | wxALL, 5 );


	workspace->Add( bSizer81, 1, wxEXPAND );


	m_menubar3 = new wxMenuBar( 0 );
	MenuHelp = new wxMenu();
	wxMenuItem* MenuHelpLog;
	MenuHelpLog = new wxMenuItem( MenuHelp, wxID_ANY, wxString( wxT("Log") ) , wxEmptyString, wxITEM_NORMAL );
	MenuHelp->Append( MenuHelpLog );

	wxMenuItem* MenuHelpProtocolLog;
	MenuHelpProtocolLog = new wxMenuItem( MenuHelp, wxID_ANY, wxString( wxT("Protocol Log") ) , wxEmptyString, wxITEM_NORMAL );
	MenuHelp->Append( MenuHelpProtocolLog );

	wxMenuItem* MenuHelpAbout;
	MenuHelpAbout = new wxMenuItem( MenuHelp, wxID_ANY, wxString( wxT("About\n") ) , wxEmptyString, wxITEM_NORMAL );
	MenuHelp->Append( MenuHelpAbout );

	m_menubar3->Append( MenuHelp, wxT("Help") );

	this->SetMenuBar( m_menubar3 );

	// Size for the tallest configuration so switching transports does not resize the frame.
	ShowTransportSettings(2);
	frameLayout->SetSizeHints(this);
	wxSize initialSize = FromDIP(wxSize(1000, 740));
	initialSize.IncTo(GetMinSize());
	SetSize(initialSize);
	ShowTransportSettings(0);
	Layout();


	this->Centre( wxBOTH );

	// Populate COM port dropdown with available ports
	this->PopulateComPorts();

	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( cFrame::TopLevelWindowCloseEvent ) );
	m_radioBox1->Connect( wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler( cFrame::ProtocolSelectionEvent ), NULL, this );
	m_toggleStartStop->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( cFrame::StartStopServerEvent ), NULL, this );
	m_buttonApplyModeByte->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( cFrame::ApplyModeByteEvent ), NULL, this );
	m_grid3->Connect( wxEVT_GRID_CELL_LEFT_CLICK, wxGridEventHandler( cFrame::GridClickEvent ), NULL, this );
	m_grid3->Connect( wxEVT_GRID_CELL_LEFT_DCLICK, wxGridEventHandler( cFrame::GridDoubleClickEvent ), NULL, this );
	MenuHelp->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( cFrame::MenuHelpLogOnMenuSelection ), this, MenuHelpLog->GetId());
	MenuHelp->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( cFrame::MenuHelpProtocolLogOnMenuSelection ), this, MenuHelpProtocolLog->GetId());
	MenuHelp->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( cFrame::MenuHelpAboutOnMenuSelection ), this, MenuHelpAbout->GetId());
}

void cFrame::PopulateComPorts()
{
	m_comboComPort->Clear();

	// Windows: Check for available COM ports by attempting to query the system
	for (int i = 1; i <= 256; ++i)
	{
		wxString portName = wxString::Format(wxT("COM%d"), i);

		// Try to open the port to check if it exists
		HANDLE hPort = CreateFileA(
			portName.mb_str(),
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL
		);

		if (hPort != INVALID_HANDLE_VALUE)
		{
			CloseHandle(hPort);
			m_comboComPort->Append(portName);
		}
	}


	// Set default selection if any ports are available
	if (m_comboComPort->GetCount() > 0)
		m_comboComPort->SetSelection(0);
}

void cFrame::ShowTransportSettings(int selection)
{
	// The transport sizer contains the listen, reply and serial field groups in that order.
	m_transportFields->Show(static_cast<size_t>(0), selection != 2);
	m_transportFields->Show(static_cast<size_t>(1), selection == 0);
	m_transportFields->Show(static_cast<size_t>(2), selection == 2);
	m_panel1->Layout();
}

cFrame::~cFrame()
{
	// Disconnect Events
	this->Disconnect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( cFrame::TopLevelWindowCloseEvent ) );
	m_radioBox1->Disconnect( wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler( cFrame::ProtocolSelectionEvent ), NULL, this );
	m_toggleStartStop->Disconnect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( cFrame::StartStopServerEvent ), NULL, this );
	m_buttonApplyModeByte->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( cFrame::ApplyModeByteEvent ), NULL, this );
	m_grid3->Disconnect( wxEVT_GRID_CELL_LEFT_CLICK, wxGridEventHandler( cFrame::GridClickEvent ), NULL, this );
	m_grid3->Disconnect( wxEVT_GRID_CELL_LEFT_DCLICK, wxGridEventHandler( cFrame::GridDoubleClickEvent ), NULL, this );

}

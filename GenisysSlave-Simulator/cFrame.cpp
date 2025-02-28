///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.0.0-0-g0efcecf)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "cFrame.h"

///////////////////////////////////////////////////////////////////////////

cFrame::cFrame( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxFrame( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 845,600 ), wxDefaultSize );
	this->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_ACTIVEBORDER ) );

	wxBoxSizer* bSizer1;
	bSizer1 = new wxBoxSizer( wxVERTICAL );

	wxBoxSizer* bSizer8;
	bSizer8 = new wxBoxSizer( wxVERTICAL );

	m_panel1 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer9;
	bSizer9 = new wxBoxSizer( wxHORIZONTAL );

	wxBoxSizer* bSizer6;
	bSizer6 = new wxBoxSizer( wxHORIZONTAL );

	wxBoxSizer* bSizer7;
	bSizer7 = new wxBoxSizer( wxVERTICAL );

	m_staticText4 = new wxStaticText( m_panel1, wxID_ANY, wxT("UDP Port Settings:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText4->Wrap( -1 );
	bSizer7->Add( m_staticText4, 0, wxALIGN_CENTER|wxALL, 5 );

	m_staticText7 = new wxStaticText( m_panel1, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText7->Wrap( -1 );
	bSizer7->Add( m_staticText7, 0, wxALIGN_CENTER|wxALL, 5 );

	m_staticText2 = new wxStaticText( m_panel1, wxID_ANY, wxT("Receiving"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText2->Wrap( -1 );
	bSizer7->Add( m_staticText2, 0, wxALIGN_CENTER|wxALL, 5 );

	m_textUDPrx = new wxTextCtrl( m_panel1, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer7->Add( m_textUDPrx, 0, wxALIGN_CENTER|wxALL, 5 );

	m_staticText3 = new wxStaticText( m_panel1, wxID_ANY, wxT("Transmitting"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText3->Wrap( -1 );
	bSizer7->Add( m_staticText3, 0, wxALIGN_CENTER|wxALL, 5 );

	m_textUDPtx = new wxTextCtrl( m_panel1, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer7->Add( m_textUDPtx, 0, wxALIGN_CENTER|wxALL, 5 );

	m_staticText5 = new wxStaticText( m_panel1, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText5->Wrap( -1 );
	bSizer7->Add( m_staticText5, 0, wxALIGN_CENTER|wxALL, 5 );

	m_staticText6 = new wxStaticText( m_panel1, wxID_ANY, wxT("Slave ID"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText6->Wrap( -1 );
	bSizer7->Add( m_staticText6, 0, wxALIGN_CENTER|wxALL, 5 );

	m_textSlaveID = new wxTextCtrl( m_panel1, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer7->Add( m_textSlaveID, 0, wxALIGN_CENTER|wxALL, 5 );

	m_staticText8 = new wxStaticText( m_panel1, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText8->Wrap( -1 );
	bSizer7->Add( m_staticText8, 0, wxALIGN_CENTER|wxALL, 5 );

	m_staticText9 = new wxStaticText( m_panel1, wxID_ANY, wxT("Frame Size [Bytes]"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText9->Wrap( -1 );
	bSizer7->Add( m_staticText9, 0, wxALIGN_CENTER|wxALL, 5 );

	m_textFrameSize = new wxTextCtrl( m_panel1, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer7->Add( m_textFrameSize, 0, wxALIGN_CENTER|wxALL, 5 );

	m_staticText10 = new wxStaticText( m_panel1, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText10->Wrap( -1 );
	bSizer7->Add( m_staticText10, 0, wxALIGN_CENTER|wxALL, 5 );

	m_toggleStartStop = new wxToggleButton( m_panel1, wxID_ANY, wxT("Start/Stop"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer7->Add( m_toggleStartStop, 0, wxALIGN_CENTER|wxALL, 5 );

	m_staticText101 = new wxStaticText( m_panel1, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText101->Wrap( -1 );
	bSizer7->Add( m_staticText101, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

	m_staticText102 = new wxStaticText( m_panel1, wxID_ANY, wxT("Last Master IP:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText102->Wrap( -1 );
	bSizer7->Add( m_staticText102, 0, wxALL, 5 );

	m_staticTextLastMasterIp = new wxStaticText( m_panel1, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextLastMasterIp->Wrap( -1 );
	bSizer7->Add( m_staticTextLastMasterIp, 0, wxALL, 5 );

	m_staticText1022 = new wxStaticText( m_panel1, wxID_ANY, wxT("Responce Counter:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText1022->Wrap( -1 );
	bSizer7->Add( m_staticText1022, 0, wxALL, 5 );

	m_staticTextResponceCounter = new wxStaticText( m_panel1, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextResponceCounter->Wrap( -1 );
	bSizer7->Add( m_staticTextResponceCounter, 0, wxALL, 5 );


	bSizer6->Add( bSizer7, 1, wxEXPAND, 0 );


	bSizer9->Add( bSizer6, 0, wxEXPAND, 0 );

	wxBoxSizer* bSizer4;
	bSizer4 = new wxBoxSizer( wxVERTICAL );

	m_staticText11 = new wxStaticText( m_panel1, wxID_ANY, wxT("Indications (Read/Write)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText11->Wrap( -1 );
	bSizer4->Add( m_staticText11, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

	m_grid3 = new wxGrid( m_panel1, wxID_ANY, wxDefaultPosition, wxSize( 340,-1 ), wxVSCROLL );

	// Grid
	m_grid3->CreateGrid( 125, 8 );
	m_grid3->EnableEditing( false );
	m_grid3->EnableGridLines( true );
	m_grid3->SetGridLineColour( wxSystemSettings::GetColour( wxSYS_COLOUR_ACTIVECAPTION ) );
	m_grid3->EnableDragGridSize( false );
	m_grid3->SetMargins( 0, 0 );

	// Columns
	m_grid3->SetColSize( 0, 30 );
	m_grid3->SetColSize( 1, 30 );
	m_grid3->SetColSize( 2, 30 );
	m_grid3->SetColSize( 3, 30 );
	m_grid3->SetColSize( 4, 30 );
	m_grid3->SetColSize( 5, 30 );
	m_grid3->SetColSize( 6, 30 );
	m_grid3->SetColSize( 7, 30 );
	m_grid3->EnableDragColMove( false );
	m_grid3->EnableDragColSize( false );
	m_grid3->SetColLabelValue( 0, wxT("Bit 7") );
	m_grid3->SetColLabelValue( 1, wxT("Bit 6") );
	m_grid3->SetColLabelValue( 2, wxT("Bit 5") );
	m_grid3->SetColLabelValue( 3, wxT("Bit 4") );
	m_grid3->SetColLabelValue( 4, wxT("Bit 3") );
	m_grid3->SetColLabelValue( 5, wxT("Bit 2") );
	m_grid3->SetColLabelValue( 6, wxT("Bit 1") );
	m_grid3->SetColLabelValue( 7, wxT("Bit 0") );
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

	bSizer4->Add( m_grid3, 0, wxALL, 5 );


	bSizer9->Add( bSizer4, 0, wxEXPAND, 5 );

	wxBoxSizer* bSizer81;
	bSizer81 = new wxBoxSizer( wxVERTICAL );

	m_staticText111 = new wxStaticText( m_panel1, wxID_ANY, wxT("Controls (Read Only)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText111->Wrap( -1 );
	bSizer81->Add( m_staticText111, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

	m_grid31 = new wxGrid( m_panel1, wxID_ANY, wxDefaultPosition, wxSize( 340,-1 ), wxVSCROLL );

	// Grid
	m_grid31->CreateGrid( 125, 8 );
	m_grid31->EnableEditing( false );
	m_grid31->EnableGridLines( true );
	m_grid31->SetGridLineColour( wxSystemSettings::GetColour( wxSYS_COLOUR_ACTIVECAPTION ) );
	m_grid31->EnableDragGridSize( false );
	m_grid31->SetMargins( 0, 0 );

	// Columns
	m_grid31->SetColSize( 0, 30 );
	m_grid31->SetColSize( 1, 30 );
	m_grid31->SetColSize( 2, 30 );
	m_grid31->SetColSize( 3, 30 );
	m_grid31->SetColSize( 4, 30 );
	m_grid31->SetColSize( 5, 30 );
	m_grid31->SetColSize( 6, 30 );
	m_grid31->SetColSize( 7, 30 );
	m_grid31->EnableDragColMove( false );
	m_grid31->EnableDragColSize( false );
	m_grid31->SetColLabelValue( 0, wxT("Bit 7") );
	m_grid31->SetColLabelValue( 1, wxT("Bit 6") );
	m_grid31->SetColLabelValue( 2, wxT("Bit 5") );
	m_grid31->SetColLabelValue( 3, wxT("Bit 4") );
	m_grid31->SetColLabelValue( 4, wxT("Bit 3") );
	m_grid31->SetColLabelValue( 5, wxT("Bit 2") );
	m_grid31->SetColLabelValue( 6, wxT("Bit 1") );
	m_grid31->SetColLabelValue( 7, wxT("Bit 0") );
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

	bSizer81->Add( m_grid31, 0, wxALL, 5 );


	bSizer9->Add( bSizer81, 0, wxEXPAND, 5 );


	m_panel1->SetSizer( bSizer9 );
	m_panel1->Layout();
	bSizer9->Fit( m_panel1 );
	bSizer8->Add( m_panel1, 1, wxEXPAND | wxALL, 5 );


	bSizer1->Add( bSizer8, 1, wxEXPAND, 5 );


	this->SetSizer( bSizer1 );
	this->Layout();
	m_menubar3 = new wxMenuBar( 0 );
	MenuHelp = new wxMenu();
	wxMenuItem* MenuHelpLog;
	MenuHelpLog = new wxMenuItem( MenuHelp, wxID_ANY, wxString( wxT("Log") ) , wxEmptyString, wxITEM_NORMAL );
	MenuHelp->Append( MenuHelpLog );

	wxMenuItem* MenuHelpAbout;
	MenuHelpAbout = new wxMenuItem( MenuHelp, wxID_ANY, wxString( wxT("About\n") ) , wxEmptyString, wxITEM_NORMAL );
	MenuHelp->Append( MenuHelpAbout );

	m_menubar3->Append( MenuHelp, wxT("Help") );

	this->SetMenuBar( m_menubar3 );


	this->Centre( wxBOTH );

	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( cFrame::TopLevelWindowCloseEvent ) );
	m_toggleStartStop->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( cFrame::StartStopServerEvent ), NULL, this );
	m_grid3->Connect( wxEVT_GRID_CELL_LEFT_CLICK, wxGridEventHandler( cFrame::GridClickEvent ), NULL, this );
	MenuHelp->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( cFrame::MenuHelpLogOnMenuSelection ), this, MenuHelpLog->GetId());
	MenuHelp->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( cFrame::MenuHelpAboutOnMenuSelection ), this, MenuHelpAbout->GetId());
}

cFrame::~cFrame()
{
	// Disconnect Events
	this->Disconnect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( cFrame::TopLevelWindowCloseEvent ) );
	m_toggleStartStop->Disconnect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( cFrame::StartStopServerEvent ), NULL, this );
	m_grid3->Disconnect( wxEVT_GRID_CELL_LEFT_CLICK, wxGridEventHandler( cFrame::GridClickEvent ), NULL, this );

}

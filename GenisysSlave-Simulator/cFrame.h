#pragma once

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/string.h>
#include <wx/stattext.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/radiobox.h>
#include <wx/textctrl.h>
#include <wx/combobox.h>
#include <wx/tglbtn.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/sizer.h>
#include <wx/grid.h>
#include <wx/listbox.h>
#include <wx/panel.h>
#include <wx/menu.h>
#include <wx/frame.h>

///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
/// Class cFrame
///////////////////////////////////////////////////////////////////////////////
class cFrame : public wxFrame
{
	private:

	protected:
		wxPanel* m_panel1;
		wxBoxSizer* m_transportFields;
		void ShowTransportSettings(int selection);

		wxRadioBox* m_radioBox1;
		wxTextCtrl* m_textUDPrx;
		wxTextCtrl* m_textUDPtx;
		wxListBox* m_slaveList;
		wxButton* m_addSlave;
		wxButton* m_removeSlave;
		wxButton* m_saveSlave;
		wxTextCtrl* m_textSlaveID;
		wxTextCtrl* m_textFrameSize;
		wxCheckBox* m_alwaysRecallResponse;
		wxCheckBox* m_includeRecallMode;
		wxTextCtrl* m_textPollTimeout;
		wxToggleButton* m_toggleStartStop;
		wxStaticText* m_staticTextLastMasterIp;
		wxStaticText* m_staticTextMasterStatus;
		wxStaticText* m_staticTextLastModeByte;
		wxTextCtrl* m_textModeByteReturn;
		wxButton* m_buttonApplyModeByte;
		wxStaticText* m_staticTextResponceCounter;
		wxStaticText* m_staticText11;
		wxGrid* m_grid3;
		wxStaticText* m_staticText111;
		wxGrid* m_grid31;
		wxComboBox* m_comboComPort;
		wxComboBox* m_comboBaudRate;
		wxComboBox* m_comboDataBits;
		wxComboBox* m_comboStopBits;
		wxComboBox* m_comboParity;
		wxMenuBar* m_menubar3;
		wxMenu* MenuHelp;

		// Virtual event handlers, override them in your derived class
		virtual void TopLevelWindowCloseEvent( wxCloseEvent& event ) { event.Skip(); }
		virtual void ProtocolSelectionEvent( wxCommandEvent& event ) { event.Skip(); }
		virtual void StartStopServerEvent( wxCommandEvent& event ) { event.Skip(); }
		virtual void ApplyModeByteEvent( wxCommandEvent& event ) { event.Skip(); }
		virtual void GridClickEvent( wxGridEvent& event ) { event.Skip(); }
		virtual void GridDoubleClickEvent( wxGridEvent& event ) { event.Skip(); }
		virtual void MenuHelpLogOnMenuSelection( wxCommandEvent& event ) { event.Skip(); }
		virtual void MenuHelpProtocolLogOnMenuSelection( wxCommandEvent& event ) { event.Skip(); }
		virtual void MenuHelpAboutOnMenuSelection( wxCommandEvent& event ) { event.Skip(); }


	public:

		cFrame( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("Genisys Slave Application"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 1100,655 ), long style = wxDEFAULT_FRAME_STYLE|wxTAB_TRAVERSAL );

		~cFrame();

		void PopulateComPorts();

};


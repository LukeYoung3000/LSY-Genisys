///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.0.0-0-g0efcecf)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#pragma once

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/string.h>
#include <wx/stattext.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/textctrl.h>
#include <wx/tglbtn.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/sizer.h>
#include <wx/grid.h>
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
		wxStaticText* m_staticText4;
		wxStaticText* m_staticText7;
		wxStaticText* m_staticText2;
		wxTextCtrl* m_textUDPrx;
		wxStaticText* m_staticText3;
		wxTextCtrl* m_textUDPtx;
		wxStaticText* m_staticText5;
		wxStaticText* m_staticText6;
		wxTextCtrl* m_textSlaveID;
		wxStaticText* m_staticText8;
		wxStaticText* m_staticText9;
		wxTextCtrl* m_textFrameSize;
		wxStaticText* m_staticText10;
		wxToggleButton* m_toggleStartStop;
		wxStaticText* m_staticText11;
		wxGrid* m_grid3;
		wxStaticText* m_staticText111;
		wxGrid* m_grid31;
		wxMenuBar* m_menubar3;
		wxMenu* MenuHelp;

		// Virtual event handlers, override them in your derived class
		virtual void StartStopServerEvent( wxCommandEvent& event ) { event.Skip(); }
		virtual void GridClickEvent( wxGridEvent& event ) { event.Skip(); }
		virtual void MenuHelpAboutOnMenuSelection( wxCommandEvent& event ) { event.Skip(); }


	public:

		cFrame( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("Genisys UDP Slave Application"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 845,600 ), long style = wxDEFAULT_FRAME_STYLE|wxTAB_TRAVERSAL );

		~cFrame();

};


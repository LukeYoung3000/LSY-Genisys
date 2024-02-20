#include "cFrame.h"

#include <wx/event.h>

class cEvents : public cFrame
{
public:

public:

	cEvents();
	void GridClickEvent(wxGridEvent& event) override;
	void StartStopServerEvent(wxCommandEvent& event) override;
	void MenuHelpAboutOnMenuSelection(wxCommandEvent& event) override;


	void IdleEvent(wxIdleEvent& event);


};
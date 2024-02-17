#include "cFrame.h"

class cEvents : public cFrame
{

public:

	cEvents();
	void GridClickEvent(wxGridEvent& event) override;
	void StartStopServerEvent(wxCommandEvent& event) override;
	void MenuHelpAboutOnMenuSelection(wxCommandEvent& event) override;



};
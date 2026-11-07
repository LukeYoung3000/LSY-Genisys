Genisys Master GUI (Windows x64)

Extract the entire ZIP and launch GenisysMaster-GUI.exe. Keep the DLLs and plugin
subfolders alongside the executable. If a Visual C++ runtime is missing, run the
included official vc_redist.x64.exe installer. No Qt SDK or Visual Studio required.

Open XML to load GenisysMaster.example.xml, adjust addresses for your test setup,
and click Start. You can also Add/Edit/Remove slave IDs while stopped. Multiple
IDs may use the same remote IP:port and share one local UDP socket. Port 0 uses
an OS-assigned local port; the status bar shows the actual bound port.

Start runs automatic polling when enabled. Recall selected/all requests full
indications manually. Red means offline; stale cached data is clearly marked.
Select a slave to view live indication bytes/bits, mode and communication counts.
Amber briefly highlights changed indication bits. Tables retain their position.
Drag panel dividers to change how much space each table receives.

Control editor: click bits or double-click Hex (F2 also works) to edit a local
per-slave draft. Include mode optionally sends the selected mode byte. Click
Send changes or Send full frame explicitly; full frame includes zero bytes.
ACKNOWLEDGED confirms the protocol response, not physical output readback.
Timeout or Stop during an unanswered control means its outcome is unknown.
Controls are never automatically retransmitted. Review before sending again.

Stop closes the socket and unlocks configuration. It retains drafts and cached
indications, marked stale. Open/New or changing slave configuration resets these
transient values. Save XML saves configuration only, never control drafts.
Console-only XML options are retained for compatibility but ignored by the GUI.
The event log can be filtered, cleared, and followed; it retains the newest 1000.

Optional command line:
    GenisysMaster-GUI.exe --config GenisysMaster.example.xml
    GenisysMaster-GUI.exe --config GenisysMaster.example.xml --start
    GenisysMaster-GUI.exe --smoke-test --screenshot preview.png

Normal launches always start stopped unless --start is explicit. Smoke-test
renders and exits automatically. Licenses and upstream source links are included.

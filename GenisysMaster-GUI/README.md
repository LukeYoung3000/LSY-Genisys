# GenisysMaster-GUI

A Qt Widgets desktop test application linked to GenisysMaster-Lib. It provides
XML configuration, multiple slave IDs on one UDP socket or COM port, scheduled polling,
manual recalls, live indication tables, control editing and an event log.
It starts stopped; click Start to open the selected transport. See PACKAGE-README.txt for
operator instructions and the commented GenisysMaster.example.xml for settings.

## Developer setup

The pinned SDK is Qt 6.11.3, MSVC 2022 x64, dynamically linked. It lives outside
Git in the workspace Tools/Qt/6.11.3/msvc2022_64 directory. From the solution
folder, run the following in PowerShell if the SDK is absent:

```powershell
.\GenisysMaster-GUI\scripts\SetupQt.ps1
```

The script requires 7-Zip (default C:/Program Files/7-Zip/7z.exe; override with
-SevenZip). It downloads official Qt base/tools/svg/translations archives,
verifies their published SHA-1 checksums, extracts the SDK, and downloads the
upstream license texts. Cached downloads stay under Tools/Qt/downloads. Use
-InstallRoot to choose another SDK root; archives are placed in its versioned
6.11.3/msvc2022_64 subfolder. Nothing is installed into Windows or added to the
machine PATH. Compiler/IDE installations are not modified.

Open GenisysSlave.sln and select Debug | x64. Set GenisysMaster-GUI as the startup
project, then build and run. The project retains the solution's v145 toolset and
links the existing master library through a project reference. Qt's prebuilt
MSVC SDK and the v145 application/master library are verified by actual builds
and startup checks. Debug and Release must use matching Qt runtime variants.
The GUI is x64-only; it is excluded from x86 solution builds.

For a Qt SDK installed elsewhere, set the user environment variable QT_ROOT
or supply /p:QtSdkRoot=C:/path/to/msvc2022_64 to MSBuild. QtSdkRoot takes precedence.
Use the pinned SDK version for reproducible results. No Qt binaries are committed.

## Qt build integration

This is a native MSBuild vcxproj in the existing solution. It invokes SDK moc/uic
as incremental CustomBuild steps for MainWindow.h, MasterSession.h and MainWindow.ui. Generated files
stay under builds/x64/<Configuration>/obj/generated. Qt VS Tools and Qt Creator
are not required for building. MainWindow.ui supplies the window/menu shell; the model-backed tables and splitters are built in MainWindow.cpp. To edit the shell visually, open MainWindow.ui with
Tools/Qt/6.11.3/msvc2022_64/bin/designer.exe; Qt VS Tools can optionally provide
Designer integration inside Visual Studio.

New Q_OBJECT headers or .ui files will need matching moc/uic CustomBuild entries
until a later migration to more extensive Qt build tooling is warranted. Runtime
DLLs and plugins are copied beside the executable using windeployqt after a new
build. Qt headers use external-header warning handling; application warnings and
SDL checks stay enabled. Qt is a dependency of the GUI only.

The startup check constructs master data/protocol/network objects and compares
a generated recall against a known vector without opening a socket. It exercises
real static-library symbols rather than just including a version header.

```bat
GenisysMaster-GUI\builds\x64\Debug\GenisysMaster-GUI.exe --smoke-test
```

--smoke-test exits automatically after rendering. Add --screenshot with a path
for visual inspection. Normal launches remain open until closed by the user.

## Release package

```powershell
.\GenisysMaster-GUI\scripts\PackageGui.ps1
```

This builds Release | x64, creates a fresh folder under builds/x64/Release/packages,
collects the Qt runtime/plugins, copies notices/license texts and SPDX manifests,
adds the official Visual C++ Redistributable installer, and produces a ZIP. Existing
packages are never deleted. Optional parameters: -QtRoot and -MSBuild.

Extract the ZIP completely before running. Another engineer needs no Qt SDK or
Visual Studio; the official VC runtime may need installation using the included
installer. The runtime is not silently installed. This checkpoint verifies a
relocated package on the development PC with Qt removed from the process PATH;
it does not substitute for testing on a separate clean Windows machine.

See PACKAGE-README.txt for recipient instructions and THIRD-PARTY-NOTICES.txt for
Qt/pugixml source and license references.

## How it works

MasterSession owns configuration, indication frames, per-slave control drafts,
and the native protocol/network objects. A precise 10 ms Qt timer services the
library's nonblocking UDP/serial processing on the GUI thread. No worker thread accesses
the frames. Table models expose existing data; the window refreshes at 100 ms
without resetting selection, scrolling or uncommitted control editors. Recent
indication changes are amber; stale data is grey. The event model retains 1000.

Controls use the same ConsoleControls helper as the console, so acknowledgement,
snapshot and uncertain-outcome behavior stay consistent. Click bits or edit Hex,
then explicitly Send changes/full frame. Include mode is optional. Stopping an
unanswered control marks its outcome unknown and retains the draft. A protocol
ACK is not physical output feedback. Reopening configuration or editing the slave
list resets transient frames/drafts; saving XML saves settings only.

The existing console XML loader and vendored pugixml are compiled into this GUI
project. A wide-path loader overload supports Windows Unicode filenames; XML
saves use QSaveFile for atomic replacement. No socket or COM port starts as a side effect of
loading XML. Console-only XML options are preserved but ignored here. Optional
CLI flags: --config path and --start. The master library itself has no Qt dependency.

## Integration checks

```powershell
.\GenisysMaster-GUI\scripts\RunGuiChecks.ps1
```

The script builds a separate GenisysMaster-GUI-Checks.exe with /p:GuiChecks=true,
then runs the real window with QtTest widget actions and the existing slave
protocol over loopback UDP and an injected serial byte-stream peer. It needs no external device or running simulator.
Checks cover shared endpoints, polling, indications, stale/offline/recovery,
bit/hex controls, mode, acknowledgements, no automatic FC retry, Stop in flight,
per-slave drafts, XML round-trips/Unicode/error handling, Add/Edit/Remove dialogs,
and event filtering/bounds. Reports and a running-window PNG go under
builds/x64/Debug. Failure returns nonzero; the check process has a 60 s timeout.
QtTest and the slave test peer are only linked in the check build, not the GUI.

The eight master/console check executables remain available through
GenisysMaster-Lib/tests/RunChecks.cmd in an x64 Native Tools prompt.

## Serial use and implementation

Choose Serial (COM) in Transport, select/type a COM name, and set baud, parity,
stop bits, write timeout and optional fixed DTR/RTS levels. The editable COM list
is populated from Windows' SERIALCOMM registry values; Refresh preserves the
selected/typed name. No QtSerialPort module or additional serial library is needed.
Eight data bits are fixed because Genisys requires them. For RS-485, use an adapter
with automatic transmit-direction control. Start/Stop locks/unlocks settings as
for UDP. In serial mode the slave editor only needs ID and frame size, and the
slave table and details show the shared COM bus. UDP stays the launch default.

Both XML examples are copied beside Debug/Release executables and included in
the Release ZIP. GenisysMaster.serial.example.xml uses the console's version-1
schema; GUI saves can be loaded by the console. Save stores the selected transport
settings and retains known UDP mappings, while serial-only IDs remain endpoint-free.
Switching serial-only IDs to UDP requires editing in real IP/port mappings before
Start or Save. Normal XML loading stays stopped, even if console-only one-shot flags
are present. --start is the explicit opt-in for immediate communications.

MasterSession reuses ConsoleRecalls so manual recalls, including Recall all,
queue behind an active exchange and ahead of new scheduled polls. Serial exchanges
are serialized across all IDs. The network Process(false) overload suspends only
new automatic requests while the queue drains; responses, deadlines and retries
continue. Controls are explicit and never queued or automatically retransmitted;
the Send buttons account for any ID owning the shared bus. Unanswered control
outcomes remain uncertain. Stop drops queued recalls and retains local drafts.

On a fatal serial I/O error, response/transaction events are consumed before
retiring the network. The GUI keeps cached data stale, shows offline/error status,
unlocks configuration, and offers Start to reconnect. Drafts are retained but never
sent by reconnecting. Ordinary response timeouts keep the port open and allow
normal offline recovery. The protocol and data-frame implementations remain unchanged.

GUI checks also exercise serial widget population/editing, refresh preservation,
ID-only dialogs, XML/Unicode round-trips and failed-save atomicity, shared-bus
recalls/control gating, byte-fragmented indications, ACKs/timeouts, port failure
and reconnect using the real session/controller with a private injected backend.
The stream peer is shared with the library's serial checks. The private factory
is only a test seam; normal builds and all user-visible transports use Windows I/O.
Native unavailable-COM diagnostics are checked too. Actual connected-COM driver
traffic still needs physical or virtual paired ports; it is explicitly reported
as skipped. Reports include GuiChecks.png (UDP) and GuiSerialChecks.png (serial).

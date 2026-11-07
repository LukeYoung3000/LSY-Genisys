@echo off
setlocal
rem ---------------------------------------------------------------------------
rem HOW TO USE
rem ---------------------------------------------------------------------------
rem 1. Open an "x64 Native Tools Command Prompt for Visual Studio".
rem 2. Change to the directory containing GenisysSlave.sln.
rem 3. Run: GenisysMaster-Lib\tests\RunChecks.cmd
rem    Alternatively, run this script using its full path from any directory.
rem
rem Success prints PASS messages followed by:
rem   All MasterDataFrame checks passed.
rem   All ProtocolGenisysMaster checks passed.
rem   All NetworkGenisysMaster checks passed.
rem   All PollingGenisysMaster checks passed.
rem   All ConsoleControls checks passed.
rem   All ConsoleConfiguration checks passed.
rem A failed check prints FAIL with an explanation. Build or check failures
rem return exit code 1; success returns exit code 0.
rem
rem HOW IT WORKS
rem ---------------------------------------------------------------------------
rem This script builds GenisysMaster-Lib in Debug x64 using MSBuild, then uses
rem the Visual C++ compiler (cl) to compile MasterDataFrameChecks.cpp and
rem ProtocolGenisysMasterChecks.cpp separately, linking each against the library.
rem
rem It runs six resulting console executables:
rem   GenisysMaster-Lib\builds\x64\Debug\checks\MasterDataFrameChecks.exe
rem   GenisysMaster-Lib\builds\x64\Debug\checks\ProtocolGenisysMasterChecks.exe
rem   GenisysMaster-Lib\builds\x64\Debug\checks\NetworkGenisysMasterChecks.exe
rem   GenisysMaster-Lib\builds\x64\Debug\checks\PollingGenisysMasterChecks.exe
rem   GenisysMaster-Lib\builds\x64\Debug\checks\ConsoleControlsChecks.exe
rem   GenisysMaster-Lib\builds\x64\Debug\checks\ConsoleConfigurationChecks.exe
rem The first executable creates data frames in memory and verifies bit/byte access,
rem bounds, control snapshots and acknowledgement, indication callbacks,
rem duplicate-event suppression, mode updates, and invalid payload rejection.
rem The second checks request bytes against GE/capture examples, response
rem parsing, CRCs, escaping, fragmented/combined messages, invalid payloads,
rem callbacks, and control acknowledgement.
rem The third executable runs real UDP loopback checks against the existing
rem slave protocol, with shared and separate endpoints, source validation,
rem partial datagram rejection and socket lifecycle checks. It also builds
rem GenisysSlave-Lib. The fourth executable checks scheduled polling, lost
rem messages, retry limits, offline recovery and controls that time out without
rem retransmission. The fifth checks console control drafts, explicit sends,
rem slave control callbacks, mode/full-frame sends and uncertain outcomes.
rem The sixth validates XML loading, defaults, error reporting and the example.
rem No external device or running simulator is required.
rem
rem All six executables are built by this script, not separate solution projects.
rem Test build products stay under builds\x64\Debug\checks. The script restores
rem the original working directory before exiting.
rem ---------------------------------------------------------------------------
where cl >nul 2>nul
if errorlevel 1 (
  echo Open an x64 Native Tools Command Prompt for Visual Studio first.
  exit /b 1
)
pushd "%~dp0.."
if not exist builds\x64\Debug\checks mkdir builds\x64\Debug\checks
msbuild GenisysMaster-Lib.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal /nologo
if errorlevel 1 goto failed
cl /nologo /EHsc /W4 /MDd /Zi /I include tests\MasterDataFrameChecks.cpp /Fo:builds\x64\Debug\checks\MasterDataFrameChecks.obj /Fd:builds\x64\Debug\checks\Compiler.pdb /Fe:builds\x64\Debug\checks\MasterDataFrameChecks.exe /link builds\x64\Debug\GenisysMaster-Lib.lib /PDB:builds\x64\Debug\checks\MasterDataFrameChecks.pdb
if errorlevel 1 goto failed
builds\x64\Debug\checks\MasterDataFrameChecks.exe
if errorlevel 1 goto failed
cl /nologo /EHsc /W4 /MDd /Zi /I include tests\ProtocolGenisysMasterChecks.cpp /Fo:builds\x64\Debug\checks\ProtocolGenisysMasterChecks.obj /Fd:builds\x64\Debug\checks\ProtocolCompiler.pdb /Fe:builds\x64\Debug\checks\ProtocolGenisysMasterChecks.exe /link builds\x64\Debug\GenisysMaster-Lib.lib /PDB:builds\x64\Debug\checks\ProtocolGenisysMasterChecks.pdb
if errorlevel 1 goto failed
builds\x64\Debug\checks\ProtocolGenisysMasterChecks.exe
if errorlevel 1 goto failed
msbuild ..\GenisysSlave-Lib\GenisysSlave-Lib.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal /nologo
if errorlevel 1 goto failed
cl /nologo /EHsc /W4 /MDd /Zi /I include /I ..\GenisysSlave-Lib\include tests\NetworkGenisysMasterChecks.cpp /Fo:builds\x64\Debug\checks\NetworkGenisysMasterChecks.obj /Fd:builds\x64\Debug\checks\NetworkCompiler.pdb /Fe:builds\x64\Debug\checks\NetworkGenisysMasterChecks.exe /link builds\x64\Debug\GenisysMaster-Lib.lib ..\GenisysSlave-Lib\builds\x64\Debug\GenisysSlave-Lib.lib /PDB:builds\x64\Debug\checks\NetworkGenisysMasterChecks.pdb
if errorlevel 1 goto failed
builds\x64\Debug\checks\NetworkGenisysMasterChecks.exe
if errorlevel 1 goto failed
cl /nologo /EHsc /W4 /MDd /Zi /I include /I ..\GenisysSlave-Lib\include tests\PollingGenisysMasterChecks.cpp /Fo:builds\x64\Debug\checks\PollingGenisysMasterChecks.obj /Fd:builds\x64\Debug\checks\PollingCompiler.pdb /Fe:builds\x64\Debug\checks\PollingGenisysMasterChecks.exe /link builds\x64\Debug\GenisysMaster-Lib.lib ..\GenisysSlave-Lib\builds\x64\Debug\GenisysSlave-Lib.lib /PDB:builds\x64\Debug\checks\PollingGenisysMasterChecks.pdb
if errorlevel 1 goto failed
builds\x64\Debug\checks\PollingGenisysMasterChecks.exe
if errorlevel 1 goto failed
cl /nologo /EHsc /W4 /MDd /Zi /I include /I ..\GenisysMaster-Console /I ..\GenisysSlave-Lib\include tests\ConsoleControlsChecks.cpp /Fo:builds\x64\Debug\checks\ConsoleControlsChecks.obj /Fd:builds\x64\Debug\checks\ControlsCompiler.pdb /Fe:builds\x64\Debug\checks\ConsoleControlsChecks.exe /link builds\x64\Debug\GenisysMaster-Lib.lib ..\GenisysSlave-Lib\builds\x64\Debug\GenisysSlave-Lib.lib /PDB:builds\x64\Debug\checks\ConsoleControlsChecks.pdb
if errorlevel 1 goto failed
builds\x64\Debug\checks\ConsoleControlsChecks.exe
if errorlevel 1 goto failed
cl /nologo /EHsc /W4 /MDd /Zi /I include /I ..\GenisysMaster-Console tests\ConsoleConfigurationChecks.cpp ..\GenisysMaster-Console\ConsoleConfiguration.cpp ..\GenisysMaster-Console\third_party\pugixml\pugixml.cpp /Fo:builds\x64\Debug\checks\ /Fd:builds\x64\Debug\checks\ConfigCompiler.pdb /Fe:builds\x64\Debug\checks\ConsoleConfigurationChecks.exe /link /PDB:builds\x64\Debug\checks\ConsoleConfigurationChecks.pdb
if errorlevel 1 goto failed
builds\x64\Debug\checks\ConsoleConfigurationChecks.exe
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1

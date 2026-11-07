# GenisysMaster-Console

UDP master test harness with polling, recalls, and explicit control sending. Build GenisysSlave.sln, select this project as
the startup project, and run without debugging (Ctrl+F5), or launch the executable
from a terminal. For Debug x64 the executable is:

    GenisysMaster-Console\builds\x64\Debug\GenisysMaster-Console.exe

With no arguments it opens a fixed, automatically refreshing dashboard, binds
UDP 0.0.0.0:7170, and configures slave 34 at 127.0.0.1:7169 with a 125-byte
indication frame and a separate 125-byte control draft. pugixml is bundled as
source for XML configuration; there are no extra runtime DLLs or package restores.

## XML configuration

From the executable's directory:

```bat
GenisysMaster-Console.exe --config GenisysMaster.example.xml
```

The build copies the fully commented GenisysMaster.example.xml beside the
executable. Copy it to a personal name such as my-test.xml before editing; future
builds refresh the example. Alternatively, pass any absolute or relative XML path,
quoting paths containing spaces. Relative paths use the process working directory,
not the executable's directory. In Visual Studio, set the Debugging command
arguments to `--config "$(ProjectDir)GenisysMaster.example.xml"`.

The XML contains a versioned GenisysMaster root, optional Udp/Polling/Console
sections, and a required nonempty Slaves section. It configures local binding,
poll interval/secure mode, response timeout/retries/recovery, plain/dashboard
output, bounded runs, and slave endpoint/frame sizes. The active example matches
slave 34 on localhost; commented entries show more IDs and another device.

XML is loaded first, regardless of the position of --config. Explicit CLI scalar
flags then override its settings. Any --slave entries replace the ENTIRE XML
slave list instead of appending to it. For example:

```bat
GenisysMaster-Console.exe --config my-test.xml --plain --poll-ms 1000
GenisysMaster-Console.exe --config my-test.xml --slave 34 127.0.0.1 7169 125
```

Use --config only once. With no config flag, existing defaults and CLI behaviour
are unchanged. Missing or malformed files, unsupported versions, unknown or
duplicate settings, missing required slave attributes, invalid IPs/ranges,
and duplicate slave IDs produce an error and exit code 1 before opening a socket.
Names are case-sensitive, boolean values are true/false, and numbers accept
strict decimal or 0x hex. Optional sections/attributes retain documented defaults.
No control values are loaded or sent by the configuration file.

Parsing lives in ConsoleConfiguration.cpp and uses the pinned, MIT-licensed
pugixml 1.16 sources in third_party/pugixml. It is compiled into the console only;
GenisysMaster-Lib has no XML dependency. Library users continue to configure the
transport through its Config/Endpoint structs.

## Dashboard (default)

The slave table stays in place as status and counters update. The selected slave
has a scrollable indication-byte view showing hex and individual bits. Changed
bits are highlighted yellow for 1.2 seconds. Status and current/stale data are
shown explicitly; cached values remain visible after a communication failure.
Routine response messages update the table quietly instead of scrolling it.

A timestamped event panel retains the latest 200 events: indication/mode changes,
status transitions, manual recalls, retries and errors. It can be scrolled without
stopping communications. The display refreshes five times per second, while the
network processing loop continues every 10 ms. The terminal must be at least
76 columns by 24 rows; smaller windows show a resize message while polling
continues. Resizing restores the normal layout automatically.

| Key | Action |
|---|---|
| Up / Down | Select a slave; the table scrolls when necessary |
| PageUp / PageDown | Scroll selected slave's data bytes |
| Home / End | First / last page of data bytes |
| C | Switch indications / controls |
| J / K | Select next / previous control byte |
| Left / Right | Select a control bit (7 at the left, 0 at the right) |
| Space | Toggle selected bit in the local draft |
| E | Enter a control-byte value; Enter stages it, Esc cancels |
| M | Enter a mode byte; use - to omit it |
| S | Explicitly send changed controls and the optional mode byte |
| F | Explicitly send the full control frame, including unchanged/zero bytes |
| R | Recall selected slave |
| A | Recall all slaves |
| [ / ] | Scroll older / newer events |
| H / F1 | Toggle keyboard help |
| Esc | Close help |
| Q / Ctrl+C | Quit and restore the previous screen |

Navigation/send shortcuts take effect immediately, without Enter. Byte/mode
edit prompts use Enter to stage the value or Esc to cancel. Help and navigation
do not pause polling. Busy slaves can refuse a recall until their active exchange
finishes. The dashboard uses a separate Windows console screen buffer and restores
the original buffer and input settings on normal exit or a handled Ctrl+C.

## Plain-text mode

Use --plain to retain the scrolling output and line commands:

```bat
GenisysMaster-Console.exe --plain
```

Plain-text mode is also selected automatically when input or output is redirected,
so captured logs and scripts do not receive screen-control data. For predictable
scripted use, explicitly pass --plain with --recall-all or --run-ms. --help always
prints plain text and exits.

## Test with GenisysSlave-Simulator

1. Select UDP in the simulator.
2. Set Receiving to 7169, Transmitting to 7170, Slave ID to 34, and Frame Size
   to 125. Start the simulator and change some indication bits.
3. Start GenisysMaster-Console. It automatically recalls slave 34, then polls it.
4. The dashboard shows ONLINE/Current, indication bytes and completion counters.
   Indication/mode callbacks appear in the event panel; FA acknowledgements are
   sent automatically.
5. Change more indication bits; the dashboard updates and highlights the changed bits.
6. Stop the simulator. After the response timeout and retry budget are exhausted,
   the console reports TIMEOUT and OFFLINE. Cached indications remain available.
7. Restart the simulator on the same ports. The master performs a full recall,
   reports ONLINE, and resumes polling. Inspect the table for current/stale
   data, busy state, retry count, completed exchanges and timeouts.

Plain-text interactive commands (with --plain):

- `r <id>`: manually request a full indication recall for that slave.
- `r all`: request a recall for every configured slave.
- `show <id>`: show cached indication bytes and mode (before first recall these
  are zero/unknown, not confirmed device values).
- `controls <id>`: show local desired controls, mode, modified flag and last outcome.
- `setbyte <id> <offset> <value>`: stage one byte locally.
- `setbit <id> <offset> <bit> <0|1>`: stage one bit locally.
- `mode <id> <value|off>`: stage an optional mode byte, or omit it.
- `send <id>`: send changed controls and the optional mode byte.
- `sendall <id>`: send every control byte, including zeros.
- `status`: show communication status and counters for every configured ID.
- `help`: list the plain-text commands.
- `q`: exit and release the socket.

Numbers accept decimal or a 0x prefix. This is a keyboard console; use a real
terminal for interactive mode. Use --recall-all or --run-ms for noninteractive runs.
A manual recall may be refused while that ID has an exchange in progress.

## Configure endpoints

Use --help to display all arguments. Repeat --slave for multiple slave IDs:

```bat
GenisysMaster-Console.exe --local-port 7170 --slave 3 127.0.0.1 7169 32 --slave 4 127.0.0.1 7169 32
```

Both IDs share one remote IP:port and use one bound master UDP socket and one
protocol object. IDs must be unique within the transport, even when remote
devices differ. Other endpoints can be added with more --slave entries. Each
entry supplies ID, numeric IPv4 address, remote UDP port, and indication-frame
size in bytes. The local control draft uses that same size. Supplying any --slave
entries replaces the default slave.

--local-ip chooses the local bind address (default 0.0.0.0). --local-port chooses
the receive port; zero requests an OS-assigned port printed at startup. The same
socket sends requests, receives responses, and sends acknowledgements. Configure
devices with a fixed response destination port to match the master's receive
port. Devices replying to the request's source port also work. Responses must
originate from the configured remote IP and port.

## One-shot mode

```bat
GenisysMaster-Console.exe --plain --local-port 7170 --slave 34 127.0.0.1 7169 125 --recall-all --wait-ms 3000
```

This disables automatic polling, sends one recall per slave (with configured
timeout/retries), processes responses and acknowledgements, and
exits with code 0 after all exchanges reach F1. It exits with code 1 if setup or
sending fails, or if the wait expires. The default wait is 3000 milliseconds.
The console's one-shot deadline is only a test-run limit, not library retry or
offline-detection logic.

## Polling settings and a bounded run

Defaults are 500 ms poll interval, 500 ms response timeout, two retransmissions,
and 1000 ms recovery interval. The poll interval starts at exchange completion.
Configure them on the command line:

```bat
GenisysMaster-Console.exe --poll-ms 500 --timeout-ms 500 --retries 2 --recovery-ms 1000
```

Use --manual to disable automatic polling/recovery while keeping explicit recall
requests, response timeouts and retries. A timed-out manual ID is released so it
can receive another command without restarting the console.

For a bounded noninteractive polling run:

```bat
GenisysMaster-Console.exe --plain --run-ms 10000 --poll-ms 500
```

This runs the normal polling/recovery loop for 10 seconds, prints final status,
and exits with code 0 if every configured slave is ONLINE with current indications,
otherwise 1. Do not combine --run-ms and --recall-all. --run-ms is a console run
limit, not a library setting. Intervals accept 1-600000 ms, retries accept 0-100. A run limit of 0 disables it.

A valid response marks an ID ONLINE. Current indications additionally require a
completed full recall. On failure the values remain cached but are marked stale.
Status callbacks report only transitions; indication callbacks report only changed
values. Repeated F2 responses still receive acknowledgements. Requests/replies for
one silent ID do not stop polling other IDs on the shared socket.

## Send controls to the simulator

Using the simulator settings above:

1. Select the slave with Up/Down and press C for the control view.
2. Use J/K to select a byte and Left/Right to select a bit. Space toggles that
   bit locally. Alternatively, E opens a byte-value prompt (decimal or 0x hex).
3. Optionally use M to stage a mode byte; Enter accepts it, while - omits mode.
   Mode is omitted by default. The simulator accepts ordinary controls without it.
4. Press S to send the changed control bytes. Editing alone never sends them.
5. Verify the simulator's control grid changes. The master shows ACKNOWLEDGED
   after a valid response and exchange completion. Its event panel records the
   control outcome.

F sends the FULL frame, including every local zero. Use this when deliberately
sending a complete control database or forcing initial zero values. S sends
only differences from the last acknowledged local baseline, plus mode if enabled;
if neither byte changes nor a mode is present, no control is sent. A busy slave
refuses the send immediately; it is not queued for later transmission.

The control view displays desired local bytes, NOT a device readback. Initial
zeros are not received from the slave. Yellow rows differ from the last
acknowledged local baseline. You may edit while a previous send is in flight:
its acknowledgement applies to that sent snapshot, and newer edits remain
modified until a subsequent explicit send. The mode byte is stored separately
from ordinary control bytes. Check-back/execute mode is not implemented.

Plain-text equivalent:

```text
setbyte 34 0 0x81
setbit 34 1 3 1
controls 34
send 34
```

To include a mode value, use `mode 34 0x05` before sending; `mode 34 off` omits it.
There is no automatic control-database initialization or restoration on recovery.
Set mode values according to the device/test case; the console does not infer them.

Control results distinguish:

- WAITING: the command was sent and awaits a response.
- ACKNOWLEDGED; finishing exchange: a valid F1/F2 acknowledged the command,
  but the remaining indication acknowledgement exchange is still in progress.
- ACKNOWLEDGED: the exchange completed.
- TIMEOUT - outcome unknown: no valid control response arrived. The slave may
  have applied the command. It is not automatically retransmitted.
- ACKNOWLEDGED; exchange failed: the control was acknowledged, but the later
  indication/FA exchange failed or timed out. This does not undo the acknowledgement.

Acknowledgement is a protocol result, not proof of physical output execution.
After a timeout, the draft remains unacknowledged; a later explicit send uses the
current edited values. Poll/recall/FA retries never automatically resend the FC
control command. Automatic recovery recalls indications only.

## Verification

From an x64 Native Tools Command Prompt in the solution directory, run:

```bat
GenisysMaster-Lib\tests\RunChecks.cmd
```

The fifth executable, ConsoleControlsChecks, sends controls through real UDP
loopback sockets to the existing slave protocol. It verifies applied byte/bit
values and the slave CONTROL_CHANGE callback, mode-only/full-frame sends,
newer edits during a pending send, dropped control responses, explicit edits
and sends after timeout, and acknowledgement tracking when final F1 is lost.
No external device or running simulator is required.

The sixth executable, ConsoleConfigurationChecks, validates the XML example,
optional defaults, all settings, decimal/hex values, shared endpoints, malformed
files, unknown/duplicate settings and invalid ranges. Failed loads leave the
previous configuration unchanged.

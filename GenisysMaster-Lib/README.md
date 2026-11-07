# GenisysMaster-Lib

Master UDP/serial library with polling, retries, communication status, recovery, and explicit controls.

The master supports UDP and Windows COM ports using a caller-driven, nonblocking processing loop.
GenisysMaster-Console is included in the solution for polling and manual recalls.
TCP is not implemented. The console and Qt GUI both support serial configuration and XML. The console includes a control editor and explicit send commands; see its README
for usage.

The static library uses the existing LSY namespace and include/src layout:

- MasterDataFrame: bit/byte API, incoming indication callbacks, and outgoing
  control snapshots awaiting explicit acknowledgement.
- ProtocolGenisysMaster: master request builders and buffered slave-response parser.
- NetworkGenisysMaster: one UDP socket or serial port, a protocol object, and registered slave IDs. Private MasterTransport backends handle Windows I/O.
- MasterLogging: independent logging callbacks, following the slave API style.

MasterDataFrame and MasterLogging have distinct names so master and slave
libraries can eventually be linked into the same test application.

Build GenisysMaster-Lib from GenisysSlave.sln using the same v145 C++ toolset
and Windows SDK as the slave library. Debug/Release and x64/x86 solution
configurations are supported. Outputs are local to this project:

    builds/<Platform>/<Configuration>/GenisysMaster-Lib.lib
    builds/<Platform>/<Configuration>/obj/

GenisysMaster-Console links this library through a project reference. See its
README for simulator settings, interactive commands, and one-shot recalls.

## Data-frame usage

Create separate MasterDataFrame objects for indications and controls, as with
the slave library. Use WriteBit/WriteByte/WriteBytes on the control frame to
stage desired controls. These local writes do not trigger callbacks.

GetUpdates prepares the changed control bytes relative to the acknowledged
baseline (initially all zero). GetUpdates(..., true) prepares the complete frame,
including zero bytes. Once prepared, the snapshot remains unchanged until
DataAck or CancelUpdate, even if the application stages newer control values. Repeated reads
never automatically acknowledge controls. DataAck acknowledges only the
prepared snapshot; newer writes remain available for the next update.
DataAck returns false when there is no pending snapshot. Preparing an update
does not send it; the protocol/transport layers perform transmission and track
responses. CancelUpdate releases an abandoned snapshot without changing the
acknowledged baseline, so a later explicit send can use the newest desired values.
Protocol CancelRequest invokes this for an unanswered control on timeout/send
failure. It does not acknowledge the command or automatically resend it.

RecvBytes is the protocol-facing incoming indication API. It expects already
validated message data (CRC/framing checks belong to the protocol layer).
It validates all offsets before applying anything and returns -1 on invalid
input, otherwise the number of received pairs. Ordinary indication offsets must
be within the frame and below E0. E0 is stored separately as the mode byte;
HasModeByte distinguishes an unknown mode from a received value of zero.
GetModeByte reads its last received value. Other reserved offsets are rejected.
Received indications become the local baseline and do not create outgoing
control updates. Use RecvBytes only on the separate indication frame.

Register callbacks with the same AddEventCallback pattern as the slave:

```cpp
LSY::MasterDataFrame indications("Indications", 32);
std::function<void(std::string, LSY::MasterDataFrame::CALLBACKEVENT)> callback =
    [&](std::string name, LSY::MasterDataFrame::CALLBACKEVENT event)
    {
        if (event == LSY::MasterDataFrame::CALLBACKEVENT::INDICATION_CHANGE)
        {
            // All values from this update are already available to read.
            const uint8_t first_byte = indications[0];
        }
    };
indications.AddEventCallback(callback);
```

INDICATION_CHANGE fires once per received update when the final indication
values differ. Repeated data, empty updates, and invalid updates do not fire it.
MODE_CHANGE fires separately when a mode value is first received or changes.
Both events see the complete applied update; indication events precede mode
events when both occur. Callbacks run synchronously on the receiving caller's
thread. Frames are not thread safe: serialize access externally and do not
mix indication reception and control transmission on the same object.

## Protocol usage

Register separate indication and control frames with AddDataFrame, using the
same slave ID and false/true direction flag as the slave library. IDs must be
1-255; each frame must contain 1-224 ordinary bytes (offsets 00-DF). A frame
cannot be shared across registrations. An indication frame is required for
every slave; BuildControl additionally requires its control frame.

The following builders return encoded wire bytes, including escaping, CRC
where required, and the F6 terminator:

- BuildPoll(id, message): secure poll by default; pass false as the third
  argument for a non-secure poll.
- BuildRecall(id, message): request all indications.
- BuildControl(id, message, all_data_flag, include_mode, mode): send changed
  control bytes by default. Set all_data_flag to send the entire control frame.
  Set include_mode to append E0 and the caller's explicit mode value. A mode-only
  control is allowed; without mode or changed controls, the builder returns false.
- BuildAcknowledgeSlave(id, message): generate a standalone secure FA request.
  Usually this is unnecessary because valid F2 responses generate FA automatically.

Each successful builder records one outstanding request for that slave. Another
builder for the same slave returns false until the exchange completes; builder
failures clear the output message. Retain the generated bytes if you need to
inspect them. Builders do not send anything. Do not manually acknowledge a
control frame while the protocol owns its outstanding control request.

Pass received bytes to ProcessMessages. It replaces that vector with any FA
replies that the caller must transmit. It preserves partial messages between
calls, handles several complete messages in one call, and validates F2 CRCs
after unescaping. F1 contains no CRC. Valid F2 payloads update the indication
frame and its callbacks, then generate FA. Further F2 responses are acknowledged
until F1 completes the exchange. Repeated indications are acknowledged even
when their values have not changed. A valid F1 or F2 in response to a control
acknowledges only the prepared control snapshot, preserving newer local writes.

GetResponses returns the successfully parsed responses from the latest
ProcessMessages call. Each exposes slave_id, ACK_MASTER or INDICATION_DATA,
control_acknowledged, and transaction_complete. An F2 leaves the exchange open
while waiting for the response to its generated FA; F1 completes it.

```cpp
LSY::ProtocolGenisysMaster protocol;
auto indications = std::make_shared<LSY::MasterDataFrame>("Indications", 32);
auto controls = std::make_shared<LSY::MasterDataFrame>("Controls", 32);
protocol.AddDataFrame(3, false, indications);
protocol.AddDataFrame(3, true, controls);
std::vector<uint8_t> request;
protocol.BuildRecall(3, request);
// Transmit request externally; supply received response bytes below.
std::vector<uint8_t> response{
    0xF2, 0x03, 0x00, 0x16, 0x01, 0x3E, 0x02, 0x08,
    0x03, 0x50, 0xE0, 0x01, 0x25, 0x9B, 0xF6};
bool valid = protocol.ProcessMessages(response);
// response now holds FA 03 02 A1 F6 to transmit; indications are updated.
```

Malformed messages, invalid CRCs/pairs, unconfigured slave IDs, unsupported
response types, and responses without an outstanding request are rejected.
They do not update frames, fire callbacks, or acknowledge controls. Partial
input alone is not an error. False from ProcessMessages reports at least one
rejection; valid messages in the same call still produce results and replies,
which the caller must inspect even when false is returned. The parser bounds
an incomplete message to 1024 wire bytes and recovers at a subsequent raw F1/F2
header after damaged input.

Use one protocol instance per ordered input stream and serialize access.
Independent peers must not interleave partial messages into the same instance;
the optional IP argument is currently unused and does not authenticate or route
responses. Genisys has no transaction sequence number, so the caller must route
responses from the expected endpoint and avoid overlapping exchanges. The protocol
class itself has no timers; the network layer manages deadlines and retries for both transports. CancelRequest
can explicitly abandon a request without acknowledging pending control data;
it cannot establish whether a device applied a control. The caller supplies mode values; control check-back/execute, common control,
and date/time messages remain unsupported. Startup control-database restoration
is not implemented.

## UDP transport

Create a NetworkGenisysMaster with Config.local_ip_ and Config.receive_port_.
Attach a protocol with AddProtocol, register each indication frame with the
protocol, and map each slave ID to an Endpoint with AddSlave. Several IDs can
share an identical endpoint; one ID can have only one endpoint per transport.
For duplicate IDs on different devices, use separate transport/protocol instances
with separate local receive ports. The protocol must not be shared by active
transports or independently processed while its transport is running.

```cpp
LSY::NetworkGenisysMaster::Config config;
config.receive_port_ = 7170;
LSY::NetworkGenisysMaster network(config);
network.AddProtocol(protocol);
network.AddSlave(3, {"127.0.0.1", 7169});
// Register slave 4 frames separately before mapping it to the same endpoint:
// network.AddSlave(4, {"127.0.0.1", 7169});
if (network.Start())
{
    network.RequestRecall(3);
    // Repeatedly call network.Process() from the application's loop.
    // GetResponses() returns all accepted responses from the latest call.
    network.Process();
    network.Shutdown();
}
```

Start binds a single exclusive IPv4 UDP socket. The same socket sends requests,
receives responses, and sends FA acknowledgements. A receive port of zero selects
an ephemeral port exposed by GetLocalPort. Endpoints require numeric IPv4
addresses and nonzero ports. Configuration cannot change while the socket is
started. All mapped IDs need registered indication frames before Start.
RequestRecall, RequestPoll, and RequestControl send a manually requested exchange;
the scheduler also issues recalls and polls when enabled. A successful request
means sent, not completed.

Process is nonblocking, draining at most 64 datagrams per call. Every response
ID must match its configured source IP and port before the datagram is passed
to the protocol. UDP frames must be complete within their datagram: incomplete
packets are discarded rather than joined with later packets or other devices.
Multiple complete frames in one datagram are supported. Generated acknowledgements
are sent to each corresponding slave's configured endpoint. GetResponses is
cleared on each Process call and includes accepted responses across the batch.
False means a socket error, rejected datagram, protocol error, or send failure;
other valid responses in that call can still have been processed. Inspect its
results even when false is returned. Callbacks execute on the Process caller.

Shutdown closes the socket and cancels outstanding requests for mapped IDs
without acknowledging control frames. It is idempotent and also runs in the
destructor. It clears scheduling state, counters and communication status. Starting
again begins with UNKNOWN status and an initial recall. Serialize access; callbacks
must not reenter processing, shut down the transport, or mutate the protocol/frame
registrations. ProcessDatagram is the protocol's self-contained UDP parsing entry
point; ProcessMessages remains available for ordered stream checks.

## Serial transport

The protocol and MasterDataFrame APIs and implementations are unchanged. Select
serial in NetworkGenisysMaster::Config, attach the same protocol/frames, register
IDs without IP endpoints, and use the same Start, Process, RequestRecall,
RequestPoll, RequestControl, status, transaction and indication callback APIs.
UDP remains the default for existing callers.

```cpp
LSY::NetworkGenisysMaster::Config config;
config.connection_type_ = LSY::NetworkGenisysMaster::Config::CONNECTIONTYPE::SERIAL;
config.com_port_ = "COM10"; // Also accepts the Windows device form "\\\\.\\COM10".
config.baud_rate_ = 9600;
config.parity_ = 'N';
config.stop_bits_ = 1;
config.response_timeout_ms_ = 1000;
config.write_timeout_ms_ = 2000;
LSY::NetworkGenisysMaster network(config);
network.AddProtocol(protocol); // Protocol already has indication/control frames.
network.AddSlave(3);
network.AddSlave(4); // Another address on the same serial wire.
if (network.Start()) {
    network.RequestRecall(3);
    // Call network.Process() regularly, just as for UDP.
}
```

The two-argument AddSlave(id, endpoint) also works in serial mode; the endpoint
is ignored. The ID-only overload is serial-only. GetLocalPort returns zero for
serial. Local IP and receive-port settings are unused. Configuration/registration
cannot change while the port is open. A failed Start leaves the port closed and
can be retried. Rebuild static-library consumers after updating the library.

Serial defaults are COM1, 9600 baud, 8 data bits, no parity, one stop bit, and a
1000 ms write timeout. Parity accepts N/E/O, case-insensitively; stop bits accept
1 or 2. Eight data bits are required to preserve Genisys headers and escaping.
The port is opened exclusively, including COM10 and higher. Hardware/software
flow control is disabled. dtr_enable_ and rts_enable_ optionally assert fixed
line levels (both default false); they do not implement flow control or RS-485
transmit-direction switching. Use an adapter with automatic direction control
for RS-485. Set the line parameters to match the device.

Only one complete exchange can own a serial bus at a time, including F2/FA/F1
and retries. Automatic requests use round-robin order among due IDs. An offline
ID consumes its configured timeout/retry budget, then gives other IDs a turn.
Manual requests return false if *any* ID is busy; they are not silently queued.
UDP retains its independent per-ID concurrency. A true serial request result
means its asynchronous write was accepted, not that it completed or was applied.

Serial reads can contain fragments or multiple frames. The network layer buffers
up to 1024 bytes per frame and passes complete frames to the existing parser.
It rejects noise, bad CRCs and replies for an ID that does not own the exchange.
Partial input is discarded when abandoning or starting an exchange. No parser or
frame changes are needed, and UDP datagrams retain their original isolation.

Writes use overlapped I/O and retain their buffers until completion; partial
writes continue the same frame. Process polls I/O without waiting. Response
phase deadlines start after the write completes and the driver's output queue
is drained. write_timeout_ms_ bounds a write plus driver drain; choose it for the
largest frame at your baud rate. response_timeout_ms_ must allow the slave's
turnaround and complete reply transmission. Controls are still sent once;
only poll/recall requests and FA acknowledgements may be retried.

A port/line/write failure closes the port, ends an active transaction with
SEND_FAILED, and marks every registered ID offline with stale cached data. It
does not acknowledge an unanswered control. A response timeout instead releases
the bus and allows the usual recovery recalls. Reconnect a failed port explicitly
with Shutdown/Start. Shutdown cancels pending I/O and waits for Windows to retire
it before releasing buffers/handles; processing and callbacks stay on the caller's
thread. No transport thread or external serial library is required.

Genisys has no transaction sequence number, so a delayed response for a new
exchange with the same ID remains indistinguishable at the protocol level.
Serial byte-stream checks run without hardware; native driver/wiring validation
requires two connected COM ports (see below). The console supports both transports through CLI/XML; the Qt GUI also supports serial selection and line settings.

Win32 references: [serial resource handles](https://learn.microsoft.com/en-us/windows/win32/devio/communications-resource-handles),
[communication timeouts](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-commtimeouts),
and [overlapped cancellation/lifetime](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelioex).
## Polling, timeouts and status

Config defaults (all intervals are milliseconds):

| Setting | Default | Meaning |
|---|---|---|
| automatic_polling_ | true | Recall initially, then poll each configured ID |
| secure_poll_ | true | Use CRC-bearing automatic polls |
| poll_interval_ms_ | 500 | Delay after completion before the next poll |
| response_timeout_ms_ | 500 | Wait for each response phase |
| retry_count_ | 2 | Additional transmissions after the initial attempt |
| recovery_interval_ms_ | 1000 | Delay after failure before a recovery recall |

Timing intervals must be nonzero. Call Process regularly (for example every
10 ms); it owns no thread and performs no blocking waits. Each slave has its own
request deadline and scheduling state, including IDs sharing one remote endpoint.
Responses are drained before timers run. Timers use a monotonic clock; only one
retry or new automatic request per slave is issued in a Process call.

At startup, the first Process issues a full recall for each idle ID. After a recall
returns indications and its FA/F1 sequence completes, indications_current becomes
true and subsequent automatic requests are polls. Any timed-out or failed request
releases that ID, marks its cached data stale and its communication OFFLINE, then
schedules a full recall after the recovery interval. Recovery never automatically
resends application controls. A failed ID does not block other IDs.

Poll/recall requests and FA acknowledgements are retransmitted byte-for-byte on
response timeout, up to retry_count_. Receipt of a valid F2 moves the transaction
to the FA response phase with a fresh retry budget and deadline. Duplicate F2s
are acknowledged without duplicate indication callbacks. Invalid packets do not
satisfy the deadline. The exchange completes at F1; receipt of F2 alone does not
complete it. A send failure for an initial request or timed retry ends the request
immediately; a failed initial FA send remains eligible for timed FA retries.

An FC control command is sent once and is never automatically retransmitted.
If it receives no valid response before its deadline, TIMEOUT is reported and its
control snapshot remains unacknowledged. If a valid F2 already acknowledged that
control, only its generated FA is eligible for retries. A later timeout refers to
the unfinished indication/FA exchange, not proof that the control was unexecuted.
The application must decide what to do with an uncertain control outcome.

GetSlaveStatus(id, status) exposes UNKNOWN/ONLINE/OFFLINE, busy,
indications_current, retries for the current response phase, completed_count,
and timeout_count. It returns false for unknown IDs or a stopped transport.
ONLINE means a valid response was received; indications_current additionally
requires a completed full recall since startup or the most recent failure.
Cached indication values are retained on failure so they remain inspectable,
but must be treated as stale while indications_current is false.

AddStatusCallback uses a function of (uint8_t, COMMUNICATIONSTATUS). It fires
synchronously when communication changes to ONLINE or OFFLINE, not on every poll.
UNKNOWN is the initial state exposed by GetSlaveStatus. Indication/mode callbacks
remain independent. GetTransactionEvents exposes RETRY, COMPLETE, TIMEOUT and
SEND_FAILED, the slave ID, whether the original request was a control, and the
retry count. Responses and transaction events are cleared at the start of each
Process call; request send failures outside Process append an immediately
inspectable SEND_FAILED event and also return false. Ordinary response timeouts
are status/events, not a false return from Process. Socket/protocol/send errors
still return false and do not prevent later timer processing.

Set automatic_polling_ to false for manual operation: explicit requests still
have response deadlines and retry limits, but no new polls or recovery recalls
are scheduled. Once a request times out, a new manual request can be made.
Do not directly cancel or build protocol requests while its transport is running;
the transport owns the matching timer state.

## Run the data-frame, protocol, UDP, polling and serial checks

Open an **x64 Native Tools Command Prompt for Visual Studio**, then run from
the solution directory:

```bat
GenisysMaster-Lib\tests\RunChecks.cmd
```

This builds the Debug x64 master library, compiles eight standalone console
executables, and runs all eight. The UDP checks also build GenisysSlave-Lib. A failure returns a nonzero exit code. Build products
stay under builds/x64/Debug/checks. MasterDataFrameChecks covers bit/byte access,
control snapshots, callbacks, modes, and payload validation.
ProtocolGenisysMasterChecks compares requests and replies against the GE
Appendix A examples (recall, acknowledgement, full indication) and the ELIXS
poll/control examples in the research notes and Capture2. It also exercises
fragmentation at every split of an escaped response, combined responses,
all byte values, invalid input, bounded buffering, and control acknowledgement.
The first two checks do not use networking. NetworkGenisysMasterChecks uses
real local UDP sockets with the existing slave protocol implementation. It
checks three IDs across shared/separate endpoints, source checks, UDP fragment
rejection, acknowledgement routing, port conflicts, shutdown and restart.
PollingGenisysMasterChecks adds lost requests/replies, bad CRC recovery,
acknowledgement retry exhaustion, zero/two-retry limits, automatic updates,
independent shared-endpoint IDs, stale-data/offline transitions, full-recall
recovery and control timeouts without retransmission.
ConsoleControlsChecks verifies console draft-to-wire behaviour against slave
control frames/callbacks, mode/full sends, acknowledgement of in-flight snapshots,
and explicit sends after a timeout using the newest edits.
ConsoleConfigurationChecks covers the console XML loader, defaults, example,
and validation. pugixml is compiled into that check and the console only; the
master library does not depend on it.
SerialGenisysMasterChecks uses a private injected byte-stream backend with the
existing slave protocol to exercise fragmentation, escaped IDs, serialized/fair
polling, delayed writes, noise/CRC recovery, stale data, control retry rules,
port failures and restart. It also checks native COM setting validation and
unavailable-port cleanup. The private test seam is not an application transport API.

The default run needs no external devices or running simulator. To additionally
exercise real overlapped Windows COM I/O, connect two ports with a null-modem cable
or configure a virtual null-modem pair, close other applications using them, then
run from the solution directory (example port names):

```bat
GenisysMaster-Lib\builds\x64\Debug\checks\SerialGenisysMasterChecks.exe COM10 COM11
```

This explicitly opens the supplied ports at 9600 8N1, verifies exclusive access,
byte-fragmented replies, escaped IDs/data, controls and pending-I/O shutdown/reopen.
No driver is installed or port guessed. It returns nonzero if pairing/I/O fails.
Without port arguments this native loopback test is reported as SKIP, not PASS.

The Process(bool allow_automatic_requests) overload can defer new automatic
polls/recalls for a call while still progressing active I/O, deadlines and retries.
Process() retains its original behavior. The console uses this to prioritize its
explicit recall queue without racing automatic serial polling. IsRunning reports
whether the socket/COM backend remains open, including after a fatal serial error.

ConsoleAppChecks builds and launches the console to verify serial diagnostics,
CLI/XML transport precedence, multi-ID UDP recalls and bounded polling. See the
console README for serial CLI flags and the commented serial XML example.

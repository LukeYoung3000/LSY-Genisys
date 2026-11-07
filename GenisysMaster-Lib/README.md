# GenisysMaster-Lib

Master UDP library with polling, retries, communication status, recovery, and explicit controls.

The master supports UDP using a caller-driven, nonblocking processing loop.
GenisysMaster-Console is included in the solution for polling and manual recalls.
TCP and serial are not implemented by the master transport. The console includes a control editor and explicit send commands; see its README
for usage.

The static library uses the existing LSY namespace and include/src layout:

- MasterDataFrame: bit/byte API, incoming indication callbacks, and outgoing
  control snapshots awaiting explicit acknowledgement.
- ProtocolGenisysMaster: master request builders and buffered slave-response parser.
- NetworkGenisysMaster: one bound UDP socket, a protocol object, and slave-ID endpoint mappings.
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
class itself has no timers; the UDP transport manages deadlines and retries. CancelRequest
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

## Run the data-frame, protocol, UDP and polling checks

Open an **x64 Native Tools Command Prompt for Visual Studio**, then run from
the solution directory:

```bat
GenisysMaster-Lib\tests\RunChecks.cmd
```

This builds the Debug x64 master library, compiles six standalone console
executables, and runs all six. The UDP checks also build GenisysSlave-Lib. A failure returns a nonzero exit code. Build products
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
No external devices or running simulator are needed.

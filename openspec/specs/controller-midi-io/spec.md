# Controller MIDI I/O Specification

## Purpose
The controller MIDI I/O layer (`JUCE/SmartGridOne/Source/MidiHandlers.hpp`, `MidiSender.hpp`, `private/src/WrldBLDRMidi.hpp`, `MessageInBus.hpp`, `TheNonagonSquiggleBoyWrldBldr.hpp`) connects physical MIDI grid controllers to the sequencing/audio core. Inbound MIDI bytes are timestamped, tagged with a route, converted to `MessageIn` events, and delivered through a timestamp-gated `MessageInBus`; outbound LED and indicator state is read from the UIState layer (SmartBusColor, EncoderBankUIState) and emitted by a dedicated realtime scheduler thread, keeping OS MIDI I/O off the audio and control threads. Grid color publication itself is owned by the grid layer (see smart-grid-runtime); parameter semantics of encoder turns belong to encoder-parameter-system.
## Requirements
### Requirement: MIDI Input Timestamping and Route Tagging
The system SHALL convert each incoming MIDI message that is relevant to Smart Grid One into a `BasicMidi` carrying a microsecond timestamp derived from the JUCE message timestamp (seconds × 1,000,000) and the input handler's route ID, then forward it to the owning integration's `MessageInBus`.
Three-byte channel/controller messages SHALL continue to be forwarded with their raw status and data bytes. One-byte MIDI realtime clock (`0xF8`), start (`0xFA`), continue (`0xFB`), and stop (`0xFC`) messages SHALL also be forwarded, preserving the realtime status byte and timestamp. Unsupported message sizes or realtime statuses outside this set SHALL be ignored unless another capability defines them.
`MidiInputHandler` persists the device name in JSON and reconnects by name via `AttemptConnect()`.

#### Scenario: Note-on arrives on an open input
- **WHEN** a 3-byte MIDI message with JUCE timestamp 1.5 seconds arrives on an input handler with route ID 6
- **THEN** a `BasicMidi` with `m_timestamp` = 1,500,000 microseconds and `m_routeId` = 6 is pushed into the integration's MessageInBus

#### Scenario: Clock realtime byte arrives on an open input
- **WHEN** a one-byte MIDI clock message with JUCE timestamp 2.0 seconds arrives on an input handler
- **THEN** a `BasicMidi` with `m_timestamp` = 2,000,000 microseconds and status `0xF8` is pushed into the integration's MessageInBus

#### Scenario: Continue realtime byte arrives on an open input
- **WHEN** a one-byte MIDI continue message with JUCE timestamp 2.5 seconds arrives on an input handler
- **THEN** a `BasicMidi` with `m_timestamp` = 2,500,000 microseconds and status `0xFB` is pushed into the integration's MessageInBus

#### Scenario: SysEx input is dropped
- **WHEN** an incoming MIDI message is a SysEx message
- **THEN** no `BasicMidi` is produced and nothing is forwarded

### Requirement: Wrld.Bldr Channel-to-Route Mapping
The system SHALL map Wrld.Bldr MIDI channels to routed `MessageIn` events in `WrldBLDRMidi::FromMidi`: channels 0 and 1 to the Encoder route (route ID 4), channel 2 to the Analog route (route ID 5) as `ParamSet7` keyed by CC number, channel 3 to the LeftGrid route (6) and channel 4 to the RightGrid route (7) as pad events decoded from note number (x = note % 8, y = note / 8), channel 5 to the AuxGrid route (8) as pad events decoded from CC number, and channel 14 to the Analog route as `ParamSet7` with the CC number offset by 2; any other channel yields `NoMessage`.

#### Scenario: Encoder turn on channel 0
- **WHEN** a CC message arrives on channel 0 with CC number 5 and value 66
- **THEN** `FromMidi` returns an `EncoderIncDec` message on route 4 at position (x = 1, y = 1) with amount 66 − 64 = +2

#### Scenario: Encoder push and release on channel 1
- **WHEN** a CC message arrives on channel 1 with value 127
- **THEN** the result is an `EncoderPush` message on route 4
- **AND** a subsequent value of 0 at the same CC yields `EncoderRelease`

#### Scenario: Left grid pad press on channel 3
- **WHEN** a note message arrives on channel 3 with note number 19 and velocity 100
- **THEN** `FromMidi` returns a `PadPress` on route 6 at (x = 3, y = 2)
- **AND** velocity 0 at the same note yields `PadRelease`

#### Scenario: Analog offset on channel 14
- **WHEN** a CC message arrives on channel 14 with CC number 0 and value 64
- **THEN** the result is a `ParamSet7` on route 5 with x = 2 (CC + 2) and amount 64

#### Scenario: Unmapped channel
- **WHEN** a message arrives on channel 9
- **THEN** `FromMidi` returns a default `MessageIn` whose mode is `NoMessage`

### Requirement: Per-Route Input Interpretation
The system SHALL configure each MessageInBus route with a `MidiToMessageIn::RouteType` (LaunchPad, Encoder, Param14, or Param7) via `SetRouteType`, controlling how raw `BasicMidi` pushed for that route is converted: LaunchPad routes produce pad press/release events, Encoder routes produce encoder increment/push/release events, and Param7 routes produce `ParamSet7` events keyed by note number; out-of-range route IDs produce `NoMessage`.
The Wrld.Bldr integration configures route 4 as Encoder, route 5 as Param14, and routes 6–8 as LaunchPad at construction.

#### Scenario: Raw MIDI on a LaunchPad-typed route
- **WHEN** a `BasicMidi` with route ID 6 is pushed into a bus whose route 6 is typed LaunchPad
- **THEN** the message is converted through `LPMidi::FromMidi` into a pad press or release event

#### Scenario: Invalid route ID
- **WHEN** a `BasicMidi` carries route ID 16 or −1
- **THEN** conversion yields a `NoMessage` event

### Requirement: Timestamp-Gated Message Delivery
The system SHALL deliver queued `MessageIn` events only when they are visible at the consumer's timestamp: `MessageInBus::Pop(msg, timestamp)` peeks the queue head and pops it only if `msg.m_timestamp <= timestamp`, otherwise it returns false and leaves the message queued.
`ProcessMessages` drains all currently-visible messages into the integration's `Apply(...)` handler; the Wrld.Bldr integration calls this once per audio sample with the current absolute time in microseconds.

#### Scenario: Future-stamped message is held back
- **WHEN** the queue head carries timestamp 2,000,000 µs and `Pop` is called with timestamp 1,999,999 µs
- **THEN** `Pop` returns false and the message remains at the head of the queue
- **AND** a later `Pop` with timestamp 2,000,000 µs returns the message

#### Scenario: Message dispatch by route
- **WHEN** a visible message with route ID 7 is popped during `ProcessMessages`
- **THEN** the Wrld.Bldr integration's `Apply` forwards it to the right grid, while route IDs 4 and 5 are forwarded to the internal encoder/analog handler

### Requirement: Realtime Output Scheduling with Latency Buffer
The system SHALL send outbound MIDI from a dedicated realtime thread (`MidiSender`, JUCE realtime priority, 100 µs poll loop) fed by a `CircularQueue<BasicMidi, 16384>`; the thread peeks the queue head and sends it only when the wall clock reaches the message timestamp (converted from microseconds to milliseconds) plus a fixed 10 ms latency buffer (`x_latencyMs`).
Messages with timestamp 0 are sent immediately. After `Shutdown()` is called, queued messages are popped and discarded instead of sent.

#### Scenario: Timestamped message waits for its send time
- **WHEN** the queue head carries timestamp 5,000,000 µs and the wall clock reads 5,005 ms
- **THEN** the message stays queued because the target time is 5,000 + 10 = 5,010 ms
- **AND** it is popped and sent to its route's output handler once the wall clock reaches 5,010 ms

#### Scenario: Immediate send for zero timestamp
- **WHEN** a `BasicMidi` with timestamp 0 reaches the queue head
- **THEN** it is popped and sent without waiting

#### Scenario: Shutdown drains without sending
- **WHEN** `Shutdown()` has been called and a message is at the queue head
- **THEN** the message is popped and discarded, not delivered to any output handler

### Requirement: Output Route Allocation
The system SHALL bind up to 16 `MidiOutputHandler` instances to MidiSender routes via `AllocateRoute(handler)`, which assigns the first free slot index as the handler's `m_routeId` and returns 16 when no slot is free.
Each output handler stores a controller shape (`m_shape`, e.g. LaunchPadX or Twister), persists its device name and shape in JSON, reconnects by name, and serializes device access with a spin lock around immediate (`sendMessageNow`) and block (`sendBlockOfMessages`) sends.

#### Scenario: Route slots fill in order
- **WHEN** two handlers are allocated against an empty MidiSender
- **THEN** the first receives route ID 0 and the second route ID 1

#### Scenario: Allocation table full
- **WHEN** all 16 route slots are occupied and another handler is allocated
- **THEN** `AllocateRoute` returns 16 and the handler's route is not bound

### Requirement: Yaeltex SysEx LED Color Encoding
The system SHALL emit Wrld.Bldr LED updates as batched Yaeltex SysEx packets beginning with header bytes F0 79 74 78 00 01 00 20, followed by one channel byte, then one 4-byte group per changed cell — [cc, red/2, green/2, blue/2] — and a terminating F7 byte.
Colors are read from `SmartBusColor` (grid writers, channels 3/4/5 for left/right/aux) or `EncoderBankUIState` (encoder button and indicator writers, channels 1 and 0). A cell is written only when its color differs from the last value sent or has never been sent, subject to a per-packet budget and a one-frame per-cell cooldown; with probability 0.001 per frame all cells are invalidated to force a full refresh.

#### Scenario: Single pad color change
- **WHEN** the left grid's SmartBusColor cell (2, 1) changes to RGB (200, 100, 50) and the grid writer runs with budget available
- **THEN** the emitted SysEx contains the 8-byte header, channel byte 3, the group [10, 100, 50, 25], and terminator F7

#### Scenario: Unchanged cells are skipped
- **WHEN** a writer runs and a cell's color equals the last value sent for that cell
- **THEN** no 4-byte group is written for that cell

#### Scenario: Budget exhaustion defers updates
- **WHEN** more cells changed than the remaining packet budget allows
- **THEN** writing stops when the budget reaches zero and remaining cells are sent on later frames after their cooldown expires

### Requirement: Three Concurrent Grid Routes with Mode Switching
The system SHALL operate three concurrent Wrld.Bldr grid routes — LeftGrid (6), RightGrid (7), and AuxGrid (8) — where `SetGridsMode(mode)` swaps the left and right grid pointers among the `GridsMode` pages (ComuteAndTheory, Matrix, Intervals, SubSequencer, Config) while the aux grid remains fixed; `ProcessFrame()` publishes each grid's colors into the integration's `UIState::m_colorBus[3]`.
Grid cell semantics and color computation belong to the grid layer (see smart-grid-runtime); this capability covers only routing and publication. Selecting a grids-mode cell also stores Controller display mode in the UI state.

#### Scenario: Grids mode swap retargets pad routes
- **WHEN** `SetGridsMode(GridsMode::Matrix)` is called
- **THEN** subsequent pad presses on routes 6 and 7 are applied to the Matrix-mode left and right grids
- **AND** the stored grids mode in the UI state reads Matrix

#### Scenario: Per-frame color publication
- **WHEN** `ProcessFrame()` runs
- **THEN** the left, right, and aux grids each write their current colors into `m_colorBus[0]`, `m_colorBus[1]`, and `m_colorBus[2]` respectively

### Requirement: Transport Clock Forwarding
The system SHALL continue to convert sequencer transport events into MIDI real-time messages on the MidiSender's dedicated clock route: `ProcessMessagesOut` maps `MessageOut::Mode::Clock`, `Start`, and `Stop` to MIDI clock, transport-start, and transport-stop messages stamped with the supplied sample-derived timestamp, then clears the buffer.

#### Scenario: Clock tick forwarding
- **WHEN** the message-out buffer contains a Clock event and `ProcessMessagesOut` runs with timestamp T
- **THEN** a MIDI clock `BasicMidi` with timestamp T is enqueued on the clock route
- **AND** the buffer is cleared afterwards

#### Scenario: Transport start forwarding
- **WHEN** the message-out buffer contains a Start event and `ProcessMessagesOut` runs with timestamp T
- **THEN** a MIDI start `BasicMidi` with timestamp T is enqueued on the clock route

### Requirement: MessageIn Bus Scheduling with Latency Buffer
The system SHALL apply a fixed future latency of about 20 ms exactly once when messages enter the `MessageInBus`. `MessageInBus::Push(BasicMidi)` SHALL preserve the original MIDI status/route semantics while converting the MIDI input to `MessageIn`, and `MessageInBus::Push(MessageIn)` SHALL set the queued `MessageIn::m_timestamp` to the input message timestamp plus the fixed latency. Audio-thread processing SHALL see both converted MIDI input and direct `MessageIn` input only when processing reaches the adjusted visible timestamp.

#### Scenario: Realtime input waits for its visible time
- **WHEN** a `BasicMidi` clock with timestamp 2,000 µs is pushed into `MessageInBus`
- **THEN** no message is popped at timestamp 21,999 µs
- **AND** a MIDI clock `MessageIn` is popped at timestamp 22,000 µs

#### Scenario: Zero timestamp realtime input is still delayed
- **WHEN** a `BasicMidi` clock with timestamp 0 is pushed into `MessageInBus`
- **THEN** the resulting `MessageIn` has timestamp 20,000 µs

#### Scenario: Direct MessageIn input is delayed once
- **WHEN** a direct `MessageIn` pad press with timestamp 6,000 µs is pushed into `MessageInBus`
- **THEN** no message is popped at timestamp 25,999 µs
- **AND** the pad press is popped at timestamp 26,000 µs

### Requirement: Realtime MIDI Message Bus Routing
The system SHALL convert routed MIDI realtime clock/start/continue/stop bytes into timestamped `MessageIn` events on a fixed realtime route in the same `MessageInBus` used by controller input. The realtime route SHALL be independent of Launchpad, encoder, analog, and Wrld.Bldr grid routes, and converted `MessageIn` timestamps SHALL include the fixed input latency.

#### Scenario: Realtime route produces clock event
- **WHEN** a `BasicMidi` with status `0xF8` is pushed into a bus configured with the fixed realtime route
- **THEN** the resulting `MessageIn` represents a MIDI clock event
- **AND** it uses the original timestamp plus the fixed input latency for audio-thread visibility gating

#### Scenario: Realtime route produces start event
- **WHEN** a `BasicMidi` with status `0xFA` is pushed into the fixed realtime route
- **THEN** the resulting `MessageIn` represents a MIDI start event

#### Scenario: Realtime route produces continue event
- **WHEN** a `BasicMidi` with status `0xFB` is pushed into the fixed realtime route
- **THEN** the resulting `MessageIn` represents a MIDI continue event

#### Scenario: Realtime route produces stop event
- **WHEN** a `BasicMidi` with status `0xFC` is pushed into the fixed realtime route
- **THEN** the resulting `MessageIn` represents a MIDI stop event

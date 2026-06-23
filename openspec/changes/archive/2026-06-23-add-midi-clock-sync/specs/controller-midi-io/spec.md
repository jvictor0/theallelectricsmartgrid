## MODIFIED Requirements

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

### Requirement: Transport Clock Forwarding
The system SHALL continue to convert sequencer transport events into MIDI real-time messages on the MidiSender's dedicated clock route: `ProcessMessagesOut` maps `MessageOut::Mode::Clock`, `Start`, and `Stop` to MIDI clock, transport-start, and transport-stop messages stamped with the supplied sample-derived timestamp, then clears the buffer.

#### Scenario: Clock tick forwarding
- **WHEN** the message-out buffer contains a Clock event and `ProcessMessagesOut` runs with timestamp T
- **THEN** a MIDI clock `BasicMidi` with timestamp T is enqueued on the clock route
- **AND** the buffer is cleared afterwards

#### Scenario: Transport start forwarding
- **WHEN** the message-out buffer contains a Start event and `ProcessMessagesOut` runs with timestamp T
- **THEN** a MIDI start `BasicMidi` with timestamp T is enqueued on the clock route

## ADDED Requirements

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

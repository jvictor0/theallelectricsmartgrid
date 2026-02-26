# MIDI I/O Scheduling

This page documents input/output MIDI transport and scheduling used by the UI/controller stack.

## Input handling (`MidiInputHandler`)

Defined in `JUCE/SmartGridOne/Source/MidiHandlers.hpp`.

- Opens JUCE input device by identifier.
- Converts incoming 3-byte MIDI messages into `SmartGrid::BasicMidi` with microsecond timestamps.
- Carries route ID (`m_routeId`) so downstream routing is explicit.
- Persists device name and supports reconnect (`AttemptConnect`) from saved config.

Integrations subclass `SendMessage(BasicMidi)` to forward into their `MessageInBus`.

## Output handling (`MidiOutputHandler`)

Also in `MidiHandlers.hpp`.

- Opens JUCE output device with thread-safe `SpinLock`.
- Stores controller shape (`m_shape`) and route ID.
- Requires concrete `Reset()` and `Process()` implementations.
- Supports:
  - immediate message send (`SendMessageNow`),
  - block send (`sendBlockOfMessages`) for buffered paths.
- Persists output endpoint + shape in JSON.

## `MidiSender` realtime dispatcher

Defined in `JUCE/SmartGridOne/Source/MidiSender.hpp`.

Key structure:

- JUCE realtime thread (`startRealtimeThread`) with tight scheduling params.
- queue: `CircularQueue<BasicMidi, 16384>`.
- route table: `MidiOutputHandler* m_outputHandlers[16]`.

Core behaviors:

- `AllocateRoute(handler)` assigns route IDs and binds handlers.
- `SendMessage(msg, routeId)` enqueues timestamped output.
- `HandleMessage()` peeks queue head and sends only when target wall-clock time is reached:
  - `targetTimeMs = msgTimestampMs + x_latencyMs`.
- `Shutdown()` enables controlled draining behavior.

## Transport clock forwarding

`ProcessMessagesOut(MessageOutBuffer&, timestamp)` converts sequencer transport events to MIDI real-time messages on `m_clockRouteId`:

- clock
- start
- stop

This decouples transport timing from controller rendering traffic.

## Why this architecture

- Avoids blocking audio/control threads on OS MIDI I/O.
- Maintains explicit per-integration route separation.
- Enables predictable timing with a single centralized output scheduler.

## Related

- [Controller Integrations](ui-controller-integrations.md)
- [Message Routing and Smart Buses](ui-routing-buses.md)

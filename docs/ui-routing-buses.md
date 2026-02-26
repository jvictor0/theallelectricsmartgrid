# Message Routing and Smart Buses

This page covers input routing and LED/state bus transport used by UI/controller layers.

## `PadUI` and `PadUIGrid`

Defined in `private/src/PadUI.hpp`.

- `PadUI` binds:
  - logical coordinates `(m_x, m_y)`,
  - route ID (`m_routeId`),
  - `SmartGrid::SmartBusColor*` for readback color,
  - `SmartGrid::MessageInBus*` for interaction output.
- `OnPress/OnRelease` enqueue `MessageIn` events (`PadPress`, `PadRelease`) with timestamps.
- `GetColor()` reads current LED color from `SmartBusColor`.

`PadUIGrid` is a lightweight factory that generates `PadUI` handles for a route.

## `MessageInBus`

Defined in `private/src/MessageInBus.hpp`.

- Queue: `CircularQueue<MessageIn, 16384>`.
- Accepts either:
  - already-constructed `MessageIn`,
  - raw `BasicMidi` (converted through `MidiToMessageIn`).
- Route typing configured by `SetRouteType(route, routeType)`.
- Time gating:
  - `Pop(msg, timestamp)` only emits messages where `msg.Visible(timestamp)` is true.
- Dispatch:
  - `ProcessMessages(processor, timestamp)` calls `processor->Apply(msg)` in-order.

## `MidiToMessageIn` route typing

Route type controls how incoming MIDI packets are interpreted:

- Launchpad pad-grid style routes,
- encoder routes,
- parameter-set routes (`Param7`, `Param14`),
- and other controller-specific mappings.

Both Wrld.Bldr and QuadLaunchpadTwister configure these route types during construction.

## `SmartBusGeneric` and `SmartBusColor`

Defined in `private/src/SmartBus.hpp`.

- `SmartBusGeneric<T>` stores an atomic 2D grid of payloads.
- `Put(...)` detects cell changes and sets a `changed` flag.
- `m_epoch` increments when visible state changes.

`SmartBusColor` is `SmartBusGeneric<Color>`, used for controller LED output state.

## Epoch/diff iteration model

The bus iterator uses epoch comparison:

- if epoch unchanged, iteration returns immediately (no work),
- if changed, caller iterates through logical grid messages.

This keeps output processing efficient for mostly-static LED frames.

## Grid-to-bus publishing

`AbstractGrid::OutputToBus(SmartBusColor* bus)`:

- writes each logical cell color into the target bus,
- tracks whether anything changed,
- increments bus epoch only when needed.

This is the primary bridge from sequencer/controller state to UI/controller LED state.

## Related

- [UI Components and Layout](ui-components-layout.md)
- [Controller Integrations](ui-controller-integrations.md)
- [MIDI I/O Scheduling](ui-midi-scheduling.md)

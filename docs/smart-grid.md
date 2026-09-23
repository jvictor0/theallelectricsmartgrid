# Smart Grid Integration

The "Smart Grid" is the overarching framework for mapping the synthesizer's internal state to physical, grid-based MIDI controllers (like the Novation Launchpad or the Yaeltex Wrld.Bldr) and the on-screen UI.

The core definitions live in `private/src/SmartGrid.hpp`, with the specific integration for the Nonagon and SquiggleBoy defined in `private/src/TheNonagonSquiggleBoy.hpp`.

## Core Concepts

The Smart Grid framework abstracts away the specifics of any particular MIDI controller. It defines a generic grid of `Cell`s.

- **Cell**: The fundamental unit of interaction. A cell can respond to presses, releases, and continuous pressure (`m_pressureSensitive`). It also has a state that dictates its current `Color` (for LED feedback).
- **Grid**: A 2D array of Cells. Grids handle routing incoming MIDI messages (converted into generic `Message` or `MessageIn` events) to the appropriate `Cell`.
- **Colors**: The framework uses a centralized `Color` enum and `HSV` representation, allowing the software to specify semantic colors (e.g., "Red", "Dim White") that are later translated into hardware-specific MIDI velocity values by the device-specific driver.

## Saved controls and runtime cells

`StateCell<T>` and `CycleCell<T>` hold a cached `State*` from a state saver. They read through `Get<T>()` and write through `Set<T>()`, so edits update the live value and reach the parameter event logger. A `StateCell` supports toggle, momentary, and set-only modes; a `CycleCell` advances through its color scheme. Flash policies remain independent of the saved value and can read live DSP booleans.

`RuntimeStateCell` instead holds a `bool*` for temporary controls and indicators. It supports toggle, momentary, set-only, and show-only modes, without registration or state-change notifications. Running, shift, noise mode, auxiliary focus, and gate indicators use this path. Its constructor takes the on color before the off color; `StateCell` retains the off-color/on-color order.

Cells resolve saved handles during construction, before audio processing. Several cells can share one registered value, such as the parent-selector pads on the topology and co-mute pages. The Theory of Time topology page derives directly from `Grid`; the obsolete `CompositeGrid`, `GridSwitcher`, `GridJnct`, and `FaderBank` helpers have been removed.

## Nonagon Smart Grid

The `TheNonagonSmartGrid` struct wraps the core `TheNonagonInternal` sequencer logic in this Smart Grid framework. Its constructor takes the standalone-mode flag and shared `SmartGridOneContext`, and registers saved values before building the cells that use them.

- It exposes the sequencer's internal state (like time loops, index arp settings, and mutes) as interactive `Cell`s.
- This allows a user to press a pad on a MIDI controller to flip a gate, change a time loop's multiplier, or mute a voice, with the physical pad lighting up to reflect the current state.

## SquiggleBoy Config Grid (Voice + Sample Routing)

`SquiggleBoyConfigGrid` handles more than source/filter mode selection:

- source machine increment/decrement and filter machine increment/decrement for the active trio,
- source selection toggles when using `Thru`,
- per-voice sample directory selection for the `Sample` source machine,
- directory explorer navigation commands (`Up/Down/Left/Right/Yes/No`) routed through `IoTaskThread`.

The directory explorer state is mirrored into UI buffers (`DirectoryExplorer::UIState`) so `VoiceConfigComponent` can render live path/list updates while browsing.

## Related
- [The Nonagon (Sequencer)](nonagon.md)
- [Encoder System](encoder-system.md)
- [Scene Manager](scene-manager.md)
- [State Saver](state-saver.md)
- [UI Components and Layout](ui-components-layout.md)

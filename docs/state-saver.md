# State and State Saver

The state saver (`private/src/StateSaver.hpp`) registers and owns named `State` objects (`private/src/State.hpp`) for non-encoder control values. A `State` connects a live value to its saved scene copies, default value, and state-change hook. `StateSaver` handles registration, lookup, JSON persistence, and scene operations across those objects.

## Scope and separation from encoder state

`StateSaver` is a fixed-width per-scene registry for non-encoder state only. Encoder state never touches `StateSaver`: encoders serialize separately via `EncoderBankBank::ToJSON()` / `FromJSON()` (`private/src/EncoderBankBank.hpp`), which iterate the encoder array and write each named parameter.

In `TheNonagonSquiggleBoyInternal`, encoder and non-encoder persistence use separate patch JSON keys:

- encoder state is written under the `"squiggleBoy"` key (via `SquiggleBoyWithEncoderBank::ToJSON()`, which serializes the `EncoderBankBank`)
- the eight-scene Nonagon registry (`ScenedStateSaver`) is written under `"nonagon"`
- the single-scene global registry (`StateSaver`) is written under `"stateSaver"`

## Role

`StateSaver` bridges live in-memory non-encoder values and serialized patch/project state:

- serializes state to JSON (`ToJSON`)
- restores from JSON (`SetFromJSON`)
- supports per-scene stored values
- supports reset-to-default behavior across scenes

It is saved and loaded alongside Nonagon and SquiggleBoy state.

`SquiggleBoyConfigGrid` also persists sample-source directory choices. It writes a `sampleDirectoryRelative` array with one relative path per voice; on restore, each non-empty path is resolved under the configured sample root and loaded asynchronously through `IoTaskThread::PushLoadAudioBufferBankFromDirectory(...)`.

## Construction and ownership

`StateSaverTemp<NumScenes>` requires a `StateManager*` and a `SmartGrid::SceneManager*` in its constructor. The manager pointers are supplied before any values are registered. The owning `TheNonagonSquiggleBoyInternal` constructs both managers before its consumers:

- the Nonagon receives both managers and constructs its `ScenedStateSaver` before creating its grids;
- the global `StateSaver` receives the state manager and a null scene manager, so `Process()` does not switch its global values between scenes;
- the encoder system receives the same scene manager through its own constructor chain, while retaining separate encoder storage.

`Insert(name, &value)` returns a `State*`. Indexed overloads use the existing JSON names `name_i` and `name_i_j`. The saver owns the allocated `State` objects and deletes them on destruction; the live values and managers remain owned by their surrounding components. Cached handles remain stable as the registration vector grows or is shuffled, and must not outlive their saver or backing values.

`Get(name)` and its indexed overloads return the registered handle, or null when the name is missing. Register values and resolve handles during setup. Name lookup throws on a thread tagged `ThreadId::Audio`; cells cache their handles during construction and use them directly when processing input. Reusing a name for a different live pointer throws.

## Live edits and the state-change hook

`State::Get<T>()` reads the live value. `State::Set<T>(value)` writes it immediately, then calls `StateManager::RecordStateChange(this)`. Callers must use the registered value's type; the handle stores a byte width rather than a runtime type descriptor.

Grid controls registered with `StateSaver` use these handles, including state toggles, cycle cells, clock/reset selectors, rhythm controls, source/filter machine selectors, source routing configuration, scene selectors, and the active trio. Other booleans and read-only indicators use `RuntimeStateCell` or their specialized cells instead. Source-monitor booleans, for example, are saved separately by the config grid and do not use `StateSaver`.

`StateManager::RecordStateChange` is currently a no-op extension point. It does not queue changes, publish notifications, or provide undo. The low-level `State::SetFromJSON`, `LoadValFromScene`, `CopyToScene`, and `RevertToDefaultForScene` methods operate directly on saved bytes and live pointers without invoking that hook. Higher-level config-grid loading and reset paths also make explicit `State::Set` calls, which do reach the hook. Scene-change flags continue to come from `SceneManager`.

## Snapshot representation

Each `State` stores compact snapshots for one registered value:

- each registered value has a fixed byte width (`1/2/4/8`)
- its buffer has capacity for eight scenes (`State::x_maxScenes * 8` bytes), with one or eight scenes used according to the owning saver
- its default copy is captured from the live value at registration
- current scene value is copied back to live pointers on load/switch

Serialization retains the existing JSON keys and byte-array format: each value emits `NumScenes * sizeof(value)` integer entries. The larger fixed buffer does not add serialized scenes to the single-scene global registry.

## Scene-aware persistence

`ScenedStateSaver` works with scene logic so scene-specific values are retained:

- each scene keeps its own stored bytes for registered fields
- save operations capture current scene state before emit
- load operations restore scene buffers and then write active scene values back to runtime variables

`Finalize()` assigns staggered blend boundaries and copies the initial live values into the other seven scenes. The global single-scene saver does not need this scene setup.

## Related

- [Scene Manager](scene-manager.md)
- [Smart Grid Integration](smart-grid.md)

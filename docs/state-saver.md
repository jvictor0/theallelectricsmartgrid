# State Saver

The state saver (`private/src/StateSaver.hpp`) handles persistence of *non-encoder* runtime control state to/from JSON. It is not the general persistence layer for the whole synth.

## Scope and separation from encoder state

`StateSaver` is a fixed-width per-scene registry for non-encoder state only. Encoder state never touches `StateSaver`: encoders serialize separately via `EncoderBankBank::ToJSON()` / `FromJSON()` (`private/src/EncoderBankBank.hpp`), which iterate the encoder array and write each named parameter.

In `TheNonagonSquiggleBoyInternal`, the two persistence paths sit side by side in the patch JSON:

- encoder state is written under the `"squiggleBoy"` key (via `SquiggleBoy::ToJSON()`, which serializes the `EncoderBankBank`)
- `StateSaver` state is written under the separate `"stateSaver"` key

## Role

`StateSaver` bridges live in-memory non-encoder values and serialized patch/project state:

- serializes state to JSON (`ToJSON`)
- restores from JSON (`SetFromJSON`)
- supports per-scene stored values
- supports reset-to-default behavior across scenes

It is saved and loaded alongside Nonagon and SquiggleBoy state.

`SquiggleBoyConfigGrid` also persists sample-source directory choices. It writes a `sampleDirectoryRelative` array with one relative path per voice; on restore, each non-empty path is resolved under the configured sample root and loaded asynchronously through `IoTaskThread::PushLoadAudioBufferBankFromDirectory(...)`.

## Internal model

`StateSaverTemp<NumScenes>` stores compact per-scene snapshots for registered values:

- each registered value has a fixed byte width (`1/2/4/8`)
- scene copies are kept in an internal scene buffer
- current scene value is copied back to live pointers on load/switch

This design keeps serialization generic while preserving exact typed values.

## Scene-aware persistence

`StateSaver` works with scene logic so scene-specific values are retained:

- each scene keeps its own stored bytes for registered fields
- save operations capture current scene state before emit
- load operations restore scene buffers and then write active scene values back to runtime variables

## Related

- [Scene Manager](scene-manager.md)
- [Smart Grid Integration](smart-grid.md)

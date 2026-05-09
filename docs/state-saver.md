# State Saver

The state saver (`private/src/StateSaver.hpp`) handles persistence of runtime control state to/from JSON.

## Role

`StateSaver` bridges live in-memory values and serialized patch/project state:

- serializes state to JSON (`ToJSON`)
- restores from JSON (`SetFromJSON`)
- supports per-scene stored values
- supports reset-to-default behavior across scenes

In `TheNonagonSquiggleBoyInternal`, it is saved and loaded alongside Nonagon and SquiggleBoy state.

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

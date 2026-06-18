## Why

Patch load currently turns some valid saved encoder float values into zero after the arena JSON migration. The most visible failures are whole-number normalized values such as `1.0` defaults for `MasterVolume`, `LPCutoff`, and source low-pass parameters, which serialize as JSON text `1`, parse as integer nodes, and then read back through `RealValue()` as `0.0`.

This needs to be fixed now because patch persistence is a core performance workflow and a saved patch can reload quieter or tonally closed even though the patch file contains the intended numeric value.

## What Changes

- Preserve numeric semantic compatibility between JSON integer tokens and float consumers.
- Ensure encoder parameter load accepts whole-number JSON values when restoring normalized float state.
- Keep audio-thread save behavior allocation-free and arena-backed.
- Add regression coverage for default non-zero whole-number float params across a real patch save/load.
- Add focused JSON numeric coverage so future parser/serializer changes do not reintroduce the type mismatch.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `arena-json`: Numeric parse/serialize round-trips must remain compatible with callers that read persisted float state from whole-number JSON tokens.
- `encoder-parameter-system`: Named encoder JSON load must restore normalized float values whether the JSON token is encoded with a decimal point or as a whole-number token.
- `scene-state-management`: Patch save/load round-trips must preserve default non-zero whole-number encoder parameters, including master volume and source/filter low-pass values.

## Impact

- Affected code: `private/src/Json.hpp`, `private/src/Encoder.hpp`, and patch persistence tests under `private/test/`.
- No new dependencies.
- No public file format migration is required; the fix should remain compatible with existing patches that contain either `1`, `1.0`, or other numeric JSON spellings.
- The implementation must preserve the audio-thread allocation contract for save serialization.

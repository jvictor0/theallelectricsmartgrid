## Why

Patch saves currently omit the sixteen SquiggleBoy fader positions, so standalone users lose gesture weights, gain faders, and other analog positions after a reload. With a WRLD.BLRD controller attached, restoring saved faders would fight the physical fader positions, so load needs to be conditional while save remains unconditional.

## What Changes

- Save all `SquiggleBoy::Input::m_faders` values into patch JSON on every patch save.
- Add a `restoreFaders` argument to the higher-level patch `FromJSON` path so callers can choose whether saved fader values are applied.
- Set `restoreFaders` at the top-level load request from WRLD.BLRD attachment state: restore saved faders when no WRLD.BLRD controller is open, and leave live/controller faders unchanged when it is open.
- Set `restoreFaders` to false for reload-style patch functionality so reloading a patch never overwrites the current live fader/controller positions.
- Preserve backward compatibility: patch files without saved faders still load successfully and keep current/default fader state.
- Document and test the startup ordering guarantee that saved configuration opens WRLD.BLRD MIDI before the current patch load is requested.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `scene-state-management`: Patch JSON now persists SquiggleBoy faders, and patch load conditionally restores them based on the caller-provided `restoreFaders` policy.

## Impact

- Affected code: `private/src/TheNonagonSquiggleBoy.hpp`, `private/src/SquiggleBoy.hpp`, `private/src/StateInterchange.hpp`, `JUCE/SmartGridOne/Source/MainComponent.h`, `JUCE/SmartGridOne/Source/NonagonWrapper.hpp`, patch load helpers/tests under `private/test/`.
- Patch JSON gains a small fader array field; old patches remain valid.
- No external dependencies or file format migration step are required.

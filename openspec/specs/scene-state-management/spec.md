# Scene State Management Specification

## Purpose
Scene state management coordinates Smart Grid One's eight performance scenes and the persistence of everything that is not an audio sample. The `SceneManager` (`private/src/SceneManager.hpp`) is the single source of truth for the two active scene indices, the global blend factor, the centralized shift flag, and per-frame change signaling consumed by the encoder parameter system (see encoder-parameter-system). The `StateSaver` registry (`private/src/StateSaver.hpp`) persists non-encoder runtime values—an 8-scene instance for Nonagon sequencer state and a single-scene instance for global configuration—while encoder state is serialized separately by `EncoderBankBank`. Patch save, load, and new-patch reset flow through a JSON state interchange, with per-voice sample directories restored asynchronously through the IO task thread (see io-task-thread).
## Requirements
### Requirement: Dual Active Scenes with Global Blend
The system SHALL maintain eight persistent scenes (`SceneManager::x_numScenes == 8`) with two active scene indices `m_scene1` and `m_scene2` (each 0–7, possibly equal) and a global blend factor in [0, 1]; scene-stored arrays are read through `GetSceneValue(values) = values[scene1] × (1 − blend) + values[scene2] × blend`.
Scene 1 is considered active when blend < 1 and scene 2 when blend > 0, so edits at an endpoint affect only the fully sounding scene.

#### Scenario: Blended scene read
- **WHEN** scene 1 is index 2 holding value 0.4, scene 2 is index 5 holding 0.8, and the blend factor is 0.5
- **THEN** `GetSceneValue` returns 0.6

#### Scenario: Endpoint blend isolates one scene
- **WHEN** the blend factor is 0
- **THEN** `Scene2Active()` is false and edits gated on active scenes (such as `ZeroCurrentScene`) modify only scene 1's stored values

### Requirement: Per-Frame Change Detection Flags
The system SHALL compare the scene indices and blend factor to the previous frame at the start of every sample (`SceneManager::Process`, called before all consumers) and publish two flags: `m_changed` when the blend factor or either scene index differs from the previous frame, and `m_changedScene` when a scene index changed, or when the blend or assignment changed while the new or previous blend sits at an endpoint (exactly 0 or 1).
Both flags are cleared each frame before re-evaluation.

#### Scenario: Mid-range blend motion
- **WHEN** the blend factor moves from 0.3 to 0.4 with unchanged scene indices
- **THEN** `m_changed` is true and `m_changedScene` is false

#### Scenario: Blend leaves an endpoint
- **WHEN** the blend factor moves from 0 to 0.1
- **THEN** both `m_changed` and `m_changedScene` are true

#### Scenario: Scene reassignment
- **WHEN** `m_scene2` changes from 1 to 4 mid-blend
- **THEN** both `m_changed` and `m_changedScene` are true on that frame

### Requirement: Consumers Refresh from Change Flags
Encoder bank processing SHALL react to the scene manager's flags each control pass: on `m_changed`, every bank re-derives all cells' live banked values from scene storage (`SetStateRecursive`, which also forces recompute); on `m_changedScene`, every bank recomputes its affecting-modulator and affecting-gesture bitmasks so gesture activations and non-zero modulations of the newly active scene pair take effect.
The 8-scene `StateSaver` instance likewise consumes scene/blend changes to swap registered discrete values between scenes.

#### Scenario: Moving the blend morphs every parameter
- **WHEN** the performer moves the scene crossfader on a frame
- **THEN** all encoder cells in all banks refresh their banked values from the newly blended scene storage on that control pass
- **AND** `EncoderBankUIState::GetValue` reflects the morphed outputs

#### Scenario: Selecting a new scene re-evaluates topology
- **WHEN** a new scene index is assigned to a deck
- **THEN** each bank recomputes which modulators and gestures affect each parameter under the new scene pair

### Requirement: Centralized Shift Flag and UI Blinker
The system SHALL hold a single shift flag on the `SceneManager` (`m_shift`), set by the owning controller layer and read by all press-and-hold consumers, and a `Blink` instance processed each frame for UI feedback such as gesture-capture blinking.
A shift-press on an encoder zeroes its modulator tree for the current scene(s) and track (or deactivates the selected gestures); the Nonagon sequencer reads the same flag for its shift behaviors.

#### Scenario: Shift-press zeroes modulators
- **WHEN** the shift flag is held and a connected encoder with no selected gestures is pressed
- **THEN** all of that parameter's modulation depth cells are set to 0 for the active scene(s) on the current track and empty depth cells are garbage-collected

#### Scenario: One flag serves all consumers
- **WHEN** the owner sets `m_shift` true
- **THEN** encoder banks and the Nonagon sequencer observe shift simultaneously without separate per-subsystem flags

### Requirement: Generic Per-Scene Value Registry
The `StateSaver` registry SHALL persist registered runtime values by pointer with a fixed byte width of 1, 2, 4, or 8, keeping one stored copy per scene in an internal buffer (`m_buf`, NumScenes × 8 bytes per value) plus a default copy captured at registration; serialization (`ToJSON`) first saves the live value into the current scene, then emits all scenes' bytes, and `SetFromJSON` restores the buffers and writes the current scene's copy back to the live pointer.
Two instantiations exist: `ScenedStateSaver` (8 scenes) holding Nonagon sequencer state, and `StateSaver` (1 scene) holding global non-scene state such as the active scene indices themselves, the active trio, and per-voice source/filter machine selections. `Finalize` copies the initial scene's values into all other scenes so every scene starts from the registered defaults.

#### Scenario: Registered value round-trips
- **WHEN** a registered 4-byte value is 7 in scene 0 and 9 in scene 3 and the saver is serialized and restored
- **THEN** the JSON encodes all eight scene copies and after `SetFromJSON` the live pointer holds the current scene's stored value

#### Scenario: Revert to default restores registration-time values
- **WHEN** `RevertToDefaultAllScenes` runs
- **THEN** every scene's stored bytes are replaced by the value captured when the field was registered
- **AND** the live pointer is rewritten for the current scene

### Requirement: Staggered Scene Switching for Discrete Values
Because registered values are discrete and cannot be interpolated, the 8-scene `StateSaver` SHALL assign each registered value a boundary point in (0, 1)—registrations are shuffled and boundary i is `(i + 1) / (count + 1)`—and, as the blend factor moves, switch each value between its scene-1 and scene-2 copies when the blend crosses that value's boundary (scene 1 while blend < boundary, scene 2 otherwise).
Only values whose boundaries lie in the swept blend interval are visited on a given frame, so a crossfade gradually flips the sequencer's discrete state from one scene to the other instead of jumping at a single threshold.

#### Scenario: Crossfade flips values progressively
- **WHEN** the blend factor sweeps from 0 to 1 across several frames
- **THEN** each registered Nonagon value switches from its scene-1 copy to its scene-2 copy at its own boundary point
- **AND** values with lower boundaries flip earlier in the sweep than values with higher boundaries

#### Scenario: Switching preserves edits
- **WHEN** a value is edited while reading from scene 1 and the blend then crosses its boundary
- **THEN** the edited value is saved back into scene 1's buffer before the scene-2 copy is loaded into the live pointer

### Requirement: Patch Persistence with Separate Encoder Serialization
The system SHALL serialize a patch as a JSON object with distinct sections: `nonagon` (the 8-scene StateSaver), `squiggleBoy` (all encoder state via `EncoderBankBank::ToJSON`, see encoder-parameter-system), `stateSaver` (the global single-scene registry), `configGrid` (configuration including sample directories), and `faders` (the sixteen SquiggleBoy analog fader values); encoder state does not pass through the `StateSaver` registry.
Save, load, and new-patch requests arrive through a state interchange polled on the audio thread: save serializes and acknowledges, load deserializes in place using the load request's `restoreFaders` policy, and a new-patch request runs `RevertToDefault(allScenes == true, allTracks == true)`, which restores encoder defaults (zeroing modulators and resetting slew state), reverts the Nonagon, resets both StateSavers, and resets the config grid.
Audio-thread serialization SHALL be real-time safe: `ToJSON` SHALL allocate every node, key, and string from a caller-owned arena (see arena-json) and SHALL NOT call the system heap allocator. The arena SHALL be created and sized on the message thread and threaded as the first argument to `ToJSON`/`FromJSON`. When an audio-thread save produces a null root because the arena was exhausted, the message thread SHALL release the arena, allocate one of at least double the capacity, and re-request the save; the arena's lifetime SHALL bracket the save round-trip so the serialized tree (pointers into the arena) remains valid through the message-thread `Dumps` and save acknowledgment.
Higher-level patch load functions SHALL pass an explicit `restoreFaders` value to patch deserialization. The top-level JUCE open/startup load path SHALL set `restoreFaders` to false when a WRLD.BLRD controller is open and true otherwise. Reload-style patch functionality SHALL set `restoreFaders` to false unconditionally. During application startup, saved WRLD.BLRD configuration SHALL be applied before the current patch load is requested, so the top-level restore policy reflects the startup controller attachment state.

#### Scenario: Patch JSON contains patch sections and faders
- **WHEN** a save is requested after SquiggleBoy fader values have been changed
- **THEN** the produced JSON object has `nonagon`, `squiggleBoy`, `stateSaver`, `configGrid`, and `faders` members
- **AND** every named encoder's per-scene values appear under `squiggleBoy`, not under `stateSaver`
- **AND** the `faders` member contains all sixteen current SquiggleBoy fader values

#### Scenario: New patch reverts everything
- **WHEN** a new-patch request is received
- **THEN** all encoders return to their default values in all scenes and tracks with modulators zeroed
- **AND** registered StateSaver values return to their registration-time defaults in every scene

#### Scenario: Audio-thread save allocates only from the arena
- **WHEN** a save is serviced on the audio thread with an adequately sized arena
- **THEN** the patch JSON including faders is produced without any system heap allocation on the audio thread

#### Scenario: Undersized arena retries with doubled capacity
- **WHEN** an audio-thread save returns a null root because the arena was too small
- **THEN** the message thread releases the arena, allocates one of at least double the capacity, and re-requests the save
- **AND** the retried save produces a complete patch JSON including faders

#### Scenario: Faders restore when requested
- **WHEN** a patch containing a `faders` member is loaded with `restoreFaders` set to true
- **THEN** the live SquiggleBoy fader array is overwritten with the saved fader values
- **AND** the analog UI state publishes the restored fader values on the next frame

#### Scenario: Faders are not restored when a controller is attached
- **WHEN** a patch containing a `faders` member is loaded through the top-level JUCE path while WRLD.BLRD is open
- **THEN** the load request passes `restoreFaders` as false
- **AND** the live SquiggleBoy fader array keeps its pre-load values

#### Scenario: Reloading a patch does not restore faders
- **WHEN** reload-style patch functionality loads a patch containing a `faders` member
- **THEN** the load request passes `restoreFaders` as false
- **AND** the live SquiggleBoy fader array keeps its pre-reload values

#### Scenario: Startup decides fader restoration after controller setup
- **WHEN** application startup loads saved configuration naming a WRLD.BLRD input and output and then loads the current patch
- **THEN** WRLD.BLRD configuration is applied before the current patch load is requested
- **AND** the current patch load uses `restoreFaders` false if WRLD.BLRD opened successfully and true otherwise

#### Scenario: Old patches without faders remain compatible
- **WHEN** a patch with no `faders` member is loaded
- **THEN** patch load succeeds
- **AND** the live SquiggleBoy fader array is not changed by the missing member

### Requirement: Patch Load Preserves Default Whole-Number Encoder Floats
Patch save/load round-trips SHALL preserve default non-zero encoder float parameters whose normalized values are exact whole numbers. Loading a patch saved from a fresh system MUST NOT turn such parameters into zero because of JSON numeric node type differences.

#### Scenario: Fresh patch preserves master and low-pass defaults
- **WHEN** a fresh patch is saved and then loaded into a fresh system
- **THEN** `MasterVolume` is restored to `1.0`
- **AND** `LPCutoff` is restored to `1.0`
- **AND** `Source1LP` is restored to `1.0`

#### Scenario: Whole-number float patch remains compatible
- **WHEN** a patch file contains encoder float fields encoded as JSON whole-number tokens such as `1`
- **THEN** patch load restores those fields to their numeric values
- **AND** no patch file migration is required

### Requirement: Asynchronous Sample Directory Restoration
The config grid section SHALL persist one relative sample-directory path per voice (`sampleDirectoryRelative`, an array with an entry per synthesis voice); on load, each non-empty path is submitted to the IO task thread via `PushLoadAudioBufferBankFromDirectory` for asynchronous loading into that voice's audio buffer bank (see io-task-thread and sampler-looper-recording), and an empty path clears any existing bank for that voice.
The audio thread is never blocked on disk IO during patch load.

#### Scenario: Non-empty path loads asynchronously
- **WHEN** a patch is loaded whose `sampleDirectoryRelative` entry for voice 2 names a directory
- **THEN** a load task for that directory targeting voice 2's buffer bank is pushed to the IO task thread and patch loading continues without waiting for completion

#### Scenario: Empty path clears the bank
- **WHEN** the entry for a voice is the empty string
- **THEN** that voice's existing audio buffer bank, if any, is released and no load task is queued

### Requirement: Reset and Edit Operations Stay Coherent Across Scenes and Patches
Encoder reset/shift operations and discrete grid-cell edits SHALL leave the system in a state where the published UI surface and the underlying computed/stored values agree, and that agreement SHALL survive scene switches and patch save/load round-trips. Specifically: a shift-press reset performed while a given scene pair is active resets the current track's modulation for the active scene(s) only and SHALL NOT corrupt the stored values of inactive scenes; and any value altered by a reset or edit SHALL be serialized and restored by a save/load round-trip exactly as it stands after the edit (encoder state through `EncoderBankBank`, discrete grid/cell state through `StateSaver`; see encoder-parameter-system and the Theory of Time topology grid in phasor-timebase).

#### Scenario: Shift-reset in one scene leaves other scenes intact
- **WHEN** a parameter is modulated in both scene 1 and scene 2, scene 1 is the active scene (blend 0), and the performer shift-presses the encoder to reset it
- **THEN** the scene-1 modulation is cleared and the published value converges to the scene-1 base value after settle frames
- **AND** switching the active scene to scene 2 republishes scene 2's still-modulated value (scene 2 was not affected by the scene-1 reset)

#### Scenario: Post-reset state round-trips through a patch
- **WHEN** an encoder is modulated, then shift-reset, then the patch is saved, the system reset to defaults, and the patch reloaded
- **THEN** the reloaded encoder has no modulation and its published value equals the base value after settle frames, matching the state immediately after the reset

#### Scenario: Discrete grid-cell state round-trips and tracks scenes as registered
- **WHEN** a discrete grid cell backed by the `StateSaver` registry (such as a Theory of Time topology multiplier) is edited, the patch is saved, the system reset, and the patch reloaded
- **THEN** the cell's stored value is restored to its edited value
- **AND** the cell's published LED color reflects the restored value

## MODIFIED Requirements

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

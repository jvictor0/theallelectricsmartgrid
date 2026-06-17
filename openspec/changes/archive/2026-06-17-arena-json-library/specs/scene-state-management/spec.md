## MODIFIED Requirements

### Requirement: Patch Persistence with Separate Encoder Serialization
The system SHALL serialize a patch as a JSON object with distinct sections: `nonagon` (the 8-scene StateSaver), `squiggleBoy` (all encoder state via `EncoderBankBank::ToJSON`, see encoder-parameter-system), `stateSaver` (the global single-scene registry), and `configGrid` (configuration including sample directories); encoder state does not pass through the `StateSaver` registry.
Save, load, and new-patch requests arrive through a state interchange polled on the audio thread: save serializes and acknowledges, load deserializes in place, and a new-patch request runs `RevertToDefault(allScenes == true, allTracks == true)`, which restores encoder defaults (zeroing modulators and resetting slew state), reverts the Nonagon, resets both StateSavers, and resets the config grid.
Audio-thread serialization SHALL be real-time safe: `ToJSON` SHALL allocate every node, key, and string from a caller-owned arena (see arena-json) and SHALL NOT call the system heap allocator. The arena SHALL be created and sized on the message thread and threaded as the first argument to `ToJSON`/`FromJSON`. When an audio-thread save produces a null root because the arena was exhausted, the message thread SHALL release the arena, allocate one of at least double the capacity, and re-request the save; the arena's lifetime SHALL bracket the save round-trip so the serialized tree (pointers into the arena) remains valid through the message-thread `Dumps` and save acknowledgment.

#### Scenario: Patch JSON contains the four sections
- **WHEN** a save is requested
- **THEN** the produced JSON object has `nonagon`, `squiggleBoy`, `stateSaver`, and `configGrid` members
- **AND** every named encoder's per-scene values appear under `squiggleBoy`, not under `stateSaver`

#### Scenario: New patch reverts everything
- **WHEN** a new-patch request is received
- **THEN** all encoders return to their default values in all scenes and tracks with modulators zeroed
- **AND** registered StateSaver values return to their registration-time defaults in every scene

#### Scenario: Audio-thread save allocates only from the arena
- **WHEN** a save is serviced on the audio thread with an adequately sized arena
- **THEN** the four-section patch JSON is produced without any system heap allocation on the audio thread

#### Scenario: Undersized arena retries with doubled capacity
- **WHEN** an audio-thread save returns a null root because the arena was too small
- **THEN** the message thread releases the arena, allocates one of at least double the capacity, and re-requests the save
- **AND** the retried save produces a complete four-section patch JSON

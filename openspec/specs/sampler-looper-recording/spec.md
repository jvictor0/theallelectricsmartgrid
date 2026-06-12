# Sampler-Looper Recording Specification

## Purpose
The sampler-looper recording system (`private/src/RecordingBuffer.hpp`, `RecordingManager.hpp`, `RecordingConfig.hpp`, `AudioBuffer.hpp`) lets each voice capture audio from a selected trio lane into an in-memory `RecordingBuffer`, validate the captured span against the Theory of Time loop structure (see phasor-timebase), and persist it as a loop-aligned mono WAV that immediately becomes playable through the voice's `AudioBufferBank` and sample source (see source-machine-oscillators). Audio capture happens on the audio thread; all disk work — directory creation, WAV writing, bank reloads — is delegated to the I/O task thread (see io-task-thread).

## Requirements

### Requirement: Per-Voice Fixed-Capacity Recording Buffer State Machine
The system SHALL give each of the 9 voices one `RecordingBuffer` with a fixed 16 MB capacity (4,194,304 float samples) and an explicit state machine over Idle, Recording, Done, and Error; `StartRecording()` acts only from Idle, clearing previous audio, zeroing the repeat count, and atomically storing the current unwound master-loop position from `TheoryOfTime` as the start position.
The audio thread owns state and buffer mutation; the I/O thread only reads the position/repeat atomics after a completed recording is handed off.

#### Scenario: Start from Idle captures the master position
- **WHEN** `StartRecording()` is called on an Idle buffer while the unwound master position reads 12.25
- **THEN** the buffer is cleared, the start position stores 12.25, and the state becomes Recording

#### Scenario: Start is ignored outside Idle
- **WHEN** `StartRecording()` is called on a buffer in Recording, Done, or Error state
- **THEN** the state, buffer contents, and stored positions are unchanged

### Requirement: Loop-Divisor Validation on Stop
The system SHALL, on `StopRecording()`, store the current unwound master position as the stop position and compute the span = stop − start as a fraction of the master loop: spans greater than 1.0 or less than or equal to 0.0 set the repeat count to 0 and the state to Error; otherwise `ComputeRecordingRepeats` selects the largest Theory of Time loop divisor whose relative size 1/externalMult fits within the span and stores that multiplier as the repeat count, transitioning to Done, or to Error when no divisor fits (repeat count 0).

#### Scenario: Half-loop recording maps to two repeats
- **WHEN** recording stops with span 0.5 and a loop with external multiplier 2 exists
- **THEN** the repeat count stores 2 and the state becomes Done

#### Scenario: Quarter-loop recording maps to four repeats
- **WHEN** recording stops with span 0.26 and the available divisors include external multipliers 2 and 4
- **THEN** the largest fitting relative size is 1/4, the repeat count stores 4, and the state becomes Done

#### Scenario: Span longer than one master loop
- **WHEN** recording stops with span 1.3
- **THEN** the repeat count stores 0 and the state becomes Error

#### Scenario: Non-aligned span
- **WHEN** recording stops with a positive span smaller than every loop's relative size
- **THEN** `ComputeRecordingRepeats` returns 0 and the state becomes Error

### Requirement: Capacity Overflow Aborts the Recording
The system SHALL transition a Recording buffer to Error when a sample arrives while the buffer already holds its full capacity, storing the current unwound master position as the stop position and discarding the sample; `WriteToFile` returns false for any buffer whose sample count is zero, whose span is not positive, or whose repeat count is not positive, so an overflowed recording never produces a file.

#### Scenario: Overflow during capture
- **WHEN** `Process(input)` is called with the buffer at 4,194,304 samples
- **THEN** the stop position is stored, the state becomes Error, and the input sample is not appended

#### Scenario: Error buffer refuses to persist
- **WHEN** `WriteToFile` is invoked on a buffer with repeat count 0
- **THEN** it returns false without opening a file

### Requirement: Recording Source Selection per Voice
The system SHALL route recording input through `RecordingConfig::Source`, one of Water, Earth, Fire, or None: the selected trio (index 0, 1, or 2) supplies its same-lane voice (source voice = trioIndex × 3 + recordingVoiceID % 3) as the `RecordingBufferWriter` feeding the destination voice's buffer; a source of None yields no writer and `StartRecording` does nothing for that voice.

#### Scenario: Earth source for a Fire-trio voice
- **WHEN** voice 7 (Fire trio, lane 1) has source Earth and recording starts
- **THEN** voice 7's recording buffer is registered with the writer of voice 1 × 3 + 1 = 4 (Earth lane 1)

#### Scenario: Source None disables recording
- **WHEN** a voice's recording source is None and `StartRecording(voiceID)` is called
- **THEN** no buffer transitions to Recording and the active-recording count is unchanged

### Requirement: Fan-Out Sample Capture
The system SHALL capture audio by having each source voice's `RecordingBufferWriter` push one sample per audio tick into every registered destination `RecordingBuffer`; buffers are registered when recording starts and deregistered when it stops, and a writer with no registrations performs no captures.
Trio-level start/stop applies the per-voice operation to all three lanes of the trio.

#### Scenario: One source feeding two destinations
- **WHEN** two destination buffers are registered with the same source writer and the writer processes a sample
- **THEN** both buffers in Recording state append that sample

#### Scenario: Trio recording starts three lanes
- **WHEN** `StartRecording(Trio::Water)` is called
- **THEN** `StartRecording` runs for voices 0, 1, and 2

### Requirement: Loop-Aligned Mono WAV Persistence
The system SHALL persist a Done recording as a mono WAV at the engine sample rate containing exactly one full master loop of samples (round(captured samples / span)): the captured audio is placed at its original loop phase, computed from the start position modulo the loop fraction 1/repeats, with silence filling positions outside the captured span, and the loop-aligned material repeated for each of the repeat count's loop divisions.
For a capture that wraps across the master-loop boundary, the wrapped tail is written first, silence fills the gap, and the beginning of the capture is written last; for a capture within a single turn, silence pads before and after the audio. `WriteToFile` returns false if the writer reports an error.

#### Scenario: Mid-loop capture is padded on both sides
- **WHEN** a Done buffer with repeats 1 spans master positions 10.25 to 10.75 and is written
- **THEN** the file holds one full master loop where the first and last quarters are silence and the captured audio occupies phases 0.25 through 0.75

#### Scenario: Wrapped capture writes tail first
- **WHEN** a Done buffer spans positions 10.9 to 11.1 (wrapping the loop boundary)
- **THEN** the portion captured after the wrap appears at the start of the file and the portion captured before the wrap appears at the end, with silence between

#### Scenario: Two repeats duplicate the material
- **WHEN** a Done buffer with repeat count 2 is written
- **THEN** the file's second half repeats the same loop-aligned material as its first half

### Requirement: Persistence Orchestration and Failure Reset
The system SHALL, when a stopped voice's buffer reaches Done, push a `PersistRecording` task to the I/O task thread targeting the voice's existing bank directory, or request directory creation when the voice has no bank or the bank's directory name is empty; if the task queue rejects the push, or if the buffer stopped in Error, the buffer is reset to Idle immediately so it can be reused.
Worker-side file naming and directory creation are specified in io-task-thread: created directories are named `YYYY-MM-DDTHH-MM-SS_voiceN` under the sample root, and files are named `_recording_NNNNN.wav` with a 5-digit zero-padded index one greater than the highest existing recording index in the directory.

#### Scenario: Done buffer with an existing directory
- **WHEN** voice 3 stops with state Done and its bank's directory name is "loops/water"
- **THEN** a PersistRecording task is pushed with relative path "loops/water", createDirectory false, the voice's bank pointer as sink, and voice ID 3

#### Scenario: Done buffer without a directory
- **WHEN** a voice stops with state Done and its bank pointer is null or its directory name is empty
- **THEN** the PersistRecording task is pushed with an empty relative path and createDirectory true

#### Scenario: Queue-full fallback
- **WHEN** the PersistRecording push returns false
- **THEN** the recording buffer is reset to Idle and no file is written

### Requirement: Post-Stop Directory Reload for All Tracks
The system SHALL, after any stop operation completes, push a `ReloadDirectory` task for every voice whose bank has a non-empty directory name, so all tracks referencing a changed directory pick up newly written recordings; reloads reuse already-loaded `AudioBuffer` objects whose file names are still present in the directory and preserve the bank position (see io-task-thread for execution and installation).

#### Scenario: Shared directory picks up a new recording
- **WHEN** two voices' banks both name directory "loops/shared" and a recording stop completes
- **THEN** a ReloadDirectory task is pushed for each voice's bank sink with path "loops/shared"

#### Scenario: Reload preserves unchanged buffers
- **WHEN** a reload runs on a directory whose existing WAV files are unchanged plus one new `_recording_00003.wav`
- **THEN** the new bank reuses the existing `AudioBuffer` objects by file name, loads only the new file, and keeps the previous bank position

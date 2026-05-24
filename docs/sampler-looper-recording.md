# Sampler-Looper Recording

The sampler-looper system gives each track a way to capture audio, turn the
capture into a loop-aligned sample, and make that sample available through the
normal sample-source path.

Each track has three related pieces:

- a recording buffer,
- a sample source,
- a sample directory.

The recording buffer is fixed per voice and lives next to the component that will
play it back. A track also has a configurable recording source. That source
selects which trio/lane output should feed the track's own recording buffer while
recording. The sample source plays back from an `AudioBufferBank`. The sample
directory is the persistent directory backing that bank. This complexity mirrors
the Octatrack's flex buffers, where recording buffers and playback assignments
are related but not the same object.

## Recording Sources

Recording is routed through `RecordingConfig`. A track's source selection chooses
which trio's same-lane voice should provide audio. For example, lane 2 on a track
configured for `Water` records Water lane 2's output into that track's own
recording buffer. A voice can select one of the internal trio sources:

- `Water`
- `Earth`
- `Fire`
- `None`

Internal recording uses `RecordingBufferWriter` on the selected source voice to
fan out source samples to every active destination `RecordingBuffer`. External
sampling is a planned extension of the same model: an external source writer
should feed selected destination buffers, which then persist into sample
directories and reload sample banks after saved files appear on disk.

`RecordingManager` coordinates recording by voice or by trio. It resolves each
voice's selected source writer, starts or stops the voice's own recording buffer,
and registers or deregisters that buffer with the source writer.

## Recording Buffers

`RecordingBuffer` is a mono in-memory capture buffer. It has a fixed capacity and
an explicit state machine:

- `Idle`
- `Recording`
- `Done`
- `Error`

On start, a buffer in `Idle` clears previous audio and stores the current unwound
master-loop position from `TheoryOfTime`. While recording, the selected source
voice's `RecordingBufferWriter` pushes one sample into every registered
destination buffer each audio tick. On stop, the destination buffer stores the
stop position and computes how the captured span relates to the available
`TheoryOfTime` loops.

This start/stop phase information is what lets a recording become a loop rather
than a raw slice of audio. If the recording span does not map cleanly into the
supported loop range, the buffer enters `Error` instead of producing a file.

## Loop-Aligned Persistence

When a recording is persisted, `RecordingBuffer::WriteToFile()` writes a mono WAV
through `MultichannelWavWriter`.

The write is phase-aware:

- If recording starts and stops within the same master-loop turn, the file is
  padded before and after the captured audio so the result fills the loop.
- If recording wraps across the master-loop boundary, the wrapped tail is written
  first, silence fills the gap, and the beginning of the capture is written last.
- If the chosen loop requires multiple repeats, the same loop-aligned material is
  written repeatedly.

The result is a sample that fits exactly one full loop of the selected time
relationship, so the sample source can play it back as part of the same
`TheoryOfTime` structure.

## Sample Directories

Sample directories are relative to the configured sample root. A track with an
existing sample directory writes its recording into that directory. A track
without an existing directory can create a UUID-named directory under the sample
root and use that directory as its sample bank.

Recorded files use the name form `_recording_NNNNN.wav`. The number is based on
the count of regular files already in the target directory, so each recording is
appended as the next sample in that directory.

Multiple tracks may share one sample directory. After recording, the system
reloads known sample directories instead of trying to identify every track that
points at the changed directory. Reloading is inexpensive, and the simpler rule
keeps shared-directory behavior predictable.

`AudioBufferBank` stores each loaded sample with its source file name. Directory
reloads can reuse buffers whose file names are still present, preserve the bank
position, and add newly recorded files.

## Sample Playback

The sample source reads from the track's `AudioBufferBank`. `SampleBankPosition`
selects or blends between files in the directory, while `SampleLoopIndex`,
`SampleStart`, `SampleLength`, and `SampleReadSpeed` control how the selected
sample is read through the grain engine.

The looper side and the sample source both use `TheoryOfTime` loop information.
Recordings use the master loop to define their captured span, and sample playback
uses selected Theory of Time loops for synchronized playback phase.

## Related

- [Source Machine](source-machine.md)
- [I/O Task Thread](io-task-thread.md)
- [Theory of Time](theory-of-time.md)
- [State Saver](state-saver.md)

# I/O Task Thread

`IoTaskThread` centralizes file-system work that should not run in the realtime
audio path. It is used by sample directory browsing, sample bank loading,
directory reloads, recording persistence, and deferred deletion of retired sample
banks.

The thread owns the absolute sample-directory root. Callers pass relative sample
directory paths, and tasks resolve those paths under the root before touching the
file system.

## Path Model

Sample banks are addressed by paths relative to the configured sample root. The
I/O task layer rejects paths that are absolute or that contain `..`, then
normalizes the path before joining it to the root.

This keeps patch state portable. Saved patches store relative sample directory
names, while the host application chooses the absolute sample root for the
current machine.

## Task Flow

Work moves through two queues:

- `m_taskQueue` carries requests to the worker thread.
- `m_acknowledgmentQueue` carries processed tasks back to the main side.

The lifecycle is:

1. A caller fills an `IoTaskElement` and pushes it to `m_taskQueue`.
2. The worker thread pops the task and processes it with the configured sample
   root.
3. The worker pushes the processed task to `m_acknowledgmentQueue`.
4. `ProcessAcknowledgments()` installs results, resets completed recording
   buffers, and schedules deferred cleanup work.

This keeps disk reads and writes away from realtime processing while still
allowing the main side to own pointer swaps and visible state changes.

## Task Types

`DirectoryExplorerCommand` sends a navigation command to `DirectoryExplorer`.
When the command confirms a selection, the selected relative directory is loaded
as an `AudioBufferBank`.

`InitializeDirectoryExplorer` initializes a directory browser from the sample
root and an optional relative directory. This lets the browser start from the
track's current sample directory when one is available.

`LoadAudioBufferBankFromDirectory` loads a fresh `AudioBufferBank` from a
relative directory. This is used by patch restore and by directory selection.

`ReloadDirectory` reloads a directory for an existing bank sink. When possible,
it reuses already-loaded `AudioBuffer` objects whose file names are still present
in the directory. This allows a shared sample directory to pick up newly written
recordings without discarding unchanged sample buffers.

`PersistRecording` writes a `RecordingBuffer` to disk. It can create a
UUID-named child directory first, writes the recording as the next
`_recording_NNNNN.wav` file, optionally reloads a bank into a sink, and resets
the recording buffer during acknowledgment.

`DeleteAudioBuffer` deletes a retired `AudioBufferBank`. Old banks are not
deleted directly when a replacement is installed; they are scheduled as a
separate task so cleanup stays out of the realtime-facing acknowledgment path.

## Acknowledgments

Tasks that produce a sample bank install it into an `AudioBufferBank**` sink
during acknowledgment. If the sink already points to a different bank, the old
bank is marked for deferred deletion.

Recording persistence also acknowledges the persisted `RecordingBuffer`. Once the
persist task has completed its worker-side work, acknowledgment returns the
destination voice's buffer to `Idle` so it can be reused.

## Relationship To Recording

Sampler-looper recording uses `IoTaskThread` for the disk half of the workflow.
Audio routing happens through `RecordingBufferWriter`, captured samples land in
the destination voice's `RecordingBuffer`, and file creation, directory creation,
sample-bank loading, and sample-bank reloading happen as I/O tasks.

This separation lets recording collect audio in the audio path while treating
disk writes and directory scans as asynchronous work.

## Related

- [Sampler-Looper Recording](sampler-looper-recording.md)
- [Source Machine](source-machine.md)
- [State Saver](state-saver.md)

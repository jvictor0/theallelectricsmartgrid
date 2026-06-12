# I/O Task Thread Specification

## Purpose
The I/O task thread (`private/src/IOTaskThread.hpp`) centralizes all file-system work — sample directory browsing, sample bank loading and reloading, recording persistence, and deferred deletion of retired banks — on a dedicated background worker so that file I/O never occurs on the realtime audio thread. Producers push `IoTaskElement` tasks into a bounded queue; the worker processes them against the configured absolute sample root and pushes completed tasks to an acknowledgment queue, which the audio side drains once per control frame to install results atomically. Sampler-looper-recording uses this capability for the disk half of its workflow, and the sample source machines (see source-machine-oscillators) consume the installed `AudioBufferBank`s.

## Requirements

### Requirement: Dedicated Background Worker Thread
The system SHALL spawn one worker thread at `IoTaskThread` construction that loops while running: it pops the next task from the task queue, processes it against the resolved absolute sample root, and pushes the processed task to the acknowledgment queue; when the task queue is empty it sleeps 100 microseconds before polling again.

#### Scenario: Task processed off the audio thread
- **WHEN** a task is pushed to the task queue
- **THEN** the worker thread pops it, performs its directory scans, WAV reads, or writes on the worker thread, and pushes the same element to the acknowledgment queue

#### Scenario: Idle polling
- **WHEN** the task queue is empty
- **THEN** the worker sleeps 100 µs and polls again without consuming results or blocking producers

### Requirement: Bounded Task and Acknowledgment Queues
The system SHALL move work through two bounded `CircularQueue<IoTaskElement, 128>` queues; every producer `Push*` method returns false when the task queue is full so callers can fall back (for example, resetting a recording buffer instead of persisting it), and the worker's acknowledgment push retries with 100 µs sleeps until accepted or the thread is shut down.

#### Scenario: Full task queue rejects a push
- **WHEN** the task queue already holds 128 pending tasks and `PushPersistRecording` is called
- **THEN** the method returns false and no task is enqueued

#### Scenario: Acknowledgment backpressure
- **WHEN** the acknowledgment queue is full while the worker finishes a task
- **THEN** the worker retries the acknowledgment push every 100 µs until space is available or `m_running` becomes false

### Requirement: Relative Path Safety
The system SHALL address sample directories only by paths relative to the configured sample root: a task rejects any path that is absolute or contains a `..` component, normalizes accepted paths with `lexically_normal`, rejects results that are empty or `.` for bank loads, and joins the normalized relative path to the absolute root before touching the file system.

#### Scenario: Absolute path rejected
- **WHEN** a load task carries the path "/etc/sounds"
- **THEN** path resolution fails and the task produces no result bank

#### Scenario: Parent traversal rejected
- **WHEN** a task carries the path "banks/../../secret"
- **THEN** the `..` components cause resolution to fail before any file-system access

#### Scenario: Normalized relative path resolves under the root
- **WHEN** a load task carries "loops//water/." with sample root "/Users/x/samples"
- **THEN** the path normalizes to "loops/water" and resolves to "/Users/x/samples/loops/water"

### Requirement: Producer Task API
The system SHALL expose one producer method per task type: `PushDirectoryExplorerCommand` (with optional bank sink), `PushInitializeDirectoryExplorer`, `PushLoadAudioBufferBankFromDirectory`, `PushReloadDirectory`, `PushDeleteAudioBuffer`, and `PushPersistRecording`; each fills an `IoTaskElement` with the task type and its parameters (relative path copied into a fixed 4096-byte buffer, sink pointer, recording buffer, create-directory flag, voice ID) and pushes it to the task queue.

#### Scenario: Load task carries its sink
- **WHEN** `PushLoadAudioBufferBankFromDirectory("loops/fire", &bankPtr)` is called
- **THEN** the queued element has task type LoadAudioBufferBankFromDirectory, relative path "loops/fire", and sink `&bankPtr`

#### Scenario: Reload captures the existing bank
- **WHEN** `PushReloadDirectory(path, sink)` is called while `*sink` points to an existing bank
- **THEN** the queued element records that bank so the worker can reuse its unchanged buffers

### Requirement: Frame-Synchronous Acknowledgment Installs Results
The system SHALL apply task results only on the audio side via `Acknowledge()`, called once per audio control frame from `NonagonWrapper::ProcessFrame()`: it pops every completed task and, for bank-producing tasks (load, reload, persist-with-sink, explorer Yes), swaps the result `AudioBufferBank*` into the caller's sink pointer; for PersistRecording tasks it resets the destination voice's `RecordingBuffer` to Idle so it can be reused.

#### Scenario: Bank pointer swap on acknowledgment
- **WHEN** a completed load task with result bank B and sink pointing at bank A is acknowledged
- **THEN** the sink is set to B during `Acknowledge()` on the audio side, not on the worker thread

#### Scenario: Recording buffer returns to Idle
- **WHEN** a completed PersistRecording task is acknowledged
- **THEN** the referenced `RecordingBuffer` is reset to state Idle with its sample data cleared

### Requirement: Deferred Deletion of Replaced Banks
The system SHALL never destroy a replaced `AudioBufferBank` in the acknowledgment path: when installing a new bank over a different existing one (or when a result has no sink), the old bank is recorded as pending-delete, and `Acknowledge()` pushes a `DeleteAudioBuffer` task for it so the destructor runs on the worker thread; the worker deletes the bank when it processes that task.

#### Scenario: Replacement schedules deletion
- **WHEN** acknowledgment installs new bank B into a sink currently holding bank A
- **THEN** bank A is not deleted inline; a DeleteAudioBuffer task referencing A is pushed to the task queue
- **AND** the worker thread later deletes A when processing that task

#### Scenario: Orphaned result is scheduled for deletion
- **WHEN** a bank-producing task completes with a null sink
- **THEN** the produced bank is marked pending-delete and scheduled as a DeleteAudioBuffer task rather than installed

### Requirement: PersistRecording Worker Workflow
The system SHALL process a PersistRecording task by validating the relative path, optionally creating a destination directory named `YYYY-MM-DDTHH-MM-SS_voiceN` (local time plus the task's voice ID) under the resolved path when the create-directory flag is set, computing the next recording file name `_recording_NNNNN.wav` (5-digit zero-padded index = highest existing recording index in the directory + 1, advancing past any name collision), calling `RecordingBuffer::WriteToFile` on that path, and — when the write succeeds and a sink is present — loading the directory as a fresh `AudioBufferBank` result for installation at acknowledgment.
A failed write produces no result bank, and the buffer is still reset at acknowledgment.

#### Scenario: Directory auto-creation
- **WHEN** a PersistRecording task for voice 4 with createDirectory true is processed on 2026-06-11 at 14:30:05
- **THEN** the directory "2026-06-11T14-30-05_voice4" is created under the sample root and the task's relative path is updated to it

#### Scenario: Next file index from existing recordings
- **WHEN** the target directory already contains `_recording_00001.wav` and `_recording_00007.wav`
- **THEN** the new file is written as `_recording_00008.wav`

#### Scenario: Failed write skips the bank reload
- **WHEN** `WriteToFile` returns false for a persist task that has a sink
- **THEN** no result bank is produced, and acknowledgment only resets the recording buffer

### Requirement: DirectoryExplorer Command Routing
The system SHALL route DirectoryExplorerCommand tasks to the explorer's state machine on the worker thread: Up, Down, Left, and Right navigate the directory tree; Yes loads the explorer's selected relative directory as a fresh `AudioBufferBank` result and resets the explorer; No resets the explorer without loading; and after every explorer command or initialization the explorer repopulates its UI states.
`InitializeDirectoryExplorer` initializes the browser from the sample root and an optional starting relative directory so browsing can begin at a track's current sample directory.

#### Scenario: Confirming a selection loads a bank
- **WHEN** a DirectoryExplorerCommand task with message Yes is processed while the explorer's selection is "loops/earth"
- **THEN** the worker loads "loops/earth" as a new `AudioBufferBank` result, resets the explorer, and refreshes the explorer UI states

#### Scenario: Declining resets only
- **WHEN** a DirectoryExplorerCommand task with message No is processed
- **THEN** the explorer resets and no bank is produced

### Requirement: Reload Reuses Unchanged Audio Buffers
The system SHALL process a ReloadDirectory task by building a map of the existing bank's `AudioBuffer`s keyed by file name, then constructing a new bank that reuses the mapped buffer for every WAV file still present in the directory, loads only files without a match, drops buffers whose files no longer exist, and copies the previous bank position onto the new bank; when the task carries no existing bank it behaves as a fresh load.

#### Scenario: Unchanged file reuses its buffer
- **WHEN** a reload runs and "kick.wav" exists in both the old bank and the directory
- **THEN** the new bank holds the same `AudioBuffer` object for "kick.wav" without re-reading the file

#### Scenario: Deleted file is dropped
- **WHEN** a reload runs and a file present in the old bank is no longer in the directory
- **THEN** the new bank omits that buffer

#### Scenario: Bank position survives the reload
- **WHEN** the old bank's position is 0.6 and a reload completes
- **THEN** the new bank's position is 0.6

### Requirement: Idempotent Shutdown Before Dependent Teardown
The system SHALL stop the worker via `Shutdown()`, which sets the running flag false and joins the thread; the call is idempotent (the destructor also invokes it), and owners holding objects that in-flight tasks reference — recording buffers, sample banks, directory explorers — must call `Shutdown()` before destroying those objects, since a task still executing on the worker would otherwise dereference freed memory.

#### Scenario: Shutdown joins the worker
- **WHEN** `Shutdown()` is called while the worker is running
- **THEN** the running flag becomes false and the call returns only after the worker thread has joined

#### Scenario: Repeated shutdown is safe
- **WHEN** `Shutdown()` is called a second time, or the destructor runs after an explicit shutdown
- **THEN** the call completes without error and without attempting to join a non-joinable thread

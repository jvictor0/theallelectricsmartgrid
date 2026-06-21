## Purpose

Defines project thread identity and async logging behavior so log producers on different threads do not share one non-thread-safe queue.

## Requirements

### Requirement: Project thread identity
The system SHALL define a shared `ThreadId` enum covering every project-owned thread and callback execution context that runs project code, including audio, message, MIDI sender, MIDI input, async IO, file writer, sample loader, MIDI bus sender, MIDI bus input, logger, and unknown contexts.

#### Scenario: Explicit worker thread starts
- **WHEN** a project-owned worker thread begins executing its thread body
- **THEN** the thread SHALL set the thread-local `thread_threadId` value to the matching `ThreadId` before running project work

#### Scenario: Message thread initialization
- **WHEN** the application initializes on the message thread
- **THEN** the message thread SHALL set `thread_threadId` to `ThreadId::Message` in one initialization location

### Requirement: Scoped callback identity
The system SHALL use scoped thread identity guards for callback execution contexts so callback identities do not leak into later work on the same physical thread.

#### Scenario: Audio callback entry
- **WHEN** an audio callback begins processing project audio work
- **THEN** the callback SHALL set `thread_threadId` to `ThreadId::Audio`

#### Scenario: Callback exit
- **WHEN** a scoped callback identity guard is destroyed
- **THEN** the guard SHALL set `thread_threadId` to `ThreadId::Unknown`

### Requirement: Per-thread async log queues
The async logger SHALL route each log message to a queue selected by the current thread-local `ThreadId` instead of sharing one producer queue across all threads.

#### Scenario: Concurrent producer logging
- **WHEN** two project threads with different `ThreadId` values log messages concurrently
- **THEN** each producer SHALL write only to the queue assigned to its own `ThreadId`

#### Scenario: Queue overflow
- **WHEN** the queue for the current `ThreadId` is full
- **THEN** the async logger SHALL increment the missed-message count for that `ThreadId` without blocking the producer

### Requirement: Round-robin log draining
The async logger SHALL drain per-thread queues in round-robin order across all `ThreadId` queues.

#### Scenario: Multiple active queues
- **WHEN** multiple thread queues contain pending log messages
- **THEN** the logger drain path SHALL write messages by rotating across the non-empty queues

#### Scenario: Missed-message reporting
- **WHEN** one or more messages have been missed for a `ThreadId`
- **THEN** the logger drain path SHALL report the missed-message count for that `ThreadId`

### Requirement: Stdout Mirroring
The async logger SHALL mirror every drained log line to stdout in addition to writing it to the configured session log file.

#### Scenario: Normal log line mirrors to stdout
- **WHEN** a queued log message is drained
- **THEN** the logger SHALL write the formatted line to the session log file
- **AND** the logger SHALL write the same formatted line to stdout

#### Scenario: Missed-message report mirrors to stdout
- **WHEN** the logger drain path reports one or more missed messages for a `ThreadId`
- **THEN** the missed-message report SHALL be written to the session log file
- **AND** the same missed-message report SHALL be written to stdout

#### Scenario: Producer path remains non-blocking
- **WHEN** a realtime producer calls `INFO(...)`
- **THEN** the producer SHALL enqueue the message without performing stdout IO

### Requirement: Standalone Tests Exercise Production Async Logging
The standalone test executable SHALL use the same async log queue, per-thread routing, drain loop, overflow counting, and missed-message reporting implementation as the SmartGridOne app.

#### Scenario: Test logs use real queue
- **WHEN** a standalone test calls `INFO(...)`
- **THEN** the message is enqueued through `AsyncLogQueue::Log`
- **AND** the queue selected by the current `ThreadId` receives the message

#### Scenario: Test drain uses real drain loop
- **WHEN** a standalone test drains pending log messages
- **THEN** `AsyncLogQueue::DoLog` performs the same round-robin queue traversal used by the app
- **AND** only startup configuration and filesystem paths differ between app and test runs

#### Scenario: Test overflow uses real missed counts
- **WHEN** a standalone test fills a thread log queue and logs one additional message
- **THEN** the async logger increments that thread's real missed-message count without blocking

#### Scenario: No embedded logger fork
- **WHEN** the async logger is compiled for any supported target
- **THEN** it does not compile an `EMBEDDED_BUILD` branch that bypasses or replaces the production async logging behavior

### Requirement: JUCE-Free Core Logger
The async logger implementation under `private/src` SHALL NOT include or depend on JUCE.

#### Scenario: Core logger compile
- **WHEN** `private/src/AsyncLogger.hpp` is compiled by the standalone test target
- **THEN** it compiles without JUCE include paths or JUCE types

#### Scenario: App configures logger path
- **WHEN** the SmartGridOne app starts
- **THEN** app-side JUCE code creates or resolves `SmartGridOne/logs`
- **AND** passes that filesystem path to the async logger before normal logging drain begins

### Requirement: Session Log Files
The async logger SHALL write drained log messages to one file per process session in a configured log directory.

#### Scenario: Session file creation
- **WHEN** the async logger is initialized with a log directory
- **THEN** it creates one log file in that directory named from the creation timestamp
- **AND** the filename includes enough time precision to distinguish multiple sessions started on the same day

#### Scenario: No in-session rotation
- **WHEN** the app continues running across multiple drain calls
- **THEN** all drained messages for that process session are appended to the same session log file
- **AND** the logger does not rotate files during the session

#### Scenario: Test log directory
- **WHEN** standalone tests initialize async logging
- **THEN** they provide a test-controlled log directory
- **AND** assertions can read the produced session log file from that directory

### Requirement: iPad Log Sync
The iPad sync script SHALL copy SmartGridOne log files from the iPad app container to the Mac SmartGridOne logs directory.

#### Scenario: Logs copied from iPad
- **WHEN** `scripts/sync_ipad.py` runs against an installed SmartGridOne iPad app
- **THEN** it syncs files from `Documents/SmartGridOne/logs` in the app container to `~/Documents/SmartGridOne/logs` on the Mac

#### Scenario: Logs retained on iPad
- **WHEN** log files are copied from iPad to Mac
- **THEN** the sync does not delete those log files from the iPad by default

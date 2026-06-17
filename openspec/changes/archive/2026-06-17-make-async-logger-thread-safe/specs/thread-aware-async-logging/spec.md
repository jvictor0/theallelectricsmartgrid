## ADDED Requirements

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

## ADDED Requirements

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

## ADDED Requirements

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

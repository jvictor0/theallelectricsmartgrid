## ADDED Requirements

### Requirement: App and historical system snapshots over Wi-Fi
The repository SHALL provide a small helper to copy selected/latest app session
logs and collect an iPad unified `.logarchive` using the shared connection.
System collection SHALL accept a recent duration or timezone-qualified start,
with finite timeout and size limits. It SHALL print the device, requested time
window where applicable, and output location, using a fresh capture directory.
It SHALL NOT require custom manifests or persisted capture status.

#### Scenario: Read the current app session
- **WHEN** the user requests an app-log snapshot
- **THEN** the log is copied locally without launching, stopping, or reconfiguring the app
- **AND** device log and recording files remain untouched

#### Scenario: Retrieve historical system messages
- **WHEN** the user requests a system snapshot for a relative duration or explicit start time
- **THEN** the helper saves an iPad log archive and prints the requested window and output path

### Requirement: Restart failed collection simply
A collection error SHALL stop the helper with a nonzero exit status and an error
message. Any retained incomplete output SHALL be visibly named as partial.
A subsequent invocation SHALL start a fresh capture without resume/repair logic.

#### Scenario: Archive transfer fails
- **WHEN** an archive transfer fails or times out
- **THEN** the command reports failure and does not present the partial output as a completed archive
- **AND** the user can rerun collection into a fresh output directory

### Requirement: Repository skill and full diagnostic taxonomy
The repository SHALL provide an `ipad-logs` skill with verified app/system
snapshot commands, archive query examples, and the investigation's full log
taxonomy. It SHALL cover app file/console logs, historical/live unified logs,
crash/watchdog/jetsam/analytics reports, sysdiagnose, power/thermal/storage
snapshots, CPU/wakeups, stackshots, native traces, instrumented captures,
internal/external audio, and historical collection/deployment provenance.
Other collection methods SHALL be documented as recipes with prerequisites and
verification status; this change SHALL NOT add mandatory live/crash/profiling
CLI modes.

#### Scenario: Analyze an audio incident
- **WHEN** an agent uses the skill for an incident
- **THEN** it collects relevant app/system snapshots and correlates timestamps
- **AND** it checks actual retained coverage, timezone, and startup/quit context before drawing conclusions

#### Scenario: Choose another evidence source
- **WHEN** the investigation needs a live stream, crash report, or profiling evidence
- **THEN** the skill explains the method, prerequisites, and evidence limits
- **AND** it identifies unverified recipes and deleted experimental tools rather than promising they work

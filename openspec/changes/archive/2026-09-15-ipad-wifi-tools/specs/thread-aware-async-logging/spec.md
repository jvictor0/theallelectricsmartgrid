## MODIFIED Requirements

### Requirement: iPad Log Sync
The iPad sync script SHALL copy SmartGridOne log files from the iPad app container
to the Mac SmartGridOne logs directory using the shared USB/Wi-Fi connection.
It SHALL refresh existing local session logs when the device files have grown,
publish only complete snapshots, and retain the iPad originals by default.

#### Scenario: Logs copied from iPad
- **WHEN** `scripts/sync_ipad.py` runs against an installed SmartGridOne iPad app over USB or Wi-Fi
- **THEN** it syncs files from `Documents/SmartGridOne/logs` in the app container to `~/Documents/SmartGridOne/logs` on the Mac

#### Scenario: Logs retained on iPad
- **WHEN** log files are copied from iPad to Mac
- **THEN** the sync does not delete those log files from the iPad by default

#### Scenario: Active session has new log messages
- **WHEN** a previously copied session log has grown on the iPad
- **THEN** the next sync updates the local copy to a complete snapshot of the newer file contents
- **AND** the snapshot contains the bytes present at the start of that transfer

#### Scenario: Log refresh is interrupted
- **WHEN** downloading a newer session-log snapshot fails before its expected byte count is received
- **THEN** the previous complete local copy remains intact
- **AND** the sync reports the failed refresh and retains the iPad original

## Purpose

Defines the shared device connection used by SmartGridOne deployment, file sync, and diagnostics for a known paired iPad over USB or Wi-Fi.

## Requirements

### Requirement: Known paired iPad and simple connection options
The tools SHALL share the project's known iPad UDID and Wi-Fi host defaults,
with `IOS_DEVICE` and `IPAD_HOST` overrides. They SHALL authenticate with existing
Apple pairing and verify the returned UDID before app access. Configuration SHALL
use defaults and arguments/environment variables, without a preferences store
or custom network-discovery requirement.

#### Scenario: Host changes
- **WHEN** the user supplies a new `IPAD_HOST` for the same paired iPad
- **THEN** the next command connects using that host without a saved configuration update

#### Scenario: Host returns the wrong device
- **WHEN** the connected device's UDID differs from the selected iPad
- **THEN** the command fails before app access or installation

### Requirement: USB and Wi-Fi connections
Deploy SHALL use Wi-Fi without requiring a USB cable or active native Xcode
tunnel. Sync SHALL default to the selected iPad over USB when attached and
otherwise Wi-Fi, with an explicit transport override. Wi-Fi access SHALL maintain
the required heartbeat and expose the developer services needed for launch.

#### Scenario: Sync while the iPad is connected to its audio hub
- **WHEN** the selected iPad is reachable over Wi-Fi but not USB
- **THEN** default sync uses its Wi-Fi connection

#### Scenario: Use the attached iPad
- **WHEN** the selected iPad is attached over USB and no transport override is given
- **THEN** sync uses USB

### Requirement: Fail and allow a fresh invocation
Connection/service failures SHALL stop the command with a nonzero exit status
and the underlying error. The helper SHALL close its services and heartbeat on
exit and use a finite operation timeout. Commands SHALL NOT automatically retry,
resume, or persist recovery state.

#### Scenario: Connection is interrupted
- **WHEN** an operation fails because the device connection or heartbeat ends
- **THEN** the command reports the error and closes its connection resources
- **AND** the user can rerun the command from the beginning

### Requirement: Simple pinned setup
`make ios-setup` SHALL create the dedicated ignored Python environment and install
the pinned device dependency. Documentation SHALL describe the Apple pairing,
Wi-Fi, Developer Mode, signing, and tool invocation prerequisites.

#### Scenario: Run after setup
- **WHEN** the environment and Apple prerequisites are in place
- **THEN** deploy, sync, and log helpers use the documented environment without temporary external scripts

## Purpose

Defines safe SmartGridOne patch, recording, and log transfer between a paired iPad and the Mac over USB or Wi-Fi.

## Requirements

### Requirement: Preserve patch and recording sync behavior across transports
`scripts/sync_ipad.py` SHALL use the shared device connection and default to
automatic transport selection. It SHALL copy missing patches in both directions
between the app container and `~/Documents/SmartGridOne/patches`. It SHALL copy
recordings to `~/Documents/SmartGridOne/recordings`, extract the stereo recording
using the existing channel-selection policy, and remove the iPad source only
after those operations complete successfully.

#### Scenario: Patches differ between Mac and iPad
- **WHEN** sync runs over either USB or Wi-Fi
- **THEN** missing patches are copied in each direction
- **AND** existing patches are not overwritten by a new conflict-resolution policy

#### Scenario: Recording transfers successfully
- **WHEN** a complete recording is downloaded and stereo extraction succeeds
- **THEN** the local recording and stereo output remain available
- **AND** the corresponding iPad recording is removed

### Requirement: Publish only complete downloads
The sync downloader SHALL write a temporary local file, detect unexpected EOF,
verify the expected byte count, and publish the final path only after successful
completion. A failed download SHALL preserve any prior complete local file and
the iPad original. A recording that changes size during download SHALL NOT be
deleted from the iPad. A later invocation SHALL retry incomplete downloads from
the beginning without checkpoints or byte-level resume.

#### Scenario: Wi-Fi disconnects halfway through a recording
- **WHEN** the transport fails or returns EOF before the expected bytes arrive
- **THEN** the command reports the file transfer as failed
- **AND** no incomplete file is published at the final local path
- **AND** the iPad original is retained

#### Scenario: Recording is still growing
- **WHEN** the device recording changes size during download
- **THEN** sync retains the iPad source and reports that the recording was not fully synchronized

### Requirement: Surface transfer and extraction failures
The sync command SHALL stop at the first failed transfer or stereo extraction,
identify the failed file, and return nonzero. Completed work SHALL remain in
place; sync SHALL NOT implement batch rollback or a recovery journal.

#### Scenario: Stereo extraction fails after download
- **WHEN** SoX returns an error while extracting a downloaded recording
- **THEN** the complete local recording and iPad original are retained
- **AND** the command returns nonzero with the extraction error

#### Scenario: Rerun after an interrupted download
- **WHEN** the user reruns sync after a transfer failed
- **THEN** the incomplete file is downloaded from the beginning
- **AND** the incomplete prior download is not mistaken for a complete local file

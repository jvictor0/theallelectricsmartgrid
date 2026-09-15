## Why

The iPad audio investigation established working Wi-Fi deployment and log access,
but its temporary scripts have been deleted. Put that working path in the repo so
building, deploying, syncing, and reading logs are simple repeatable commands.

## What Changes

- **BREAKING**: Remove all Rack SDK configuration and plugin targets from the root
  Makefile. Forward maintained app commands to JUCE; plain `make` builds the app.
- Add `make ios-deploy` to build Release, upgrade SmartGridOne over Wi-Fi, and
  launch it on the unlocked iPad. Keep `deploy-ios`, `ios-build`, and `ios-install`.
- Share the known paired-iPad connection, heartbeat, and developer tunnel between
  deploy, sync, and log helpers. Use simple device/host overrides.
- Make `scripts/sync_ipad.py` work over USB or Wi-Fi, refresh growing logs, and
  avoid deleting recordings after incomplete downloads or failed stereo extraction.
- Add a small helper for app-log snapshots and historical system archives, plus
  a repository `ipad-logs` skill with the full diagnostic taxonomy and recipes.
- Use a pinned Python environment with straightforward one-time setup.

Commands stop on failure, report the failing step, and can be rerun from the
beginning. No automatic retry/resume, rollback machinery, deployment receipts,
custom capture manifests, saved device preferences, or custom network discovery.
Live/crash/profiling methods are documented in the skill, not new CLI modes.

## Capabilities

### New Capabilities

- `ipad-device-access`: Connect to the known paired iPad over USB or Wi-Fi using
  simple overrides and share the working service connection.
- `ipad-file-sync`: Preserve patch/recording sync behavior over either transport
  and check download completion before removing recording originals.
- `ipad-log-diagnostics`: Retrieve app/system snapshots over Wi-Fi and document
  the investigation's other diagnostic sources and methods.

### Modified Capabilities

- `supported-build-topology`: Remove root Rack SDK integration, forward app
  targets to JUCE, and add ordered Wi-Fi deployment.
- `thread-aware-async-logging`: Sync iPad logs over USB/Wi-Fi and refresh growing
  session logs while retaining originals.

## Impact

Affects both Makefiles, `scripts/sync_ipad.py`, small shared device/deploy/log
helpers, a pinned dependency file, focused tests, and `.agents/skills/ipad-logs/`.
The existing iOS Xcode project and signing settings remain the build source.
Initially use the investigation's `pymobiledevice3==11.12.1` in an ignored local
virtual environment, separate from the existing 7.4.0 environment.

No audio-engine changes or recreation of deleted experimental instrumentation.
The taxonomy preserves knowledge about advanced sources without requiring new
implementations of them.

Source: [Check Maya44 audio errors](codex://threads/01a08480-d0c3-7920-88a4-4940cdf34f7d).

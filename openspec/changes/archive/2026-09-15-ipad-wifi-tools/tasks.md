## 1. Shared connection and setup

- [x] 1.1 Add a dedicated pinned requirements file and a simple venv/pip `make ios-setup`; document pairing, Wi-Fi, Developer Mode, signing, SoX, and tool invocation.
- [x] 1.2 Add `scripts/ipad_device.py` using the recovered pairing, UDID check, Wi-Fi heartbeat, and developer tunnel. Use known defaults plus `IOS_DEVICE`/`IPAD_HOST` overrides and USB-first automatic selection for sync; no saved preferences or custom discovery.
- [x] 1.3 Verify the pinned API and live Wi-Fi connection, and check that errors/timeouts close resources and exit without automatic retry or recovery state.

## 2. Makefiles and deployment

- [x] 2.1 Remove all Rack configuration/targets from the root Makefile and replace them with JUCE forwarding, including the default app build and existing app targets.
- [x] 2.2 Expose `ios-build`, `ios-install`, `ios-deploy`, and `deploy-ios` in both Makefiles. Build for generic iOS into a known product path and order the stages correctly under `make -j`.
- [x] 2.3 Add a small deploy helper to package the expected signed app, upgrade it over Wi-Fi, and launch it. Print each step and stop on error; no receipts, hashes, preflight workflow, or rollback.
- [x] 2.4 Verify root/JUCE Make forwarding and that a failed build prevents install. Deploy to the unlocked iPad and confirm launch and saved app data; check that a launch failure reports installed-but-not-launched.

## 3. Wi-Fi sync

- [x] 3.1 Adapt `scripts/sync_ipad.py` to the shared async connection and House Arrest container API while preserving patch and recording behavior; refresh growing app logs.
- [x] 3.2 Use a temporary download file, detect short reads, and rename only complete downloads. Delete an iPad recording only after successful stereo extraction and a stable source-size check; stop on the first failure.
- [x] 3.3 Test interrupted downloads and failed extraction retain the iPad recording, a fresh invocation retries from the beginning, and growing logs refresh. Smoke-check USB/Wi-Fi reads without deleting user recordings for connectivity testing.

## 4. App/system log helper and skill

- [x] 4.1 Add `scripts/ipad_logs.py` with only app and historical-system snapshot modes, simple time selection, fixed timeout/size limits, fresh output directories, and ordinary error output. Leave failed captures visibly partial; no manifests or resume logic.
- [x] 4.2 Create `.agents/skills/ipad-logs/SKILL.md` and adapt `log-taxonomy.md` into its reference. Provide working snapshot/query commands and clearly labeled recipes for other sources; no new live/crash/profiling CLI implementation.
- [x] 4.3 Verify a Wi-Fi app snapshot and a small system archive, validate skill metadata/links, and check failure output plus timezone/coverage guidance. Record any unavailable live checks as outstanding.

## 5. Final review

- [x] 5.1 Run the focused checks and OpenSpec validation; confirm the tools have no dependency on deleted temporary scripts and the implementation stays within the simple fail-and-rerun design.

## Context

The root Makefile is an obsolete Rack plugin build. The JUCE iOS Make targets
assume an active native CoreDevice tunnel. `scripts/sync_ipad.py` uses synchronous
USB APIs, selects the first device, and skips existing logs even when they grow.

The earlier investigation proved direct Wi-Fi access with Apple pairing, a
heartbeat, and a developer tunnel. Its recovered procedure and diagnostic sources
are preserved in [log-taxonomy.md](log-taxonomy.md).

## Goals / Non-Goals

**Goals:**

- Remove Rack from the root Makefile and provide simple JUCE forwarding.
- Build, upgrade, and launch with `make ios-deploy` over Wi-Fi.
- Reuse that connection for USB/Wi-Fi sync and app/system log snapshots.
- Preserve the full log taxonomy and useful recipes in a repository skill.
- Keep commands short, stop on failure, and support a fresh manual rerun.

**Non-Goals:**

- Automatic retries, resumable jobs, checkpoints, or rollback orchestration.
- Deployment receipts, hashes, capture manifests, or saved preferences.
- Custom discovery, multiple device profiles, or persistent tunnel daemons.
- New live/crash/profiling CLI modes, monitoring, or app instrumentation.

## Decisions

### 1. One shared connection helper and a pinned environment

Use `scripts/ipad_device.py` from the deploy, sync, and log scripts. Port the
working pairing/heartbeat sequence and developer tunnel rather than designing a
transport framework. Use context managers/finally blocks for cleanup and a normal
operation timeout. Propagate errors to the command; do not reconnect or switch
transport midway through failed work.

Authenticate with the existing Apple pairing record (`autopair=False`), check the
returned UDID, and maintain the `Marco`/`Polo` heartbeat during Wi-Fi service use.
Developer launch uses the recovered CoreDeviceProxy TCP tunnel, userspace dial
plane, and Remote Service Discovery connection. Native Xcode discovery was
unreliable in the investigation, which is why the direct path is needed.

Pin `pymobiledevice3==11.12.1` in a dedicated requirements file. `make ios-setup`
creates an ignored virtual environment and installs it using ordinary Python
venv/pip commands. Document trust, Wi-Fi connections, Developer Mode, signing,
and SoX once. Do not build an installer or pairing workflow.

### 2. Known iPad defaults with simple overrides

Default to UDID `00008122-001439681EA1001C` and the last working Wi-Fi address
`192.168.1.53`; verify the current address during implementation. Read `IOS_DEVICE`
and `IPAD_HOST` environment/Make overrides and allow matching CLI options. A
failed connection tells the user to check the address, pairing, and unlock state.
There is no preferences file, address cache, or required discovery stage.

Deploy uses Wi-Fi. Sync defaults to `auto`: use USB if this iPad is attached,
otherwise use Wi-Fi. An explicit transport option can force either path. Select
by UDID, since the iPad's display name is `iPhone (2)` and another iPhone is paired.
This small selection branch avoids a general device-management layer.

### 3. Linear build, package, install, launch

Remove all Rack includes, variables, plugin source/distribution lists, flags,
and plugin targets from the root Makefile. Forward `all`, `build`, `clean`, and
`run` to the JUCE Makefile. Both locations expose `ios-build`, `ios-install`,
`ios-deploy`, and the existing `deploy-ios` alias.

Build using the existing iOS project/scheme, Release by default,
`generic/platform=iOS`, and a known output path. `ios-build` needs no device.
Make prerequisites/recipes enforce build before install even under `make -j`;
a build error stops the recipe before any older app can be installed.

The small deploy helper packages that signed app as an IPA, installs with
`InstallationProxyService.install_from_local(..., cmd='Upgrade', developer=True)`,
and launches through `AppServiceService`. An in-place upgrade preserves app data.
`ios-install` installs the existing expected build without launching. Check the
app exists and has the expected bundle ID; use Xcode/platform signing errors.

Print the app path, target, and current step. On an error, exit nonzero and leave
completed work in place. If launch is rejected after install, say it installed
but could not launch and show the error. The user can unlock/fix the cause and
rerun the whole command. There are no persisted stage records, artifact hashes,
preflight/recovery workflows, or automatic rollback.

### 4. Minimal sync adaptation with complete downloads

Migrate sync to the shared asynchronous connection and House Arrest
`VendContainer` API; `VendDocuments` failed for this app in the investigation.
Keep missing-patch copies in both directions and the existing recording policy:
download, extract stereo with SoX, then delete the iPad recording.

Use a temporary local file and rename it only after receiving the expected byte
count. An empty read before completion raises an error. Before deleting a
recording, check that its size did not change during transfer and that stereo
extraction succeeded. These few checks are necessary because the original will
be deleted. Keep iPad logs and refresh local snapshots when they grow.

Stop at the first failure. Previously completed file transfers stay completed;
no transaction log or rollback is needed. A retry downloads an incomplete file
from the beginning; it does not resume bytes from a prior attempt.

### 5. App/system snapshots and a comprehensive skill

`scripts/ipad_logs.py` has only `app` and `system` modes using the shared
connection. App mode copies the selected/latest session log. System mode wraps
`OsTraceService.collect` with a recent-duration default, optional `--since` or
`--last`, and sensible fixed size/timeout limits. Use a fresh capture directory
and print the device, requested time window, and output path. Do not create a
manifest, file inventory, status database, or overwrite-management interface.

On failure, print the error and leave any partial capture clearly named as
partial. The user starts another capture. Routine collection leaves playback and
device files alone; it does not invoke the sync script's recording deletion.

The skill supplies working app/system commands, `/usr/bin/log show --archive`
examples, and the full [taxonomy](log-taxonomy.md). Other methods, including live
streams and crash reports, remain recipes using upstream tools/APIs and the
shared connector where appropriate; they are not implementation acceptance gates.
Identify unverified recipes and prerequisites honestly. Archive coverage, timezones,
and device event versus host receipt time belong in the analysis guidance.

## Risks / Trade-offs

- Address changes → Update `IPAD_HOST` and rerun; no discovery subsystem.
- Pairing, lock, or developer-service failure → Show the underlying error and
  prerequisite guidance; the user fixes it and reruns.
- Interrupted download before recording deletion → Keep the original and retry
  the download from the beginning.
- Historical logs have limited retention → Inspect actual archive coverage when
  analyzing an incident; a requested time window does not guarantee its retention.

## Migration Plan

Add the environment/helper, replace the root Makefile, adapt sync, then add the
snapshot helper and skill. Verify the pinned API and live Wi-Fi path during
implementation. Focus tests on stage ordering and recording preservation on
failure; use live smoke checks for the actual connection and service calls.
No data-format migration or recovery framework is needed.

## Open Questions

No product decisions are pending. Confirm the iPad's current host and the pinned
library's recovered connection APIs when implementing the helpers.

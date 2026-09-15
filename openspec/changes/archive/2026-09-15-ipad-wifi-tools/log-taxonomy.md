# iPad log sources and recovered Wi-Fi procedure

Research supporting [the design](design.md). This change implements app-log and
historical system snapshots. Other sources remain skill recipes and reference
material with their prerequisites and verification status; the taxonomy is not
a requirement to implement a collector for every source. Historical receipts,
hashes, and manifests below describe past experiments, not new tooling features.

## Recovered working path

Source: [Check Maya44 audio errors](codex://threads/01a08480-d0c3-7920-88a4-4940cdf34f7d),
including its deployment receipts and September 8–14 diagnostic captures.

- iPad UDID: `00008122-001439681EA1001C`; iPad Air 13-inch (M3).
  Its display name is `iPhone (2)`, so do not select it by name alone.
- Last successful address: `192.168.1.53`. This is a historical address, not an
  identity or a promise that DHCP will preserve it.
- Bundle: `com.theallelectricsmartgrid.smartgridone`.
- Direct TCP lockdown authenticated with the existing Apple usbmux pairing record.
  `autopair=False`; validate the returned UDID before using a service.
- Maintain `com.apple.mobile.heartbeat`: receive `Marco`, send `Polo`, and keep
  responding while the connection is open. Direct Wi-Fi service access failed
  without this exchange in the original experiments.
- App files: `HouseArrestService.create` with the app container (`VendContainer`).
  The experiment found `VendDocuments` returned `InstallationLookupFailed`.
- Install: signed app → IPA → `InstallationProxyService.install_from_local`,
  using the in-place `Upgrade` operation and `developer=True`.
- Launch: CoreDeviceProxy TCP tunnel, a process-local userspace network stack,
  Remote Service Discovery (RSD), then CoreDevice `AppServiceService`.
  This worked while native Xcode device discovery reported the iPad disconnected.
- Launch requires an unlocked iPad. Installation succeeded while locked in one
  historical run, but the later launch correctly failed.
- The recovered tooling used `pymobiledevice3` 11.12.1. The existing sync environment
  uses 7.4.0 and synchronous APIs. Use a dedicated dependency file and environment
  for the shared asynchronous implementation.

The temporary connection helper and its environment have been deleted. Historical
commands mentioning `/private/tmp/smartgrid-device-tools` are provenance, not
working installation instructions.

## Log taxonomy

| Source | Access and artifacts | What it establishes | Limits |
| --- | --- | --- | --- |
| SmartGridOne persisted app log | House Arrest app container, `/Documents/SmartGridOne/logs/*.log` | Requested/actual rate and block size; active channels and routes; timing gaps, overruns and xruns; MIDI counters; platform thermal state; recording/FileWriter events | Messages depend on the installed build. The current production log has aggregate counters; older experiment builds emitted much more detail. Missing messages alone do not prove nothing happened. |
| App stdout / debugger console | The async logger mirrors drained lines to stdout; a compatible attached launch/debug console can capture them | Immediate app messages while console capture is attached; complements the persisted session file | Console output is not automatically an iPad unified-log archive. Historical file/stream access does not prove every console attach route works over Wi-Fi; verify the chosen route. Attaching via a fresh launch can disturb the session. |
| Historical iPad unified system log | `OsTraceService.collect` → `.logarchive`; query on the Mac with `/usr/bin/log show --archive` | Kernel USB errors and enumeration, `usbaudiod` transfer/recovery events, `audiomxd` audio-session changes, process lifecycle, storage failures, power/thermal messages | Retention and requested size can limit coverage. Establish actual first/last retained timestamps. This is the iPad archive, not the Mac's own unified log. |
| Live iPad system log | `OsTraceService.syslog`, historically `stream_flags=0x184`, saved as JSONL | System events received during a reproduction, with device timestamps and host receipt times | Starts when attached; is not historical replay or an exhaustive USB capture. Bound duration and output size; preserve capture failures. |
| Crash, watchdog, jetsam, and analytics reports | `CrashReportsManager`; targeted `.ips` and diagnostic files | Exceptions, watchdog termination, memory-pressure kills, process/OS identity, and available stacks | Existing reports describe recorded incidents, not continuous runtime. Historical startup crashes were pulled using the older USB CLI; verify the pinned API's Wi-Fi retrieval before claiming it is tested. Avoid deleting reports during collection. |
| Sysdiagnose bundle | Existing report under diagnostic storage, or a separately initiated Apple sysdiagnose | Broad retrospective context, system logs and diagnostic snapshots | Larger and more intrusive to create; completion takes time. Listing for a sysdiagnose is not evidence it was created or retrieved. Do not equate it with `OsTraceService.collect`. |
| Battery, power, thermal, and storage snapshots | Lockdown battery/disk-usage domains; Diagnostics/IORegistry; supporting `powerd` and thermal system messages | Charge state, external power, available storage, and exposed sensor values at observation time | Battery temperature is not SoC, hub, or MAYA temperature. Charging off does not mean external power is disconnected. Private telemetry units need validation. |
| Process CPU and wakeup sampling | DVT `Sysmontap` over the developer connection; JSON samples | Process CPU, memory, interrupt wakeups, context switches, thread counts, where exposed | Cumulative counters require deltas and elapsed time. Process-wide wakeups cannot identify the responsible thread. CPU usage is not an audio deadline percentage. |
| Stackshots | DVT `CoreProfileSessionTap.get_stackshot`; JSON or raw `.kcdata`; symbolication with matching executable/dSYM | Thread identity, stacks, scheduling state and cumulative thread CPU; sampled locations such as UI painting | A few snapshots are not a continuous Time Profiler flame graph. Preserve binary UUIDs, image slides, timestamps and symbols. |
| Native kernel scheduler/workgroup/audio/USB trace | Bounded CoreProfileSessionTap acquisition; `.kdebug`, `.kcdata`, chunk indexes, trace configuration and decoded events | Wake-to-run delays, running versus waiting, thread scheduling and emitted USB/audio events | High volume and collection overhead; requires an explicit profiling task. Validate timebase/stackshot anchors, loss markers and actual coverage. The historical 10-second captures were about 275 MB; setup and drain left gaps. No raw USB bus packets or automatic pre-trigger buffer were obtained. |
| Instrumented app/native audio captures | Experiment-only per-callback records, native RemoteIO timing/lifecycle data and binary sample dumps | Callback entry/exit, input-render status, timestamps, sample contents before output, audio-unit lifecycle | Requires the matching instrumented build and decoder. These deleted experiment helpers/features are not promised by the normal Release build. |
| Internal recording and file metadata | App recordings, WAV header/format, file size, FileWriter messages | Audio recorded inside the app, recording start and format; evidence about the writer path | A pristine internal recording does not prove the analog output was pristine. Reading a header does not establish the entire recording is intact. |
| External audio capture | Mac microphone or K-Mix analog input capture, WAV plus timing sidecar | Audible output corruption, dropout timing, periodic modulation and waveform behavior | Requires an audio path from the interface to a recorder; it is not an iPad log or something Wi-Fi alone supplies. Account for capture latency and alignment. |
| Collection and deployment provenance | Capture manifests, app-file hashes, install receipts, build logs, screenshots and user observation timestamps | Which files/build were used, capture bounds and operational context | These corroborate other evidence; an install receipt alone is not an exact hash read back from the running on-device executable. |

### Reading an audio incident

Correlate app timing counters with kernel/`usbaudiod`/`audiomxd` records in a
clearly stated local-time window. Useful system terms from the task include
`MAYA`, `0xe00002ed`, `transaction error`, `restarting IO`, `zero-length`,
`ioDriftNS`, and the relevant device endpoints. Include launch, route-change and
quit context so startup/shutdown activity is not counted as a playback fault.

Also inspect storage terms (`ENOSPC`, `no space`, `write failed`, `storage
pressure`) when the symptom involves recording or an app stall. Identify the
emitting process: an Apple analytics writer error is not a SmartGrid FileWriter
error. Recurring virtual-audio-device frame-count diagnostics occurred during
clean periods too and must not automatically be attributed to MAYA.

Use device event timestamps for correlation; host receipt time measures transfer.
Preserve timezone offsets and account for app logs crossing midnight. One task
query missed events with an end-time restriction, so check actual archive coverage
and retry an apparently empty window without that restriction before concluding
there were no matching events.

## Upstream references

- [Pinned pymobiledevice3 release](https://pypi.org/project/pymobiledevice3/11.12.1/)
- [Lockdown implementation at the pinned version](https://github.com/doronz88/pymobiledevice3/blob/v11.12.1/pymobiledevice3/lockdown.py)
- [Upstream tunnel guide](https://github.com/doronz88/pymobiledevice3/blob/master/docs/guides/ios17-tunnels.md)

Inspect the pinned source for API details during implementation; the current
upstream guide can describe behavior added after the pin.

# SmartGridOne iPad diagnostic taxonomy

The `app` and `system` snapshot commands in the skill are implemented and
verified by this repository. Other sources below are investigation recipes.
Check their prerequisites and verification status before promising a capture.

## Connection and identity

- Device UDID: `00008122-001439681EA1001C`, iPad Air 13-inch (M3).
- Historical Wi-Fi address: `192.168.1.53`; override stale DHCP addresses with
  `IPAD_HOST`.
- Bundle ID: `com.theallelectricsmartgrid.smartgridone`.
- Direct TCP lockdown uses the existing Apple pairing record with
  `autopair=False`, validates the returned UDID, and maintains the
  `com.apple.mobile.heartbeat` Marco/Polo exchange.
- App container access uses House Arrest `VendContainer`. The investigation
  found `VendDocuments` failed for this app.
- Developer launch uses CoreDeviceProxy, a process-local userspace tunnel, RSD,
  and CoreDevice AppService. Launch requires an unlocked iPad.

## Sources

| Source | Access and artifacts | What it establishes | Limits and status |
| --- | --- | --- | --- |
| SmartGridOne persisted app log | **Verified:** `ipad_logs.py app`; House Arrest `/Documents/SmartGridOne/logs/*.log` | Requested/actual rate and block size; channels and routes; timing gaps, overruns/xruns; MIDI counters; thermal state; recording/FileWriter events | Messages depend on the installed build. Aggregate counters and older detailed experiment logs differ. Missing messages do not prove nothing happened. |
| App stdout or debugger console | Attach a compatible launch/debug console; the app's async logger mirrors drained lines to stdout | Immediate messages while attached; complements the persisted log | **Recipe, unverified over this Wi-Fi path.** Attach can require a fresh launch and disturb the session. Console output is not an iPad unified archive. |
| Historical iPad unified system log | **Verified:** `ipad_logs.py system` creates a `.logarchive`; query with `/usr/bin/log show --archive` | Kernel USB errors/enumeration, `usbaudiod` transfer/recovery, `audiomxd` session changes, process lifecycle, storage, power, thermal | Retention and size limits constrain coverage. Establish actual first/last timestamps. This is the iPad archive, not the Mac unified log. |
| Live iPad unified log | `OsTraceService.syslog`, historically with `stream_flags=0x184`, saved with device and host-receipt timestamps | Events during a reproduction | **Recipe, not a CLI mode and not reverified.** It starts at attachment, is not historical replay, and may lose messages. Bound duration and output size. |
| Crash, watchdog, jetsam, analytics | `CrashReportsManager`; copy selected `.ips` and diagnostic files without deleting them | Exceptions, watchdog termination, memory pressure kills, process/OS identity, available stacks | **Recipe, historically used by older USB tooling; pinned Wi-Fi retrieval unverified.** Reports cover recorded incidents rather than continuous runtime. |
| Sysdiagnose | Retrieve an existing bundle from diagnostic storage, or initiate Apple sysdiagnose separately | Broad retrospective system context | **Recipe.** Creation is intrusive, large, and slow. A listing does not establish creation or retrieval; this differs from `OsTraceService.collect`. |
| Battery, power, thermal, storage | Lockdown battery/disk domains; Diagnostics/IORegistry; supporting `powerd` and thermal messages | Charge state, external power, free storage, exposed sensors at observation time | **Recipe.** Battery temperature is not SoC, hub, or MAYA temperature. Charging off does not mean external power is disconnected. Validate private telemetry units. |
| Process CPU and wakeups | DVT `Sysmontap` over the developer connection; JSON samples | Process CPU/memory, interrupt wakeups, context switches, thread counts where exposed | **Recipe, historically used.** Compute deltas over elapsed time. Process-wide wakeups cannot identify a thread; CPU is not an audio deadline percentage. |
| Stackshots | DVT `CoreProfileSessionTap.get_stackshot`; preserve JSON/raw `.kcdata`, binary UUIDs, image slides, timestamps, executable, dSYM | Thread identity, stacks, scheduling state, cumulative thread CPU, sampled locations | **Recipe, historically used.** A few samples are not a continuous Time Profiler trace. Matching symbols are required. |
| Native kernel scheduler/workgroup/audio/USB trace | Bounded CoreProfileSessionTap capture; preserve `.kdebug`, `.kcdata`, chunk indexes, configuration, decoded events | Wake-to-run delays, running/waiting state, thread scheduling, emitted USB/audio events | **Recipe, historically used.** High volume and overhead; validate timebase anchors, loss markers, and coverage. Ten-second historical captures were about 275 MB and had setup/drain gaps. No raw USB bus packets or pre-trigger buffer were obtained. |
| Instrumented app/native audio | Experiment-only per-callback records, RemoteIO timing/lifecycle, binary sample dumps | Callback entry/exit, input-render status, timestamps, samples before output, audio-unit lifecycle | **Historical recipe only.** It requires matching instrumented builds and decoders; deleted experiment helpers are not part of the Release tool. |
| Internal recording and metadata | App recordings, WAV header/format, file size, FileWriter messages | Audio inside the app, recording start/format, writer-path evidence | Reading a header does not establish full integrity. A clean internal recording does not prove clean analog output. |
| External audio capture | Mac microphone or K-Mix analog-input WAV plus timing sidecar | Audible corruption/dropout timing, modulation, waveform behavior | **External recipe.** Requires a physical audio path and alignment for capture latency. Wi-Fi cannot provide it. |
| Collection and deployment provenance | Build logs, captures, screenshots, observation timestamps, and historical manifests/receipts/hashes | Which build/files and operational context were involved | Corroborating evidence only. A historical install receipt is not an exact hash read from the running device. New tools intentionally do not create receipts, hashes, or manifests. |

## Audio incident workflow

Start with the app snapshot and a small historical system archive. Correlate app
timing counters with kernel, `usbaudiod`, and `audiomxd` records in a stated local
time window. Useful terms from the original investigation include `MAYA`,
`0xe00002ed`, `transaction error`, `restarting IO`, `zero-length`, `ioDriftNS`,
and the relevant USB endpoints.

Include startup, route-change, and shutdown context so lifecycle noise is not
classified as a playback fault. Search `ENOSPC`, `no space`, `write failed`, and
`storage pressure` for recording or stall symptoms. Identify the emitting
process: an Apple analytics-writer failure is not a SmartGrid FileWriter failure.
Recurring virtual-audio-device frame diagnostics also appeared in clean periods
and do not by themselves implicate MAYA.

Use device event timestamps. Preserve timezone offsets and handle app logs that
cross midnight. If a constrained `/usr/bin/log show` query is empty, inspect the
archive's real coverage and retry without an end-time restriction before saying
there were no matching events.

## Provenance

The connection and taxonomy were recovered from the Codex task
`codex://threads/01a08480-d0c3-7920-88a4-4940cdf34f7d`. Temporary tools under
`/private/tmp/smartgrid-device-tools` were deleted and are not dependencies.
The maintained environment pins `pymobiledevice3==11.12.1`.

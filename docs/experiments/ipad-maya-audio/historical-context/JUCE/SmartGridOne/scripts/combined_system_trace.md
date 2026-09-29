# Combined native history capture

`combined_system_trace.py` reuses the eleven filters and no-periodic-stack-sampling configuration that worked in the four September 10 native captures. The helper retains scheduler/workgroup, audio, USB and trace metadata events in the same raw stream. It does not decode private USB arguments, record audio samples, provide the full Instruments Audio System Trace template, or create an automatic fault trigger.

The CLI is deliberately offline: it prints the reusable profile, imports no device package and opens no connection.

```sh
python3 -B JUCE/SmartGridOne/scripts/combined_system_trace.py --dry-run
python3 -B JUCE/SmartGridOne/scripts/test_combined_system_trace.py
python3 -B JUCE/SmartGridOne/scripts/test_usb_trace_details.py
```

An authorized experiment runner can invoke the helper directly, within its existing authenticated `DvtProvider` context. `dvt`, `time_config` and `output_base` below are runner-owned values; this is an invocation fragment, not a standalone connection command. Fetch the trace-name table and time configuration before acquisition and preserve them with the run. Use one paired connection per runner and await each capture/drain before requesting another. Do not make app-file or battery queries during acquisition.

```python
from combined_system_trace import Capture

metadata = await Capture(
    dvt, time_config, output_base,
    seconds=10, max_bytes=300000000, max_drain_seconds=90,
)
```

The runner must put this scripts directory on its module search path. The installed dependency used for the no-connection configuration check was `pymobiledevice3 11.12.1`, at `/private/tmp/smartgrid-device-tools/bin/python`. Importing the module or running its CLI needs only the standard library. `MakeTap` verifies the existing tap configuration and stackshot signature before starting; a dependency mismatch requires review.

## Filters and provenance

| Filter | Selected context |
| --- | --- |
| `0x01400000` | Mach scheduler |
| `0x01ab0000` | Workgroup intervals |
| `0x05240000` | IOKit audio |
| `0x052d0000` | IOKit USB |
| `0x05360000` | IOKit extended audio |
| `0x06050000` | Driver audio |
| `0x060e0000` | Driver USB |
| `0x07000000` | Trace data / thread lifecycle |
| `0x07010000` | Trace strings / thread names |
| `0x07020000` | Trace information / loss markers |
| `0x36ff0000` | Audio class selection retained from validated configuration |

The original helper is `/private/tmp/smartgrid-system-trace-r2-20260910/smartgrid_trace_stream_20260910.py`, SHA-256 `16c9bad55bde4a860b20f50792dd7b7f07f34756656019e580b7456455d4e3d3`. Filter-family labels follow its preserved `smartgrid-xnu-kdebug.h` and device `trace-codes.json`. Labels identify selection scope, not a promise that the device emits every selected event. No new argument meaning is assigned.

The helper preserves `rp=100`, `bm=0`, `tk=3` and removes the trigger's `ta` and `csd` keys exactly as in the working predecessor. It does not infer circular-buffer behavior from `bm`, nor set up a fault trigger. See [the experiment report](../../../../2026-09-10-native-system-trace.md) for capture overhead, scheduler interpretation and the failed long acquisition.

## Enforced bounds and artifacts

The helper rejects requests above ten seconds, above 300,000,000 payload bytes or above 90 seconds of draining, including non-finite values. Start and stop calls have 20- and 25-second timeouts. Failed startup receives a bounded best-effort stop as well. On normal completion, the acquisition request is followed by a stop and then draining; there is no attempt to acquire throughout the drain. Timer bounds depend on the host event loop and transport responding; they are not real-time guarantees or device-kernel memory limits.

The byte bound is checked **before writing each complete received message** and includes raw trace, stackshots and notices together. A message that would exceed it is rejected whole, the tap is stopped and failure metadata identifies its size. The retained prefix remains intact. JSON metadata/chunk indexes are additional small sidecars; message count is independently capped at 10,000. This does not bound buffering internal to the device or transport library. The original helper checked 1.5GB after writing; this helper deliberately strengthens that limit for the new experiment.

Outputs use a fresh prefix, with existing artifacts rejected before tap construction:

- `.kdebug`: complete received raw chunks, in order, without analysis-window clipping.
- `.stackshot-N.kcdata`: raw initial stackshot for later thread identity and matching mach/wall anchor extraction.
- `.notice-N.bplist`: raw notices, including any rejected-start notice; private fields remain uninterpreted.
- `.chunks.jsonl`: kind, source path where applicable, raw offset, byte count and host receipt timestamp per message.
- `.trace.json`: exact profile, timing configuration, request/stop/cleanup times, counts, limits and status/error.

The five-second receive-idle rule follows the prior helper and is a **drain heuristic**, not proof of an end-of-stream acknowledgement. The status remains `captured_pending_decode`. Missing raw data or initial stackshot, cap violations, transport errors and drain timeout are failures. Propagate helper exceptions and close the runner's provider; if `stop_error` is present, explicitly report that stop was not confirmed and require device-state verification before another trial. Preserve partial artifacts as failed captures.

Only after decoding should an analyst establish actual first/last event times, valid timebase/stackshot mapping, loss markers and scheduler chain consistency. Use the initial stackshot's paired mach/wall timestamps; cached `time_config` wall time is provenance only. Host receipt times describe transfer, not device event occurrence.

## Available pre-error history

The earlier ten-second acquisitions retained 9.711–9.723 seconds, after profiler startup exclusion. Their raw files were 273.5–277.8MB; total received payload, including stackshots/notices, was 274.2–278.5MB. They fit this cap but leave limited margin. If workload or transfer behavior exceeds it, fail explicitly; do not silently lengthen the request or enlarge the cap. A prior 90-second request retained only 34.793 seconds before bounded abort and is not a validated long-history option.

Pre-error native history exists only when acquisition was already active before the fault. A fault near the beginning may have almost no history. A fault late in a clean burst can have approximately the preceding retained portion of that burst. Faults during setup, draining or gaps have no new native history from this helper. The earlier drain tails lasted roughly 42–49 seconds after stopping, so repeated bursts are not continuous coverage. There is **no automatic trigger or rolling pre-trigger buffer** here.

Preserve the whole combined raw trace for audio/scheduler context. `usb_trace_details.py` still exports bounded USB-specific windows; its default 100ms-before/250ms-after selection is only an analysis view and can be widened explicitly up to ten seconds per side, subject to its event cap and clipping flags. Those USB windows are not a substitute for the combined raw stream. Correlate it with the app's native callback/lifecycle metadata, unified log and analog recording from the same authorized experiment.

No live capture, installation, playback, device query or setting change was performed to prepare this helper. The revised caps and cleanup paths are covered by offline tests; hardware retention and overhead require a later readiness-gated trial.

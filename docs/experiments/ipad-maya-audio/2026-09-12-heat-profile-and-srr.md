# September 12: SmartGrid heat profile and SRR-like episode

Status: measured, app stopped at approximately 12:49:49 PDT. This run is not a clean trial. The user heard sustained square-wave-like amplitude modulation with inharmonic distortion around 12:44–12:45, then an abrupt recovery, and reported the iPad hot to the touch later. Exact subjective onset/offset seconds were not marked.

## Setup and build

The iPad Air 13-inch (M3) remained on the Satechi hub with MAYA44 USB+, WRLD.BLDR and 90 W PD input. SmartGridOne was launched remotely at 12:40:40 in normal DSP, visible UI, autoplay and MIDI-worker mode using the saved `ms` patch. The app log verified JUCE 8.0.15, requested and actual 48 kHz/512 frames after a brief 256-frame startup transition, zero requested/active app inputs, four active outputs and the MAYA route. The installed app Mach-O UUID `32EBF23D-2FFA-3338-B018-575D0720C861` matches the Release binary in this worktree, SHA-256 `cc8739cce6b7bb0178bdb86b0584abe29355afbf9ab07f127f5d8682c01fd151`. This identifies the symbol source; it does not assert a clean repository diff.

## Heat and process work

The symptomsd battery sensor read 25.69 C at 12:40:30, 28.50 C at 12:45:10, 29.39 C at 12:47:30 and 30.50 C at 12:49:54. It peaked at 30.59 C after the app stopped, consistent with thermal lag. External power stayed connected. `battery-charging=0` and `battery-fully-charged=1` through 12:47:30; charging changed to 1 at 12:47:34, well after the first 3.70 C rise. The battery sensor is not a direct processor or hub thermometer. The app reported `thermal=0` (Nominal) and `lp=0` in every settled logged callback.

DVT sysmontap sampled SmartGridOne at about 68–70% of one CPU core during a 12-second interval around 12:42:12, approximately 8,500 internal wakeups/s and 9,500 context switches/s. Its process physical footprint was already about 1.88 GB and continued rising. Two stackshots at approximately 12:43:01 and 12:47:08 attribute the intervening CPU time to:

| Thread | CPU time over 247.7 s | Core equivalent |
| --- | ---: | ---: |
| AURemoteIO::IOThread | 101.72 s | 41.1% |
| JUCE message/UI thread | 50.36 s | 20.3% |
| IoTaskThread | 4.51 s | 1.8% |
| MidiSender | 0.40 s | 0.2% |

The instrumented DSP median was approximately 4.2–4.6 ms per 10.667-ms callback in the run, close to the audio thread's 41% of a core. The additional message/UI cost is outside the DSP meter. Symbolicated instantaneous message-thread stacks showed JUCE painting `GangedRandomLFOComponent<3>::Draw` / `CoreGraphicsContext::fillEllipse` and `AnalyserComponent::Draw` through `SmartGridOneVisualizerMain::paint`. These are examples of active work, not a statistical per-function flame graph. The installed `IoTaskThread` still polls its queue with a 100-microsecond sleep; its wake frequency plausibly contributes to the process wakeup count, but per-thread wakeups were not measured. The MIDI sender sleeps 2 ms on iOS and used little CPU. Stackshot task size grew from 1.904 to 2.039 GB over those four minutes; whether this is retained cache, expected state, or a leak is untested.

For comparison, the prior AUM/Drambo 48 kHz/512 run started at the same 25.69 C battery temperature and remained about 25.7–25.8 C over ten minutes while externally powered and not charging. Its visible DSP meter was in the mid-30% range; that meter does not include host UI, other app threads or GPU work. This is strong evidence of higher total device work in SmartGrid, not a controlled attribution of heat to one thread.

The official Xcode Time Profiler listed the Wi-Fi-paired iPad offline, so a full statistical flame graph was unavailable in this topology. DVT stackshots and thread CPU totals were available. A wider repeated stackshot sampler obtained two more samples before the device diagnostic stream exceeded its buffer limit; no per-function percentages should be inferred from those few samples.

## Audio and USB episode

Settled callbacks remained at 48 kHz/512 with Nominal thermal and no measured DSP overrun at the matched failures. The callback trace contains 14 long gaps and x-run increments after startup, all approximately 161–180 ms. The kernel archive contains a matching MAYA44 USB+ endpoint `0x82` transaction error (`0xe00002ed`) and Core Audio I/O restart for each of the 14. The first two were at 12:43:35 and 12:45:04; 12 more occurred from 12:46:59 through 12:49:37, making this run much less clean as it warmed. The app had requested zero inputs, but `usbaudiod` still ran the MAYA input-transfer manager; the transaction errors were on that input endpoint. This establishes the error → restart → callback-gap chain, not the initiating fault.

Between the first and second resets, `usbaudiod` logged input zero-length transfers and timestamp drift steps: 2.02 ms at 12:43:50, 4.04 ms at 12:44:02, 6.06 ms at 12:44:10, 8.08 ms at 12:44:12, 10.10 ms at 12:44:13 and 12.12 ms at 12:44:57. The USB restart at 12:45:04 coincides with the user's approximate abrupt recovery. This supports a relation among dropped input transfers, growing driver timing discrepancy, periodic audible modulation and reset, but does not prove the analog waveform shape or whether the fault starts in MAYA, the hub/USB path or iPadOS. No K-Mix analog recording was made in this specific run. The earlier pure-tone analog finding remains separate evidence that periodic corruption can occur downstream of pristine DSP.

## Next high-value measurement

A controlled comparison of normal SmartGrid UI-on vs UI-render-off at the same patch, 48 kHz/512 and topology should measure whole-process CPU, wakeups, battery temperature, drift, transaction errors and both audible symptom classes. Earlier UI-off trials already reproduced both audio symptoms, so reducing UI heat is not by itself a complete fix. The comparison tests whether the extra ~20% UI core and wake activity alter heat and error frequency. A true Xcode Time Profiler capture would need Xcode to see the device online (likely a direct wired developer connection, changing the live USB topology), or a different supported profiling route. Avoid treating the instantaneous stackshots as a full flame graph.

## Raw evidence

- App log: `/private/tmp/smartgrid-srr-heat-app-final-20260912.log`
- System archives: `/private/tmp/smartgrid-srr-heat-20260912-1245.logarchive` and `/private/tmp/smartgrid-srr-heat-post-20260912-1250.logarchive`
- Decoded USB windows: `/private/tmp/smartgrid-srr-heat-usb-window.log` and `/private/tmp/smartgrid-srr-heat-post-usb-signals.log`
- Battery records: `/private/tmp/smartgrid-srr-heat-battery.json` and `/private/tmp/smartgrid-srr-heat-post-battery.json`
- Stackshots: `/private/tmp/smartgrid-heat-stackshot.json`, `/private/tmp/smartgrid-heat-stackshot-2.json`, and `/private/tmp/smartgrid-heat-stackseries-20260912.jsonl`
- Final state: `/private/tmp/smartgrid-srr-heat-final-status.json`

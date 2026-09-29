# Clock, lifecycle and combined history extension — September 11

User approved all three proposed additions. Implemented, reviewed and signed in the existing diagnostics worktree. No live device access, deployment, playback or new recording occurred. Main checkout and desktop app remain untouched. [Current build provenance](2026-09-11-clock-lifecycle-build.json) supersedes the earlier native-probe artifact for the next iPad test.

## Retained experiment

Normal DSP/UI/MIDI, zero requested inputs,48k/512, autoplay. Metadata capture on; PCM capture off. SMARTGRID_NATIVE_CAPTURE=0 disables the extra capture for an overhead control. Prior pristine internal recording and missing-callback evidence remain the leading sporadic-failure context. Timestamp gaps/scalars do not establish damaged samples or physical oscillator behavior.

## Clock data

Native binary SGRAW002 carries252-byte headers with rateScalar. Its Apple validity flag is preserved; absent/invalid/nonfinite values are unavailable, not0or1. Decoder accepts old SGRAW001 too. clock-trend.csv records the host/sample timestamp relationship and scalar; clock-trend.svg is a bounded overview with at most8192points and explicit sampling. Segments restart on invalid/nonmonotonic timestamps, unit/rate changes, sample discontinuities and capture loss. Exact counts/extrema remain; quantiles/plot are explicitly sampled. Actual iPad scalar availability remains unverified. CSV is the full trend; uniform plot sampling can miss a brief event.

## Lifecycle data

The native writer produces a companion .sgraw.lifecycle.jsonl using the same mach clock/JSON wall anchor as callbacks. Fields: sequence is attempted-event identity (concurrent producers can enqueue out of sequence); ticks is capture/receipt time; unit is native pointer identity; event names the operation; value is the requested value or notification reason; status is the returned OSStatus/NSError code where available; phase0is notification receipt,1call entry,2call return. Sort by ticks to correlate timing, preserve original file order as provenance. Queue/drop/discard/write counts are in .sgraw.json. A unit pointer may be reused; use lifecycle and timestamp resets when segmenting.

Hooks preserve original JUCE calls: AudioOutputUnitStart/Stop, AudioUnitInitialize, AudioComponentInstanceDispose. Session hooks record active state, category/options, preferred rate and buffer duration (microseconds), with returned BOOL/NSError results while retaining original logNSError behavior. Existing JUCE handlers record interruption type, route-change reason and media-service loss/reset receipt before their normal handlers run. App open/close/configuration-restart events provide origin context. App.open status -1 means initialization error, -2 means missing/wrong-rate device; its existing app log retains the reason text. App close/restart return status0means the void method returned, not proof of successful hardware recovery; inspect nested events.

Notification timestamps are when JUCE receives the notification, not guaranteed hardware event time. Existing periodic session/route snapshots supplement these scalar lifecycle records; previous-route object payloads and exact underlying hardware occurrence times are not captured. Any early JUCE discovery before StartRemoteIOCapture is outside capture coverage.

A separate1024-entry owned-record queue accepts multiple producer threads without waiting, allocation, logging or I/O. The existing writer drains it; file cap16MiB, explicit failures/loss. Only administrative start/stop may wait, outside audio processing. Review caught an idle-to-stop drain race; final writer observes stop before its last empty check and drains close events. Hardware startup/shutdown coverage and overhead still need preflight.

## Combined kernel history

[Reusable helper and exact invocation](historical-context/JUCE/SmartGridOne/scripts/combined_system_trace.md). Same eleven previously validated USB/audio/scheduler/workgroup/trace filters. Enforced request<=10seconds, total payload<=300MB, drain<=90seconds, bounded startup/stop and message count. Complete received raw chunks retained, plus initial stackshot/notices and timing metadata. No automatic fault trigger or rolling buffer. Earlier ten-second requests retained~9.7seconds and had~42–49second drain gaps; history exists only inside each acquisition, not throughout an hour. New caps/cleanup are offline tested and need live preflight. The offline CLI cannot open a device connection.

No newly exposed driver queue depth, per-transfer completion fields or raw wire packets are claimed. The USB-only analysis window can be widened without throwing away the combined source trace. Align native events using the stackshot mach/wall pair, never host transfer receipt times. Keep the K-Mix analog recording and app logs throughout normal playback; avoid device file queries during acquisition.

## Validation and next action

Signed iOS Release build and host signature verification passed; shared JUCE source unchanged and generated module hash verified. Four source-hook tests and14decoder tests pass. Native record tests6cases/716assertions and actual C++→Python v2payload roundtrip passed. Lifecycle queue6cases passed with ASan/UBSan and ThreadSanitizer; wrapper regression1case/30assertions preserves call count/status/order. Independent review and deterministic shutdown interleaving check passed. Combined helper11tests and USB analysis9tests pass. A flaky helper timeout test was replaced with controlled helper-clock progression; production bounds unchanged.

Next iPad session: install/launch, verify audible normal transport,48k/512,zero app inputs,MIDI output, readbacks, rate-scalar availability and zero capture loss; assess new probe cost before interpreting a measured interval. Then record analog plus native metadata/lifecycle, retain bounded combined system bursts, retrieve afterward and correlate the first error with preceding events. Do not interpret a quiet unobserved interval, missing scalar or dropped trace records as evidence of normal USB behavior.

# Final measurement evidence review

**Verdict: The runtime measurement gate is complete and supports the reported outcome, with one metadata-label correction. The MIDI worker change did not eliminate the long audio interruptions. No causal thermal remedy is established.**

Reviewed the nine requested JSON artifacts for `/private/tmp/smartgrid-ipad-midi-sysex-worker-20260910`, selected raw app-log MIDI/thermal records, and the narrow source mapping for the thermal fields. This follow-up does not repeat the completed source review.

## Findings

### Minor — distinguish nominal recording end from wrapper completion

File: `/private/tmp/smartgrid-ipad-midi-sysex-worker-20260910.window.json`, fields `ended_local` and `ended_epoch_ns`.

`ended_local` is the nominal recording end, `2026-09-10T15:25:41.652355-07:00`, exactly 960 seconds after the recorded local start. However, `ended_epoch_ns` decodes to `2026-09-10T15:25:41.998906-07:00`, near wrapper completion, and subtracting `started_epoch_ns` gives 960.34653 seconds. The two end fields therefore denote different events despite their parallel names.

The supplied correlation and thermal summaries use the nominal local end and 960-second duration, so this does not change their findings. Rename or explicitly label the epoch field as wrapper completion, or retain it separately while supplying a nominal epoch end. Do not silently mix it with the nominal recording window in later analysis. The existing independent-clock/start-latency limitation still applies after that labeling correction.

## Verified conclusions

- **Recording and continuity:** `.window.json` reports successful recorder exit and exactly 960.0 seconds of WAVE frames. `.end-verification.json` identifies the same app log as still latest and reports unchanged configuration and patch hashes. This evidence supports a preserved measurement; it is not itself a sample-accurate timestamp alignment.

- **Long failures remained:** `.correlation.json` contains 23 rows with both callback and waveform matches, 23 unique callback identities, and 23 unique waveform intervals, each surrounded by signal. Its totals report 23 MAYA transaction failures and 23 audio restart records. `.callbacks.json` contains 23 matching long callback gaps, ranging from 217.146 to 230.057 ms, and native xrun increments exactly 1 through 23. All 23 increments have distinct callback identities. The last is at approximately 15:21:37.331, recording second 715.679. These are measured failures after moving normal MIDI submission to the worker; an audio-fix claim would contradict the evidence.

- **Waveform widths are method-dependent:** The 23 refined intervals have duration minimum/median/maximum 222.5625 / 226.0833 / 230.7083 ms. Their duration arithmetic is internally consistent. These wider low-variation intervals include analog settling around the stricter near-noise cores; do not substitute their width for either callback-gap duration or the near-noise threshold result without naming the method.

- **No dense periodic episodes were detected:** `.periodic.json` contains 16 short candidates, and summing its 96 ten-second windows independently gives 16. Every ten-second window has at most one candidate, and its dense-episode list is empty. This supports absence of the previously scored dense pattern under the stated detector. It does not establish click-free audio or rule out other subtle corruption; the 23 long interruptions remain separate findings.

- **Worker activity and queue claims agree with raw records:** Independently extracted the 955 app-log MIDI diagnostics from wall-clock seconds 15:09:41 through 15:25:41. Their counter ranges and monotonicity match `.midi-validation.json`. Enqueued and submitted both rise from 2,333 to 42,252, a delta of 39,919. All sampled enabled/running values are 1. The sampled SysEx depth ranges from 0 to 2; the basic depth ranges from 0 to 5. Full, invalid and deliberate-discard counters remain zero. Diagnostic wall-clock stamps have a maximum adjacent gap of two seconds, consistent with low-rate observation rather than one sample at every exact second. Report 39,919 as the delta between edge diagnostic samples, not as a sample-exact recording-boundary total. Submissions count completed handler calls, not hardware acknowledgments; sampled depth is not a queue high-water mark, and the existing basic queue has no overflow counter.

- **Callback format remained constant:** The callback evidence covers 89,540 callbacks, all at 48 kHz and 512 frames, without sequence holes or logged logger-miss lines. Low-power and muted state remain zero. Recorded instrumented DSP duration has median/p99/max 4.712 / 5.120 / 7.273 ms with no measured overruns. This only measures the instrumented region; it excludes later callback logging and outer framework/native work.

## Thermal interpretation

The source mapping in `JUCE/SmartGridOne/Source/AudioPlatformDiagnostics.mm:16` takes `NSProcessInfo.processInfo.thermalState` directly, with the diagnostic mapping 0=nominal, 1=fair, 2=serious, 3=critical. `MainComponent.cpp:523` updates that value at roughly one-second intervals, and callbacks log the cached value. Thus the callback segment boundary is the observed update time, not an exact physical thermal onset.

Raw app records show nominal at 15:21:46.323 and serious at 15:21:47.326. `.thermal-analysis.json` divides the run into 67,572 nominal callbacks with all 23 long gaps and 21,968 serious callbacks with no long gaps; those segment counts sum to 89,540. The serious segment lasts approximately 234.326 seconds. Its boundary is about 9.995 seconds after the last long callback gap, so the ordering is clear despite the clock-fit residual of about 14.35 ms.

The selected system records corroborate a policy event in the same second: thermalmonitord logs pressure level 20 at 15:21:47.026262, and audiomxd at 15:21:47.027193 logs a handler for pressure level `Moderate`, followed by `disengaging Thermal mxCoreSession`. That is about 299 ms before the app's sampled serious-state marker. The system's `Moderate` pressure label and the app's `serious` NSProcessInfo state belong to different reported scales; they must not be presented as contradictory values of one enum. The message does not establish what changed in the physical audio path or that it caused the subsequent clean interval.

The supported observation is therefore: all long failures occurred before the sampled thermal transition, and none occurred during the remaining roughly 234 seconds while SysEx transport continued. This temporal association is a new variable to investigate, not proof that heating, throttling, cooling, or that policy message fixed the transport. The transition also means the exposure did not remain thermally nominal throughout.

## Timing and review limits

Correlation uses a conservative 750 ms matching neighborhood. Reported callback-resume minus WAVE quiet-end offsets span approximately 96.068–135.070 ms and are not causal USB recovery-latency measurements. The clock-fit residual, independent recorder/device clocks, recorder startup latency and second-resolution MIDI boundary selection must remain visible in the final report.

This was a bounded evidence review. No tests, builds, code edits, device/service queries, process actions, capture actions or Git mutations were performed. The only write was this report. Physical acknowledgment, deliberate saturation recovery, clock jitter under forced load and graceful shutdown remain outside this measurement's validation, as in the source review.

## Follow-up: timing-label finding resolved

**Narrow verdict: resolved.** Compared the corrected `.window.json` with `.window-before-epoch-clarification.json` and the raw recorder `.json`. Both ISO recording boundaries are unchanged. The corrected `started_epoch_ns` and `ended_epoch_ns` exactly match those ISO timestamps at microsecond precision, and their difference is exactly 960,000,000,000 ns. The original independently sampled epoch values are preserved unchanged as `wrapper_started_epoch_ns` and `wrapper_ended_epoch_ns`, matching both the before-copy and raw recorder metadata. `wrapper_ended_local` still matches the original completion time. The only changed fields are the two nominal epoch fields, the two added wrapper epoch fields, and the explanatory timing note.

Read `/private/tmp/smartgrid_finalize_trial_window.py` without executing it. Its finalization logic retains the original wrapper epoch/completion fields, derives both nominal epochs from the ISO start and WAVE-duration-derived end using integer timedelta arithmetic, and records the distinction in the timing note. This resolves the identified labeling mismatch for this artifact and in the helper's generation logic. Recording ISO bounds and duration are unchanged, so the earlier measurement conclusions stand; no broader analysis was rerun. This follow-up only read the requested metadata/helper and appended this resolution to the review.

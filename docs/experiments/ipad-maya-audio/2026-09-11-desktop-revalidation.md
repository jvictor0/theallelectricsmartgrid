# Desktop revalidation, September 11, 2026

## Protocol and live state

User left Mac connected to same Satechi hub, WRLD.BLDR, MAYA44 USB+, and PD powering hub, and requested extended artifact revalidation with same patch. K-Mix remains on a separate Mac USB bus; analog return recorded on inputs 3/4.

Capture started **13:41:34 PDT**, planned four consecutive one-hour files, ending approximately 17:41 PDT. Small capture gaps between files must be excluded. SmartGrid is not restarted between files. Runner stops capturing after four hours and leaves the app running.

Source of truth: `/private/tmp/smartgrid-desktop-revalidation-20260911/state.json`. Each `hour-NN` has WAV, JSON timing/process metadata, system log, stderr and wrapper log. Configuration, patch and device snapshot copied into same directory. Free disk was 43 GiB before test. Runner refuses another segment below 5 GiB free.

## Build and transport

User explicitly authorized `make run` in main checkout. Built successfully from `/Users/joyo/theallelectricsmartgrid/JUCE/SmartGridOne`, main commit ba0fe08cce29b20861545e3779ef08b914f9d452. Normal DSP and UI, WRLD.BLDR MIDI input/output enabled, MAYA input/output configured, 48 kHz / 512 frames confirmed in app startup logs. Binary SHA256 73aef3433ab3478fdafe323112576a20641f49dd80d886319ecab24276b55679. Patch `ms/2026-09-08T21-06-40.json`, SHA256 a0e797a87de665e300dcf8ff484774ee93639c8c7d425a67ddc0aabb2223952a.

Initial GUI launch hung waiting for Documents permission; Mac locked. Stalled PID53829 terminated before successful CLI launch. This is excluded from recording exposure. CLI launch PID56289 loaded patch successfully. Transport initially stopped: first probe measured only -87 dBFS RMS. One standard MIDI Start (0xFA) injected through CoreMIDI into the existing WRLD.BLDR source started playback; second probe and long-capture first 8 seconds measured ~-31 dBFS RMS and ~-22 dBFS peaks. Thus no autoplay code change was necessary. Helper `/private/tmp/smartgrid-midi-start-20260911.c` documents exact event. No source changes made on main; pre-existing iOS project modification left intact.

App log `/Users/joyo/Documents/SmartGridOne/logs/2026-09-11T13-38-43-238.log`; make output `/private/tmp/smartgrid-desktop-main-make-run-20260911.log`. This main build reports ordinary DSP xrun messages but does NOT have the iPad experimental per-callback/thermal tracing. Do not infer absent callback gaps or thermal states from unavailable fields. One startup xrun 19.232 ms occurred before capture; exclude from steady-state results.

## Evaluation

Monitor recorder health, analog continuity and disk space. Evaluate both sporadic interruptions and periodic partial-buffer corruption. Compare waveform candidates with system USB/CoreAudio events and app xrun messages; distinguish musical transients/silence and capture faults. No artifacts detected is evidence for this exposure only. First probe only establishes working signal, not stability.

Heartbeat `check-desktop-hour-audio-test` updated to active every 10 minutes, quiet when healthy, analyzing completed segments and notifying on meaningful faults or final completion. Stop capture safely if sustained silence/failure is found. Final results pending.

## Hour 1 analysis

13:41:34–14:41:35 PDT: WAV contains exactly 3,600 seconds at 48 kHz stereo/24-bit. Recorder stderr empty, zero steady app xrun messages, zero matching USB/CoreAudio transaction/overload/restart/stall candidates. Zero clipped samples. No near-flat runs at least 1 ms; two 0.5–0.5625 ms runs, with no evidence of recurring blanking. Per-second RMS range -31.65 to -30.68 dBFS; no sustained attenuation. Difference-energy and sample-step screens produced no identified artifact; maximum single-sample step 0.06093 may be normal patch content and is not labeled a glitch. These are waveform screens, not a sample-exact clean-reference comparison; tiny clicks or non-blanking corruption are not categorically excluded.

Saved analysis: `/private/tmp/smartgrid-desktop-revalidation-20260911/hour-01-analysis/summary.json`, per-second metrics and near-flat run list. Reusable scanner: `/private/tmp/smartgrid-desktop-revalidation-analyze.py N`. Hour 2 is recording with actual audio verified around -31 dBFS RMS. No action required.

## Hour 2 analysis

14:41:35–15:41:35 PDT: exactly 3,600 seconds recorded. No capture stderr, steady app xruns, clipped samples, or near-flat runs at least 0.5 ms. Per-second RMS -31.64 to -30.68 dBFS; waveform screening found no identified sporadic dropout or periodic blanking. Maximum sample step 0.06049, comparable to hour 1.

At 15:33:47 the kernel explicitly killed idle `usbaudiod` (memorystatus, reason 9), followed by an XPC session restart and new daemon PID. Surrounding analog signal continued, with no matched app xrun/transaction error/blanking. This is a service-lifecycle event, not demonstrated audio-stream failure. A second broad search hit, `NSTallLocalizedStrings`, is a false positive. Preserve both raw candidates with interpretation in `hour-02-analysis/summary.json`. No new artifact to notify. Hour 3 recording verified healthy.

## Hour 3 analysis

15:41:36–16:41:36 PDT: exactly 3,600 seconds captured; no recorder stderr, steady app xruns, clipped samples, or near-flat runs >=1 ms. Three near-flat runs >=0.5 ms; longest 0.604 ms. No identified recurring blanking. Per-second RMS -31.66 to -30.68 dBFS, maximum sample step 0.05997, comparable to preceding hours.

At 15:57:44 there were 14 control-endpoint (0x00) stalls, status 0xe0005000, across MAYA/WB/K-Mix and many unrelated USB peripherals within ~22 ms. Exact request not logged. These differ from the iPad input-stream transaction error status/endpoint. Around that event the analog RMS and step metrics remained in normal range, with no blanking >=1 ms or app xrun. At 16:25:21 another usbaudiod XPC service restart likewise had no matched waveform interruption. Neither event is classified as an audio artifact. Raw candidates and interpretations retained in hour-03-analysis/summary.json.

Hour four is recording with analog audio verified around -31 dBFS RMS. Monitoring cadence is now 30 minutes per user request.

## Hour 4 and final result

16:41:36–17:41:36 PDT: exactly3,600seconds captured; no recorder stderr, steady app xruns, clipping, or near-flat run >=1ms. One0.542ms near-flat run. Per-second RMS -31.68 to -30.68dBFS; max sample step0.06226, comparable to earlier hours. At16:59:04 and17:38:12 the kernel killed idle usbaudiod reason9, followed by XPC restart. Surrounding analog metric windows show no matched interruption; `NSTallLocalizedStrings` remains a search false positive. Window metrics and interpretations saved with raw candidates.

All four one-hour files are complete:14,400seconds of analog exposure, with small file handoff gaps excluded. No identified sporadic dropout or recurring blanking; no steady app xrun or capture error. No near-flat run >=1ms across the four files. This is strong additional evidence for desktop stability under this patch/topology, not proof of universal safety or absence of every subtle click: waveform screens lack a sample-exact clean reference, and this main build lacks per-callback tracing. SmartGrid remains running. Recording is complete; the recurring monitor is being removed after reporting.

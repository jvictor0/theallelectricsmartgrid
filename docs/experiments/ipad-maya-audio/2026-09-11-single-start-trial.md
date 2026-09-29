# Single-start RemoteIO lifecycle experiment

Authorized by user: remove the start/stop/configure sequence and run again. Existing diagnostic worktree, JUCE8.0.15,48k/512,zero inputs,normalDSP/UI/MIDI/autoplay, same patch/hardware/power. No wavetable or worker changes.

Hypothesis: the temporary RemoteIO and session cycling contributes to subsequent driver failures. Constructor selects final Playback category, submits48k and512/48k seconds before one activation, reads hardware information and leaves session active. open configures and starts real RemoteIO without another activation or category transition. Remove the temporary-unit wait. Output-only experimental path explicitly rejects nonzero inputs. Shared JUCE untouched; version-pinned app-local generated source only. Expected lifecycle: one activation, one initialize, one start, no disposal/deactivation before measurement. Existing rate/buffer helper remains to verify/read negotiated state; unexpected extra preference changes cause preflight gate failure.

Plan/checklist:
- [x] Baseline lifecycle gate fails for multiple starts/activation/deactivation and late preferences.
- [x] Build app-local source with --single-start, validate signature and preserve binary/source hashes.
- [x] Upgrade over Wi-Fi preserving patch/config. Start K-Mix3/4 capture before app launch.
- [x] Preflight: lifecycle gate, real48k/512, MAYA output4/input0, full workload, nominal thermal, analog signal. Do not call elevated startup a matched nominal trial.
- [x] Measure900seconds without device polling/profiler, stop recorder, retrieve native/app/lifecycle prefixes and system archive.
- [x] Correlate USB failures, restarts, callback gaps and analog long/periodic damage. Record thermal coverage; distinguish startup from steady playback. Clean result requires repeat/reversal, failure establishes this lifecycle is not necessary.

Raw root:/private/tmp/smartgrid-single-start-trial-20260911. Baseline:/private/tmp/smartgrid-explicit48-cooled-trial-20260911-r1. No changes to main. Runtime lifecycle gate is the meaningful integration test; source snapshots and native capture preserve the actual executed sequence.

## Deployed and measuring

Binary 6194cc7925c25f4b269eca878dc90741aff2e47cbc8a4bbab982ad1d5de9f646. Signature/build passed; patch/config SHA256 unchanged. PID30652. Single-start lifecycle gate passed, one activation/initialize/start, preferences before activation, no dispose/deactivation/PlayAndRecord. Native metadata has no capture loss. Last200callbacks nominal thermal0, real48k/512, zero inputs/four MAYA outputs,625SysEx submitted without queue faults at preflight. Analog prestart-43.18dBFS. Measurement started 2026-09-11T20:55:04.920398-07:00 for900seconds. ControllerPID77724 automatically stops analog, retains coherent diagnostic prefixes and collects system archive. App remains running. Startup xr1 retained separately, not labeled USB error without archive correlation. Real close/interruption can still reactivate session; this test removes initial redundant cycling.

## Completed

Nominal throughout;22MAYAerrors in20restart/native/longanalog clusters.24reviewed isolated short plateaus, no rapid periodic cluster detected. Startup lifecycle intervention verified but sporadic failure persists. [Full result](2026-09-11-single-start-result.md). Recording and archive complete; app left running. Completion heartbeat removed after reporting.

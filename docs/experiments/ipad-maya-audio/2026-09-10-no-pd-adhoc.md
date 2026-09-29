# September 10: spontaneous failures with PD disconnected

The user disconnected the charger from the hub **before** the normal-DSP app launch and reported many glitches with no interaction. Archived system and app logs confirm seven MAYA transaction-error → audio-driver I/O restart → callback-stall episodes during approximately 170.39 seconds of playback. The iPad remained nominal, with sampled external power and charging both off. PD pass-through is therefore not necessary for the established sporadic failure chain.

## Condition and observation boundaries

- Same installed JUCE 8.0.15 diagnostic binary, SHA-256 `a9902d35b72d13556c831daa1c30c71eb6b7429cf76408e35042952483a5f873`.
- Normal DSP, UI, autoplay and MIDI send worker enabled explicitly. App inputs remain zero, outputs four. Saved patch/configuration unchanged.
- iPad powers the same hub/MAYA/WB bus; charger absent before launch, not disconnected during playback.
- Launch request 17:51:26.285036 PDT; first callback 17:51:33.473278. Observation ends 17:54:23.863692, **before** diagnostic traffic. Archive collection began 17:54:29.329; none of that collection period counts toward this result.
- No analog recording. This establishes logged interruption episodes during the user's glitchy session, not exact waveform alignment, total audible glitch count, or absence of periodic distortion.

## Failures and callback timing

All seven kernel failures name `MAYA44 USB+` endpoint `0x82`, status `0xe00002ed (transaction error)`. Each is followed by an `audiomxd` restarting-I/O record and a long callback arrival gap. Five additional restart records precede playback and are classified as startup configuration, not spontaneous failures.

| USB transaction error, PDT | Driver restart, PDT | Returning callback | Callback gap, ms |
| --- | --- | ---: | ---: |
| 17:52:03.397 | 17:52:03.430 | 2809 | 220.224 |
| 17:52:13.872 | 17:52:13.907 | 3771 | 225.118 |
| 17:52:49.356 | 17:52:49.392 | 7078 | 224.290 |
| 17:53:06.900 | 17:53:06.935 | 8703 | 217.135 |
| 17:53:11.224 | 17:53:11.258 | 9089 | 217.482 |
| 17:53:32.868 | 17:53:32.904 | 11098 | 229.398 |
| 17:53:40.044 | 17:53:40.079 | 11751 | 221.291 |

15,836 callbacks were recovered without sequence holes or logger-miss reports. Actual rate was 48 kHz throughout, with initial 470-frame callbacks settling to 512. Native sample-discontinuity counter rose 0→7, matching the seven stalls. Measured DSP duration median/p99 was 4.119/4.986 ms. The only measured processing overrun was the first initialization callback (18.328 ms at 470 frames); none occurred during settled playback. The largest processing duration in the 100 ms preceding any stall was 4.974 ms, below the 10.667 ms budget. These timings cover the instrumented processing region, not subsequent callback logging or surrounding native work.

The MIDI worker was enabled and running in all 170 sampled diagnostic lines, with no queue faults/discards and 7,079 additional SysEx handler submissions. Handler calls are not physical output acknowledgments. One driver report counted two zero-length input transfers at 17:52:00.550; drift reports are preserved in the result JSON.

## Thermal and power observations

The first four callbacks had unknown thermal state during initialization; all subsequent 15,832 callbacks reported **nominal (0)**, including all seven stalls. Low Power Mode remained off. No thermal-pressure transition was recovered in the observation window.

Nine `symptomsd` power snapshots consistently report `battery-power-connected 0` and `battery-charging 0`. Battery temperature raw values rose 3179→3319, approximately 31.79→33.19 °C **if** these private fields use hundredths of a degree. This is battery telemetry with unverified scaling, not a measured SoC, hub or MAYA temperature. Battery voltage fell 4261→4188 mV; reported screen brightness remained 600000.

## Interpretation and next decision

This is a positive no-PD reproduction of the same sporadic USB/restart/callback-gap chain seen with PD present. It weakens a charger/pass-through-specific explanation and rules out PD charging as a necessary condition for these interruptions. It does not exclude shared hub/MAYA power integrity, USB-host/driver behavior or workload-dependent scheduling; the iPad now supplies the peripheral rail. Nor does this short unmatched run establish a rate difference between power sources or resolve periodic blanking.

A repeated test whose sole question is whether failures can occur without PD is now redundant. The proposed induced-Serious/nominal crossover remains informative; comparing that policy response under the two power arrangements is still a separate, unperformed interaction test. No thermal override, rebuild, new recording or automatic experiment was started in this analysis.

## Evidence

Raw artifacts use `/private/tmp/smartgrid-ipad-no-pd-adhoc-20260910-1751`: `.logarchive`, `.app.log`, `.system.json`, `.window.json`, `.verification.json`, `.end-verification.json`, `.callbacks.json`, `.midi-validation.json`, `.collection.json` and `.result.json`. The durable [result JSON](summaries/no-pd-adhoc-20260910-result.json) contains the exact matched records, prior-processing checks and all power snapshots. Raw file and archive hashes are recorded in the [evidence manifest](evidence-manifest.json).

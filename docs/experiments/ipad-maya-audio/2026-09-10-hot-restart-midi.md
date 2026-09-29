# Same-binary hot restart and MIDI-output check — September10,2026

Status: **completed and analyzed; user confirmed WRLD.BLDR LEDs work after restart.** K-Mix3/4 capture started2026-09-10T16:14:25.249392-07:00 for960seconds, expected finish2026-09-10T16:30:25.249392-07:00. First45.965seconds have music around-32/-30dBFS and no long near-silence candidate. This is preliminary analog evidence, not a completed or fully scored trial.

The user requested MIDI output restored and the current test redeployed to see whether glitches return immediately. They then clarified that input works but WRLD.BLDR LEDs are not updating. The installed worker was already enabled and submitting about42 SysEx messages per second. Configuration still named WRLD.BLDR as both input and output. Those handler-call counts do not establish physical delivery, and the missing LED output introduces a possible physical-MIDI-load confound into the preceding quiet interval.

## Intervention and verification

Preserved the existing session's full fixed-size log through16:11:48:55,225,532bytes, lastcb354128, xr23, thermal2, unmuted48k/512, samePID19365. Queried supported device conditions without enabling any. All condition groups report inactive. The same reviewed IPA was then upgraded in place16:12:50–16:12:59; config and patch hashes were identical before/after. Binary SHA256 remainsa9902d35b72d13556c831daa1c30c71eb6b7429cf76408e35042952483a5f873.

Launched at16:13:45 with normal DSP, UI rendering, autoplay and MIDI worker enabled. PID19797, log2026-09-10T16-13-39-784.log. Actual MAYA48k/512, zero app inputs/four outputs, internal clock, JUCE8.0.15 verified. Startup included470frames then200consecutive512-frame callbacks. The iPad was already serious(2) at startup. SysEx submitted7→213 in six samples, no full/invalid/discard events; physical LED observation is still required. Screenshot shows normal active UI and100% battery with a charging icon; this does not measure charging current or prove PD power was unchanged.

No application source or MIDI routing was altered for this redeployment. Restart reopens endpoint handles and sends the existing handshake. It tests whether that restores MIDI and whether audio failures return while serious thermal state persists. It does not isolate thermal policy. If LEDs remain unresponsive, do not label this a confirmed physical-MIDI-on exposure; investigate the MIDI path before interpreting a quiet interval as a completed control.

## Capture and follow-up

Recorder wrapperPID29458, SoXPID29465, execsession96221. No iPad service queries, screenshots, conditions, builds or configuration changes during the measured capture. Preserve any user steering and exact observed LED state. If no steering changes the protocol, finish960seconds, collect archive/app log, and score sporadic USB/restart/callback/analog failures and periodic sub-buffer damage independently. Thermal may remain serious; do not impose a nominal gate that would discard this deliberately hot-start condition.

[Start metadata](summaries/hot-restart-midi-20260910-start.json). [Thermal control research](research/2026-09-10-thermal-controls.md). [Prior quiet intervals](2026-09-10-thermal-quiet-history.md).

## Completed result and startup correction

The960-second recording finished successfully at16:30:25 with empty stderr. It contains no long near-silence interval and no dense periodic episode. The89,998 callbacks are all48k/512, serious thermal2, unmuted, with no sequence hole, logger miss, xr increment or gap above20ms. DSP median/p99/max4.330/5.131/5.231ms; max arrival gap10.690ms. Native xr stayed2. System archive records no MAYA transaction failure, driver restart, zero-length transfer or ioDrift report inside the measured window.954 MIDI samples show running worker and40,092 SysEx handler submissions, queue depth0–2, no full/invalid/discard count. Three isolated short candidates were inspected and do not establish the prior periodic corruption.

**There were two startup failures before analog recording:**16:13:56.119 and16:13:58.047 MAYA endpoint0x82 transaction errors each match a driver restart and217–219ms callback gap. Thermal was already serious. They occurred during the startup verification period, which included Wi-Fi service traffic and a screenshot; no K-Mix recording covered them. Separately, the first initialization callback used470frames and17.296ms DSP time. Thus the16-minute measured interval was quiet, but the whole restarted session was not failure-free.

The user confirmed LEDs are working in this build. The previous session's absent LEDs remain a historical confound; this confirmation does not retrospectively prove that traffic. [Completed data](summaries/hot-restart-midi-20260910-result.json).

The user shortened the subsequent planned hour to **five additional minutes**, then requested cooling and the same-build repeat if clean. Continuation uses the historical filename prefix smartgrid-ipad-hot-hour-20260910 but its explicit stop request is300seconds; actual WAV duration will be retained. No source/configuration changes or thermal overrides are authorized as part of this comparison.

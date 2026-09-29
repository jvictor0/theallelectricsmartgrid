# Cooled explicit48 enumeration trial: completed result

The change did not eliminate either symptom. All startup preferred-rate requests were48000; there were no exploratory4000/192000/intermediate requests. Fifteen measured minutes produced30MAYA endpoint0x82 transaction errors(status0xe00002ed),30HAL restarts,30native callback gaps, and30corresponding long analog interruptions. A brief periodic near-flat stereo burst also remains. No further build, deployment or experiment was started during analysis.

## Exposure and controls

Measured20:09:13.144833–20:24:13.168596PDT,900.023763seconds. Archive collection completed20:25:14. Recorder returned0. Startup was recorded before measurement; native prefix extends20:08:27.479–20:24:14.156. The prior interrupted Serious-state pilot is preserved separately and not included. App stayed on the same explicit48 binary7de6c81de5ba5012ce3f97617fbb07b6deff655418e22877dbcaebdf7e21c86f with normalDSP/UI/MIDI,zeroappinputs,48k/512 and unchangedpatch/config. No induced thermal condition or kernel-profiler bursts.

All83,927measured native callbacks correlate to nominalthermal0 app records. Native frame count512/rate48000 throughout. No measured nativeprocessingoverrun, callback-lock failure, missing application callback or nonzero render return. Processing min/median/p99/max3.555/3.952/5.000/5.162ms against10.667ms budget. No lifecycle event during measurement;36startup events retained, only48000 preferred-rate request. Unit pointer constant. Native capture88,296records,zero sequence holes/reported drops andzero queue/overlap/lifecycle loss. Expectedstatusrunning describes a downloaded coherent prefix while the app remains running, not a stopped capture.

## Long and periodic waveform findings

Thirty long intervals survive all three1ms-RMS thresholds. Strict8samplelocalvariation flat interiors last171.542–180.333ms (median173.948ms); nativeentry gaps165.713–176.884ms. Ordered native/USB/restart/analog events match, including close clusters. A shared offset fit of analog end edges to native resumes has residual−7.355to+14.618ms. Recorder first-sample host timestamp is unavailable; do not interpret fit offset as hardware latency.

The conservative flat scan yields78short candidates in the full measured overlap; these have NOT all been individually adjudicated and are not claimed as78confirmed artifacts.44candidates cluster within WAV301.760188–302.955854seconds, about1.196seconds, immediately before the long dropout beginning nearWAV302.956seconds. Visual inspection confirms abrupt common stereo near-flat plateaus in the cluster, predominantly at21.33ms cadence (every two512frame periods), with some additional/missed detections. Strict interiors0.25–1.417ms. Broad native window around the cluster has119callbacks andmaximumprocessing4.977ms. This reproduces periodic sub-buffer blanking without a measured fullcallback-duration overrun; no outgoingPCM was captured, so it does not localize the damaged samples.

Figure: [Periodic cluster](figures/explicit48-cooled-periodic-20260911.png).

## Clock limits and interpretation

Named system USB report walls are198.623–209.402ms before the paired native resume walls; unlike the previous run, they also precede prior nativeentry by26–36ms. These mappings are not a common-clock execution trace. Preserve the discrepancy and use ordered event/interval correspondence only; no claim that callback execution precedes/follows first raw USB failure is made for this run. No kernel bursts were acquired, so this run supplies no new scheduler-state measurement.

Failure incidence about2.00/minute versus1.83/minute in the earlier nominal baseline. There is no obvious suppression, but unequal exposure/profiler conditions and a single comparison do not estimate a small effect. More decisively, the change is verified and BOTH symptoms still occur: active rate enumeration is not a necessary trigger. The earlier pristine internal DSP recording remains in force; do not reopen DSPsamplecorruption as the presumed sporadic explanation. Keep48k/512 and the user's desktop-first comparison priority. Temporary RemoteIO/session cycling remains the next proposed initialization dimension; no automatic new change is authorized by this result itself.

Evidence root:/private/tmp/smartgrid-explicit48-cooled-trial-20260911-r1. result.json contains all30matched event rows, thresholds and coverage; final-decoded contains callback/clockdata; analog-analysis contains waveforms, allthresholdcandidates, scriptsandplot; system.raw.json/system-signals.json/system.logarchive preserve driver reports. App left running; recording ended. Completion heartbeat removed after reporting.

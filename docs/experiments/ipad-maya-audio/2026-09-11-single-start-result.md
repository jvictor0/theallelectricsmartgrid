# Single-start lifecycle experiment: result

The change did not eliminate sporadic failures. Twenty long analog dropouts correspond to twenty native callback gaps and twenty HAL restarts; twenty-two MAYA endpoint0x82 transaction errors(status0xe00002ed) occur in those twenty clusters. Eighteen clusters have one error; two have a second error after the restart report (20:56:59 and21:06:27). These are separate reported completions, not twenty-two separate analog gaps.

## Controls and exposure

Measured20:55:04.920–21:10:04.997PDT on2026-09-11,900.077seconds. Binary6194cc7925c25f4b269eca878dc90741aff2e47cbc8a4bbab982ad1d5de9f646. Same patch/config,normalDSP/UI/MIDI/autoplay,zero inputs,four MAYA outputs,48k/512. All84,083measured native callbacks correlate with nominal thermal0 app records. Native processing min/median/p99/max3.837/4.255/4.630/4.802ms against10.667ms block duration; no measured processing overrun, native callback branch failure or render error.

Lifecycle has14events, all startup: final Playbackcategory,48k and512/48k preferences before a singleactivation, singleAudioUnitInitialize and singleAudioOutputUnitStart. No temporary-unit disposal, deactivation, PlayAndRecordcategory or measured lifecycle changes. Same unit throughout. Native prefix87,772records, no sequence holes/decode error/reported drops or writer queue/overlap/lifecycle loss. Recording return0. Archive collection completed21:10:59. App remains running; no next test authorized or started.

## Waveform

Twenty long gaps are robust across all three1ms-RMS thresholds. Strict flat interiors171.708–179.688ms, median174.969ms. Native entry gaps166.462–176.620ms. Ordered matching gives twenty USB/restart/native/analog clusters. Fitted analog end-edge timing residual−7.696to+13.316ms; do not interpret fitted zero as physical latency. No raw kernel common-clock capture, so no fine causal ordering or new scheduler claim.

Twenty-four strict short plateaus were all visually reviewed: abrupt common stereo near-flat intervals, with discontinuous edges,0.292–0.729ms strict interiors. They are isolated; shortest detected spacing0.422seconds. No rapid periodic blanking cluster like the prior1.2second burst was found. This is absence in this finite recording/screen, not proof the periodic symptom is fixed, nor a complete count of every possible short artifact. [Short waveform review](figures/single-start-short-review-20260911.png); [long waveform review](figures/single-start-long-review-20260911.png).

## Interpretation

Prior nominal explicit48 baseline:30errors/30restarts/30long gaps in15minutes. Current:22errors/20restarts/20long gaps in15minutes. Fewer observed events is not strong evidence of improvement from one variable-rate trial; decisive finding is persistence despite verified elimination of temporary startup units/session cycling. This original startup sequence is not necessary for the sporadic failure mechanism. The pure-tone and pristine internal DSP recording evidence remains intact. There were no app-native processing overruns during the measured dropouts. Isolated short plateaus also persist; periodic symptom needs further exposure before drawing a conclusion.

Raw data and scripts:/private/tmp/smartgrid-single-start-trial-20260911. result.json contains all matched event rows. Native decoder, analog screening and finalization scripts retained. The initial matching check intentionally failed when it assumed every error precedes its restart; inspection found second error reports22ms and90ms after restart in two clusters. Matching uses a bounded100ms neighborhood with all22errors accounted for. This report preserves that distinction rather than forcing one-to-one counts.

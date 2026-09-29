# Earlier and current quiet intervals — September 10, 2026

A read-only check at15:53 retrieved the current log tail and the complete earlier11:27 log. **Normal iPad SmartGrid already had a35m08s interval without another native callback discontinuity before the JUCE upgrade and frame SysEx worker routing.** The current unchanged session has a32m09s interval by the latest retained callback at15:53:46. Both intervals span an iPad nominal-to-serious thermal transition. This is stronger repeated evidence for a thermal/OS-policy association, not an isolated thermal intervention or proof that either symptom is fixed.

## Trace comparison

| Condition | Last long callback gap / resumption | Last retained callback | Interval with no further gap over20ms or xr increment | Serious thermal first sampled |
|---|---|---|---:|---|
| Earlier normal/UI-on, JUCE8.0.2, before SysEx worker routing | 11:38:58 | 12:14:06 | 2107.945301s /35m08s | 11:40:52 |
| Current normal/UI-on, JUCE8.0.15, frame SysEx on MIDI worker | 15:21:37 | 15:53:46 | 1929.497696s /32m09s | 15:21:47 |

All times are September10 PDT. Both traces show unmuted48k/512 callbacks, zero app inputs/four outputs, normal DSP/UI and autoplay. The older version is established by the upgrade provenance: its log precedes the first8.0.15 launch at12:14:37, and the upgrade report already identified a tail of that same preceding log as8.0.2. The full log's startup markers now verify normal processing/UI, requested and actual rate/block, and input/output counts.

The historical log contains262,834 callbacks from11:27:21–12:14:06, five xr increments with217–228ms arrival gaps, and no logger-miss lines. Its last failure is at11:38:58, followed by197,625 contiguous callbacks including the resumed callback, with unchanged xr5. After that resumption the largest arrival gap is10.708ms. Thermal serious is sampled114.302s after the last failure; no further failure appears through the log's end.

The current trace joins the previously retrieved complete log and a bounded16MB tail. All63,953 overlapping callback records have identical parsed timing/state fields. The combined trace contains252,758 callbacks, with no logger-miss lines. The quiet interval contains180,895 contiguous callbacks including the resumed callback, unchanged xr23 and maximum subsequent gap10.752ms. Thermal serious is sampled9.994s after the final failure. No relaunch, deployment, setting change, recording, or system-archive collection occurred during this history check.

## What changed and what this establishes

No deliberate app/configuration change occurred when the current session became quieter. However, the last dropout also restarted the audio driver around15:21:37, about ten seconds before the thermalmonitord/audiomxd policy events at15:21:47.026/47.027. We therefore cannot claim thermal state was the only changing condition. The app's serious label is iPad NSProcessInfo state2; no MAYA or hub thermal reading exists. The observed audiomxd message describes disengaging its Thermal mxCoreSession, but the precise consequences have not been established.

The earlier35-minute interval shows that long quiet stretches are not unique to the new JUCE version or SysEx routing. Both intervals becoming sustained around serious thermal state strengthens that lead, while the unequal timings, driver recovery and uncontrolled background state prevent a causal claim. This comparison does not reverse the completed trial result: the worker-routed build first reproduced23 matched USB/restart/callback/analog interruptions in its measured16-minute capture.

**These are quiet callback intervals, not verified fully clean analog recordings.** No corresponding analog capture is established for the full historical interval; the current K-Mix capture ended15:25:41. Periodic partial-buffer corruption can occur with continuous callbacks and unchanged xr, and isolated clicks are not excluded by this trace. The older desktop one-hour control was separately clean by its retained analog/callback metrics, but is a different platform.

[Machine-readable comparison and source hashes](summaries/thermal-quiet-history-20260910.json). [Current measured trial and driver/thermal evidence](2026-09-10-midi-sysex-worker.md). [Historical upgrade provenance](2026-09-10-juce815-comparison.md).

# WRLD.BLDR disconnect/reconnect experiment — September 9, 2026

Seven MAYA USB input endpoint 0x82 transaction failures match seven recorded analog dropouts, seven long audio callback gaps, and seven native xrun increments. Six failures occurred with WRLD.BLDR physically absent. One occurred after it was connected. WRLD.BLDR is therefore not necessary for this failure under the tested configuration.

| Recorded playback phase | Duration | Matched failures | Failures/min |
| --- | ---: | ---: | ---: |
| Controller absent | 244.629 s | 6 | 1.47 |
| Controller connected | 131.260 s | 1 | 0.46 |

This is a short, sequential comparison with only seven events and bursty spacing. The observed rate did not rise after reconnection; the lower second-phase rate does not establish a beneficial controller effect or prove equal underlying rates.

## Timeline and configuration

- 22:20:29.569: MAYA enumerates on hub port 4. No WRLD.BLDR enumeration occurs until 22:25:22.
- 22:20:42: SmartGridOne begins callbacks on MAYA. Requested and actual settings are 48,000 Hz, 512 frames, zero app input channels, and four output channels. Initial sample count is 470 for the first callback; all remaining 38,378 callbacks are 512 frames at 48 kHz.
- 22:21:17.790: K-Mix capture begins, inputs 3–4, stereo 48 kHz/24 bit.
- 22:22:53.946, 22:22:56.729, 22:23:45.916, 22:24:46.507, 22:25:13.079, 22:25:20.691: six MAYA transaction failures with the controller absent.
- 22:25:22.419: WRLD.BLDR enumerates on hub port 1. It has three endpoint-0 setup errors and a failed remote-wake-enable message during attachment. These are controller setup messages, not MAYA playback failures. No associated long audio callback gap or analog dropout occurs at attachment.
- 22:27:23.681: one subsequent MAYA playback transaction failure.
- 22:27:33.664: app enters background; RemoteIO stops at 22:27:33.679 and the app callback log ends. Terminal waveform silence begins here after cross-device alignment. This is excluded from fault counts.
- 22:27:33.702: hardware rate changes to 44.1 kHz after app audio stops. No rate change occurred during the analyzed SmartGridOne playback interval.
- 22:27:35.293 onward: final disconnect error burst; hub hardware-connection-lost at 22:27:35.558. All excluded.
- 22:35:41: recording finalized after handover. Full file duration 862.976 s; playback analysis covers only the earlier session. SoX exit code 0 and empty stderr.

## Correlation and timing

All seven errors fall inside their corresponding callback-pause intervals within 25 ms tolerance. A single linear alignment maps analog gap midpoints onto callback-pause midpoints: offset 0.160842 s, drift 12.743 ppm, maximum residual 0.691 ms. This establishes event correspondence, not hardware-synchronized ordering or the initiating cause.

Analog dropout flat interiors: 273.271–283.396 ms (median 281.417). Callback gaps: 270.969–278.299 ms. Each recovery reports driver lockDelayMS 200. This run's gaps are about 100 ms longer than the preceding output-only run, whose driver lock delay was 100 ms, despite unchanged app input configuration. Recovery delay is therefore not fixed by the app input count.

During capture, 35,061 consecutive callbacks all deliver 512 frames at 48 kHz, unmuted. Measured DSP p50/p99/max: [4.674, 5.114, 5.676] ms against a 10.667 ms buffer budget; no measured DSP overruns. There is one 15.374 ms startup DSP overrun on the very first 470-frame callback, about 35 seconds before capture and over two minutes before the first USB failure. No later measured overrun. Timing excludes native framework work and diagnostic enqueue overhead.

Host thermal state is nominal throughout captured callbacks. Native xrun counter rises 0→7. No callback sequence holes or reported async logger misses. The near-flat detector finds seven playback gaps of at least 20 ms plus terminal stop silence. It does not exclude subtler clicks or prove that all waveform artifacts are detected.

## What this changes

Neither app input consumption nor physical WRLD.BLDR presence is necessary for the reproduced dropout mechanism. The output-only build still causes input-side USB traffic in the iPad driver, and these failures remain on MAYA endpoint 0x82. This does not establish why the traffic exists or identify the faulty component. It also does not rule out controller influence on probability, unrelated MIDI work, hub behavior, or interaction between the app's wider workload and iPad audio scheduling.

The strongest remaining contrast is SmartGridOne versus the clean Drambo and desktop controls. A useful next isolation would preserve the current iPad audio setup while replacing the SmartGrid processing workload with a generated tone, to test whether the full callback workload is necessary. That would be a new build/test; no implementation changes were made for this analysis.

## Artifacts

- Recorded event timeline and waveform: `timeline.png`.
- Matched events: `matched-events.csv`.
- Structured correlation: `correlation-summary.json`.

Preservation note, September 28: these three relative artifact paths were
already missing from the original journal. They were not reconstructed during
the repository import; the reported measurements above are preserved as written.

- Source archive: /private/tmp/smartgrid-ipad-20260909-6.logarchive
- System extract: /private/tmp/smartgrid-ipad-no-controller-system.txt
- App log: /private/tmp/smartgrid-ipad-no-controller-app-logs/2026-09-09T22-20-38-405.log
- Capture: /private/tmp/smartgrid-ipad-no-controller-20260909.wav

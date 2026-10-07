# October 6: eight periodic corruption bursts captured on K-Mix inputs 3/4

The first capture is complete and analyzed. **Eight sustained analog corruption bursts match the user's listening reports, including the coarse episode around 17:00.** Both channels show synchronized short cuts with timing near the configured 512-frame period. The evidence favors a downstream buffer/timing-alignment hypothesis but does not locate the failing component. Clean generated DSP remains established prior evidence; this experiment did not repeat that test. No reset, app restart, deployment, cable/power change or sysdiagnose was performed during that first capture. The user subsequently performed an upstream reconnect during the continuation, recorded below.

A separate 15-minute continuation began at **17:07:33 PDT** after another user report. It is still recording at this journal checkpoint and is expected to end around 17:22:33. Its completion and analysis must be checked separately. The main capture's completed status does not apply to the continuation.

## Captured events

The user requested recording physical interface inputs 3/4 and explicitly identified periodic corruption rather than startup silence. The Mac's K-Mix receives Maya's analog output. The verified main recording contains **1,575 seconds**, stereo, 48 kHz, 24-bit. Recorder launch was **16:38:21.601 PDT** and successful exit was **17:04:36.975**.

| Approximate local onset | Approximate recovery | Spectral duration | Seconds since preceding onset |
| --- | --- | --- | --- |
| 16:39:15 | 16:39:25 | 10.3 s | — |
| 16:41:06 | 16:41:13 | 7.6 s | 111.0 |
| 16:42:34 | 16:42:38 | 3.7 s | 88.2 |
| 16:44:25 | 16:44:33 | 8.0 s | 110.7 |
| 16:46:59 | 16:47:14 | 15.5 s | 153.9 |
| 16:50:22 | 16:50:41 | 19.0 s | 203.3 |
| 16:54:55 | 16:55:14 | 19.3 s | 273.0 |
| 17:00:19 | 17:00:46 | 27.0 s | 323.8 |

These are recorder launch plus sample offsets, rounded to seconds, **not calibrated wall-clock boundaries**. Recorder warnings below add alignment uncertainty. The user initially dictated `4:44:57-ish to 4:55:15`, then explicitly confirmed **16:54:57–16:55:15**. The eighth event was reported as “5:00-ish” and “really coarse.” Earlier reports included approximately 16:35, before the reliable recording began.

The proposed fixed two-minute recurrence is not supported. Later onset intervals lengthen markedly, and later bursts also last longer. A changing relative clock/buffer phase could produce such behavior, but this time series alone does not prove that mechanism or a thermal cause.

**Material change:** the spectral detector also flags intervals after approximately 17:02. The user confirmed starting to jam and changing the material then. Those are unclassified spectral candidates, not additional confirmed glitches. The machine-generated finalizer's `spectral_bursts` list is a detector output and includes those candidates; use the eight-event table above for the user-matched result.

**Unrecorded follow-up:** the user reported another burst at **17:06:13–17:06:38**. This lies after the completed recording and before the continuation started; its waveform was not captured. Preserve the listening observation without presenting it as a measured analog interval.

## What the waveform adds

All eight matched episodes contain broadband disturbance and repeated short near-silence intervals in both channels. A 150 ms comparison from the first and eighth episodes visually shows aligned abrupt cuts and flat sections in both channels. Their similarity supports treating the coarse 17:00 episode as the same observed waveform phenomenon.

The eighth episode lasts **27.0 seconds**, compared with 19.0 and 19.3 seconds for the preceding two. Its detector finds 310 stereo quiet runs of at least 1 ms; their maximum length is approximately **7.02 ms**, versus 7.42 ms in the first episode. Its greater total duration is established; the data do not show uniquely longer individual holes. The count depends on signal content and threshold and is not a count of USB errors, all inserted zero intervals, or a perceptual severity scale.

There is evidence of block-related timing beyond selecting adjacent holes near an expected separation:

- Fold **all detected quiet-run starts** within each second of the eighth episode against candidate periods from 450 to 600 samples, in 0.1-sample steps. Across 23 seconds with at least eight detections, mean circular coherence peaks near **512 samples** (0.810); comparison values are 0.237 at 480, 0.326 at 500, 0.772 at 511 and 0.769 at 513. The peak has finite width and does not establish an exact oscillator frequency. At nominal 48 kHz, 512 samples is 10.667 ms, matching the configured app buffer size.
- In eighth-burst WAV seconds 1320–1325, the start-edge phase remains around sample 325–334 modulo 512 while median detected width grows from about **1.94 to 4.60 ms**. Later, during seconds 1336–1341, the end-edge phase is relatively stable near sample 499–510 while widths contract. The seventh episode has similar growth. These are threshold-derived edge measurements on musical material, not a native buffer trace.

**Working interpretation:** repeated partial blanking tied to a buffer-scale period, with changing cut width, is compatible with two timing/buffer boundaries moving relative to one another. Candidate locations include the native output handoff, iOS USB audio buffering/clock handling, and the Maya side. It is stronger evidence for investigating downstream timing than for revisiting the synthesizer's sample generation. It does not establish which candidate is responsible, prove implicit feedback, or exclude the physical USB path as an initiating factor.

## Underflows: positive evidence, unknown logging completeness

The retained message is `AUAInputTransferManager_completeBlock USB underflow`, emitted by Apple's `usbaudiod` for Maya input transfer manager `0x390`. This describes the **Maya → iPad input path**, while the audible symptom is **iPad → Maya playback**. The negative numeric argument is undocumented in the sources found; do not label it frames, packets, milliseconds or an OSStatus code.

The initial archive contains four earlier input-underflow messages at **16:32:58.580**, **16:34:11.371**, **16:34:11.443** and **16:34:11.483**, before the first capture request. The live stream then contains **612 additional messages** between device times **16:39:44.041 and 16:49:01.112**. The final historical archive contains **the same 612 numeric arguments in the same order**, with rendered wall times approximately 16:39:44.063–16:49:01.149. Do not add the live and historical counts together as separate incidents.

**The user raised an important interpretation correction: repeated driver errors may be throttled.** Agreement between the live stream and historical archive makes loss confined to our live collector less likely, but both can share driver-side suppression or unified-logging loss. No explicit Maya-underflow suppression notice was found, and no public source for this private call site's throttle policy was established. Absence of a suppression notice is not evidence that suppression is absent.

Therefore:

- Recorded messages are positive evidence that the USB input driver reported underflow conditions.
- Their count is a count of retained messages, not a validated count or rate of all underflows.
- A gap in messages does **not** establish a healthy interval, disprove ongoing underflows, or weaken a possible common timing cause of input and output trouble.
- The mismatch between logged-message times and audible bursts makes the message stream unsuitable as a complete audible-fault detector. It does not disprove underflow causality.

The earlier live checkpoints noted later bursts after the last recorded underflow. **Withdraw any implication that the underlying underflows necessarily stopped at that timestamp. Only the retained messages stopped.** The waveform establishes the audible intervals independently.

Apple's [USB audio design considerations](https://developer.apple.com/documentation/technotes/tn3190-usb-audio-device-design-considerations) describe clock/feedback arrangements that can couple input and output timing. Maya's actual feedback/clock arrangement is not established here; do not assert it uses implicit feedback.

## Device state and archive coverage

The initial archive retains events from **16:19:19.000 through 16:34:20.999 PDT**. Maya enumerates at 16:30:18.843; device object `0x376` and input transfer manager `0x390` are explicitly associated with it. The last recorded startup StartIO is at 16:30:53.965. The private `ioDriftNS` field increases to **250,519,000 at 16:33:27.491**, before the reliable analog capture. This is not a calibrated analog delay and is not available through the later measured bursts. Three transaction errors at 16:30:23 identify **WRLD endpoint zero**, not Maya; they are not evidence that the resolved WRLD firmware defect returned.

Live audio/USB logging retains device events from **16:36:09.678 through 17:04:36.969**. It received 232,293 events and saved 2,912 matching records; the helper stopped at its deadline. The final historical archive retains **16:34:49.000 through 17:04:51.968**. It has no retained `StartIO`, `StopIO`, USB transaction-error or `ioDriftNS` message in the inspected audio/USB records. Treat these as coverage-qualified log observations, not guaranteed absence of the corresponding internal activity.

The final SmartGrid log retains records through **17:04:10**. It continues reporting Maya at 48 kHz/512 frames, four inputs and four outputs, nominal thermal state, low-power mode off and no MIDI submission errors. Only the initial audio-timing record at 16:30:54 is present (one processing overrun, no xruns, long gaps or format mutes); no subsequent timing-counter change or app audio reopen is retained. This is aggregate evidence, not a per-callback trace. Periodic `Delay buffer wrap around` messages are not by themselves fault evidence.

The finalizer completed successfully at **17:05:42.620**, including app/system captures, waveform analysis, closed-header verification and archive decoding. Its success concerns capture and analysis, not resolution of the Maya fault.

## Capture quality and reproducible analysis

The original FFmpeg/AVFoundation recorder began around **16:34:37** and wrote one-minute wall-clock files, but the first nominal minute contains only **53.152 seconds of PCM**. Those files remain preserved, including the 16:35 report, with unreliable timing/continuity. FFmpeg was stopped at 16:38:57.442 after verifying the SoX replacement.

The main SoX/CoreAudio recording selects K-Mix physical channels using `-c 8 K-Mix ... remix 3 4`, at 48 kHz/24-bit. Its closed WAV header verifies 1,575 seconds and 453,600,080 bytes. SHA-256: `71e561e5573db2ca435573fd485857c50b8fb4716246997c27b202c2839aa185`.

There are **five** Mac recorder discarded-buffer warnings at approximate recorded elapsed seconds **12.15, 322.75, 1184.57 (twice), and 1508.45**. All lie outside the eight matched burst intervals; the final warning is during the changed-material interval. The discarded durations are unknown, so this is not a globally lossless capture or a calibrated sample-to-wall-clock mapping. These Mac recording warnings are separate from iPad USB input underflows. User listening reports independently corroborate the sustained episodes.

The spectral detector calculates 6–20 kHz RMS in each channel with 100 ms Hann-windowed FFTs, requires both channels above -80 dBFS, bridges gaps up to 0.5 seconds and retains intervals at least one second long. All eight matched events persist at thresholds from -85 to -75 dBFS, shifting boundaries by at most 0.2 seconds. Typical earlier background high-band RMS is about -104 dBFS. This criterion depends on the material, as the post-17:02 change demonstrates; it is not a universal glitch detector.

Quiet-run detection requires both channels below -65 dBFS for at least 1 ms. It can miss inserted flat regions because of analog residuals and can include naturally quiet material. A bounded-memory, ten-second chunk implementation was verified identical to the original implementation over their shared first 377.3 seconds. Use actual PCM bytes for an active WAV; its provisional header duration is not valid until close.

## Artifacts on the Mac

Raw data and derived artifacts remain outside Git in:

`/Users/joyo/Documents/SmartGridOne/diagnostics/20261006T233430Z-periodic-maya/`

Key files:

- `sox-kmix-inputs-3-4.wav`, `sox-status.json`, `sox-recorder.log`, `final-wav-validation.log`, `completed-capture-qualification.json`.
- `analysis-final.json`, `analyze-maya-periodic-chunks-20261006.py`, `maya-burst-details.json`, `maya-period-coherence.json`, `summarize-maya-capture-20261006.py`.
- `maya-waveform-comparison.png` compares first/eighth episodes; `sox-50-70s-spectrum.png` and `burst-1647-spectrum.png` show earlier spectra. Excerpt time axes are recording-relative, not wall-clock time.
- `user-observations.jsonl` preserves reports and corrections.
- `live-audio-usb.jsonl`, `live-status.json`, `live-collector.log`, `maya-periodic-live-20261006.py`.
- `final-app/2026-10-06T16-30-51-324.log`, `final-system.logarchive/`, `final-system-audio.log`, `final-system-all.log`, `finalization-status.json`, `finalizer.log`, `finalize-maya-periodic-20261006.py`.
- Earlier limited-quality `kmix-inputs-3-4-20261006T*.wav`, `recorder.log`, `recorder-change.json` and checkpoint analysis files remain for provenance.
- Active continuation: `sox-continuation-1706.wav`, `sox-continuation-1706-status.json`, `sox-continuation-1706.log`, `maya-continuation-20261006.py`. The filename refers to the triggering report; actual launch was 17:07:33.108. Recording is bounded to 900 captured seconds. No continuation analysis or completion is claimed here.

Initial app snapshot: `/Users/joyo/Documents/SmartGridOne/diagnostics/20261006T233422.751006Z-app/2026-10-06T16-30-51-324.log`.

Intermediate app snapshot: `/Users/joyo/Documents/SmartGridOne/diagnostics/20261006T234913.166614Z-app/2026-10-06T16-30-51-324.log`.

Initial system archive: `/Users/joyo/Documents/SmartGridOne/diagnostics/20261006T233419.377615Z-system.logarchive/`; decoded initial logs are in the run directory.

## What would discriminate the remaining mechanisms

The capture establishes a repeatable waveform shape and timing scale, not a root cause. A future native output-boundary trace aligned with analog capture could establish whether valid blocks reach iOS on time while the analog signal is being cut. That would test the handoff after the already-established clean DSP, not retest the synthesizer. The requested audio-reset button would provide a separate controlled recovery experiment; no such reset was performed here. A software recovery result still would not establish that Maya was power-cycled or uniquely locate the original fault.

## User-initiated upstream reconnect during continuation

The user chose to unplug/replug and explicitly identified **the iPad-to-hub cable**. The continuation recorder was already running. A historical archive retains actual events from **17:01:33.000 through 17:11:33.996 PDT**, including the unrecorded 17:06 listening report and the reconnect. The log shows:

- **17:09:53.573–17:09:53.688:** Maya endpoint transaction errors, StopIO, whole-hub hardware connection loss and termination of Maya and WRLD. These errors accompany the deliberate unplug and must not be counted as spontaneous periodic-glitch causes.
- **17:09:57.546:** upstream hubs re-enumerate.
- **17:10:02.310:** Maya re-enumerates at 12 Mbps.
- **17:10:03.781–17:10:04.848:** seven Maya StartIO attempts interleaved with StopIO. Driver startup reports excessive zero-length packets and increases `lockDelayMS` from 24 through 100, 150, 200 to 250. The final recorded StartIO in this archive completes at 17:10:04.848. These are observed startup/recovery messages, not proof of the cause of the preceding periodic symptom.

This test rebuilds the upstream USB topology and Maya driver/stream state. It is **not** an isolated Maya-only disconnect, does not establish that Maya lost power, and cannot by itself distinguish host, hub, device or connection-state causes. A sustained clean post-reconnect interval would be useful; immediate recovery is ambiguous because earlier episodes already self-cleared. No successful recovery or disappearance of recurrence is claimed at this checkpoint.

Artifacts are in the same local run directory: `reconnect-system.logarchive/`, `reconnect-system-all.log`, `reconnect-system-audio-selected.log`. A separate filtered live stream began at device time **17:11:21.967** and is bounded to 17:22:33: `reconnect-live-audio-usb.jsonl`, `reconnect-live-status.json`, `reconnect-live-collector.log`, with helper `maya-reconnect-live-20261006.py`. It started after the physical reconnect; the historical archive supplies that transition. The continuation WAV still requires final header/quality verification and analysis.

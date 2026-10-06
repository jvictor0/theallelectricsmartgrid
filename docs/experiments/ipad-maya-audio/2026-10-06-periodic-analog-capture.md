# October 6: periodic glitches captured on K-Mix inputs 3/4

Initial findings recorded at approximately 16:42 PDT. Capture remains active at this checkpoint, with an intended end around 17:04:37 PDT. This is not a completed-run report.

The user reported intermittent periodic corruption, spontaneous recovery before the initial request, and further bursts at approximately **16:35**, **16:39**, **16:41**, and **16:42**. The first three lasted a few seconds; the user described 16:42 as possibly shorter. A proposed two-minute cycle remains untested. These are listening reports with minute-level timing. The user explicitly identified the periodic symptom, not persistent startup silence, and requested recording interface inputs 3/4.

## Useful evidence so far

- The latest persisted SmartGrid log begins at 16:30:51. Its settled setup at 16:30:54 is Maya, 48 kHz, 512 frames, four inputs and four outputs. Through the initial snapshot's last record at 16:33:55, thermal state is nominal and MIDI submissions report no errors. The one initial audio timing record reports one processing overrun and no xruns, long gaps or format mutes. This is aggregate app evidence, not a per-callback trace or an analog quality measurement.
- The first iPad archive retains actual events from **16:19:19.000 through 16:34:20.999 PDT**. Maya enumerates at 16:30:18.843; driver device object `0x376` and input transfer manager `0x390` are explicitly associated with Maya. The final recorded Maya StartIO in this archive is at 16:30:53. Subsequent reported `ioDriftNS` values increase, reaching 250,519,000 at 16:33:27.491. This private field is not a calibrated analog delay.
- Maya input-transfer underflows occur at **16:32:58.580**, **16:34:11.371**, **16:34:11.443**, and **16:34:11.483**. They precede the archive request at 16:34:19. The initial filtered archive has no later StartIO/StopIO after the settled startup. The three retained transaction errors at 16:30:23 identify WRLD endpoint zero, not Maya; do not conflate those enumeration events with periodic audio corruption or the resolved WRLD firmware defect.
- The external recording contains a clear corruption burst matching the user's **16:39** report. A spectrogram of SoX recording seconds 50–70 shows added broadband energy starting around recording second 53 and clearing around second 63. This corresponds approximately to **16:39:15–16:39:25 PDT**; launch/capture latency and the early recorder warning below prevent millisecond-accurate wall-clock attribution.
- During recording seconds 54–60, both channels repeatedly approach silence together. A preliminary detector requiring both channels below -65 dBFS for at least 1 ms finds many short holes; the longest observed so far is about 7.42 ms. Many adjacent hole starts are approximately 10.67 ms apart, consistent with the configured 512-frame period at 48 kHz. This is waveform evidence of periodic partial blanking, not proof of the failing software or hardware component. Clean generated DSP remains established context.
- The 16:41 and 16:42 reports are marked for analysis; it was not yet analyzed at this checkpoint. The recording and live logs continue. No audio reset, app restart, deployment, cable/power change or sysdiagnose was initiated by this capture.

## Capture quality and limitations

The first recorder used FFmpeg/AVFoundation on the Mac's K-Mix, selecting zero-based channels 2/3 as stereo, at 48 kHz/24-bit. It started around **16:34:37 PDT** and wrote one-minute wall-clock segments. Verification found only **53.152 seconds of PCM in the first nominal minute**, so this path cannot support precise timing or attribution of its discontinuities. Its files are preserved, including the interval containing the 16:35 listening report, but the recording defect must be considered when interpreting them.

A replacement using the previously employed SoX/CoreAudio path started at **16:38:21.601 PDT**, with K-Mix eight-channel input and explicit `remix 3 4` stereo output. It records the unchanged Mac interface at 48 kHz/24-bit. The first recorder was stopped at 16:38:57.442 after verifying the replacement. SoX sample duration tracked elapsed time: about 35.612 captured seconds versus 35.837 seconds since process launch, then 81.977 versus 82.193 seconds. SoX reports one discarded-buffer/overrun warning around recording second 12.15, during the overlap with FFmpeg, before the 16:39 burst. Preserve that limitation; there was no additional warning in the checked burst interval. This is not a claim of loss-free or calibrated sample-to-wall-clock alignment.

The active SoX WAV header contains a provisional duration until recording closes. Use actual available PCM bytes for interim analysis, and verify the final header/duration after completion.

Filtered live iPad audio/USB logs began receiving device events at **16:36:09.678 PDT**, using the existing paired Wi-Fi connection and `OsTraceService.syslog` flags `0x184`. Device timestamps and host receipt timestamps are saved separately. Live capture ends at the same planned wall time or at a 128 MiB saved-data limit. It may omit events and impose load; absent messages do not establish absence of a fault. Initial historical collection overlaps the first recorder's startup, while the subsequent analog capture uses live logging rather than repeated heavyweight diagnostics.

## Artifacts on the Mac

All raw data remains outside Git:

- Run directory: `/Users/joyo/Documents/SmartGridOne/diagnostics/20261006T233430Z-periodic-maya/`
- Main ongoing recording: `sox-kmix-inputs-3-4.wav` in that directory. `sox-status.json` and `sox-recorder.log` retain its command, launch timing, process ID, warnings and eventual completion result.
- Earlier limited-quality recordings: `kmix-inputs-3-4-20261006T*.wav`; `recorder.log` and `recorder-change.json` describe their capture and replacement.
- User markers: `user-observations.jsonl`.
- Live device evidence: `live-audio-usb.jsonl`, `live-status.json`, `live-collector.log`; helper source preserved as `maya-periodic-live-20261006.py`. Status counters are finalized when the helper exits, not continuously updated.
- Spectrogram of the 16:39 burst: `sox-50-70s-spectrum.png`. Its horizontal axis is seconds within a 20-second excerpt starting at recording second 50, not wall-clock seconds.
- Initial app snapshot: `/Users/joyo/Documents/SmartGridOne/diagnostics/20261006T233422.751006Z-app/2026-10-06T16-30-51-324.log`.
- Initial system archive: `/Users/joyo/Documents/SmartGridOne/diagnostics/20261006T233419.377615Z-system.logarchive/`. Derived `system-all-before.txt` and `system-audio-before.txt` are in the run directory.

Next analysis is to finish the bounded capture, validate recorder continuity and final WAV length, identify additional analog bursts, and correlate their intervals with the retained device logs. No recovery or permanent fix has been demonstrated.

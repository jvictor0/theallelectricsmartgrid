# September 12: 5 ms idle I/O polling and normal MIDI scheduling

Status: **clean controlled trial, provisional result**. SmartGridOne was stopped at 13:50:15 PDT after recording and archive collection. This run does not establish that either change fixed the underlying fault.

## Intervention and setup

The only changes from the immediately preceding build were `IoTaskThread::Run` empty-queue sleep from 100 µs to 5 ms, and `MidiSender` from JUCE real-time thread creation to ordinary `startThread()`. The latter kept the existing 2 ms MIDI worker loop, iOS native timestamp scheduling, 20 ms lead, and SysEx handoff. The 100 µs retry when the I/O acknowledgment queue is full was unchanged. The app ran normal DSP, visible UI, autoplay, and active MIDI output with the saved `ms` patch. It used JUCE 8.0.15, requested and actual 48 kHz/512 frames, zero requested/active app inputs, and four active outputs on MAYA44 USB+. The iPad remained on the Satechi hub with WRLD.BLDR and 90 W PD. The signed binary SHA-256 was `1b1d60fa3b5cf85e370e70fb470b6eb669a72d5b54936d53bcd83457a235ad1c`; installation preserved the app's config and patch bytes.

`IoTaskThread` can receive jobs from startup sample-bank restore, sample-directory UI actions, and recording save/reload. No per-frame enqueue site was found. The old stackshot attributed approximately 15,570 syscalls/s to that thread over 247.7 s, consistent with a 100 µs empty-queue sleep loop, but the queue's depth and task count were not instrumented. This is evidence for idle polling, not a proof that no I/O jobs accumulated.

## Observations

Launch: 13:09:11 PDT. The two 48 kHz, 24-bit stereo K-Mix recordings covered 13:11:45.841–13:27:46.164 and 13:28:52.902–13:44:53.221, 960 s each, with a 66.7 s analog gap between them. The app continued running through that gap. The user reported the iPad cool to the touch and the audio sounding perfect around 13:41. These are listening and touch observations, not instrument readings.

Both recording windows contained 90,028 consecutive app callbacks, all 48 kHz/512, app thermal state Nominal, low-power state off, native xrun counter zero, no gap over 20 ms, and no measured DSP overrun. The largest callback gap was 10.731 ms in the first window and 10.733 ms in the second; DSP median was 4.560 and 4.555 ms respectively. The callback clock-to-wall fit had up to approximately 19 ms residual, so do not use it for sample-accurate analog alignment.

The K-Mix signal median was approximately −32 dBFS in both files. The long-dropout screen found no interval of at least 30 ms below either 0.0003 or 0.001 RMS with adjacent active audio. The short-flat screen found zero candidate flattened chunks under both tested thresholds (5e-5 and 1.5e-4); applied unchanged to the September 11 known-bad recording, its stricter threshold finds 1,377 events and a dense 17-second burst. These detectors target the observed long silence and short periodic plateau signatures, not every possible audible artifact.

The iPad archive covers the app launch and both recordings through 13:45:42. It contains zero MAYA transaction errors (`0xe00002ed`), zero zero-length-transfer reports, and zero `ioDriftNS` reports after launch. Its single Core Audio “restarting IO” entry was at 13:09:12 during app configuration, before the recordings. The `symptomsd` battery sensor rose from 26.69 °C at 13:09:14 to 29.19 °C at 13:45:42, with a plateau near 28.79 °C for much of the run; external power stayed connected, charging changed from on to off at 13:17, and logged screen brightness stayed 600000. This is battery temperature, not hub, MAYA, or processor temperature. The earlier bad run rose from 25.69 to 30.59 °C in about ten minutes, but its initial conditions were not matched tightly enough to attribute the difference to this patch.

Short process samples measured approximately 8,582→840 interrupt wakeups/s (−90.2%), 9,612→1,845 context switches/s (−80.8%), and 21,473→5,967 Mach-plus-Unix syscalls/s (−72.2%) from old to new. System CPU changed from 3.47% to 2.81% of a core while user CPU was higher in the new short sample (66.1%→77.3%); no total-CPU reduction is claimed. The new measurements are process-wide and do not directly count I/O worker jobs.

## Interpretation and next decision

The matched cool, Nominal, clean run and large wakeup reduction make excessive idle polling a plausible contributor. The result remains provisional: an earlier SmartGrid build had an approximately 35-minute quiet interval, and this trial changed the I/O poll period and MIDI thread scheduling together. Repeat the same binary from a cool start for at least an hour before declaring reliability. If that repeats cleanly, isolate the variables in a controlled comparison. A low-cost I/O task enqueue/process/depth counter can settle the job-leak question directly in a subsequent diagnostic build.

## Evidence

- Build, install, launch, preflight: `/private/tmp/smartgrid-idle5-midi-normal-20260912/{build,install,launch,preflight}.json`
- Analog and recorder metadata: `analog-r1.wav`, `analog-r1.json`, `analog-r2.wav`, `analog-r2.json` in the same directory
- Analog screens: `long-r1.json`, `long-r2.json`, `short-r1.json`, `short-r2.json`, and `short-positive-control.json`
- App callback traces: `app-final.log`, `callback-first-capture.json`, and `callback-r2.json`
- Process samples: `old-sysmon-baseline.json`, `sysmon.jsonl`, and `sysmon-summary.json`
- USB and thermal: `system.logarchive`, `system.logarchive.collection.json`, and `system-signals.log`
- Final state: `stop.json`

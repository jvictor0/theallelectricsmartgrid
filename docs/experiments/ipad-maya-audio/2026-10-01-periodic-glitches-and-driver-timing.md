# 2026-10-01 — Periodic corruption returns while Maya remains streaming

Status: recurring periodic playback corruption, unresolved. The user explicitly identifies this as the original periodic failure, and reaffirms that DSP generation is clean. No new test of DSP content was needed. This investigation collected app logs, iPad system archives and one sysdiagnose; it did not deploy, restart the app or media services, install a profile, or change USB/power.

**The first episode cleared without a recorded USB audio restart, while the driver's reported timing discrepancy continued to grow.** Later bursts recur within the same streaming session. This resembles the [September 10 periodic-blanking recovery without restart](2026-09-10-juce815-comparison.md), rather than establishing another instance of the later persistent-silence fault. A separate empty-transfer episode and hub reconnect precede the reported periodic window and are recorded below.

All times are October 1, PDT (UTC−07:00). Capture directories use UTC and therefore begin `20261002`. Device: iPad Air M3, iPadOS 26.6.1 / 23G83, Maya and WRLD.BLDR through the hub. The exact running app binary was not read back or matched to a hash.

## User observations

| Approximate time | Report |
| --- | --- |
| Before 20:35, for a few minutes | The original periodic playback problem was audible. Exact onset was not marked. |
| 20:35 | Glitches cleared spontaneously; playback continued. The user confirmed no recovery intervention at this point. |
| 20:39 | Another burst lasting a few seconds. |
| 20:42 | Another recurrence; duration not specified. |
| 20:47 | Another recurrence; duration not specified. |

These are approximate listening timestamps, not sample-aligned waveform boundaries. No analog output recording was obtained during this incident. The previously established clean DSP/internal-recording evidence remains applicable context; the absence of a new analog recording limits exact correlation of these bursts with individual driver events.

## Driver timing and spontaneous recovery

Maya's last recorded StartIO before the periodic episode is **20:27:50.985**. Its driver device object is identified by creation records as Maya. The private `AUAAudioDevice_updateTimeStamp ioDriftNS` field then rises monotonically in the retained reports:

| Event time | Reported `ioDriftNS`, converted to milliseconds |
| --- | ---: |
| 20:27:51.700 | 0.250 |
| Around 20:33:00 | 3.000 |
| 20:35:00.223 | 17.125 |
| 20:37:04.752 | 49.980 |
| Start of 20:38 | 77.646 |
| Start of 20:39 | 203.543 |
| 20:39:38.953, last retained drift report | 245.730 |

The first archive has no recorded Maya transaction error, empty-input transfer, USB input underflow or I/O restart from **20:28 through 20:37:04.957**. In particular, the user-confirmed 20:35 recovery does not coincide with a recorded reset, reconnect or sample-rate change. The discrepancy continues rising after audible recovery. After removing overlap between the first two archives, there are **1,575** drift reports from 20:27:51.700 to 20:39:38.953, with no negative value step or sample-time reversal in that sequence.

Later drift reports cease. Other usbaudiod records continue through 20:43:15.976, so this is not simply the end of all retained driver logging. Missing further drift values must not be converted to zero, or described as a demonstrated correction, reset or known threshold. `ioDriftNS` is a private diagnostic field: its numeric value is not a measured analog delay or a direct count of lost output samples.

The follow-up archive records **249 Maya input-transfer underflows from 20:38:34.481 to 20:41:06.130**: 168 in the 20:38 minute, 80 in 20:40, and one in 20:41. The transfer-manager creation record associates this object with Maya. There are no recorded USB audio StartIO/StopIO calls, transaction errors, empty-input reports, pipe stalls or USB hardware disconnects in that archive. Two generic matches for “enumerated” concern display enumeration by powerlogHelperd, not USB devices; they are excluded from USB findings.

The final archive adds **32 Maya input underflows at 20:43:20.738–20:43:22.514**, shortly after the follow-up archive request at 20:43:13.540. It has no recorded StartIO/StopIO, transaction error, empty-input transfer, pipe stall, hardware disconnect or new drift report through 20:48:27.971. Other usbaudiod records remain present through 20:48:26.150. In particular, the reported **20:47 burst has no matching new USB error or restart** in this retained window.

The underflows are downstream driver evidence, specifically on the input path, rather than measurements of the output waveform. They do not establish a one-to-one match to the listening reports. The first episode and its 20:35 recovery precede these underflows, and some underflow clusters follow diagnostic requests. Do not equate them with the periodic audible symptom without a synchronized analog capture.

## App and driver state during the episode

The app snapshots consistently report Maya as input/output at actual **48 kHz / 512 frames**, four inputs/four outputs, thermal state 0 and low-power mode off. The later app snapshot continues through **20:44:39**. The preferred-rate field becomes 192 kHz after reconnection, but actual session and device rate remain 48 kHz; the preference is not an active 192 kHz stream.

The final app timing-change message is at 20:28:31: zero reported long gaps, three startup/reconnection xruns, three processing overruns and zero format mutes. No further timing-change message is present through 20:44:39. The current reporting code emits these records when counters change, with throttling. This is limited aggregate evidence and should not be presented as a per-callback trace or proof of analog output quality.

A sysdiagnose-triggered driver state dump at **20:39:18.252** independently reports Maya active/running, nominal 48 kHz, and physical input/output formats of 48 kHz, four channels, 16 bits. Output mute is off. Its declared “Clock Is Stable” flag is YES despite the surrounding discrepancy reports; that property alone is not a demonstrated health detector for this failure.

## Earlier startup/reconnect context

Before the periodic window, there is a separate sequence resembling the persistent empty-transfer startup problem: first excessive-empty-packet warning at **20:27:20.330**, app log beginning at 20:27:30, and empty-transfer/underflow reports until the hub disconnect around **20:27:39**. The hub and peripherals then re-enumerate; Maya appears at **20:27:48.769**, with the last brief empty-input report at 20:27:50.658 and final StartIO at 20:27:50.985.

The initial archive contains 18 transaction-error messages around removal/re-enumeration: 13 on Maya IN 0x82, one each on Maya control 0x00 and endpoint 0x83, and three on WRLD control 0x00. These are not counted as 18 spontaneous periodic glitches. The reason for the 20:27 reconnect was not confirmed by the user at the time of writing. The known later corruption occurs after that reconnect, within the new Maya streaming session.

## Collection effects and interpretation

The first app capture began at 20:36:32, after the first episode had already cleared. The sysdiagnose was requested at **20:38:18.911**, became ready at **20:40:53.101**, and finished transferring at **20:42:44.275**. Thus the 20:39 and 20:42 reports overlap diagnostic generation or transfer, and the later underflows begin during collection. Collection could affect timing/load; this is not a controlled test of its effect. It cannot explain the first reported episode, and the 20:47 recurrence was reported after the preceding captures had finished.

The useful finding is another spontaneous periodic recovery with continuous I/O and growing driver timing discrepancy, consistent with the earlier journal's failure class. It strengthens investigation of USB audio timing/buffer alignment after clean audio generation. It does **not** identify whether the initiating mechanism is Maya, the hub or iPadOS, establish a monotonic drift-to-audibility relationship, or demonstrate a software fix. The [earlier drift interpretation](research/2026-09-10-drift-derivative-hypothesis.md) already records recovery while discrepancy grows and the limitations of treating this field as a physical clock measurement.

## Local artifacts

Findings are versioned in this journal. Artifacts remain on the Mac under `/Users/joyo/Documents/SmartGridOne/diagnostics/`; none are added to this journal update's Git payload.

| Evidence | Path beneath that directory |
| --- | --- |
| First app snapshot | `20261002T033632.615631Z-app/2026-10-01T20-27-25-840.log` |
| First system archive; retained actual events 20:17:45.134–20:37:04.957 | `20261002T033703.097623Z-system.logarchive/` |
| Sysdiagnose and capture metadata | `20261002T033818.230853Z-after-periodic-maya-recovery-sysdiagnose/` |
| Follow-up system archive; retained actual events 20:34:00.004–20:43:15.991 | `20261002T034313.540076Z-system.logarchive/` |
| Follow-up app snapshot | `20261002T034510.749873Z-app/2026-10-01T20-27-25-840.log` |
| Final system archive covering the 20:47 report; retained actual events 20:43:00.000–20:48:27.971 | `20261002T034826.085012Z-system.logarchive/` |
| Analysis, timestamped user reports, deduplicated drift series, selected driver state and summaries | `20261001-maya-periodic-recurrence/` |

The first archive was requested from 20:15, but its actual retained events start at 20:17:45; earlier coverage is not claimed. Retained bounds do not guarantee every possible event was logged. The sysdiagnose is **711,991,939 bytes**, expected size equals received/file size, `gzip -t` passed, and the tar index was read successfully. Audio Glitch Trace profile status was not rechecked today; it was last reported uninstalled on September 28. This is a general sysdiagnose, not a claimed Audio Glitch Trace capture.

All captures have finished. No recovery or permanent fix was demonstrated, and no background monitoring remains active. The next useful discriminator would be synchronized external analog output and USB timing evidence with collection effects controlled, rather than another test of DSP sample generation.

# October 6: charging chimes, power changes and Maya stream starts

## Question and scope

The user performed ad hoc unplug/replug tests and asked whether SpringBoard plays the connection sound, what triggers it, and why it is not heard on every plug. Collected historical iPad system logs over Wi-Fi without deploying, restarting the app, or initiating a hardware intervention. Requested the preceding 15 minutes; retained dated records span **17:42:53.000–17:57:54.991 PDT**, October 6, 2026 (235,101 dated records).

The user subsequently confirmed that the **17:56–17:57 changes were hub external power only**, with the iPad-to-hub cable left connected. These were exploratory actions, not a controlled reproduction of periodic corruption. There was no simultaneous analog recording or per-event listening confirmation.

## Five charging-chime requests, with two different output routes

SpringBoard PID 34 makes the charging/chime decision and requests the sound. The audio daemon `audiomxd` records the playback request and start. The explicit sound is SSID **1106**, category **ConnectedToPower**. It is distinct from the **ScreenLocked** sound, SSID 1100, also present at 17:55:33.

| Time, PDT | External power recognized | Audio start | Logged output route | USB context |
| --- | --- | --- | --- | --- |
| 17:55:14 | 14.059 | 14.091 | Speaker | Hub enumerates at 17:55:15.566; Maya at 17:55:20.343. |
| 17:55:38 | 38.626 | 38.703 | Speaker | Hub enumerates at 17:55:40.128/.130; Maya at 17:55:44.909. |
| 17:56:20 | 20.036 | 20.064 | USB / Maya | Existing USB connection remains present. |
| 17:56:22 | 22.093 | 22.115 | USB / Maya | Existing USB connection remains present. |
| 17:57:12 | 12.788 | 12.807 | USB / Maya | Existing USB connection remains present. |

All five decisions say `Should chime: YES`, `isChimeDisabled: NO` and `_powerSourceWantsToPlayChime: YES`. Their playback decisions say `suppressesAudio = NO` and `Audio = 1`, followed by a logged audio start. Thus these five requests were not marked suppressed. This establishes requested/scheduled playback, not independently verified sound from the speaker or Maya's analog outputs.

The USB route is identified by the physical device UID `AppleUSBAudioEngine:ESI Audiotechnik GmbH:MAYA44 USB+:140000:2,1` in the volume/route records, rather than inferred solely from the generic word USB. The first two sounds were requested before Maya enumeration, while the later sounds used the already-present interface. This routing difference is a plausible explanation for a charging chime being absent from the iPad speaker; it does not establish why an individual sound was inaudible at Maya's output.

For this window, every retained NO-to-YES external-power transition has a charging-chime request. Other battery/power updates, including power removal, have `Should chime: NO` and `_powerSourceWantsToPlayChime: NO`. The chime therefore is not a generic notification for every USB event. Decisions were YES with both screen-on and screen-off states. Two requests occurred only about two seconds apart. These observations do not establish Apple's complete suppression/debounce policy.

## Power changes and audio starts are separate from USB enumeration

Whole-hub disconnects at **17:55:09** and **17:55:32** terminated Maya and were followed by full enumeration. In contrast, external-power changes at **17:56:17 NO → 20 YES → 21 NO → 22 YES**, and **17:56:49 NO → 17:57:12 YES**, have no corresponding hub/Maya termination or re-enumeration in the retained logs. This agrees with the user's confirmation of hub-power-only actions. It shows that this setup can keep the logical USB connection alive across these power changes; it does not measure the peripheral supply rail or exclude a transient.

The 17:56:20 charging sound is followed by Maya `performStartIO` at **20.069** and StopIO at **17:56:25.161**. The 17:56:22 chime occurs during that interval. The 17:57:12 sound is followed by StartIO at **12.811** and StopIO at **15.932**. Both starts log a 24 ms startup lock delay. System sounds can therefore start an idle Maya audio stream without a fresh USB enumeration or a SmartGrid launch. No claim of audible correctness or periodic-failure reproduction follows from those starts.

The new Maya instance was configured at 48 kHz at **17:55:46.259**, then 44.1 kHz at **17:55:48.373**. At 48.351, SpringBoard auxiliary session `0x13bc05a` starts with category `SystemSoundsAndHaptics/Skeuomorphic`; at 48.357, its preferred output rate is 44100. This is separate from the earlier charging chime at 17:55:38, which used the speaker. There is no later physical rate-setting call in the retained window; the 17:56:20 Maya route dump reports 44100 Hz.

## Correction to the original 16:30 startup interpretation

Rechecking the [original attachment](2026-10-06-periodic-analog-capture.md#clarification-who-requested-the-rate-changes-and-what-lock-delay-means) distinguishes two SpringBoard activities. At **16:30:12.624**, SpringBoard requests ConnectedToPower / SSID 1106. It is routed to **Speaker** at 12.625, and audio starts at 12.629, before Maya enumerates at 16:30:18.843.

The later SpringBoard session `0x13bc057`, which requests 44.1 kHz at **16:30:36.667**, is separate `SystemSoundsAndHaptics/Skeuomorphic` activity. Its exact audible effect has not been established. **Do not identify that first Maya 44.1 kHz transition as the charging chime.** The broader observation remains that system audio started/configured Maya before SmartGrid launched. Neither event establishes the cause of the later periodic corruption.

## Local artifacts

Raw artifacts remain on the Mac; only findings are committed:

- Archive: `/Users/joyo/Documents/SmartGridOne/diagnostics/20261007T005753.421654Z-system.logarchive`
- Decoded log: `/Users/joyo/Documents/SmartGridOne/diagnostics/20261006T233430Z-periodic-maya/plug-tests-1757-all.log`
- Selected event summary: `/Users/joyo/Documents/SmartGridOne/diagnostics/20261006T233430Z-periodic-maya/plug-tests-1757-summary.json`
- Original startup: `/Users/joyo/Documents/SmartGridOne/diagnostics/20261006T233430Z-periodic-maya/system-all-before.txt` and `system-audio-before.txt`; original archive `/Users/joyo/Documents/SmartGridOne/diagnostics/20261006T233419.377615Z-system.logarchive`.

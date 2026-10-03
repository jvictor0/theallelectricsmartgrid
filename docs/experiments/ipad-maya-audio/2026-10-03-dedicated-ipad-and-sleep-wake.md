# October 3: dedicated iPad operation and the sleep/wake hypothesis

WRLD's [one-way MIDI firmware defect is resolved](2026-10-03-wrld-usb-reset-firmware-defect.md#resolution-update),
with successful hardware behavior reported by the user after deployment. Maya
audio is the remaining investigation. This removes the one-way MIDI symptom
as support for a common Maya/WRLD failure mechanism. Maya's persistent startup
silence and periodic mid-performance corruption remain distinct observations;
their sharing a cause is not established.

The user suspects Maya becomes unresponsive after the iPad has been “turned
off” while connected. They clarified this means **screen locked/asleep, not
fully shut down**. No device settings, audio session policy, deployment or
physical connections were changed during this documentation/research update.

## Existing evidence supports testing sleep transitions

- [September 28 evening](2026-09-28-evening-recurrence-before-deployment.md):
  about 37 minutes of streaming without the later fault signature; SmartGrid
  backgrounds at 19:20:11, driver StopIO at 19:20:12, Idle Sleep at 19:20:23;
  first Maya empty-transfer failure at 19:23:01.733 when SpringBoard starts
  system audio after wake. This precedes the later deployment/app launch.
- [October 3 startup](2026-10-03-startup-silence-and-pd-power.md): display/wake
  at 10:25:08; SpringBoard starts system audio; first Maya fault at
  10:25:09.532, before SmartGrid's first system record at 10:25:16.694.

These observations favor a USB resume or audio-start-after-idle hypothesis for
startup silence. September 28 shows that days of sleep are not necessary for
that observed sequence. They do not locate the fault within Maya firmware, the
hub or iPadOS, nor establish when an unobservable latent bad state began.
USB3 resume anomalies were seen, but Maya is on the USB2 branch. Do not turn
correlation into a proven USB3 cause. The clean DSP evidence remains established.

## Apple's documented operating model

Apple explicitly discusses iPads kept on power for kiosks and point-of-sale
systems, including automatic long-duration charge management. This is an
anticipated deployment, not inherently misuse of an iPad. [Apple charge
management](https://support.apple.com/en-us/111822).

For a dedicated appliance, **Single App Mode** on a supervised iPad keeps the
chosen app available and reopens it after restart. Its options can disable
Auto-Lock and the Sleep/Wake button. Apple Configurator can configure this;
supervision setup is a separate deployment decision, not necessary for a simple
awake-versus-asleep experiment. [Single App Mode](https://support.apple.com/en-ca/guide/apple-configurator-mac/cadbf9c172/mac),
[App Lock options](https://support.apple.com/guide/deployment/dep80a981/web).

Apple's `isIdleTimerDisabled` guidance says ordinary audio apps should generally
allow the screen to turn off: appropriately configured audio playback/recording
can continue. Keeping a performance UI visible is a different need. A display
sleep setting is not a documented per-device USB power-state or recovery
control. [UIKit idle-timer guidance](https://developer.apple.com/documentation/uikit/uiapplication/isidletimerdisabled?language=objc).

Apple's audio-session guidance calls for the background-audio mode when needed,
handling interruption and route changes, deactivating an idle audio session
when moving to the background, and reactivating appropriately after suspension.
It advises against streaming silence merely to avoid suspension. These are
lifecycle requirements to audit if needed, not proof of a SmartGrid violation
or a remedy for an already-stuck USB peripheral. [Audio Session Programming
Guide](https://developer.apple.com/library/archive/documentation/Audio/Conceptual/AudioSessionProgrammingGuide/AudioGuidelinesByAppType/AudioGuidelinesByAppType.html).

USB-C iPads expose **Settings → Privacy & Security → Wired Accessories**,
including Always Allow. That controls connection authorization. Apple says an
accessory connected after unlock remains connected when the screen relocks;
ordinary relocking is not supposed to revoke an established connection. This
setting is worth recording for a dedicated rig, but it does not disable USB
suspend or prove a fix for Maya's enumerated-but-failing stream. [Accessory
access](https://support.apple.com/en-us/111806).

For battery longevity in an installation, this iPad Air M3 supports **Battery →
Battery Health → 80% Limit**; Apple lists iPad Air M2 and later. This is a battery
care measure, not an audio workaround. [Charging and battery care](https://support.apple.com/en-au/118418).

## Proposed comparison and provisional operating choice

During soundcheck through the end of a performance, keeping SmartGrid visible,
Auto-Lock off and external power steady is a reasonable provisional way to
avoid the suspected sleep/wake transition. This is an investigation-specific
precaution, not an Apple requirement for audio or a demonstrated solution to
mid-performance glitches. Leaving the display and DSP running continuously
for days would also change heat and workload; do not treat it as a clean
one-variable proof.

The useful controlled comparison holds USB wiring, power, app build and audio
settings fixed. Begin each trial with confirmed audible output:

| Condition | What it separates |
| --- | --- |
| Stop/start actual audio I/O while remaining awake | Audio stream restart without device sleep. |
| Stop I/O, lock/sleep, wake, restart | Additional effect of sleep/resume. |
| Continue actual playback with the screen locked, if the installed app supports it | Screen locking while streaming, versus stopped I/O and deeper idle. |

Use logs to confirm actual driver StopIO/StartIO and sleep; a transport pause
alone is insufficient. Check audible Maya output before and after each trial,
and preserve a failure before recovery. Match idle durations and repeat;
include longer idle only if short intervals do not reproduce. Do not alter
accessory authorization, charge limit, app policy and cabling simultaneously.

If only the stopped/sleeping condition fails repeatedly, prioritize device/host
resume while I/O is stopped. Failure in the awake stop/start condition instead
shows sleep is unnecessary. Failure during continued locked-screen streaming
would show stopping I/O is unnecessary. None of these outcomes alone assigns
blame to the hub, Maya or iPadOS. All are proposed tests, not completed results.

The [periodic-glitch experiment](next-experiments.md#october-2-priority-periodic-corruption-during-a-performance)
retains priority when that rare symptom is active. Eliminating startup silence
does not by itself validate mid-performance reliability.

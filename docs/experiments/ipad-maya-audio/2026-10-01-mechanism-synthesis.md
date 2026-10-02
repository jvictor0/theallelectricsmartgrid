# 2026-10-01 — Mechanisms that could connect the audio and MIDI failures

This is a ranked working explanation, not a root-cause finding or a demonstrated fix. The user emphasizes that the original periodic symptom had not recurred for at least two weeks, possibly three. Short clean hardware substitutions cannot establish prevention. Separate the rare transition into a susceptible state from the repeated bursts once that state exists.

No new device capture, deployment, reset or hardware intervention was performed for this synthesis. Existing captures and primary technical documentation were reviewed. Clean DSP generation is established context.

## Leading model: bad stream initialization or resume, followed by timing trouble

The leading audio mechanism is a disagreement between USB audio timing and buffer positions after a startup, resume or reconfiguration. Possible locations are the iPad's USB/audio stack, the hub's transaction handling, and Maya's streaming/clock-control implementation. The current evidence does not rank those individual owners reliably.

Audio travels through several independently maintained states: the app's sample buffers, Core Audio's device timeline, USB transfer queues, and the interface's sample clock and buffers. Correct samples at the app boundary do not guarantee correct presentation at the converter. USB audio uses scheduled isochronous transfers without retransmission; a device can stay enumerated while streaming fails. [Microsoft's description of isochronous transfers](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/transfer-data-to-isochronous-endpoints) supports this protocol distinction; Windows recovery APIs are not being proposed as iPad APIs.

The periodic symptom particularly suggests an incorrect rate estimate, unstable clock-control loop, or buffer/timestamp alignment that repeatedly enters and leaves a bad phase. A small uncorrected rate mismatch can accumulate slowly. For scale only, 50 parts per million at 48 kHz is 2.4 samples per second; accumulating 512 samples would take about 213 seconds. This is an illustration, not a measurement of Maya's clocks or downstream buffer size. A constant mismatch alone does not explain seconds-long bursts with clean intervals: a further correction cycle or phase-dependent failure would be needed.

The evidence favoring this family of mechanisms is spontaneous recovery with continued streaming and growing driver timing discrepancy, both in the earlier journal and on October 1. It weakens a simple rule that increasing discrepancy always means increasing audible damage. The private `ioDriftNS` field is not calibrated physical clock error, and the October listening timestamps lack synchronized analog output. There is no demonstrated modulo-buffer relationship or drift threshold. See [the October capture](2026-10-01-periodic-glitches-and-driver-timing.md) and [earlier contrary/supportive cases](research/2026-09-10-drift-derivative-hypothesis.md).

[Apple TN3190](https://developer.apple.com/documentation/technotes/tn3190-usb-audio-device-design-considerations) explains why clocking matters: devices may follow USB timing, adapt their rate, or provide feedback; input traffic can supply output timing in implicit-feedback configurations. It also describes ambiguity when inferring older devices' clock arrangements. We have not established Maya's actual synchronization/feedback mode from endpoint descriptors, so implicit feedback is a possible connection between its input faults and output silence, not a finding.

Persistent silence could be a more severe stuck stream/clock state, but its identity with periodic corruption remains unproven. Restarting media services/usbaudiod and the app failed to clear the observed silent state; an upstream USB reconnect succeeded. That favors state surviving those software restarts, including device firmware, hub or lower host-controller state. A daemon restart can also re-create the same faulty state, so this does not exonerate host software.

## The MIDI failure may be introduced by the recovery

The strongest observed sequence is: Maya silent; iPad-to-hub reconnect restores Maya; WRLD OUT transfers stall while IN still works; WRLD's own cable reconnect restores MIDI. On September 28 the first WRLD OUT stall preceded Maya's re-enumeration. This is evidence against explaining that particular stall as Maya audio traffic consuming the bus. See [the recovery timeline](2026-09-28-silent-maya-usb-reset-recovery.md).

USB directions have separate logical endpoints over the same cable. One stalled OUT endpoint need not stop IN traffic. App port reopening does not by itself clear all host/device endpoint state, and successful CoreMIDI submissions do not acknowledge physical delivery. [USB endpoints and pipes](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-endpoints-and-their-pipes) and [driver recovery operations](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/how-to-recover-from-usb-pipe-errors) describe these distinctions.

A powered hub may keep peripherals powered while its upstream cable is removed. Reconnecting the upstream bus and removing a peripheral's own power are different interventions. A bus reset should reset protocol state, but an implementation defect could leave firmware or a host endpoint improperly initialized. WRLD-only reconnection resets both its device and its per-device host state, so success does not uniquely identify firmware. The sequence may therefore contain two bugs connected by a recovery action, without requiring a simultaneous common failure.

## Hub/power versus scheduling alternatives

The saved registry shows Maya and WRLD both running at 12 Mbps under the hub's 480 Mbps USB2 function. The physical hub also exposes a separate USB3 function. Its USB2 descriptor advertises `bDeviceProtocol=2`, indicating multi-transaction-translator capability; do not assume a single shared translator bottleneck. [Microsoft's hub protocol explanation](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-faq--introductory-level) documents the descriptor meaning. The active translator configuration was not independently established.

The hub remains a plausible common trigger through full-speed transaction translation, suspend/resume, or its peripheral power supply. Stable iPad charging status does not measure Maya's 5 V supply. A brief electrical event could initiate a lasting bad state, but no rail measurement demonstrates one. The September 28 wake/stream-start recurrence with external power already connected means initial power-plug order alone is insufficient. There is no evidence of a PD renegotiation every few minutes.

A recurring system task, host-controller scheduling problem, or power/thermal control cycle is another explanation for minutes-separated bursts. It is lower-ranked than timing/alignment for the periodic symptom, given earlier regular callbacks and evolving waveform cadence, but is not excluded. October diagnostic collection overlaps some later underflows; those cannot all be assigned to the spontaneous fault. The first episode and later 20:47 report prevent collection load from explaining all reported bursts.

Simple steady bandwidth exhaustion and random cable errors fit the recurring clean/bad pattern less well, and do not by themselves explain a persistent one-direction MIDI stall after a reset. Marginal electrical behavior can still trigger state bugs. No brief clean control or missing log message excludes it.

## Discriminating evidence, without assuming an easy reproduction

For persistent Maya silence, the missing narrow recovery test remains **Maya's own USB cable only**, while leaving iPad, hub power and WRLD connected. Prior successful Maya recovery used the entire upstream connection. A Maya-only success would show that resetting the whole hub is unnecessary for recovery; it would still reset both Maya and its per-device host state.

For periodic bursts, the useful measurement is synchronized physical output and USB packet timing/counts, ideally including actual synchronization descriptors and any feedback, while the state is present. This would separate correctly delivered samples with converter trouble from bad packet delivery or timing. Wi-Fi app/system logs alone cannot directly observe that boundary. Ordinary further DSP checks do not resolve it. These are proposed discriminators, not tests started today.

## Local evidence

The registry reviewed here is inside `/Users/joyo/Documents/SmartGridOne/diagnostics/20261002T033818.230853Z-after-periodic-maya-recovery-sysdiagnose/sysdiagnose_2026.10.01_20-38-18-0700_iPhone-OS_iPad_23G83.tar.gz`, members `ioreg/IOUSB.txt` and `ioreg/IOService.txt` under the archive's named root. Temporary extracted copies are under `/private/tmp/maya-periodic-20261001/sysdiagnose-selected/ioreg/`.

The active Maya audio interfaces each expose one endpoint, and there is a separate HID interface with one endpoint. Endpoint address 0x83 must not be called an audio feedback endpoint merely from its address. The registry excerpt does not include the endpoint descriptors needed to establish the audio synchronization modes. Raw artifacts remain local and are not part of this journal update.

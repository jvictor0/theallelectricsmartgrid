# October 3: startup silence, upstream reconnect and PD power topology

The first retained Maya fault begins at **10:25:09 PDT**, during system-sound startup immediately after waking the iPad, before SmartGrid launches and before the user's power changes. Two external-power cycles leave the USB devices connected and the fault ongoing. The user-confirmed iPad-to-hub reconnect subsequently ends the known empty-transfer signature but introduces persistent WRLD.BLDR output-endpoint stalls. **The user still reports silence, including when trying the built-in speakers; the absence of further Maya empty-transfer errors does not establish audible recovery.**

This is a startup-silence investigation. No periodic distortion was reported in this episode. The rare periodic corruption during performance remains the highest-priority symptom, with its separate [experiment plan](next-experiments.md). Clean internal DSP output is already established evidence and was not retested.

All times below are October 3, 2026, America/Los_Angeles (UTC−07:00). The agent collected logs and diagnostic state over Wi-Fi, without deployment, app restart, media-service reset, or physical recovery intervention. Physical changes below were the user's own actions.

## User observations

- Returned to the iPad and opened SmartGrid; audio was silent with moving meters.
- Tried cycling external power and the built-in iPad speakers; reported that silence persisted.
- Confirmed the full USB disconnect around 10:26:14 was unplugging the **iPad-to-hub cable**. It was not a Maya-only reconnect.
- Identified the hub as the Satechi 4-in-1 USB-C hub with PD, [Amazon ASIN B0DHBFDFWC](https://www.amazon.com/dp/B0DHBFDFWC), corresponding to Satechi's ST-H4CPDM product family.
- Reports Maya and WRLD receive power both with charger attached/iPad absent and with iPad attached/charger absent. Asked whether the iPad can be configured never to power peripherals.

The built-in-speaker trial is corroborated by an app route snapshot. Its exact listening interval was not recorded. The later app and system snapshots show Maya reselected; do not assume the iPad remained on speakers when the answer arrived.

## Retained timeline

| Time, PDT | Evidence and interpretation |
| --- | --- |
| 07:45:05.059 / .303 | USB 3 host port reports an unexpected suspend link state; AppleT8122USBXHCI times out waiting for SLEEP. |
| 07:46:06.978–07:46:07.001 | USB3 hub connection lost and re-enumerated. |
| 07:47:27.625–07:47:28.854 | USB3 resume unexpected link state, simulated/deferred connection loss, and re-enumeration. |
| 07:49:48.960–07:50:11.064 | Another suspend/SLEEP-timeout sequence and USB3 reconnect/resume errors. Maya and WRLD are on the USB2 branch; these events do not establish that Maya disconnected. |
| 10:25:08.150–.217 | Display/backlight on and wake records. |
| 10:25:08.864–.898 | SpringBoard creates a SystemSoundsAndHaptics session and requests output on Maya; Maya StartIO follows. |
| 10:25:08.914 / .922 | Driver sets Maya input/output clocks to 44.1 kHz for this system-sound startup. |
| 10:25:09.532 / .540 | First excessive-zero-length-packet warning and first empty-input-transfer messages. This is the first confirmed Maya failure in retained logs. |
| 10:25:16.694 | First SmartGrid system record, after the Maya fault has begun. App log filename is 10:25:18.031; persisted snapshots begin at 10:25:23 with actual 48 kHz / 512 frames. |
| 10:25:18.784 | Maya input-underflow messages begin. |
| 10:25:38.424–10:25:39.565 | iPad changes to battery power, then AC. No full USB disconnect/re-enumeration; Maya failures continue. |
| 10:26:03.317–10:26:08.339 | Second battery/AC cycle, again without full USB disconnect; Maya failures continue. |
| 10:26:13.868–.992 | Transaction errors during removal, then USB2 and USB3 hub hardware-disconnection records. User confirms upstream cable removal. |
| 10:26:16 | App closes WRLD MIDI ports. |
| 10:26:19.075 / .834 | Hub and then WRLD re-enumerate. |
| 10:26:20.156 | WRLD endpoint 0x01 OUT starts repeatedly returning `0xe0005000 (pipe stalled)`. |
| 10:26:21 | App reopens both WRLD ports. Reconnect logic ran; successful CoreMIDI submissions do not confirm wire delivery. |
| 10:26:23.841 | Maya re-enumerates. |
| 10:26:24 | App snapshot reports built-in microphone and Speaker, 48 kHz / 512 frames, one input/two outputs. System route records report unmuted output and virtual volume 1.0. |
| 10:26:25.989 / .997 | Last excessive-empty warning and empty-input-transfer message. |
| 10:26:26.377–.394 | Final Maya StartIO in this sequence. No further empty-input/underflow/transaction errors or USB disconnects appear through the follow-up archive ending 10:40:48.903. |
| 10:27:25–10:30:26 | App snapshots show Maya again, actual 48 kHz / 512 frames, four inputs/four outputs. |
| 10:36:04.769 | Maya driver state dump reports active/running, stable clock, nominal 48 kHz, four-channel 16-bit input/output, output mute NO and both output volume controls at scalar 1.0. These are declared driver states, not measurements of analog output. |
| 10:40:48.903 | WRLD OUT stalls are still present at the end of the follow-up capture. Today's physical MIDI behavior was not separately confirmed by the user. |

The initial archive contains 13 excessive-empty warnings, 6,576 empty-input-transfer messages and 4,565 input underflows. Its 54 transaction errors are confined to the user's disconnect/re-enumeration interval, not spontaneous failures during settled playback. The follow-up archive contains 66,702 WRLD OUT-stall messages, including the 14,022 in the initial capture; these overlapping counts must not be added. Neither decoded selection contains `ioDriftNS` records, which does not prove zero clock drift. Microframe timestamp `calcError` debug fields are estimator residuals, not additional transaction failures.

## What this establishes, and what remains unknown

The first bad transfer precedes this SmartGrid launch and both user power cycles. Together with [September 28's evening recurrence](2026-09-28-evening-recurrence-before-deployment.md), this strengthens an audio-start-after-idle or USB suspend/resume hypothesis for startup silence. Earlier USB3 sleep/resume trouble is a possible precursor, not a proven cause. The retained USB2 records do not show a Maya disconnect before the user's upstream unplug.

We cannot date the beginning of a latent bad state. Logs show when failed transfers first become observable after I/O starts. The most recent preceding app session retrieved is October 1, 20:27:25–21:22:04; it does not establish the last time audio was audibly healthy. Although two days of logs were requested, the selected processes have different retention bounds: USB-audio records begin October 3 at 07:38:08, selected kernel USB/audio records at 07:38:31, powerd at October 2 00:15:04, and audiomxd at October 1 21:44:18. Older driver history is unavailable in this capture.

The upstream reconnect stopped the initial Maya signature and reproduced the known WRLD OUT-stall pattern. It **did not establish that audio recovered**: the user's continued silence report, including the speaker trial, remains unexplained. Running/unmuted driver state cannot validate physical playback. This episode therefore cannot be reduced to a proven Maya-only fault or a proven power-switching fault.

The 44.1 kHz system-sound start is worth retaining, but it is not a necessary prerequisite: September 28's comparable onset was at 48 kHz. Likewise, the initial driver lock-delay request of 650 ms (capped to 600) versus 250 ms after reconnect is an observation, not a decoded health indicator or established mechanism.

## Power topology and the user's proposed restriction

### Later clarification: an upstream cable and coupler are present

The user subsequently confirmed that the **currently failing setup** uses
`iPad → USB-C male-to-male cable → female-to-female coupler → hub's captive plug`,
rather than plugging the hub directly into the iPad. Coupler model, cable model,
exact length and the date this arrangement was introduced are not yet supplied.
The user describes the added arrangement as very short/small, reports stable
operation apart from the investigated faults, and says the reverse orientation
does not work. They marked the working orientation and consistently use it.
This reduces concern about accidental orientation changes; it does not measure
the assembled link's electrical behavior. Do not assume every earlier journal
run used it. USB enumeration does not reveal
these passive components, so prior “iPad-to-hub cable” descriptions do not
establish a direct physical connection.

A suitably wired coupler can work in a fixed orientation. Orientation alone,
however, does not validate the entire extended connection: extra cable and
contacts affect signal quality and power-path resistance, and USB-C's CC/VCONN
connections must remain correct. CC handles attachment, orientation and PD
communication; charging or enumeration alone is not an end-to-end reliability
test. [Infineon CC explanation](https://community.infineon.com/t5/Knowledge-Base-Articles/FAQs-on-CCGX-EZ-PD-CCGx-Product-USB-Type-C-Cables-EMCA/ta-p/247747),
[TI signal-integrity discussion](https://www.ti.com/document-viewer/lit/html/SSZTAQ5/GUID-1C471AB6-06C8-4439-9206-831A76936EE4).

This is an additional candidate to isolate, not an established fault. A direct
hub-to-iPad comparison should keep the charger and downstream devices/cables
unchanged. **Immediate recovery after removing the extension is ambiguous:**
it also reconnects/resets the upstream USB path, which has previously restored
Maya without changing the cable arrangement. Compare repeated matched
idle/sleep/wake trials with the original arrangement and the direct connection.
For rare periodic corruption, record exposure and recurrence; a short clean
direct run cannot prove elimination. USB3 signal issues alone would not
establish the cause of Maya's USB2 audio failures. No physical change or trial
was performed by this journal update.

### Power sources

USB data-host role and power direction are separate: a host can receive power while controlling peripherals. [USB-IF's PD overview](https://www.usb.org/usb-charger-pd) explicitly describes reversible power direction and powered hubs supplying their host. [Satechi's product specification](https://satechi.com/products/4-port-usb-c-hub-with-pd) lists up to 100 W PD input and 75 W host output. These are ratings, not measured consumption.

| Connections | Expected power flow, based on the product function and user observations |
| --- | --- |
| Charger only | Charger → hub → Maya/WRLD; no iPad data host. |
| iPad only | iPad → hub → Maya/WRLD. |
| Both | Charger supplies hub/peripherals and supplies the iPad through PD; the iPad remains USB data host while receiving power. |

At 10:32 the iPad reports external power connected and charging, with a `pdcharger` adapter description and 15,000 mV adapter-voltage metadata. This supports receiving power at that snapshot. It does not reveal the hub's internal circuit or measure the peripheral supply rail. Satechi's public material examined does not specify its source-selection circuit, transition timing, or Fast Role Swap implementation. Do not infer continuous source juggling, a brownout, or a particular PD negotiation from these observations.

**Removing external power alone is not a reliable power reset for this setup.** The user's observation that the iPad alone powers the devices, plus two charger-state transitions without USB re-enumeration today, supports continued operation from the iPad. Conversely, disconnecting the iPad while leaving the charger attached resets the upstream USB connection but need not power-cycle Maya or WRLD. This distinction matters when interpreting earlier recovery tests.

No supported iPadOS setting or app API to force “never supply accessory power while retaining USB data” was found in the Apple documentation reviewed. [Apple documents the iPad's ability to power other USB devices](https://support.apple.com/en-us/108894); its [Wired Accessories controls](https://support.apple.com/en-us/111806) concern connection authorization, not a fixed power role. This is a documented-capability search result, not proof about every private system interface. Satechi's [quick-start guide](https://support.satechi.com/hc/en-us/articles/38844624913435-Quick-Start-Guide-4-Port-USB-C-Hub-with-Power-Delivery-ST-H4CPDM) provides no such switch either.

Hardware could enforce the desired condition: a hub whose downstream power requires the external supply and cannot fall back to host power. Merely being advertised as a powered hub or PD hub does not establish that property; it would need explicit vendor confirmation or measurement. No replacement product has been selected or tested. Changing hubs would also change USB-controller behavior, so improvement alone would not isolate power switching as the cause.

A source-transition fault remains plausible, but today's data does not prove it initiated silence. The historical [September 10 no-PD run](2026-09-10-no-pd-adhoc.md) also reproduced the sporadic USB-error/restart/callback-gap chain with charging absent throughout. That limits a universal PD-only explanation; it does not resolve the separate periodic-distortion mechanism. No new experiment was performed.

## Follow-up: USB sniffing and the available Raspberry Pi 5

The user has a Pi 5 and proposed either an inline USB observer or a controlled device in the spare hub port. These are different experiments; no device was configured or connected during this research.

For the priority periodic-audio symptom, the most direct proposed observation is **iPad → existing hub → passive USB analyzer → Maya**, with the analyzer's capture connection on the Mac and a simultaneous recording of Maya's analog output. Existing registry evidence identifies Maya as full-speed USB (12 Mbps), so capture on that downstream cable does not require a USB3 analyzer. [Cynthion](https://greatscottgadgets.com/cynthion/) supports low/full/high-speed USB capture with Packetry; [Beagle USB 480](https://www.totalphase.com/products/beagle-usb480/) is another documented analyzer. These are candidates, not purchased or validated equipment. Capture duration, dropped-packet reporting, payload retention and timestamp alignment must be verified before trusting a rare-event capture.

The decisive comparison is actual PCM payload and packet cadence at Maya's cable versus its analog output. Missing/corrupted audio data or abnormal delivery timing would localize a problem to the stream arriving at Maya, though one downstream trace cannot by itself separate iPad scheduling from hub forwarding. Correct payload and timing during distorted analog output would strengthen a device-side clock/buffer/DAC or electrical explanation, rather than proving any one component. A further capture upstream of the hub could distinguish forwarding from upstream delivery. This directly observes a later boundary than the already-clean internal recording.

[Raspberry Pi documents Pi 5 gadget mode on its USB-C connector](https://www.raspberrypi.com/news/usb-gadget-mode-in-raspberry-pi-os-ssh-over-usb/), and [Linux provides a USB MIDI gadget function](https://docs.kernel.org/usb/gadget-testing.html). A custom diagnostic MIDI device in the spare port could log its own reset/configuration/suspend/resume events and timestamp MIDI deliberately sent to it. A simultaneous loss on this probe and WRLD would support a wider host/hub event; continued delivery to the probe while WRLD fails would narrow the problem toward WRLD's path or endpoint. It cannot exonerate all shared components because ports, endpoints and transfer types can fail independently. It does not receive Maya's audio or WRLD-addressed traffic through an ordinary gadget controller, and simply attaching it does not cause SmartGrid to send it a heartbeat. Its own supply should be arranged so that the Pi's substantial load does not confound the power experiment. No power wiring design has been selected.

The Pi could also replace the iPad as USB host and use [Linux usbmon](https://docs.kernel.org/usb/usbmon.html) to capture submitted/completed transfers, including isochronous status and payload. usbmon observes the Linux driver's host-controller boundary, not every physical wire event, and it cannot inspect the iPad's controller from a neighboring port. A reproduction under Linux would be informative; a clean run would not clear the iPad-specific path. This is a host-substitution control, not passive observation of the existing failure.

Using the Pi as an active audio proxy would require presenting an audio device to the iPad while driving Maya from a separate host controller. It introduces its own buffers, clocks, timing and power topology. It is a substantial implementation and is not the first proposed instrument for this timing-sensitive problem.

For the power-handoff hypothesis, a **PD analyzer on the iPad-to-hub connection** can log source/sink roles, negotiations and resets; the charger-to-hub connection is a separate PD link. A PD analyzer on one link cannot report every negotiation on the other. [Total Phase's PD analyzer documentation](https://www.totalphase.com/support/articles/217474437-usb-power-delivery-analyzer-user-manual/) describes CC protocol capture and VBUS monitoring separately from USB-data pass-through. Measuring fast voltage dips at Maya's own supply requires an appropriately fast voltage recorder/oscilloscope, not inference from iPad charging status or a slow display-only power meter. A spare-port voltage measurement can reveal a shared supply event but may miss a Maya-port-specific problem.

## Local artifacts and capture limitations

All evidence below remains on the Mac under **`/Users/joyo/Documents/SmartGridOne/diagnostics/`**, outside Git. Only this report and the journal index are committed.

| Evidence | Path beneath diagnostic root |
| --- | --- |
| Initial system log archive, requested two days | `20261003T172823.713070Z-system.logarchive/` |
| Current app log, through 10:30:26 | `20261003T173021.106077Z-app/2026-10-03T10-25-18-031.log` |
| USB tree and battery/adapter state | `20261003T173201.304841Z-startup-silent-state/` |
| Three preceding app logs, September 30 and October 1 | `20261003T173419.658655Z-preceding-app-sessions/` |
| **Incomplete sysdiagnose download** and failure metadata | `20261003T173504.779326Z-startup-silence-after-power-route-changes-sysdiagnose.partial/` |
| Successful follow-up system archive, selected records 10:25:47.002–10:40:48.903 | `20261003T174047.395100Z-system.logarchive/` |
| Decoded selections, excerpts, precise counts and evidence manifest | `20261003-startup-silence/` |

Sysdiagnose was requested at 10:35:05.321 and became ready at 10:37:10.684. The Wi-Fi tunnel reset during transfer, and collection terminated with `ConnectionTerminatedError` at 10:39:05.466. The local tar.gz contains **128,008,142 of 718,757,367 expected bytes**. It is incomplete and unverified, retained explicitly as `.partial`; it was not repaired, extracted or counted as a successful full capture. The subsequent ordinary log archive succeeded and includes the Maya state dump produced during sysdiagnose generation. Its successful retrieval does not complete the missing sysdiagnose bundle.

The diagnostics-relay USB snapshot provides a device tree, not the complete IOService property dictionaries or verified USB session-ID comparison available in the earlier full sysdiagnose. Audio Glitch Trace installation was not checked; it was last reported uninstalled September 28. No analog recording was made. All capture processes have finished; no monitoring or recovery task remains active.

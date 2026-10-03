# October 3: WRLD one-way MIDI and a USB-reset firmware defect

## Finding and confidence

The user reports a reliable reproduction: start with working bidirectional MIDI,
unplug/reconnect only the iPad-to-powered-hub cable, and WRLD → iPad continues
working while iPad → WRLD feedback stops. They confirmed the controller was in
that state during this investigation. Earlier WRLD-only power cycles restored
feedback; restarting SmartGrid did not.

**A concrete receive-endpoint reinitialization defect was found in the YTX
firmware source and reproduced using that source in a local register model.**
The locally saved firmware ELF contains the same defective branch. It is a
strong causal candidate for this hardware symptom, but the firmware actually
flashed on WRLD has not been identified byte-for-byte and no corrected firmware
has been tried on the controller yet. The current hardware state was preserved.

This finding concerns one-way WRLD MIDI. It does not establish a cause or fix
for Maya's periodic audio corruption or startup silence.

## Fresh failed-state evidence

All times below are October 3, 2026, PDT (UTC−07:00). The saved iPad system
archive covers **11:05:05–11:20:06**, according to `log stats`; it includes one
log-loss event, so counts describe retained messages. The app snapshot contains
the session beginning at 11:00:05 and runs through 11:24:17.

| Time | Observation |
| --- | --- |
| 11:13:18.695 / .719 | USB2 and USB3 upstream hubs report hardware connection lost. |
| 11:13:18.716 | WRLD and Maya are removed because the upstream hub is terminating. |
| 11:13:21 | SmartGrid logs both WRLD MIDI ports disconnected. |
| 11:13:24.832 / .833 | USB2 and USB3 hubs enumerate again. |
| 11:13:25.578 / .579 | WRLD enumerates at 12 Mbps and configuration 1 is selected. |
| 11:13:25.904 | First retained WRLD endpoint `0x01` completion with `0xe0005000 (pipe stalled)`, zero bytes transferred. |
| 11:13:27 | SmartGrid successfully reopens both WRLD MIDI input and output ports. |
| 11:20:06.996 | Same OUT failure still present at the end of the capture. |

There are **45,435 retained WRLD OUT pipe-stalled completions** in this interval.
SmartGrid's send counters continue increasing, with native API `errors=0` and
the SysEx queue being submitted. App reconnect handling therefore did run;
successful API submission did not mean USB delivery succeeded.

WRLD also has endpoint-zero transaction errors while enabling remote wake,
followed by a compliance message. These are preserved in the context extract;
they are not sufficient to explain the missing bulk OUT initialization.

The kernel's `pipe stalled` label is **not a wire capture proving that WRLD
transmitted a literal USB STALL handshake**. The chip documentation says a
disabled OUT endpoint discards packets. The precise path from that condition
to Apple's reported pipe status remains unmeasured.

## Firmware mechanism

Inspected checkout: `/Users/joyo/ytx-controller`, branch `joyo`, commit
`a2468bb5e3330648827afd97f7a7722b9a577259`.

Relevant paths relative to that repository:

- `hardware/yaeltexv2/samd/cores/arduino/USB/USBCore.cpp`: static
  `epHandlers[7]` around line 92; `initEndpoints()` around 453; `initEP()` around
  468; `SET_CONFIGURATION` around 852; end-of-reset handling around 893.
- `hardware/yaeltexv2/samd/cores/arduino/USB/SAMD21_USBDevice.h`:
  `DoubleBufferedEPOutHandler` constructor around 236, `release()` around 373.
- `hardware/yaeltexv2/samd/libraries/MIDIUSB/src/MIDIUSB.cpp`: descriptor order
  and endpoint allocation place MIDI receive on OUT 1 and transmit on IN 2
  (`0x82`) in the default non-CDC configuration.

The causal sequence predicted by this code is:

1. WRLD stays powered by the hub when the iPad cable is removed. The firmware's
   receive-handler object remains in RAM.
2. Reconnection includes a USB bus reset. The SAM D21 hardware clears nonzero
   endpoint configuration registers and endpoint interrupt enables/flags.
   This behavior is specified in [Microchip DS40001882G, section 32.6.2.4,
   printed page 696](https://ww1.microchip.com/downloads/en/DeviceDoc/SAM-D21DA1-Family-Data-Sheet-DS40001882G.pdf).
3. The host selects a configuration. Bulk IN initialization runs every time.
   Bulk OUT initialization, however, only constructs/configures a handler
   when its pointer is null. An existing pointer makes it skip the operation.
4. The OUT endpoint therefore remains disabled and its receive interrupt
   remains off, while IN is enabled. WRLD can send but cannot accept new MIDI.
5. A full controller reboot clears the pointer with the rest of startup RAM,
   allowing the initial constructor to configure OUT again.

The [current Arduino SAMD USB core](https://github.com/arduino/ArduinoCore-samd/blob/master/cores/arduino/USB/USBCore.cpp)
recreates an existing OUT handler during initialization. That implementation
supports the need to reinitialize, but is not a patch to copy blindly into
this older fork: the local handler owns two allocated buffers and has no
destructor to free them. Repeated deletion/reallocation would also occur in
USB interrupt handling. A focused repair should restore endpoint and buffer
state safely, with reset/receive concurrency considered.

## Local reproduction and provenance

The saved `reproduce.py` extracts the actual `EPHandler`,
`DoubleBufferedEPOutHandler`, and `USBDeviceClass::initEP` source verbatim. It
compiles them with a minimal register-admission model and sends a four-byte USB
MIDI CC packet through the extracted handler. The modeled bus reset clears
the documented endpoint configuration and interrupt state while preserving
descriptor SRAM and firmware objects.

Observed result:

| Condition | OUT type | OUT interrupt | IN type | Receive result |
| --- | --- | --- | --- | --- |
| Cold boot / first configuration | Bulk | Enabled | Bulk | Packet received |
| USB reset / second configuration | Disabled | Disabled | Bulk | Packet rejected |
| Fresh MCU boot control | Bulk | Enabled | Bulk | Packet received |

This reproduces a real initialization defect in the extracted code. It is a
single-threaded model, not a USB wire simulator, live register inspection, or
test of interrupt/main-loop races. It does not independently establish which
firmware is flashed or prove the cause of the physical incident.

Disassembly of the existing
`/Users/joyo/ytx-controller/ytx-main-controller/build/ytx-main-controller.ino.app.elf`
confirms the same skip: `initEP` begins at `0x1d88c`; the branch at `0x1d8d8`
returns when the stored OUT handler is nonnull. The ELF's identity relative to
the current device is unverified.

The repository and installed Arduino core under
`/Users/joyo/Documents/Arduino/hardware/yaeltexv2/samd` have identical SHA-256
hashes for both inspected files:

- `USBCore.cpp`: `2f6355abd4480f1beb8aa9a15d7b8da3d8b3209dcec1e4bb75e3ab04d05830d5`
- `SAMD21_USBDevice.h`: `41796c2335c2f86b28ac2669c7e11668a0bbe65bb64ac7e42474cb6632912779`

## Implementation handoff and decisive test

The user authorized a new YTX task to fix the identified firmware defect:
[Fix WRLD MIDI receive after USB reconnect](codex://threads/01a10308-37e1-75e1-8f5d-e6e714de83e1).
It was started with the source findings, local reproduction, artifact paths,
evidence limits, and instructions to implement/test in an isolated managed
worktree. No firmware flashing, app restart, or hardware reset was authorized
as part of that task.

The decisive physical validation is a separately approved corrected-firmware
trial: verify working bidirectional MIDI, keep the hub/WRLD powered, repeatedly
disconnect/reconnect only the iPad-to-hub cable, and check actual feedback and
controller input after each cycle. Confirm the built firmware uses the changed
core rather than the unchanged global Arduino installation. Compare iPad OUT
failures before and after. A pass would establish the fix for this reproduction;
it would not resolve the separate Maya investigation.

If the physical symptom persists, read the controller's endpoint configuration,
interrupt and receive-completion state through an independent diagnostic path,
or capture USB traffic. Do not make diagnostic replies depend exclusively on
the broken host-to-controller bulk MIDI channel.

## Local artifacts

Raw artifacts remain on the Mac and were not added to Git or uploaded:

- System archive:
  `/Users/joyo/Documents/SmartGridOne/diagnostics/20261003T182005.198209Z-system.logarchive`
- App log:
  `/Users/joyo/Documents/SmartGridOne/diagnostics/20261003T182505.544252Z-app/2026-10-03T11-00-01-812.log`
- Analysis/reproduction directory:
  `/Users/joyo/Documents/SmartGridOne/diagnostics/20261003-wrld-usb-reset/`

The analysis directory contains `current-usb.log`, `reconnect-context.log`,
`archive-stats.txt`, `current-summary.json`, `source-provenance.json`,
`existing-build-initEP.disassembly.txt`, `reproduce.py`, generated `repro.cpp`,
the local `repro` executable, and `repro-result.txt`. Its `manifest.json` records
sizes and SHA-256 hashes for those ten files, verified after preservation.
No capture remains running.

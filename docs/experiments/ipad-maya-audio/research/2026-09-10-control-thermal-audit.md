# Archived control thermal audit — September 10, 2026

The system archives did preserve power/temperature evidence, but the recovered SonoBus and longer Drambo records do **not** establish that either control was HOT or continuously nominal. They contain no in-run absolute NSProcessInfo-equivalent thermal classification or thermal-pressure value. Absence of a transition log is not a nominal measurement. The controls remain useful, with significant thermal and configuration gaps.

## Recovered control evidence

| Control | Exposure and output | Rate/buffer | Thermal record |
|---|---|---|---|
| SonoBus | 3600 s analog + USB logs clean; 3555 s after startup exclusion | Verified48k/256; active app channels and native callback distribution unknown | No absolute classification or pressure change found |
| Drambo longer | 790.773 s; clean listening and USB logs; no analog capture |48k/four-channel RemoteIO formats; callback buffer unknown | No in-run classification; nominal context about15min earlier |
| Drambo after SmartGrid |226.982 s; no >=20ms analog gap or USB failures/restarts |48k; callback buffer unknown | Private thermal-level transitions2→1→0, not public API states; prior SmartGrid and post-run termination snapshot nominal |
| Desktop SmartGrid |3600 s;337,549 callbacks, no new xrun/USB error or >=20ms analog gap |48k/512 actual callbacks | App thermal trace nominal0 throughout on the Mac |

## SonoBus

Window: 2026-09-10 13:25:49.141815-0700 through 2026-09-10 14:25:49.141815-0700. Retained analog recording; zero >=20 ms near-silence candidates, zero short/long flat candidates or dense periodic episodes; zero USB transaction failures/restart/zero-length/ioDrift matches. MAYA 48000 Hz, actual aggregate 256 frames; app active channel masks and native callback frame distribution unknown.

No in-run absolute public thermal-state or OS pressure value found. No relevant transition found. NOT established hot and NOT established continuously nominal.


Battery/power: 182 archived symptomsd snapshots cover 2026-09-10 13:25:51.985654-0700 through 2026-09-10 14:25:37.140911-0700, with at most 20.088s between samples. Raw battery-temperature first→last is 3009→2879; range 2869–3009. If the private field is hundredths of Celsius, that is 30.09→28.79°C, range 28.69–30.09°C. Units/calibration are not established in these logs. These are battery measurements, not the SoC, hub or MAYA.

- [2026-09-10 13:25:51.985654-0700](/private/tmp/smartgrid-sonobus-thermal-broad-audit-20260910.json:2557): external-power=1, battery-charging=1, fully-charged=0, raw battery-temperature=3009.
- [2026-09-10 13:30:38.212169-0700](/private/tmp/smartgrid-sonobus-thermal-broad-audit-20260910.json:3670): external-power=1, battery-charging=0, fully-charged=1, raw battery-temperature=2969.
- [2026-09-10 14:25:37.140911-0700](/private/tmp/smartgrid-sonobus-thermal-broad-audit-20260910.json:10053): external-power=1, battery-charging=0, fully-charged=1, raw battery-temperature=2879.

Thermal daemon logging was present: 1449 rows in the measured window, 2026-09-10 13:25:52.041513-0700–2026-09-10 14:25:47.050536-0700, at most 5.048s apart. Most rows are repeated property-read errors, which provide logging coverage but no absolute pressure value. OS logs use iPad wall-clock timestamps; analog comparison uses nominal cross-device clock alignment, not a shared sample clock. SonoBus log show warned of wall-clock adjustment; rows filtered individually by timestamp.

No application callback trace is available for this control. Original exposure/configuration analysis: [2026-09-10-sonobus-control.md](../2026-09-10-sonobus-control.md).

## Longer Drambo

Window: 2026-09-09 18:19:48.014000-0700 through 2026-09-09 18:32:58.787000-0700. User clean listening report; no analog recording. Zero settled USB errors/restarts/rate changes/zero-length warnings. 351 USB timestamp diagnostics. MAYA 48000 Hz, four-channel RemoteIO input/output formats. Actual callback frame size unknown.

No in-run absolute public thermal-state or OS pressure value found. Earlier 18:04:36.650059 app-termination context explicitly nominal, about 15 min before this run; no in-run continuous classification.

- [2026-09-09 18:04:36.650059-0700](/private/tmp/smartgrid-drambo-long-thermal-broad-audit-20260910.json:13742): exited with exit reason (namespace: 10 code: 0xdeadfa11) - OS_REASON_SPRINGBOARD | <RBSTerminateContext| domain:10 code:0xDEADFA11 explanation:killed from app switcher  ProcessVisibility: Background ProcessState: Suspended ThermalInfo: (     "Thermal Level:   0",     "Thermal State:   nominal" ) reportType:None maxTerminationResistance:Interactive>, ran for 1768101ms

Battery/power: 40 archived symptomsd snapshots cover 2026-09-09 18:19:57.148909-0700 through 2026-09-09 18:32:57.132588-0700, with at most 20.027s between samples. Raw battery-temperature first→last is 3019→2969; range 2969–3019. If the private field is hundredths of Celsius, that is 30.19→29.69°C, range 29.69–30.19°C. Units/calibration are not established in these logs. These are battery measurements, not the SoC, hub or MAYA.

- [2026-09-09 18:19:57.148909-0700](/private/tmp/smartgrid-drambo-long-thermal-broad-audit-20260910.json:28809): external-power=1, battery-charging=0, fully-charged=1, raw battery-temperature=3019.
- [2026-09-09 18:32:57.132588-0700](/private/tmp/smartgrid-drambo-long-thermal-broad-audit-20260910.json:30636): external-power=1, battery-charging=0, fully-charged=1, raw battery-temperature=2969.

Thermal daemon logging was present: 319 rows in the measured window, 2026-09-09 18:19:52.456266-0700–2026-09-09 18:32:57.413380-0700, at most 5.006s apart. Most rows are repeated property-read errors, which provide logging coverage but no absolute pressure value. OS events use iPad timestamps; recorder-to-iPad correlation is wall-clock/event alignment rather than hardware synchronization. In short run only SmartGrid gaps support fitted 0.145307 s offset, 12.368 ppm drift, 2.088 ms maximum residual; Drambo has no callback trace.

No application callback trace is available for this control. Original exposure/configuration analysis: [smartgrid-long-drambo-report.md](/private/tmp/smartgrid-long-drambo-report.md).

## Drambo after SmartGrid

Window: 2026-09-09 22:05:52.697000-0700 through 2026-09-09 22:09:39.679000-0700. Analog capture plus logs: no >=20 ms near-flat gap and zero settled USB errors/restarts. Subtle periodic-hole exclusion is less strong than SonoBus modern full-duration detector result. MAYA 48000 Hz; Drambo exact callback frame size unknown. Immediately follows SmartGrid at verified 48k/512, zero app inputs/four outputs on same connected hardware.

No in-run NSProcessInfo trace. thermalmonitord mTLL/kernel thermal-level went 2 to 1 at 22:06:47.29 and 1 to 0 at 22:07:12.29; NOT the NSProcessInfo 0–3 scale. Previous SmartGrid app trace nominal; 22:09:50.835094 post-run termination context explicitly nominal.

- [2026-09-09 22:04:12.293238-0700](/private/tmp/smartgrid-drambo-short-thermal-audit-20260910.json:3682): <Notice> mTLL = 2
- [2026-09-09 22:06:47.294639-0700](/private/tmp/smartgrid-drambo-short-thermal-audit-20260910.json:5666): <Notice> mTLL = 1
- [2026-09-09 22:07:12.295366-0700](/private/tmp/smartgrid-drambo-short-thermal-audit-20260910.json:6038): <Notice> mTLL = 0
- [2026-09-09 22:09:50.835094-0700](/private/tmp/smartgrid-drambo-short-thermal-broad-audit-20260910.json:47900): Sending terminate request: <RBSTerminateRequest| predicate:<RBSProcessPredicate <RBSProcessHandlePredicateImpl| app<com.theallelectricsmartgrid.smartgridone(1F41222D-2A59-442C-8218-5AB943B89696)>:12931>> allow:(null) context:<RBSTerminateContext| domain:10 code:0xDEADFA11 explanation:killed from app switcher  ProcessVisibility: Background ProcessState: Suspended ThermalInfo: (     "Thermal Level:   0",     "Thermal State:   nominal" ) reportType:None maxTerminationResistance:Interactive>>
- [2026-09-09 22:09:50.865868-0700](/private/tmp/smartgrid-drambo-short-thermal-broad-audit-20260910.json:47931): exited with exit reason (namespace: 10 code: 0xdeadfa11) - OS_REASON_SPRINGBOARD | <RBSTerminateContext| domain:10 code:0xDEADFA11 explanation:killed from app switcher  ProcessVisibility: Background ProcessState: Suspended ThermalInfo: (     "Thermal Level:   0",     "Thermal State:   nominal" ) reportType:None maxTerminationResistance:Interactive>, ran for 428603ms
- [2026-09-09 22:04:12.293709-0700](/private/tmp/smartgrid-drambo-short-thermal-broad-audit-20260910.json:11657): performSpecificCommandGated: Thermal level changed: 2 (Current thermal level: 0)
- [2026-09-09 22:04:12.293774-0700](/private/tmp/smartgrid-drambo-short-thermal-broad-audit-20260910.json:11688): Thermal level: 2
- [2026-09-09 22:06:47.296075-0700](/private/tmp/smartgrid-drambo-short-thermal-broad-audit-20260910.json:37663): performSpecificCommandGated: Thermal level changed: 1 (Current thermal level: 2)
- [2026-09-09 22:06:47.296208-0700](/private/tmp/smartgrid-drambo-short-thermal-broad-audit-20260910.json:37725): Thermal level: 1
- [2026-09-09 22:07:12.296629-0700](/private/tmp/smartgrid-drambo-short-thermal-broad-audit-20260910.json:38466): performSpecificCommandGated: Thermal level changed: 0 (Current thermal level: 1)
- [2026-09-09 22:07:12.296737-0700](/private/tmp/smartgrid-drambo-short-thermal-broad-audit-20260910.json:38528): Thermal level: 0

Battery/power: 12 archived symptomsd snapshots cover 2026-09-09 22:06:12.159923-0700 through 2026-09-09 22:09:32.146201-0700, with at most 20.006s between samples. Raw battery-temperature first→last is 3379→3289; range 3259–3379. If the private field is hundredths of Celsius, that is 33.79→32.89°C, range 32.59–33.79°C. Units/calibration are not established in these logs. These are battery measurements, not the SoC, hub or MAYA.

- [2026-09-09 22:06:12.159923-0700](/private/tmp/smartgrid-drambo-short-thermal-broad-audit-20260910.json:31432): external-power=1, battery-charging=0, fully-charged=0, raw battery-temperature=3379.
- [2026-09-09 22:08:32.149220-0700](/private/tmp/smartgrid-drambo-short-thermal-broad-audit-20260910.json:40477): external-power=1, battery-charging=1, fully-charged=0, raw battery-temperature=3259.
- [2026-09-09 22:09:32.146201-0700](/private/tmp/smartgrid-drambo-short-thermal-broad-audit-20260910.json:41373): external-power=1, battery-charging=1, fully-charged=0, raw battery-temperature=3289.

Thermal daemon logging was present: 92 rows in the measured window, 2026-09-09 22:05:57.297938-0700–2026-09-09 22:09:37.301132-0700, at most 5.001s apart. Most rows are repeated property-read errors, which provide logging coverage but no absolute pressure value. OS events use iPad timestamps; recorder-to-iPad correlation is wall-clock/event alignment rather than hardware synchronization. In short run only SmartGrid gaps support fitted 0.145307 s offset, 12.368 ppm drift, 2.088 ms maximum residual; Drambo has no callback trace.

No application callback trace is available for this control. Original exposure/configuration analysis: [report.md](/private/tmp/smartgrid-ipad-no-input-analysis/report.md).

## How strongly are the controls matched?

SonoBus is the strongest alternate-app exposure: a full hour with analog and active USB daemon evidence, matching the substantial dropouts and periodic blanking detectors. Its48k/256 buffer differs from SmartGrid48k/512. A public release/source match does not prove the installed binary’s source configuration. Its source playback, active channel masks, CPU/UI workload and MIDI behavior also differ.

The short Drambo comparison is closely adjacent to a failing SmartGrid run on the same connected hardware, with analog coverage for both. It lasted3m46.982s, not a measured five minutes. The longer Drambo control lasted13m10.773s, but lacks analog recording and callback timing. A separate earlier cross-app Drambo trial lasted about3m34s with about3min at48k, and SmartGrid was also clean in that short sequence. No exact five-minute Drambo exposure was established by this bounded audit.

Long Drambo battery snapshots are roughly29.69–30.19°C if the conventional hundredths scale applies; SonoBus roughly28.69–30.09°C. Short Drambo is warmer (roughly32.59–33.79°C) and cools while private thermal-level values decline. None establishes a HOT/serious public thermal state. Parent’s latest SmartGrid battery series is3289→3550 (roughly32.89→35.50°C under the same unit assumption), but battery temperature alone does not identify the OS thermal threshold.

Charging is not a held constant: SonoBus changes from battery-charging1 to0/fully-charged1 at13:30:38; long Drambo is charging0/fully-charged1; short Drambo changes charging0→1 by22:08:32. All recorded in-window snapshots remain externally powered. Some higher-level services call an attached route ‘Charging’ even when battery-charging is0; this is not a contradiction to collapse or a current/watt measurement.

The remaining alternatives include session/buffer/channel configuration, different CPU/UI/MIDI scheduling load, OS thermal/DVFS policy, accumulated USB clock/drift state and reset history, and hardware power/temperature effects. These controls show the hardware can work with other application conditions; they do not isolate the initiating defect to SmartGrid source code. They also do not establish that all clean controls were in the same hot regime. Parent’s latest cooled result includes3failures after serious, and charging was already off before all21failures, so neither universal protection by serious state nor a charging-stop-at-serious account fits all observations.

Desktop SmartGrid supplies a strong one-hour48k/512 code/workload comparison with measured nominal Mac thermal state, but the host/driver/platform differs and it does not hold iPad temperature or charging constant. The full desktop WAV has since been deleted according to the retained research evidence; arrays and summary remain.

## Audit scope and retained files

Only existing local reports, captures and saved logarchives were read. No devices were queried, no experiments were run, and no application or repository code was changed. Raw thermal/power decodes use bounded windows and are retained with their timestamps, JSON indices, line numbers and SHA256 hashes in the machine-readable evidence. SonoBus’s archive reported a wall-clock adjustment; individual timestamps were filtered explicitly. Raw battery values are retained without treating their inferred Celsius conversion as public thermal state.

Machine-readable evidence: [smartgrid-control-thermal-audit-20260910.json](/private/tmp/smartgrid-control-thermal-audit-20260910.json).

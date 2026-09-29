# Thermal behavior and controllable conditions — September10,2026

Apple describes system-wide responses to increased thermal state: discretionary background work such as photo analysis can pause at fair; at serious, performance is affected and some framework workloads and background restoration are reduced or paused. These are OS/framework responses; application workload reductions require explicit app behavior. Our source search finds SmartGrid only samples/logs NSProcessInfo thermalState and lowPowerModeEnabled; it does not change DSP/UI/MIDI behavior based on them. [Apple WWDC19](https://developer.apple.com/videos/play/wwdc2019/422/).

At sufficiently high internal temperature, charging can slow/stop and the display can dim; these are possible device protections, not changes established in our session. We have not measured iPad charging current, hub temperature or MAYA temperature. [Apple temperature guidance](https://support.apple.com/en-us/118431).

Xcode Device Conditions can induce serious thermal state without physically heating the device, and Apple describes system behavior under the condition as matching the reported state. It is a floor: it cannot force a genuinely hotter device down to nominal. Turning the profile off does not physically cool the device. [Apple thermal testing explanation](https://developer.apple.com/videos/play/wwdc2019/422/).

## Verified locally on this iPad

Read-only DVT ConditionInducer enumeration at16:12 exposes ThermalFair, ThermalSerious and ThermalCritical. ThermalCondition is inactive, activeProfile empty. The GPUPerformanceState group also exposes minimum/medium/maximum profiles and is inactive. No CPU-frequency control or charging-disable condition was returned. The authenticated Wi-Fi developer connection can access this service; no condition was enabled in this check. Raw enumeration:/private/tmp/smartgrid-ipad-midi-retest-20260910.available-conditions.json.

The existing archive records audiomxd disengaging a Thermal mxCoreSession at15:21:47.027 near the iPad transition. The specific private policy's consequences are not established, and public app controls to reproduce that individual action have not been identified. Do not equate its internal Moderate label with NSProcessInfo Serious or assume sample rate/block size changed; the trace remains48k/512.

## Queued experiment after MIDI-output verification

Use the same binary/workload with physical MIDI delivery confirmed. Once naturally nominal, compare no thermal condition with induced Serious while keeping the developer connection and monitoring activity matched. Verify reported thermal state and whether the same audiomxd policy event occurs, and score both analog symptoms. The current hot restart is a separate test and has no induced condition. GPU minimum could later separate GPU performance from the full thermal response, but has not been activated. Missing LEDs mean the preceding physical MIDI traffic may have stopped; handler submissions do not exclude that alternative explanation.

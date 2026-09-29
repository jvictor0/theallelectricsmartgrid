# Explicit 48 kHz enumeration test — September 11

User authorized implementing the narrow initialization change and running the test. Set JUCE_IOS_AUDIO_EXPLICIT_SAMPLERATES=48000 in both iOS Xcode target configurations and the iOS Projucer exporter. No JUCE source rewrite needed; the existing explicit-list branch bypasses active rate enumeration. This is the only functional change relative to the previous native-clock build. Prior diagnostic/application source hashes match; compiler response files include the macro; same iPhoneOS18.5 SDK. Build and strict signature verification passed. Baseline package remains retained for reversal.

Signed binary SHA256: 7de6c81de5ba5012ce3f97617fbb07b6deff655418e22877dbcaebdf7e21c86f.

In-place Wi-Fi upgrade completed19:45:53, patch/config hashes unchanged. Normal/autoplay/UI/MIDI launch PID30204 verified. Preflight lifecycle36events, no loss, preferred-rate requests[48000] only. The prior baseline gate correctly rejects its requests[4000,192000,45100,44100,48000,4000,192000,45100]. Native settled callbacks48k/512, zero app inputs/four outputs, inputEnableIO0, Playback/Default/options1. MIDI worker running;586SysEx submitted at preflight with no queue faults. Analog K-Mix3/4 signal−43.1dBFS; recorder stderr empty. Startup native entry gaps43.255ms and34.122ms are preserved and excluded from steady measurement; their presence is not a claim of a spontaneous USB error.

**Thermal limitation:** preflight thermalSerious, whereas previous measured baseline was nominal. No induced condition is active. Battery94%, externalconnected, chargingfalse. A quiet run requires cooler repeat and reversal before attributing benefit; failures remain useful for checking whether exploratory probing is necessary.

Analog recording started19:46:19PDT with20minute maximum. Measured900second interval started19:47:35.714PDT, scheduled end20:02:35.714. Controller then stops analog, downloads coherent native/lifecycle/app prefixes, and collects1800second iPad archive. App left running. No kernel bursts in this incidence test; previous baseline had failures outside its bursts. Capture-overhead conditions therefore are not perfectly identical, and a positive/negative contrast should be reversed before causality claims.

Raw root: /private/tmp/smartgrid-explicit48-trial-20260911. State/status/events, build/install/preflight/enumeration-gate, source configuration snapshots and scripts are preserved there. Current status: measuring. A30minute heartbeat checks completion and performs analysis; no short-interval user notifications. Score USB failures, restart/callback gaps, long analog interruptions and short/periodic blanking separately. No test result yet.


## User stopped the hot pilot for passive cooling

The user requested stopping playback and waiting for nominal instead of continuing the Serious-state run. Controller interrupted19:49:55PDT, analog stopped, coherent native/lifecycle/app prefixes retained (19,464native records). Controller reports stopped_with_error/CancelledError(User stop), which is intentional cancellation, not an unexplained capture failure. No automatic archive was collected on this canceled path. Measured pilot lastedabout140seconds; do not call it the planned fifteen-minute result or a clean run.

SmartGridPID30204 receivedSIGTERM19:50:10 and device API verified bundlePID0 at19:50:12. No thermal override, hardware or power change. Thirty-minute monitor now waits for passive cooldown, then starts a fresh same-build capture only after nominal is verified. If a brief launch probe remains elevated, stop again and wait; preserve probe separately. Fresh artifact root required; priorSTOP/data must not be overwritten.


## Nominal confirmed and cooled trial resumed

At user request, checked after app-off cooldown since19:50:12. Brief launch preflight at20:08:42 verified thermal0 across last200settled callbacks, no induced conditions, actual48k/512, normalDSP/UI/MIDI, zeroinputs/fourMAYAoutputs. Startup enumeration gate passed, preferred-rate requests[48000]. Same installed explicit48 binary, no reinstall or source change. Patch/config preserved. AppPID30347. Native preflight has no sequence holes/reported dropped records. K-Mix analog about−43.1dBFS.

Fresh raw root/private/tmp/smartgrid-explicit48-cooled-trial-20260911-r1. Measured interval starts2026-09-11T20:09:13.144833-07:00; planned900second end2026-09-11T20:24:13.144833-07:00. Recording/controller verifiedrunning; controller automatically stops recording and collects prefixes/system archive. Thermal evolution during the interval still needs final analysis. Thirty-minute monitor now follows this cooled run; interrupted hot pilot remains separate.


## Completed cooled result

All900seconds stayednominal.30USB/restart/native/analog long failures plus a brief periodic blanking burst remain, despite verifiedonly48000 rate requests. The change does not eliminate either symptom. [Full result](2026-09-11-explicit48-cooled-result.md). Recording and collection finished; no new deployment/test started.

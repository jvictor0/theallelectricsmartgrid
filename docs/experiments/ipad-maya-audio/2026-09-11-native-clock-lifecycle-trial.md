# Native clock/lifecycle trial — September 11

## Authorized protocol and live state

User reports iPad plugged into the system and ready. Wi-Fi connected to paired iPad15,5 on iPadOS26.6.1; PD connected and charging,93% battery. K-Mix on Mac captures analog inputs3/4 at48k24-bit. MAYA/iPad route verified; current WB physical topology is the user's existing system, MIDI submissions verified but no new visual LED check requested.

Root `/private/tmp/smartgrid-clock-lifecycle-trial-20260911`; controller state `status.json`, exact stage markers `events.jsonl`. Analog recorder started18:48:01, before launch, maximum30minutes; controller should stop it at measured interval end. Source/controller snapshots saved in root. Four combined10-second bursts with bounded drain,60seconds baseline before first,30seconds between and120seconds after final. Continuous native metadata/lifecycle and analog recording; no thermal override. Full logs collected after measurement, app left running. Main desktop app was already absent from process check; no desktop process terminated by this experiment.

## Installation/startup exclusions

Initial IPA rejected because macOS AppleDouble sidecar `._SmartGridOne.app` appeared as an app bundle. Saved failure `install.json`. Repacked without sidecars; executable unchanged. In-place installation succeeded18:48:41 (`install-r2.json`); patch/config hashes preserved. App launched normal/autoplay around18:49, installed binary SHA256 f3aabe8df0a75aa968d48bd3ce9261d977d239141583e401295a19e370748f69. Setup/preflight/device transfer times are excluded from measured native fault conclusions; exact measured start in events.

## Preflight evidence

Normal DSP/UI/MIDI enabled,48k/512 settled for final200app callbacks,zero app inputs/four outputs, MAYA44 USB+ route. Direct RemoteIO readback: input bus1 EnableIO=0, output bus0 EnableIO=1, stream48k four planar float32channels, MaximumFramesPerSlice4096(capacity, not delivered frame count), Playback category/Default mode/options1,10.667ms session duration. Thermal nominal; low-power off. MIDI worker running and419SysEx submitted with no queue faults at snapshot.

904native records decoded,0sequence holes/drop reports,PCM omitted. Scalar valid in all904, range1.0–1.000006591; median1.000005000. These are reported clock relationships, not measured physical oscillator drift. New bookkeeping wall cost median0.208us,p991.417us,max2.292us; excludes entry hook/queue commit. Native processing includes startup max19.436ms; do not claim all startup callbacks met deadline.50lifecycle events retained with0loss/nonzero call result, including actual temporaryPlayAndRecord→Playback setup sequence. Existing zero-input experiment already selected Playback; this readback does not make category selection a new experiment.

Analog last5seconds RMS approximately−43.36dBFS confirms signal. App/library patch config retained. Preflight file prefixes stop at published writer byte counts while app runs, so metadata statusrunning is expected; not a claim of a complete stopped capture. Queue/record counts match the saved prefix.

## Completed analysis

Capture and archive collection finished at18:59:53PDT. The actual measured interval was18:51:27.395548–18:59:06.663194,459.267646seconds (7m39s), not30minutes. Recording included preceding setup,665.002667seconds total. App was left running; analog recording stopped at interval end. Analysis completed after the user's30-minute check-in.

### Sporadic long interruptions

The measured interval contains14native entry gaps of165.374–175.546ms,14MAYA endpoint0x82 transaction-error reports(status0xe00002ed),14following HAL restarting-IO reports, and14long analog interruptions. Three earlier failure/gap pairs occurred during settling;17total native/log pairs are preserved. Analog low-variation interiors are171.500–177.833ms, with shorter noise-floor cores because the analog signal settles. Matching ordered analog end edges to native returns with one shared offset leaves−8.543 to+4.861ms residuals; the recorder lacks a first-sample host timestamp, so this is correspondence, not absolute-latency calibration.

All42,847measured native callbacks are512frames/48kHz. Native processing range3.513–5.143ms,median4.645ms,p994.969ms, against10.667ms budget. No measured overrun, callback-lock failure, skipped application callback or nonzero native return status. These return statuses are not downstream USB completion statuses. All measured thermal samples are nominal; low-power off. Direct preflight input EnableIO readback remains0 despite input-endpoint driver errors. No observed app/JUCE lifecycle event during the measured interval; all50events are startup. AudioUnit pointer stays constant. The system's lower-level HAL restarts therefore do not require an app-visible route/interruption event or app-triggered AudioUnit recreation.

### Raw kernel ordering

Four bursts retain38.868797seconds,17,409,912events,2,066,433scheduler switches. No reported lost-event markers or per-CPU switch-chain mismatches. Five distinct raw USB failure groups appear in bursts1,2,4; each has two adjacent status-bearing rows from AppleUSB20HubPort@00140000. Private arguments remain undecoded; no physical bus-packet capture is claimed.

Using common mach ticks directly: four errors happen inside a normally timed native callback; the fifth happens2.483ms after its callback exits. All five callbacks finish within budget. RemoteIO then remains in TH_WAIT until recovery, with no intervening runnable event. Once awakened, wake-to-run delay is0.917–1.792microseconds. UI and MIDI workers continue to receive CPU during the wait. This argues against audio being runnable but starved by UI during the long gap. It does not establish the initiating cause of the USB error, nor exclude system activity affecting the USB path before the error.

Named unified-log timestamps are19.909–22.876ms later than stackshot-derived raw-event wall times. Preserve that discrepancy; use native/raw shared ticks for fine ordering. In particular, the fact that a named log report follows a callback exit does NOT mean the earliest raw error did. Earlier experiments' ordering must not be copied onto these five events.

### Clock and shorter waveform evidence

The reported rateScalar returns to exactly1.0 at every resumed gap. Immediately before most failures it is roughly−15to−18ppm from unity; one second closely spaced failure occurs at unity. Measured maximum+1145.281ppm occurs1.004seconds AFTER the18:56:42recovery. All305callbacks above+100ppm occur0.502–2.241seconds after a recovery. This run does not show a common escalating rateScalar precursor; large observed spikes are post-recovery. Reported timestamp relationships are not direct physical oscillator measurements and do not rule out an unobserved clock/feedback problem.

The analog analysis separately identifies18abrupt short stereo plateaus, refined interiors0.583–0.854ms. They are sporadic, with1.265–38.709second spacing, and are not coincident with the17long native gaps. No sustained periodic near-zero blanking train was detected. These short analog features do not establish their location/source; this run intentionally omitted outgoing PCM. The original glitch-free internal DSP recording remains prior evidence against the DSP as the source of the established long-dropout symptom; nothing here invalidates it.

Seven long interruptions occur in baseline and seven between trace request and drain completion. These are unequal exposures, not a causal profiler comparison. Four traces cover only38.9seconds, so absence of an event class outside that coverage is not evidence of absence. Recorded native/lifecycle prefixes have no reported queue loss or sequence holes; metadata statusrunning is expected because coherent prefixes were downloaded while the app continued.

### Evidence and implications

Raw and derived artifacts: `/private/tmp/smartgrid-clock-lifecycle-trial-20260911`.

- `native-analysis.json`, `correlation.json`: scoped counts, all17error/gap/restart pairs, scalar analysis, provisional analog alignment.
- `final-decoded/summary.json`, `clock-trend.csv`, `clock-trend.svg`: version2native decoding and clock trends.
- `kernel-analysis/kernel-summary.json`, `report.md`: five shared-tick callback/fault correlations and retained scheduler/USB coverage.
- `analog-analysis/report.md`, `accepted-intervals.csv`, `long-candidates.png`, `short-candidates.png`: reviewed waveform intervals and periodic screen.
- `system.raw.json`, `system-signals.json`, `system.logarchive`: retained named driver/HAL evidence.

This pass strengthens the USB-failure→system recovery→missing callbacks/analog interruption account. New diagnostics did not identify app-driven lifecycle resets, callback overruns, or a shared growing rateScalar precursor. The next analysis question is the initiating USB failure/clock-feedback behavior; treating the long gap as DSP lateness is not supported. Keep the short plateaus and the earlier sustained periodic symptom separate until a common mechanism is demonstrated. No fix is claimed and no further app/hardware configuration change was made during analysis.

Expanded filter coverage limitation:0x0524,0x0536,0x0605,0x060e and0x36ff yielded no events in these bursts. Received classes include scheduler0x0140,workgroup0x01ab,USB0x052d andmetadata. Enabling those additional filters therefore supplied no new event detail about the initiating fault in this run.

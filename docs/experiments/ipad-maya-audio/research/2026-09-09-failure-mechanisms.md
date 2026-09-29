# Two audio failure patterns: mechanistic assessment

The tone experiment supports a continuing render timeline whose output is periodically blanked farther along the audio path. It does not reproduce the earlier driver-stop/restart mechanism. The strongest new numerical evidence is that the recovering waveform has hole starts on a 384-frame grid and hole ends on a 512-frame grid, while intact tone fragments stay in phase across the holes. This substantially narrows the kind of timing defect worth testing, without locating its owner or identifying a ring-buffer implementation.

This assessment uses existing recordings, callback arrays, and system extracts only. No app code, configuration, deployment, or device state was changed. Reproducible numerical checks are in [failure-mechanism-checks.py](/private/tmp/smartgrid-research-20260909/failure-mechanism-checks.py), with output in [failure-mechanism-checks.json](/private/tmp/smartgrid-research-20260909/failure-mechanism-checks.json).

## Established distinction

| Observation | Earlier isolated failures | Sustained tone corruption |
| --- | --- | --- |
| USB evidence | Endpoint 0x82 transaction failure, then stop/start | Zero-length input transfers and drift changes; no playback transaction failure or restart |
| App callback delivery | Pauses for about 0.17–0.37 s in the newer normal-DSP sessions | Continues at 512 frames, with thirteen isolated roughly 12.8 ms spacings |
| Native xrun diagnostic | Increments with the newer matched restart events | Remains 2 throughout the capture |
| Analog output | One long dropout per failure/recovery | Sub-buffer holes that recur and change shape for minutes |
| Render workload | Typically milliseconds; no measured playback DSP overrun | Median 20 us, maximum 32 us |

The changing restart lock delay accounts for much of the long-dropout duration: 24, 100, 200, and 300 ms recovery settings accompany approximately 0.09, 0.175, 0.277, and 0.365 s analog gaps. That identifies a recovery mechanism; it does not identify what originally caused the endpoint transaction failure.

The tone run is a counterexample to normal synthesis, ProcessFrame/ProcessSample, or their callback-driven MIDI/state work being necessary for the periodic symptom. It is not a counterexample to every remaining app workload or to the audio framework having a timing defect. Its xrun diagnostic observes sample-time continuity, so continuous timestamped callbacks can coexist with bad output samples.

## New checks on the actual waveform

I checked three-second windows of the original 48 kHz, 24-bit stereo WAV. A separate detector requires both channel magnitudes to remain below a direct amplitude threshold for at least twelve samples. It does not use the original rolling-variance detector. Thresholds of 0.0002, 0.0005, and 0.001 full scale yield the same hole counts in each tested window; zero crossings in the healthy 1 kHz tone do not satisfy this duration requirement.

| Capture window starts | Holes in three seconds | Repeating arrangement | Representative widths at 0.001 threshold |
| --- | ---: | --- | --- |
| 20 s | 0 | Clean | None |
| 417 s | 281 | About one per 512 frames | About 42–74 frames in an early local sequence |
| 440 s | 375 | Four unequal fragments per 32 ms | More complex, not a uniform 125 Hz pulse train |
| 479 s | 375 | Four unequal fragments per 32 ms | More complex, not a uniform 125 Hz pulse train |
| 485 s | 281 | Three per 32 ms; start spacings 8, 8, 16 ms | About 38, 166, 296 frames |
| 498 s | 188 | Two per 32 ms; start spacings 8, 24 ms | About 38, 166 frames |
| 519 s | 93 | One per 32 ms | About 38 frames |

Counts at window boundaries need not be exact integer multiples of the per-cycle count. The direct amplitude threshold includes more transition samples than the earlier flat-interior detector, so its measured widths and near-zero fraction are somewhat larger. It confirms the pattern rather than replacing the report's conservative flat-interior measurements.

For the three late windows, hole starts lie on an 8 ms / 384-frame grid and ends lie on a 512-frame grid. I corrected the independent recorder clock using the measured carrier ratio, 1000.04759/1000, then measured residuals around each grid. At 498 and 519 s, the 1st–99th percentile residuals are within about half an audio frame for both grids. At 485 s, starts have the same precision; ends are within approximately -1.2 to +1.9 frames, consistent with boundary-shape differences. The approximately 128-frame differences between the widths follow the difference between the two grid sizes.

The arithmetic fits the observed superperiod:

`lcm(384, 512) = 1536 frames = 32 ms`.

The raw driver log independently contains input transfer blocks eight USB-frame numbers apart, reported eight milliseconds apart. This makes an 8 ms transfer-block cadence a particularly relevant candidate for the second grid. The waveform does not prove which transfer manager produces the grid, how much audio is queued, or how many buffers exist. A 32 ms period is not evidence that a physical ring has exactly 1536 frames.

The 1 kHz tone creates an important ambiguity: its own 48-frame period and a 512-frame callback also have a 1536-frame least common multiple. The 32 ms period alone would therefore be weak evidence for an 8 ms USB block. The extra evidence is the direct amplitude detector's 384-frame start grid, the 512-frame end grid, the 128-frame width increments, and the independent eight-millisecond block logs. These are substantially more specific than the superperiod alone, but a second tone frequency or a nonperiodic test signal would still remove the ambiguity more decisively.

I also fitted the surviving tone segments individually to a common continuous carrier, trimming 24 samples off both sides of each gap boundary and requiring at least 96 samples between detected holes before trimming. Across the tested damaged windows at 417, 426, 440, 479, 485, 498, and 519 s, segment phase stays within about ±0.03 degrees of its window's median phase. Median fit residuals are about 0.12–0.19% of carrier amplitude. The surviving waveform follows one continuous tone timeline through the holes.

This is strong evidence against the oscillator simply stopping for the duration of each hole. Those hole widths are not generally whole 1 ms tone periods; a pause of that length would shift the resumed carrier phase. The observation is consistent with blanking, unavailable-data zero fill, or replacement of samples while the source timeline continues. It cannot distinguish those mechanisms from each other. A pure periodic tone also cannot reveal stale/repeated samples shifted by an exact integer number of tone periods.

## Timing evidence and two attractive models that fail

The stable tone run contains 26 logged zero-length transfers in 22 reports. Each of the thirteen major drift steps follows the next pair of zero-length transfers. The first step, for example, follows two zero-length transfers at 22:52:33.336 and is logged at 22:52:33.392. The major drift increment is approximately 2.020833 ms, equal by units to 97 audio frames. Two full-speed one-millisecond transfer intervals at 48 kHz would nominally span 96 frames. This is suggestive clock/accounting evidence, not proof that two packets contained exactly 96 missing frames or that the private metric measures samples discarded from output.

All thirteen steps pair with a stretched callback about 245.5–263.4 ms later. Independent app clock fitting has sub-millisecond residuals, so a quarter second is not explained by the async logger's drain-time prefix or a coarse clock fit. Driver timestamp records frequently advance sampleTime by 12,000 frames, a quarter second at 48 kHz; an update/publication cadence is a plausible contributor to the observed delay. The logs do not establish the precise update algorithm.

The callbacks do not simply catch up immediately after each stretched interval. Comparing median host elapsed time minus nominal sample elapsed time over callbacks -100:-10 and +10:+100 around each event gives sustained local phase changes of approximately 2.145–2.168 ms. Ordinary timing slope and smaller adjustments continue between events. The full captured run accumulates about 24.069 ms of host-versus-nominal elapsed-time residual. These values differ from the private drift total and its per-step increments; treating all of them as interchangeable measures of lost output samples would be wrong.

The correlation at onset is real: after five major steps, the drift label is 10.354 ms and the recording remains clean; after the sixth, it is 12.375 ms and holes begin. A 512-frame callback lasts 10.667 ms. However:

1. **A universal one-buffer threshold is not established.** In the original 44.1 kHz / 257-frame run, the repeating damage developed with logged drift around 2.34–3.385 ms, below that callback's 5.828 ms duration. Different startup state, hidden margins, or different semantics would be needed to generalize the threshold.
2. **A simple monotonically increasing deficit fails.** Tone quality later improves substantially without a reset while the private drift number keeps increasing.
3. **A simple 512-frame sawtooth fails too.** In the final three states, the drift labels convert to approximately 1079, 1176, and 1273 frames. Their residues modulo 512 rise from 55 to 152 to 249 frames, while the near-flat fraction falls from about 30% to 12% to 2%. The waveform requires more than the proposed rule that damage is just the drift remainder within one callback buffer.

A more complicated interaction between multiple timing boundaries remains plausible. The successful evidence is the two waveform grids and the timed state transitions, not an identified wrap length or a solved implementation.

## Ranked hypotheses

1. **An input-transfer timing/accounting disturbance interacts with output buffer availability or timestamp alignment.** This best fits the zero-length-transfer pairs, drift steps, callback rephasing, continuing source timeline, and 384/512-frame output grids. Its strength is as a description of the failure chain. It does not assign fault to JUCE, Core Audio, the USB driver, the hub, or the MAYA. A driver timestamp adjustment could be a response to an earlier disturbance rather than its initiating cause.
2. **A shared scheduling or peripheral-state problem produces different outcomes under the two workloads.** A nonfatal zero-length transfer can coexist with continued streaming; a transaction failure instead sends the stream into driver recovery. The normal callback's several-millisecond work, callback-driven MIDI/state operations, and UI activity differ from the tone run and could affect probability without any measured DSP call exceeding its nominal budget. Conversely, interface/USB state can affect both failure types without app CPU time being the initiator. This hypothesis explains why reducing work might remove restart episodes while leaving timing corruption, but the present sequential experiment does not distinguish these alternatives.
3. **The remaining app/framework path corrupts output independently of USB timing.** It is not logically ruled out: the tone retains JUCE, diagnostics, UI, and other infrastructure. It is less economical because it must explain the repeated input-transfer/drift associations and both waveform grids. An out-of-bounds write, stale buffer, or incorrect timestamp handling could still sit inside that remaining path. Only boundary captures can localize the first incorrect buffer.
4. **A recording artifact or tone-generator numerical defect.** These are strongly disfavored by clean recorder logs, the independent direct-amplitude detector, matched changes across channels, the continuing sine phase, and the driver/callback correspondence. The sine phase accumulator is bounded and all output samples are filled. The recorder's independent clock explains small frequency/rate differences, not repeated millisecond-long near-zero spans at the observed two grids.

## Could the earlier restarts have hidden accumulation?

Yes, this is a supported possibility, not a demonstrated explanation for all normal-versus-tone differences. The original 44.1 kHz capture contains an especially useful example: the persistent periodic distortion disappears at the 23:43:55.771 transaction-triggered restart, the rate stays 44.1 kHz, and the drift diagnostic resets. The preceding error-free stretch lasted roughly six minutes. A separate 512-frame run also shows a timing excursion followed by a transaction error and a much smaller subsequent drift label. Restarting clears relevant stream state and can clear the audible periodic symptom.

The tone has no such resets for roughly 9.45 minutes of settled playback; its first sustained holes arrive roughly 7.52 minutes after settled RemoteIO start. Frequent restart events could keep another run from reaching a vulnerable state. This is a plausible masking mechanism and means the disappearance of long dropouts need not imply that the entire stream has become healthier.

There are two important limits. First, the no-controller normal-DSP run had seven transaction failures but no logged zero-length transfers, while the tone had 26 zero-length transfers and no transaction failures. The input-enabled 512-frame run had only two logged zero-length transfers alongside thirty errors. We cannot assume those normal runs were progressing along the same accumulation curve between resets. Second, the short clean SmartGrid trial had zero-length transfers and some drift reports without an observed stable failure; a zero-length report alone is not sufficient evidence of an audible dropout. Cross-run counts are in [cross-run-input-transfer-summary.json](/private/tmp/smartgrid-research-20260909/cross-run-input-transfer-summary.json).

The lighter callback is a credible influence on failure probability, but the experiment changed more than arithmetic cost. It bypassed processing-driven MIDI and state/UI updates as well. The observations cannot tell whether the absence of transaction failures came from CPU service margin, changed auxiliary traffic, a different startup/device state, or ordinary run-to-run variation. It is also possible for the two symptom families to share an underlying transport condition without sharing one immediate trigger.

## Next discriminating checks

1. **Use a phase-distinct signal.** In a later authorized experiment, choose a tone whose period does not divide a millisecond, or a deterministic nonrepeating sequence. If 384/512 grids and a 1536-frame gate period remain, the USB/render geometry interpretation gains force. It will also expose whole-cycle sample repetition or deletion that 1 kHz can conceal.
2. **Capture samples at successive boundaries.** Compare source generation, the native render buffer returned to RemoteIO, and the analog output, alongside sampleTime/hostTime and frame count. A pristine native returned buffer during analog holes places blanking downstream of that boundary; zeros already present there place it upstream. A finite memory trace can preserve this evidence without assuming the existing xrun counter detects the problem.
3. **Separate render cadence from startup negotiation.** A later native RemoteIO test should record actual rather than requested callback size and preserve the physical topology. Avoid assigning an observed improvement solely to JUCE removal if startup rate probing, channel configuration, or packet state also changes. A separate controlled buffer-size change can test whether the hole-end grid follows the actual render block and whether the predicted least common multiple changes.
4. **Test reset and workload separately.** A controlled restart after established corruption can test state clearing without changing sample rate. A separate minimal-tone run with representative additional callback work, and another with the relevant MIDI/state work, can distinguish CPU load from those other changes. These are experiments, not recommended fixes. Changing all of them at once would leave the current ambiguity intact.
5. **Require recurrence beyond a single quiet run.** Compare successful and failed runs using time since the last restart, zero-length counts, driver timing changes, and actual stream settings. This tests the masking hypothesis directly and avoids interpreting zero transaction errors during one tone run as proof that their cause has been removed.

The narrow confirmed conclusion is that the tone's sustained corruption is periodic blanking of a continuing signal associated with USB input-transfer/timing events, while the earlier isolated long dropouts are driver recovery pauses. The remaining question is where a timing disturbance first becomes incorrect output, and why the two workloads steer the stream toward different failure outcomes.

## Follow-up: does the approximately 47.6 ppm match imply clock mismatch?

The endpoint numbers match even more closely when their exact observation interval is used: driver drift changes by 26.270667 ms over 550.022 s between its first and last recorded major states, averaging **47.7629 ppm**, versus the measured carrier ratio of approximately **+47.59 ppm**. Using the entire roughly 567 s settled playback interval instead gives approximately 46.3 ppm. This is an interesting numerical coincidence, not yet an identified clock relationship.

The existing trajectory rejects the simplest explanation: a constant 47.59 ppm mismatch accumulated smoothly and corrected in fixed 97-frame quanta. Such a process would produce one 2.020833 ms quantum about every **42.463 s**. Instead, intervals from the initial state through the thirteen steps are **210.247, 109.256, 38.248, 48.007, 2.248, 42.504, 3.000, 2.752, 15.504, 40.248, 5.008, 11.248, and 21.752 s**. At the fourth major step, the observed accumulated drift is about **11.227 ms below** the smooth 47.59 ppm prediction, more than five step quanta; it catches up only later. Every major increase instead coincides with another pair of reported zero-length transfers. A constant underlying mismatch could still influence when a device or queue misbehaves, but this would require additional state or irregular loss, beyond simple periodic clock correction.

The app timing also does not supply an independent smooth +47.6 ppm confirmation. Across its full captured callback interval, elapsed host time minus nominal sample time grows at **+45.177 ppm** in the monotonic clock, or **+34.463 ppm** after mapping to iPad wall time. These averages include the thirteen abrupt callback phase shifts. In the longer intervals between those shifts, the same elapsed-time error slopes are approximately **-4 to -7 ppm** in the monotonic clock and **-15 to -17 ppm** in mapped wall time. The observed global slope is dominated by discrete corrections, and changing the clock domain changes its numerical value.

Most importantly, the measured carrier ratio compares the MAYA's effective output sample clock with the **independent K-Mix capture sample clock**. The driver counter and app timing concern the iPad's audio/USB/host timing relationships. We have no calibrated K-Mix-to-iPad sample-clock relationship, so the two ppm values cannot be equated directly. Existing data therefore falsifies the simple steady-mismatch/regular-correction model, while leaving a more complex clock-inference or packet-accounting connection open. The correct use of the ppm match is as a testable lead, not evidence that a 47.6 ppm physical clock mismatch has been measured at the failing interface boundary.

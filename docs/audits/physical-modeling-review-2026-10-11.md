# Physical modeling synthesizer audit — 11 October 2026

Audited commit: `72ca151d64425d522641a4bcbf0ccff04d21d539`.

This is the historical, pre-fix report. The [current PM design](../source-machine.md#physical-modeling-source-machine) and [implementation plan](../superpowers/plans/2026-10-11-physical-modeling-corrections.md) describe the follow-up changes. Compile the audit probes against the audited commit above; their legacy comb API and source locations deliberately refer to that revision.

## Implementation follow-up

Commits `9247e9c` and `03b9564` implement the morph fix, one-event-per-block PM envelope delivery, processing-rate-aware explicit release, and the fundamental-preserving tuned decay comb. The comb also corrects negative-polarity tuning. PM's intentional 4× processing, 100 µs–2.5 s envelope ranges, and inverted amplitude-depth behavior remain intact.

The final standalone CMake/CTest run at `03b9564` passed **582 test cases and 91,664,771 assertions**. The tests cover fractional pitches at 48/192 kHz, both polarities, several DAMP settings, five-second fundamental decay, short and near-center DECAY settings, upper-partial loss, DC decay, reset, excited parameter transitions, and audio/UI complex-response agreement. Envelope integration checks cover consecutive-block retriggers, explicit release, and preserved phase-driven running decay.

Exact pitch and T60 apply to settled parameters. Moving parameters crossfade the feedback paths over 5 ms. DAMP now means damping amount, so existing patches intentionally sound different despite retaining their saved parameter IDs. Delay memory grows from 64 to 128 KiB per comb. These are host DSP checks; no iPad deployment, performance measurement, or listening evaluation was performed.

Independent task and whole-change reviews found no outstanding correctness issues. The final naming-only cleanup in `284ef96` passed its focused AHD suite (9 cases, 28,255 assertions) and scoped re-review; production DSP is unchanged from the full-suite-tested `03b9564`.

## Original findings

The morphing SVF has an incorrect angle scale, and the comb applies a per-sample decay coefficient once per delay circulation. These are confirmed defects. The second explains very long low-note tails; damping removes higher partials faster, leaving a sine-like fundamental. Negative-feedback tuning, oversampled note-off release, and physical-model trigger reuse also have errors.

This audit adds this report, a [standalone DSP measurement probe](physical-modeling-probe-2026-10-11.cpp), and an [envelope trigger probe](envelope-trigger-probe-2026-10-11.cpp). It does not change production DSP, presets, or specifications. Measurements use the production C++ headers at the source's actual 192 kHz processing rate and the amp/modulation envelopes' 48 kHz rate. No device listening or full application validation was performed.

**1. [P1] Comb decay time is applied at the wrong interval.**

Location: [`CombFilterWithOnePole.hpp`](../../private/src/CombFilterWithOnePole.hpp), `SetDampingTime`, lines 159–174; feedback application in `Process`, lines 187–197.

The knob maps to T60, the intended time to lose 60 dB, between 1 ms and 5 s. The implementation calculates:

```text
feedback magnitude = 10^(-3 / (T60 × processing_sample_rate))
```

That is attenuation for one sample. A stored signal encounters it once per circulation around the delay, not once for every elapsed sample. For an ideal delay of D samples, the required magnitude is:

```text
feedback magnitude = 10^(-3 × D / (T60 × processing_sample_rate))
```

The present ideal-loop T60 is approximately D times the requested value. At 50 Hz, positive feedback, and 4× oversampling, D is 3,840 samples. With the knob at 1, an impulse echo at five seconds has lost only **0.0157 dB**, instead of 60 dB. Its measured attenuation implies roughly **19,139 seconds** to lose 60 dB. This isolated test sets the damping alpha to 1 to expose the coefficient error; alpha 1 is not the source's maximum-cutoff setting. Negative feedback has the same defect, with a half-period circulation.

The actual available damping settings also reproduce excessive decay. After a one-second sine excitation, input is set to exactly zero. Comparing AC RMS in the 1–2 s and 6–7 s tail windows:

| Played frequency | Damping cutoff | Requested T60 | Loss over five seconds | T60 estimated from that slope |
| --- | --- | --- | --- | --- |
| 55 Hz | 1,000 Hz | 5 s | 3.624 dB | 82.8 s |
| 110 Hz | 1,000 Hz | 5 s | 28.765 dB | 10.4 s |
| 55 Hz | 23,952 Hz | 5 s | 0.0232 dB | 12,928 s |
| 440 Hz | 23,952 Hz | 5 s | 3.201 dB | 93.7 s |

The slope estimates are extrapolations, not hours-long renders. The probe suppresses estimates once the measured AC tail falls below its measurement threshold.

A separate test runs `PhysicalModelingSource::ProcessUBlock` itself: 100 ms of noise, followed by setting PM amplitude to 1 to close the idle excitation. At 55 Hz, 1 kHz damping, and maximum positive feedback, **88.6% of the remaining AC power is at the fundamental in the 6–7 s window**. The source also retains DC from the finite noise burst because its positive-feedback loop has unity damping-filter gain at DC. This contributes to stored energy and headroom use. These measurements are before the downstream voice filter and amp.

Repair direction: calculate attenuation for one complete feedback circulation, accounting for pitch and feedback polarity, and define whether T60 represents an upper bound before additional one-pole loss. Verify decay over time across pitches, both polarities, and sample rates. Reassess residual DC and gain after that correction before adding further damping mechanisms.

**2. [P1] The SVF morph traverses full cycles instead of quarter cycles.**

Locations: [`StateVariableFilter.hpp`](../../private/src/StateVariableFilter.hpp), `GetMorphedOutput`, lines 124–140; duplicated in [`PhysicalModelingSource.hpp`](../../private/src/PhysicalModelingSource.hpp), `UIState::TransferFunctionValue`, lines 49–68.

`Math::Sin2pi(t)` and `Math::Cos2pi(t)` take phase in turns. Each half of the morph passes t from 0 to 1, traversing 360 degrees. A positive equal-power crossfade needs 0 to 90 degrees. Basis-output measurements of the actual function give:

| Morph | Actual output, ignoring tiny lookup rounding |
| --- | --- |
| 0 | LP |
| 0.125 | BP |
| 0.25 | −LP |
| 0.375 | −BP |
| 0.5 | LP |
| 0.500001 | Almost pure BP |
| 0.625 | HP |
| 0.75 | −BP |
| 0.875 | −HP |
| 1 | BP |

Thus the default midpoint is low-pass, the maximum is band-pass, and there is a discontinuity at the midpoint. Parameter slewing does not remove that discontinuity in the mapping. The source visualizer repeats the same faulty weights.

Repair direction: use quarter-turn arguments, such as `Cos2pi(0.25 * t)` and `Sin2pi(0.25 * t)`, in both audio and display paths, ideally from one shared weight calculation. Test endpoints, interior gains, and continuity around 0.5. The [source-machine specification](../../openspec/specs/source-machine-oscillators/spec.md) currently repeats the faulty full-cycle formula; it also needs correction when the behavior is repaired.

**3. [P2] Negative-feedback tuning compensates the damping phase at twice the played frequency.**

Location: [`CombFilterWithOnePole.hpp`](../../private/src/CombFilterWithOnePole.hpp), `SetParams`, lines 102–125.

Negative feedback requires a half-period delay. Doubling `combFreq` obtains that half-period, but the code then uses the doubled frequency to evaluate the damping filter's phase delay. The damping phase must be evaluated at the actual played frequency. The difference matters when the damping cutoff is near the note.

For a requested 110 Hz note and 200 Hz damping, the current loop's phase closure is **106.981 Hz**, about **48.2 cents flat**. The rendered decaying impulse tail measures approximately **106.660 Hz**; a damped pole's oscillation frequency need not exactly equal the unit-circle phase-closure frequency. Changing only the probe's delay compensation to evaluate the one-pole at 110 Hz brings phase closure to **109.990 Hz**. Production code is untouched.

Repair direction: retain the played frequency for phase compensation, and halve only the nominal delay for negative feedback. Test both polarities over a range of damping cutoffs, distinguishing phase closure from the frequency of a strongly damped transient.

**4. [P2] Physical-model note-off release decrements at four times the intended rate.**

Locations: [`PhysicalModelingSource.hpp`](../../private/src/PhysicalModelingSource.hpp), the per-oversample AHD call, lines 203–207; [`AHD.hpp`](../../private/src/AHD.hpp), `State::Release`, lines 247–257.

The setter computes decay increments in base-rate samples at 48 kHz. Running attack/hold/decay uses absolute phase and base-rate sample position, but release subtracts the increment once per `Process` call. The physical source calls it at 192 kHz. Giving the setter a 10 ms decay and releasing from full level reaches idle by **2.667 ms**, measured in 0.167 ms microblocks. The code implies a fourfold speedup, with sample/block rounding in the measurement.

This shortens excitation release; it does not explain the long resonator tail. Repair direction: make release advancement use the correct elapsed time without dividing increments also used by the absolute-time running envelope. Test normal decay and explicit release separately at base rate and 4× oversampling.

**5. [P1] One physical-model trigger is processed 32 times in its microblock.**

Locations: [`PhysicalModelingSource.hpp`](../../private/src/PhysicalModelingSource.hpp), lines 203–207; [`AHD.hpp`](../../private/src/AHD.hpp), trigger handling at lines 188–192 and attack evaluation at lines 213–219.

The physical source renders eight base-rate samples at 4× oversampling, making 32 calls to `AHD::Process` with the same true `m_trig`. Every call overwrites `m_startOutput` with the previous output. Attack evaluation then adds the entire elapsed time since the original trigger to that new starting level. This repeatedly counts elapsed attack time rather than merely holding or restarting the envelope at zero.

With settled parameters, a 10 ms attack, and a clock advancing at normal speed, the last internal sample in the first block occurs at 0.161458 ms. The production source reaches **0.258333** raw envelope amplitude; an otherwise identical reference which delivers the trigger only on internal sample zero reaches **0.016146**. The first-block endpoint is **16 times too high**. The source's trigger remains true after rendering the block, and the wrongly captured starting level persists into subsequent blocks.

The other active AHD envelopes do not exhibit these two oversampling errors in the current application path:

| Envelope | Calls seeing one trigger | Release from full level with 10 ms setting |
| --- | --- | --- |
| Physical-model excitation | 32 internal calls | Complete by 2.667 ms at block resolution |
| Voice amp | 1 base-rate call | 480 samples, 10 ms |
| Voice modulation | 1 base-rate call | 480 samples, 10 ms |

The production wiring explains the difference. `TheNonagon::ProcessSample` clears the control trigger on the next base-rate sample (`TheNonagon.hpp:1483`). `TheNonagonSquiggleBoyInternal::ProcessSample` then copies controls and processes the synth each sample (`TheNonagonSquiggleBoy.hpp:523`). `SquiggleBoyWithEncoderBank::SetEncoderParameters` calls `SetGates` each sample (`SquiggleBoy.hpp:1663`), and `AmpSection::Process` calls each AHD once at base rate (`SquiggleBoy.hpp:349`). The physical source has already rendered its entire oversampled microblock before the next base-rate trigger clear occurs.

The probe exercises the actual `PhysicalModelingSource::ProcessUBlock` and `AmpSection::Process` implementations, comparing PM with a single-trigger AHD reference and driving the amp/modulation callers with the same one-base-sample pulse established by that wiring. It is not a full-application timing test. Both base-rate envelopes match the expected attack values. All **13 existing AHD/absolute-time envelope tests, 28,272 assertions**, also pass; the AHD test helper explicitly clears the trigger after its first processing call, so it does not reproduce the physical source's repeated-trigger caller behavior.

The standalone `ADSR`, `ADSP`, and `PhasorEnvelopeInternal` definitions have no production instances in the current repository search. They are not evidence of additional affected live voice envelopes. Voice LFO sample-and-hold triggers use the same base-rate refresh and are not processed in the physical source's internal loop.

Repair direction: treat a trigger as an event consumed once by each envelope, while envelope evaluation can run at any sample rate. Delivering the PM event only on internal sample zero fixes the immediate caller error. A shared API separating trigger/release events from time-based evaluation would make the contract explicit. Do not indiscriminately clear `Input::m_trig` inside `AHD::Process`: the amp's scope writers also inspect that input flag after envelope evaluation. Release advancement must also use the correct elapsed time. Test repeated internal evaluations, retriggers on consecutive microblocks, and base-rate/oversampled timing equivalence.

**Relevant behavior and coverage**

- PM amplitude is an inverted envelope-depth control. At idle, amplitude 0 supplies full excitation; amplitude 0.5 supplies approximately **0.240253** excitation gain after the exponential curve; amplitude 1 closes it apart from a minute floating-point slew residual. Continuous excitation below 1 is documented behavior, not an additional confirmed defect. It can keep energizing the comb indefinitely.
- The linear SVF recurrence itself passed its existing response and stability tests. The morph error is in the output mixing, not evidence of SVF self-oscillation.
- All **10 existing SVF/comb tests, 38 assertions**, passed. They never call `GetMorphedOutput` or `SetDampingTime`; comb tests set raw feedback directly. They therefore cannot catch the two principal defects. The existing physical-model envelope test checks hold and running decay, not oversampled release.
- The source visualizer's caller supplies the correct 1/4 frequency scaling. The duplicated morph weights, rather than missing oversampling scaling there, are the confirmed display issue.
- At 192 kHz the 16,383-sample delay limit restricts very low tuning to roughly 11.7 Hz with positive feedback or 5.9 Hz with negative feedback, subject to phase compensation. The 55 Hz reproductions are comfortably inside that range.

**Reproduction**

Run from the repository root:

```sh
clang++ -std=c++17 -O2 -Iprivate/src \
  docs/audits/physical-modeling-probe-2026-10-11.cpp \
  -o /tmp/physical-modeling-probe-2026-10-11
/tmp/physical-modeling-probe-2026-10-11

clang++ -std=c++17 -O2 -DDOCTEST_CONFIG_NO_SHORT_MACRO_NAMES=1 \
  -Iprivate/src -Iprivate/test private/test/support/TestMain.cpp \
  private/test/unit/dsp_svf.cpp private/test/unit/dsp_comb.cpp \
  -o /tmp/pm-audit-existing-tests
/tmp/pm-audit-existing-tests

python3 scripts/generate_build_info.py . /tmp/pm-envelope-audit-generated/SmartGridBuildInfo.hpp
clang++ -std=c++17 -O2 -Iprivate/src -I/tmp/pm-envelope-audit-generated \
  docs/audits/envelope-trigger-probe-2026-10-11.cpp \
  -o /tmp/envelope-trigger-probe-2026-10-11
/tmp/envelope-trigger-probe-2026-10-11

clang++ -std=c++17 -O2 -DDOCTEST_CONFIG_NO_SHORT_MACRO_NAMES=1 \
  -Iprivate/src -Iprivate/test private/test/support/TestMain.cpp \
  private/test/unit/dsp_ahd.cpp private/test/unit/absolute_time_envelopes.cpp \
  -o /tmp/pm-audit-envelope-tests
/tmp/pm-audit-envelope-tests
```

The probe prints observations and does not claim passing regression coverage. `nan` in its decay-estimate columns denotes a tail below the measurement threshold, not a non-finite DSP output. Probe compilation and execution and the targeted existing tests completed successfully. Full-suite, application build, and device checks are outside this audit.

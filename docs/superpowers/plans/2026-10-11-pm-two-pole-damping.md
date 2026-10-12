# PM Two-Pole Damping Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the PM comb's bandpass blend with a two-pole low-pass for a gentler rolloff, retain radial decay and fundamental tuning, and deploy the result for an iPad audition.

**Architecture:** Use a unity-DC Butterworth low-pass in the loop and take output after that filter. DAMP controls cutoff relative to the fundamental, while the full `H0(z/r)` transform continues to control the decay ceiling. Compensate the loop delay for the low-pass phase at the fundamental. The user explicitly accepts approximate fundamental decay in exchange for two poles and gentler timbre.

**Tech Stack:** Header-only C++17 DSP, doctest/CMake, signed Xcode iOS Release deployment.

## Global Constraints

- Work in the existing managed detached worktree; starting commit `06bd484`. Do not merge or push.
- Preserve PM 4× oversampling, AHD event consumption/release factor, morph correction, and 100 µs–2.5 s excitation-envelope ranges.
- Use two poles, Q=1/sqrt(2), unity DC gain, with no gain normalization at the fundamental and no bandpass/dry damping blend.
- Keep persisted parameter `PMCombDampenCutoff`, its default 0.5, and the DAMP label. Map `d=DAMP`, `fc/Fs=min(0.49, 8*(f0/Fs)/(d*d))` for d>0; d=0 bypasses the damping filter. Increasing DAMP closes the cutoff; maximum DAMP is 8× fundamental before the Nyquist clamp.
- Keep signed DECAY mapping `T60=0.001*5000^abs(2*knob-1)`, negative below center, positive above. Exact center stays dry, including DAMP>0.
- Preserve the radial transform for every memory. All stationary modes decay at least as fast as the selected T60. Low-pass loss may shorten fundamental decay; do not claim exact invariance under DAMP. At maximum DAMP/5 s, expected examples at 192 kHz are 55.3 Hz: 4.9757/4.95165 s positive/negative; 880 Hz: 4.64521/4.33760 s; 5013.7 Hz: 4.04652/3.39876 s. Document the stronger high-note shortening explicitly.
- Preserve 16384-sample shared delay, normalized pitch clamp `[cycles/16383,0.25]`, and bounded 5 ms two-branch transitions with coalesced targets and no audio-thread allocations/locks/buffer copies.
- Follow AGENTS.md C++ style: structs, public members, Allman braces, m_ members, x_ constants, HammerCase functions, no C-style casts/default arguments; comments end in a separate `//` line.

### Task 1: Replace the loop damper, output topology, and matching UI transfer

**Files:** `private/src/TunedDecayComb.hpp`, `private/src/PhysicalModelingSource.hpp`, `private/src/ForEachSmartGridOneParam.hpp`, `private/test/unit/dsp_comb.cpp`, `private/test/unit/dsp_physicalmodeling.cpp` if needed, `docs/source-machine.md`, `docs/glossary.md`, `docs/drawing-filter-response.md`, `openspec/specs/source-machine-oscillators/spec.md`.

**Interfaces:** Keep `SetParams(double normalizedFrequency, double decayKnob, double dampKnob, double sampleRate)`, `Process(float)`, `Reset()`, and `PopulateUIState(UIState*)`. Branch processing may accept the excitation in addition to the delayed sample. Replace obsolete damping-blend coefficients with an explicit bypass flag, and expose the numerator low-pass response to UIState.

- [x] Add a failing audio regression for low-pass output: steady complex/sinusoidal excitation well above the DAMP cutoff must roll off instead of retaining the direct excitation. Compare maximum DAMP and bypass at a short enough DECAY to settle quickly, including two separated stopband frequencies; assert measured attenuation, not coefficient values. Retain center dry behavior.
- [x] Run the focused test and record the intended failure before changing production code.

```sh
clang++ -std=c++17 -O2 -DDOCTEST_CONFIG_NO_SHORT_MACRO_NAMES=1 -Iprivate/src -Iprivate/test private/test/support/TestMain.cpp private/test/unit/dsp_comb.cpp private/test/unit/dsp_physicalmodeling.cpp -o /tmp/pm-lowpass-tests
/tmp/pm-lowpass-tests --test-case='TunedDecayComb: low-pass*'
```

- [x] Implement the low-pass TPT output and phase compensation. For settled coefficients:

```text
omega = 2*pi*f0/Fs
g = tan(pi*fc/Fs), k = sqrt(2)
L(z) = g^2*(1+z^-1)^2 /
       ((1+k*g+g^2)+2*(g^2-1)*z^-1+(1-k*g+g^2)*z^-2)
theta = (sign>0 ? 2*pi : pi)
D = (theta + arg(L(exp(j*omega)))) / omega
N = max(1, floor(D)-1)
delta = D-N
a = (tan(omega/2)-tan(omega*delta/2)) /
    (tan(omega/2)+tan(omega*delta/2))
A(z) = (a+z^-1)/(1+a*z^-1)
H0(z) = L(z)/(1-sign*z^-N*A(z)*L(z))
r = exp(-ln(1000)/(Fs*T60))
H(z) = H0(z/r)
```

For DAMP=0 or exact DECAY center, L is identity. Retain the existing allpass lattice and double TPT state precision. Each branch computes its transformed allpass delay return, combines `input + sign*return`, then applies the transformed low-pass (TPT `v2`) to that sum. Mix branch outputs and write the mixed result into the shared delay. Keep filter state behavior bounded when bypassing/re-enabling damping. At the very highest negative-feedback pitches D can be slightly below 2: use N=1 and a positive delta below 1, instead of unsigned underflow or an algebraic zero-delay read.

- [x] Update UI transfer including the numerator. With mix m, low-pass numerators P0/P1, and complete signed return loops R0/R1, display `((1-m)*P0+m*P1)/(1-((1-m)*R0+m*R1))`. Include all radial terms and bypass/center states. Validate both a settled response and a frozen transition response against actual audio.
- [x] Replace old exact-DAMP-invariance assertions deliberately: preserve tight bypass decay/pitch assertions, add independent literal maximum-DAMP decay expectations above (within 1%), and retain broad pitch/decay checks across fractional low/mid/high pitches, both feedback signs, and 48/192 kHz. Measure at least one upper partial's tuning and loss, not just the fundamental, because dispersion is the user's concern. Preserve sample-by-sample radial-impulse scaling, center/reset, boundaries, DC bounded decay, and modulation tests. Do not merely loosen every old test.
- [x] Run focused tests, fix regressions, and update current docs/specs/comments/parameter description. State the new cutoff mapping, output filtering, phase compensation, approximate fundamental decay, and Nyquist limits. Historical audit/old implementation plans remain historical.
- [x] Configure/build/run the full standalone suite once before committing. Use Release with asserts retained to reduce temporary disk use; the previous large `/tmp/pm-corrections-build` was removed after saving its test log.

```sh
cmake -S private/test -B /tmp/pm-lowpass-build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE='-O2'
cmake --build /tmp/pm-lowpass-build -j4
ctest --test-dir /tmp/pm-lowpass-build --output-on-failure
```

- [x] Self-review and commit only the task files and this plan. Report measured decay/pitch/upper-partial behavior, focused/full test results, and any remaining limits.

## Completion and audition

- [x] Obtain task and final independent reviews; resolve findings.
- [x] Build/install/launch the new commit with `make ios-deploy`, using the existing initialized pinned JUCE dependency and `scripts/.venv-ios`.
- [x] Read the latest iPad app log with `scripts/ipad_logs.py app`, verify startup, and report the deployed commit and the audible-control change. Keep the worktree for further listening feedback.

## Verified audition build

- Implemented at `a2f59b8`; the final review's catalogue fixture wording correction is `e105771`. Task review and whole-branch review found no DSP, UI, morph, or envelope correctness issue. Scoped re-review confirmed the fixture correction and closed the only minor finding.
- Focused tests: 16 cases / 1,008,574 assertions passed. Full standalone suite: 589 cases / 91,664,983 assertions passed, with `-O2` and assertions enabled. The final fixture-only correction passed a direct description-consistency check and `git diff --check`; suites were not repeated for inert metadata.
- `make ios-deploy` built, signed, upgraded, and launched `e105771395ca20d516223cb6052ff24ec6f89ccf` on the paired iPad successfully. The built binary contains that commit. Deploy log: `/tmp/pm-two-pole-deploy-e105771.log`.
- Completed app-log capture: `/tmp/pm-two-pole-ipad-startup-e105771/2026-10-11T12-57-53-073.log`. Retained events are all at device time 12:57:57 on 2026-10-11; the app log does not encode a timezone offset. It confirms 48 kHz / 512 frames, four inputs and outputs via MAYA44 USB+, and WRLD.BLDR MIDI. Initial counters include one gap/overrun/format mute and zero xruns; this startup snapshot is not an extended timing or listening test.
- Detailed test/review evidence preserved at `/tmp/pm-two-pole-e105771-verification.md`; full suite output remains in `/tmp/pm-lowpass-build/Testing/Temporary/LastTest.log`. This plan's temporary SDD workspace was removed after review completion. The worktree remains available for listening feedback; no merge or push was performed.

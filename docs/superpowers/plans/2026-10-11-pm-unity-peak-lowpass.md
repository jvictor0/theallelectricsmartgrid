# PM Unity-Peak Low-Pass Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Allow a 2× closed cutoff while preserving fundamental decay, open exponentially to 128×, and deploy for the iPad audition.

**Architecture:** A two-pole low-pass with Q/gain derived to place its unity peak at fundamental and its actual prototype -3 dB point at the requested cutoff. Retain H0(z/r), fundamental phase compensation, and existing comb topology.

**Tech Stack:** C++17, double TPT/allpass state, doctest/CMake, signed iOS deployment.

## Global Constraints

- Existing managed detached worktree, base `52fba8784d9db7afd8c635f8e05048422f79eeb7`; no merge/push.
- DAMP control d maps to prototype -3 dB cutoff `c=min(0.49,f*exp2(1+6*d))`, with f the clamped normalized fundamental. Endpoint/midpoint ratios 2/16/128. Parameter identifier/label/default 0.5 stay unchanged; default cutoff intentionally becomes 16×.
- Two poles only. Prototype peak at fundamental is unity and no frequency exceeds unity. Exact DECAY center remains the only bypass. No dry blend, no fixed resonance at the moving cutoff, no uniform gain boost of the old Butterworth filter.
- Preserve signed DECAY duration, radial loss on every memory, fundamental phase compensation, 16384 delay, pitch limits, bounded coalesced 5 ms transitions, post-filter output/excitation, and other morph/AHD/oversampling behavior.
- Follow repository C++ style. Update current docs/specs; older audit/design/plan files remain historical.

### Task 1: Implement variable-Q unity-peak damping and its UI

**Files:** `private/src/TunedDecayComb.hpp`, `private/src/PhysicalModelingSource.hpp` comments, `private/test/unit/dsp_comb.cpp`, `private/test/unit/dsp_physicalmodeling.cpp` only if needed, `docs/source-machine.md`, `docs/glossary.md`, `docs/drawing-filter-response.md`, `openspec/specs/source-machine-oscillators/spec.md`, this plan and its paired design.

**Interfaces:** Keep public methods unchanged. Add per-branch double `m_k` and `m_gain` coefficients and matching atomic snapshots. Allpass/radial processing and shared-delay transition topology remain.

- [x] Add/run failing regressions before production: actual branch audio with its copied coefficient `m_r=1` (explicitly testing the prototype, delayed return 0) must have unit gain at f0 and 1/sqrt(2) at the requested cutoff. Use f0=100 Hz at 192 kHz, literal control/cutoff fixtures `{0,200}, {.25,565.685424949238}, {.5,1600}, {.75,4525.48339959390}, {1,12800}`; 0.05 s warmup plus 0.1 s Hann projection via existing Project helper, multiplying magnitude by 4. Tolerance 0.002 at cutoff and 0.001 at fundamental. Include closed DC gain near 0.948683298 (low-frequency approximation), and a frequency sweep of the actual prototype response checking magnitude <=1+1e-6, including exact f0. Shape tests use r=1; complete comb tests below verify the actual radial transform.
- [x] Implement coefficients and filtered output:

```cpp
double cutoff = std::min(0.49, normalizedFrequency * std::exp2(1.0 + 6.0 * dampKnob));
double t = std::tan(x_pi * normalizedFrequency);
double v = std::tan(x_pi * cutoff) / t;
double shape = std::hypot(1.0, v * v - 1.0);
double u2 = 1.0 / shape;
c.m_g = t * std::sqrt(shape);
c.m_k = std::sqrt(2.0 * (1.0 - u2));
c.m_gain = std::sqrt((1.0 - u2) * (1.0 + u2));
c.m_h = 1.0 / (1.0 + c.m_k * c.m_g + c.m_g * c.m_g);
double filterPhase = c.m_bypass ? 0.0 : -std::atan2(c.m_k * std::sqrt(u2), 1.0 - u2);
```

Use filterPhase in existing D/N/delta compensation. Do not redeclare t later; reuse it for the allpass coefficient. Preserve TPT state updates, returning `c.m_gain * v2`. Bypass clears dormant integrators as before. Replace obsolete global fixed x_k usage with per-branch k; a clearly named sqrt(2) default constant is fine. In UI Numerator use `gain*g*g*(1+q)^2 / ((1+k*g+g*g)+2*(g*g-1)*q+(1-k*g+g*g)*q*q)`, where q=r*exp(-j*omega), including k and gain snapshot stores.

- [x] Restore tight rendered fundamental decay at all DAMP values, both signs, fractional low/mid/high and clamped-boundary pitches, and 48/192 kHz. Existing broad 5-second test should now expect 60 dB loss for every fixture, within the existing 0.15 dB tolerance, phase pitch error <0.002 Hz, absolute crossing relative pitch error <0.0002. Delete the obsolete approximate-decay/boundary-loss exceptions, not the assertions. The old darkest-decay literal table is superseded; remove redundant test or replace with meaningful selected-duration coverage. Preserve full radial sample-by-sample identity test, which covers shorter DECAY values.
- [x] Preserve actual upper-mode tuning/loss checks. The old closed-endpoint third-mode fixtures no longer apply: its third mode now dies in milliseconds. At f0=220 Hz, Fs=192 kHz, DECAY 5 s, DAMP=1/3 (8× -3 dB cutoff), independently solved third-mode fixtures are negative: 658.545543665752 Hz, -3.819368033379 cents, T60 1.427293671700 s; positive: 659.270770269177 Hz, -1.913886817808 cents, T60 2.210498614261 s. Reuse the existing 0.2/0.4 s projection windows, 0.2-cent and 1% tolerances for these fixtures. Keep full-open third-mode near-harmonic/5 s checks. For the closed endpoint, independent third-mode predictions are negative 636.849346134625 Hz / 0.013876206521 s and positive 654.515578308296 Hz / 0.028245094224 s: document the limitation and verify rapid high-mode suppression through emitted audio without relying on late-time projection dominated by fundamental leakage.
- [x] Preserve stopband suppression assertions (closed/open at 8/16 kHz, -25/-45 dB limits), center/reset, pitch bounds, DC bounded decay, excited modulation, and settled/frozen-transition UI audio agreement. Any failure must be investigated; do not widen tolerances just to pass.
- [x] Run focused tests and update current documentation/comments/specs: 2×→16×→128× actual prototype -3 dB range, fundamental unity peak, Q tending to Butterworth while opening, lower sub-fundamental gain, exact stationary fundamental decay, dispersion/rapid upper-mode loss, Nyquist limits, UI k/gain, unchanged persisted default with changed midpoint sound.

```sh
clang++ -std=c++17 -O2 -DDOCTEST_CONFIG_NO_SHORT_MACRO_NAMES=1 -Iprivate/src -Iprivate/test private/test/support/TestMain.cpp private/test/unit/dsp_comb.cpp private/test/unit/dsp_physicalmodeling.cpp -o /tmp/pm-unity-peak-tests
/tmp/pm-unity-peak-tests
```

- [x] Run full suite once on final production/tests, retaining assertions in the existing build. Save logs outside the temporary SDD workspace as well.

```sh
cmake -S private/test -B /tmp/pm-lowpass-build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE='-O2'
cmake --build /tmp/pm-lowpass-build -j4
ctest --test-dir /tmp/pm-lowpass-build --output-on-failure
```

- [x] Self-review, check diff, and commit only task/design/plan files. Report red/green evidence, measured behavior, full results, and concerns; leave Completion to parent.

Implementation measurement notes (controller-approved): added lower clamp-bound fixtures retain strict fundamental phase/decay coverage at all five DAMP controls. Sub-3 Hz cases use 30 s rendered excitation and 20/f0-second windows, retaining five-second late and two-second middle separations. Crossing precision stays on every original fractional/upper-bound fixture; low-bound waveform crossings are biased by residual modes. Closed-endpoint independent third-mode predictions are documented; tests retain actual emitted-audio upper-mode suppression and add closed prototype 3f0 attenuation, avoiding a total residual metric contaminated by positive second/nonoscillatory modes.

## Completion

- [x] Obtain task and final whole-branch reviews; resolve findings.
- [ ] Build/install/launch via `make ios-deploy`, then capture iPad startup with `scripts/.venv-ios/bin/python scripts/ipad_logs.py app`.
- [x] Record verified revision/results, preserve review evidence, and remove only this plan's temporary SDD workspace. Keep worktree for audition feedback.

## Verified result

- Implemented `918874d28e4fba32591b82ba325c50f85b3cd7ae`. Actual prototype cutoff is exponential 2×→16×→128× at control 0/0.5/1, clamped below processing Nyquist. The unity peak at the fundamental preserves its stationary selected decay while upper modes decay faster.
- Focused tests: 16 cases, 1,009,127 assertions passed. Full suite: 589 cases, 91,665,536 assertions passed with DSP assertions enabled. Only subsequent cleanup removed an unused test helper; final rebuild and focused rerun passed. Maximum measured five-second decay error was 0.00687386 dB. Closed millisecond third-mode lifetimes remain independent predictions, with suppression verified through actual audio as described above.
- Task and final whole-branch reviews approved with no findings. Signed iOS build passed and the binary embeds `918874d`. The existing `PatchArena.hpp:41` unused-variable Release warning remains unrelated.
- Installation succeeded, but iOS blocked launch because the iPad was locked. User unlock requested; launch and startup capture remain pending. Deployment log: `/tmp/pm-unity-peak-deploy-918874d.log`.
- Detailed test/review evidence is preserved in `/tmp/pm-unity-peak-918874d-verification.md`. This plan's temporary SDD workspace was removed after clean reviews. The managed worktree is retained for audition feedback; no merge or push.

# PM Exponential Cutoff Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Make PM DAMP an exponential increasing cutoff from 8× to 128× fundamental and deploy the revised audition.

**Architecture:** Change only the cutoff mapping and endpoint semantics of the existing two-pole radial comb. No topology change; the default midpoint keeps its current cutoff.

**Tech Stack:** Header-only C++17, doctest, signed iOS deployment.

## Global Constraints

- Existing managed detached worktree; base `d3c03cc553ab342494321870d6fb8c1fd2a1ba74`. No merge/push.
- `fc/Fs = min(0.49, (f0/Fs) * exp2(3 + 4*d))`, where d is the existing clamped DAMP control. 0/0.25/0.5/0.75/1 select 8/16/32/64/128 times fundamental. Exact DECAY center is the only bypass; neither DAMP endpoint bypasses.
- Preserve parameter ID/default 0.5/label; preserve two poles, radial memories, phase compensation, output topology, pitch bounds, coalesced transitions, morph/AHD/oversampling.
- Follow repository C++ style. Current docs/specs change; previous audit and implementation plans remain historical.

### Task 1: Reverse and exponentialize the cutoff

**Files:** `private/src/TunedDecayComb.hpp`, `private/src/PhysicalModelingSource.hpp`, `private/test/unit/dsp_comb.cpp`, `docs/source-machine.md`, `docs/glossary.md`, `docs/drawing-filter-response.md`, `openspec/specs/source-machine-oscillators/spec.md`, this plan and its paired design.

**Interfaces:** Keep all existing APIs. Keep the center-only `m_bypass` flag and its UI handling.

- [x] Add/run a failing audio regression for equal knob distances giving equal octave intervals. Use literal fixture pairs `{0,800}, {.25,1600}, {.5,3200}, {.75,6400}, {1,12800}` at 100 Hz fundamental and 192 kHz processing. Configure a fresh comb with DECAY 1, copy its live `Branch`, and render `branch.Process(sin(2*pi*fixtureHz*i/rate), 0.0)` so the delayed return is zero while the real damper and radial memories run. Warm for 0.05 s, project the next 0.1 s; assert sine gain is within 0.002 of `0.7071067811865476` at each expected cutoff. This catches reversed, linear, inverse-square, wrong-range, and zero-bypass mappings through emitted audio.
- [x] Implement only the mapping change:

```cpp
c.m_bypass = decayKnob == 0.5;
double cutoff = std::min(0.49, normalizedFrequency * std::exp2(3.0 + 4.0 * dampKnob));
```

- [x] Update existing endpoint-dependent tests: darkest-mode fixtures now use DAMP 0 with the same independent literals/tolerances; open comparisons use DAMP 1 and are described as open, not bypass. Preserve existing near-five-second and near-harmonic open tolerances (0.15 dB, 0.002 Hz, 0.2 cents, 0.02 s). At the `f0/Fs=0.25` boundary, replace the obsolete bypass-based 60 dB expectation with independently solved negative/positive five-second loss literals 60.497498756/60.486678806 dB at 48 kHz and 61.989995024/61.946715222 dB at 192 kHz, retaining the 0.15 dB and 0.002 Hz tolerances. Ordinary full-open fixtures retain 60 dB expectations. In stopband tests compare closed/open amplitude, preserving -25/-45 dB thresholds. Reverse the upper-partial suppression sweep to 1→0. Preserve center/reset, UI audio comparisons, radial identity, boundaries, and modulation checks. Do not weaken fixtures to pass.
- [x] Run focused tests, update current docs/comments/specs to 8×→128×, exponential increase, center-only bypass, Nyquist limits, and darkest numbers now at DAMP 0. The midpoint/default remains 32×. Remove claims that a noncenter setting bypasses damping. No parameter label or persisted format change.

```sh
clang++ -std=c++17 -O2 -DDOCTEST_CONFIG_NO_SHORT_MACRO_NAMES=1 -Iprivate/src -Iprivate/test private/test/support/TestMain.cpp private/test/unit/dsp_comb.cpp private/test/unit/dsp_physicalmodeling.cpp -o /tmp/pm-exponential-cutoff-tests
/tmp/pm-exponential-cutoff-tests
```

- [x] Self-review, `git diff --check`, and commit task/design/plan files. Report red/green evidence, results, and concerns. The prior full suite passed for the unchanged topology; focused DSP/PM coverage is appropriate for this parameter mapping change, so do not repeat the full suite without a specific concern.

## Completion

- [x] Obtain task and final independent reviews; resolve findings.
- [x] `make ios-deploy`, verify success, and capture startup using `scripts/.venv-ios/bin/python scripts/ipad_logs.py app`.
- [x] Record tested/deployed revision and remove this plan's temporary SDD workspace after preserving evidence. Keep worktree for audition feedback.

## Verified result

- Implemented and deployed `8a1b4f35bc3cdad55dfa592b5edf6a0298b00930`. The control reaches 8×, 16×, 32×, 64×, 128× fundamental at 0, 0.25, 0.5, 0.75, 1 before the Nyquist clamp. Default midpoint remains 32×.
- New audio regression failed four of five fixtures on previous production, then passed. Focused comb/PM tests passed: 17 cases, 1,008,579 assertions. Task and whole-branch reviews found no issues. No full-suite repeat was needed for this mapping-only update.
- Signed iOS build and `make ios-deploy` succeeded; binary embeds the deployed revision. Deployment evidence: `/tmp/pm-exponential-deploy-8a1b4f3.log`.
- Completed startup capture: `/tmp/pm-exponential-ipad-startup-8a1b4f3/2026-10-11T13-11-27-247.log`. Retained app events are all at device time 13:11:31 on 2026-10-11; the log does not include a timezone offset. Audio initialized at 48 kHz / 512 frames, 4 inputs/4 outputs with MAYA44 USB+, and WRLD.BLDR MIDI opened. Initial counters show one startup gap/overrun/format mute and zero xruns; this snapshot does not establish extended timing or audible quality.
- Detailed evidence preserved in `/tmp/pm-exponential-8a1b4f3-verification.md`. This plan's temporary SDD workspace was removed after reviews. Worktree retained for audition feedback; no merge or push.

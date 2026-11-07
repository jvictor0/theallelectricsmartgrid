## Context

Running AHD stages integrate phase-derived samples. Explicit release instead subtracts a fixed decay increment per audio sample. Theory of Time retains its current global period even when transport phase resets to zero, and external sync retains or updates its frequency estimate while stopped.

## Goals / Non-Goals

**Goals:** Integrate the basic clock during release, preserve trigger-captured scaling, respond to live tempo changes, and finish tails with stopped transport. Preserve fractional call timing and long-release precision.

**Non-Goals:** Continuing phase modulation after Stop, changing normal AHD stage progression, changing gate/release triggers, or advancing transport phase while stopped.

## Decisions

- Expose `GetUnmodulatedGlobalPhaseIncrement()` on `TheoryOfTimeBase`, calculated from its current global period. This makes the basic clock's per-engine-sample increment available independently of transport state without an additional phase accumulator.
- Release integrates `clockIncrement * capturedRatio * capturedEnvelopePeriodSamples * releaseFactor` reference samples per call. This is the same scaling as normal forward unmodulated AHD progression. Current tempo remains live; topology edits cannot replace captured timing.
- Use the basic clock for every explicit release, including mute releases during playback. Modulation and phase resets never affect this ramp. Retaining a last modulated delta could freeze or reverse release and would violate the chosen unmodulated behavior.
- Require the existing clock pointer for active release, as for Running. Update isolated release fixtures to supply a clock; do not silently fall back to sample-only timing.

## Risks / Trade-offs

- Existing presets' release durations change away from a unit reference-sample rate → document the tempo-relative behavior and test ratios that do not cancel.
- Release fixtures previously omitted a clock or period → configure them explicitly and retain their precision and event-consumption assertions.
- External sync's effective frequency includes synchronization corrections → use the same effective basic rate that drives the existing clock, without inventing a separate tempo estimate.

## Migration Plan

No saved-state changes. Build and run the core suite in the existing worktree. Reverting the source change restores fixed sample-time release.

## Open Questions

None.

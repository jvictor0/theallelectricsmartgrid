## Context

Partial Machine tracking is implemented by `SpectralModelGeneric::SearchAndMerge` in `private/src/SpectralModel.hpp`. On each hop, existing atoms are processed strongest-first. Each atom searches analysis peaks from `atom.m_analysisOmega - m_omegaDensity` through `atom.m_analysisOmega + m_omegaDensity`, merges with one preferred candidate, and marks nearby candidates consumed so they do not create duplicate atoms.

The current preference function is magnitude-only after the organic-versus-synthetic rule. This can make an existing atom jump to a farther peak when that farther peak is slightly stronger than a nearby continuation. The `PartialMachineDensity` control is mapped through `InputSetter::m_density` into `m_omegaDensity`, and the code uses that value as a per-side radius, so the total match window is `2 * density`.

## Goals / Non-Goals

**Goals:**

- Make replacement atom selection prefer stable nearby continuations without ignoring magnitude.
- Define a small, deterministic theta function whose units match the current normalized frequency model.
- Preserve the existing density-window behavior, atom consumption behavior, attack/decay/portamento slews, and organic-over-synthetic preference.
- Add focused unit coverage that proves a nearby candidate can beat a farther, slightly louder candidate inside the same density window.

**Non-Goals:**

- Do not add a new user-facing parameter for hysteresis strength.
- Do not change the `PartialMachineDensity` knob range, preset schema, UI, visualizers, or frequency-dependent lane model.
- Do not change synthesis, pitch shifting, panning, reduction, or OLA behavior.

## Decisions

### Use density as the per-side linear-frequency falloff distance

The distance term should be normalized by the same `m_omegaDensity` radius used to build the search window:

```text
distance = abs(candidate.m_analysisOmega - atom.m_analysisOmega)
density = input.m_omegaDensity.Process(atom.m_index)
distanceWeight = max(0, 1 - distance / density)
theta = candidate.m_analysisMagnitude * distanceWeight
```

This makes theta equal to magnitude at the atom's current analysis frequency and zero at the search boundary. The distance is linear frequency distance, not volt-per-octave distance and not `FrequencyDependentParameter::FrequencyToLinear`. Conceptually this can be read as `distanceHz / densityHz`; implementation can use cycles per sample because both values scale by sample rate and the ratio is unchanged. Since `SearchAndMerge` treats density as `+/- density`, using `density / 2` would make candidates in the outer half of the current valid window score zero and would narrow the effective tracking window without changing the density parameter itself.

Alternative considered: use `theta = magnitude / (1 + distance / density)`. That keeps far candidates alive at the boundary, but it does not make theta near zero at the density limit and gives less hysteresis against boundary jumps.

### Keep organic-over-synthetic priority before theta

`AnalysisAtom::IsPreferred` currently prefers non-synthetic analysis atoms over synthetic harmonic atoms before comparing magnitudes. That priority should remain first so future synthetic harmonics cannot steal organic tracks just because a generated harmonic is closer.

The preferred comparison should therefore be:

1. Prefer organic over synthetic when candidate types differ.
2. Otherwise compare theta scores.
3. Use raw magnitude as a deterministic tie-breaker.
4. Use lower distance as a final tie-breaker.

### Move preference scoring into a helper near AnalysisAtom

The implementation should keep the scoring logic local to `SpectralModelGeneric`, likely by replacing or overloading `AnalysisAtom::IsPreferred` with a function that accepts the target atom frequency and density. The helper should not depend on Partial Machine types; `SpectralModelGeneric` is shared generic DSP infrastructure.

### Track whether an in-window candidate was found

`SearchAndMerge` should explicitly track whether the loop has found at least one candidate inside the upper bound before merging. That keeps the new distance-aware comparison honest and prevents a candidate immediately after the window from being treated as `bestIt` just because `lower_bound` returned it.

## Risks / Trade-offs

- More stable tracking can make large frequency jumps less eager. Mitigation: the score still multiplies by magnitude, so a much stronger far candidate can win inside the window; candidates outside the window remain new atoms as before.
- Boundary candidates score zero. Mitigation: this matches the requested "near zero at density" behavior, and remaining unconsumed peaks above the death threshold can still spawn new atoms.
- Tests that assume magnitude-only selection may need updates. Mitigation: add focused tests around the new preference contract and leave broad Partial Machine synthesis tests unchanged.

## Context

TheoryOfTime owns a continuous master-phase coordinate F and its chosen forward-history
partial inverse G. ScopeWriter stores real-time samples and builds FIR LODs on
the UI thread. Delay audio, periodic scopes, and modulation overlays share G.

## Goals / Non-Goals

**Goals:** phase coordinates independent of tempo; continuous delay playback
across transport resets; explicit periodic axes; generic reversible envelope
markers; phase-distance LFO slew; symmetric delay magnitude display in dB;
Lean/Veil proofs with reviewed C++ transition correspondence.

**Non-Goals:** hardware floating-point proofs, compiled-code verification,
exact reversal of LFO filter state, changing audible envelope behavior, redesigning
LOD filters, transport generations, or UI locking on the audio thread.

## Decisions

- `PhaseClock` maintains `q = musicalPhase + offset` while running. Tempo changes
  affect future increments only. Stopped samples advance by the nominal positive
  increment. Restart adjusts the offset and retains q; real warp reversals still
  move backwards. The former sample-space `WriteTapeHead` is removed.
- G maps q to absolute engine time on a fixed grid of 2^18 points per master
  cycle. Forward intervals overwrite their crossed points; reverse travel leaves
  G unchanged. The main inverse spans four cycles with a fixed 32 MiB payload:
  two owned fallback endpoints, one exact phase cut, and relative replacement
  timestamps per cell. A separate visit reassigns older islands below its cut to
  the fallback. Within a closed-cell visit, keep the minimum cut and its first
  timestamp; boundary touches reopen the endpoint without discarding that minimum. Partial first traversals use their confined entry/frontier interval.
  There is no pool or reversal-count limit. UI views reject coordinates ahead of
  the cursor, outside retention, or with endpoints newer than the real cutoff.
- Read heads project continuous phase at their selected speed and wrap into the
  selected loop strictly behind q. No independent restart rephasing occurs.
  DelayLine maps through G and converts only its output to host samples. Grain
  analysis-window safety remains a real-time operation after inversion.
- Periodic streams publish origin and period alongside their other metadata.
  The current-cycle coordinate is used when it is not ahead of the cursor;
  otherwise the previous-cycle coordinate is used. No startup stretching or
  inferred period is needed. Delay uses origin 0, period 1. LFOs use the clock
  offset and the common participating loop period. They do not record starts.
- Envelope/event scopes retain real and warped starts/ends. Descending movement
  undoes crossed boundaries generically; backward callbacks do not append starts.
- The time-warp scope uses a separate unmodulated PhaseClock/inverse and the
  warp LFO's explicit common period. It stores displacement rather than wrapped
  phase, adds the unwarped coordinate in the renderer, and breaks the path at
  wraps and the current/previous-cycle join. Its inverse needs two master cycles
  (16 MiB with branch storage), since the plotted period is at most one. MIDI
  clock corrections can reverse unmodulated travel. Cursor visibility and longest-source selection use mapped
  real sample duration rather than logical cycle width for all bound scopes.
- Scope publication uses atomic payloads and a revision; acceptance copies a
  coherent view with a bounded retry budget. Level-zero samples are atomic;
  higher levels belong to the UI thread. Overlays share the same periodic axis
  and inverse as their primary scope. Unbound scopes retain real-time behavior.
- LFO mix slew scales with absolute modulated phase increment. The warp generator
  uses nominal unmodulated increments; stopped oscillators settle at nominal rate.
  Small coefficients keep the stable approximation; large phase steps use expm1
  without a frequency cutoff clamp. This is distance-based smoothing, not undo.
- Delay scopes hold the magnitude of completed host writes. Existing LODs form
  the envelope; the visualizer applies dB scaling and fills symmetrically about
  each lane center. The signed return scope for spectra is unchanged.
- `formal/PhaseTime` proves the exact mathematical contracts and reachable Veil
  invariants. Source anchors and full-file hashes connect transitions to C++ and
  detect drift. `make lean-test` runs both source checks and kernel axiom audits.

## Risks / Trade-offs

- Live inverse changes during paint can create unavailable points. Timestamp
  cutoffs prevent newer writes from being treated as accepted history.
- Inverse phase capacity and sample-history capacity are separate bounds.
- Real-time FIR filtering does not commute with nonlinear time warping; the pure
  composition theorem states its S = L composed with F hypothesis explicitly.
- Phase-scaled slew is exact in the ideal model; discrete varying inputs still
  incur integration error and reversing does not recover previous filter state.
- Fixed-grid scatter cost scales with phase travel; delay performance smoke tests
  cover runtime validity, not iPad deadline guarantees.

## Migration Plan

No persisted patch format changes. Implement the clock and inverse, update delay
read heads, publish explicit axes, then validate native regressions, visualizer
pixels, delay performance smoke cases, and Lean proofs in stable build trees.

## Open Questions

None blocking. Mathematical scope and implementation boundaries are documented
in `formal/PhaseTime/README.md`.

## Compact inverse operating range

FixedCell proves causal reads, exact historical preimage existence, island
reassignment, one-cell sampled-phase residual and constant memory. InverseGrid
provides generic interpolation, codec and spectral bounds. Its finite Fourier
band/amplitude hypothesis is a declared contract, not a claim about today's warp
LFO; filtering remains deferred. Reassignment intentionally relaxes latest-forward
timestamp selection. The native sweep compares availability, phase error and
causality across production and finite-Fourier inputs, retaining latest-timestamp
metrics as diagnostics. Performance measurement remains separately deferred at
the user's request; no timing conclusions follow from correctness runs.

# Linkwitz–Riley splitter

[The executable definitions](../LinkwitzRileySplitter.lean) describe a balanced
binary tree for any positive band count. Each internal node carries a cutoff;
bands are enumerated from low to high. A one-band tree is the identity. Odd
counts divide into floor/ceiling halves. `Topology.balanced_bands` proves that
the topology has exactly the requested number of leaves. Zero is outside the
public contract and the reconstruction theorem requires `0 < numBands`.

Every filter section uses the shared `TPTSVF` recurrence and coefficient tuner.
At a node, write `D(s) = s² + k g s + g²`, where `g = tan(π cutoff)` and
`k = 1/Q`, `Q = cos(π/4) = 1/√2`. The shared SVF taps are

- `LP(s) = g² / D(s)`;
- `HP(s) = s² / D(s)`;
- `AP(s) = (s² − k g s + g²) / D(s)`.

Two sections produce `L = LP²`. The other branch is `H = AP − L`.
`highSplitTransfer_eq_highPassSquared` proves `H = HP²` when `k² = 2`
and the denominator is nonzero. Thus these are genuine LR4 branches. Both
are −6.0206 dB at their common cutoff, and `L + H = AP`.
The AP tap uses `fma(-2*k, bandPass, input)` and the SVF's two existing
integrator states; it needs no separate delay buffer.

## Phase compensation and reconstruction

A subtree's reconstruction phase is the product of the AP factors at all
its internal nodes. Before entering either child, a branch passes through an
independent cascade of the AP factors from its sibling subtree. This gives
every output the phase of the nodes it bypasses. For four bands, with root
cutoff `r` and low/high child cutoffs `l` and `h`, the transfers are

```
B0 = Lr * Ll * APh
B1 = Lr * Hl * APh
B2 = Hr * Lh * APl
B3 = Hr * Hh * APl
B0 + B1 + B2 + B3 = APr * APl * APh
```

[Transfer.lean](Transfer.lean) proves the per-band product for the general
tree, that its sum equals the common phase, and that this phase has unit
squared magnitude. The complex numerator of each AP is the conjugate of
its denominator. Positive `g,k` exclude denominator zeros, including DC
and Nyquist in homogeneous half-angle coordinates. Cutoff ordering is useful
for the intended frequency partition; reconstruction does not require it.

[Realizes.lean](Realizes.lean) connects these formulas to the executable
`State.bands` array, including its actual sum. It composes the existing
TPTSVF stream proofs through projections, cascades, and subtraction. The
underlying SVF proofs transport their analog rational functions through the
bilinear homomorphism. Homogeneous evaluation at `(1-q,1+q)` is related to
the prepared complex frequency response by `Snapshot.bandResponse_delay`.

[Tuning.lean](Tuning.lean) proves that valid cutoff specifications establish
those tuning relations and valid snapshots, that coefficient updates preserve
all histories and output fields, and that repeated updates are idempotent.
The transfer theorems assume fixed coefficients and initially zero integrator
states. They do not assert unity gain during retuning, with nonzero histories,
or after nonlinear band processing. Exact real/complex proofs do not bound
IEEE rounding or the C++ trigonometric lookup approximation.

[Channels.lean](Channels.lean) proves processing, tuning, and capture commute
with channel projection, and lifts each band realization to independent SIMD
lanes.

## Generated interface

Only the four-band specialization is exported:
`LinkwitzRileySplitter4BandGeneric<Number, Size>`. The topology is reduced at
generation time into fixed nested records. There is no runtime tree and no
collection of filter records. The four scalar-band outputs form
`std::array<Vector, 4> m_state.m_bands`, where `Vector` is
`MultiChannelNumber<Number, Size>`.

The export has eight SVF sections: two at each of three split nodes and two
sibling compensation sections. `LogicalSpec` stores the three cutoffs as
`m_tree.m_value` (middle), `m_tree.m_low.m_value` (low), and
`m_tree.m_high.m_value` (high). All are cycles per sample, with `0 < cutoff < ½`.

- `ComputeCoefficients(spec)` updates section coefficients while preserving history.
- `MakeSnapshot(spec)` returns the three `(g,k)` pairs directly from the same specification.
- `Process(input)` updates the state and its four band outputs.

There are no emitted response or transfer methods. A later consumer can
combine the proved complex band transfers with its gains and then take the
magnitude of that sum. The generated header is a pipeline artifact; production
C++ integration and the multiband saturator rewrite are separate work.

Validation includes native impulse/sinusoidal reconstruction against an
independent direct-form allpass cascade for 1, 2, 3, 4, 5, 7, and 8 bands;
every generated four-band state field against native Lean; float/double SIMD
widths 1–4 with different lane inputs; coefficient-only writes and exact
history preservation; and snapshot capture against the Math4096 functions.

[Prepared.lean](Prepared.lean) supplies a reusable coefficient bank and reset
operations for the [Lean multiband saturator](../MultibandSaturator/README.md).
It prepares each node once through the shared SVF tuner and copies those
coefficients to its split and compensation sections. The existing generated
splitter interface is unchanged; packed preparation and multiband emission
will use this additional pure interface.

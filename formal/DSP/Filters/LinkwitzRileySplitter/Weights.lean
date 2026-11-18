import DSP.Filters.LinkwitzRileySplitter

/-! Real amplitude weights after factoring the common LR all-pass phase.
Half-angle coordinates avoid a tangent singularity at Nyquist. -/
namespace DSP.LinkwitzRileySplitter

def splitWeight {R : Type} [Mul R] [Add R] [Div R]
    (g sine cosine : R) : R × R :=
  let gc := g*cosine
  let gc2 := gc*gc
  let sine2 := sine*sine
  let low := gc2*gc2
  let high := sine2*sine2
  (low/(low+high), high/(low+high))

def bandWeight {R : Type} [OfNat R 1] [Mul R] [Add R] [Div R] :
    (t : Topology) → TreeData (TPTSVF.Snapshot R) t → R → R → Nat → R
  | .leaf, _, _, _, _ => 1
  | .branch low high, s, sine, cosine, i =>
    let w := splitWeight s.value.g sine cosine
    if i < low.bands then bandWeight low s.low sine cosine i * w.1
    else bandWeight high s.high sine cosine (i-low.bands) * w.2

end DSP.LinkwitzRileySplitter

import DSP.Filters.TPTSVF

namespace DSP

/-- The app's `LinkwitzRileyCrossover`: a first `TPTSVF` section splits the
input, and its lowpass and highpass each pass through a second section. The
outputs are `lowSecond.lowPass` and `highSecond.highPass`. With the app's
dampings 2 cos(π/8) and 2 cos(3π/8) the branches are fourth-order Butterworth. -/
structure LinkwitzRileyCrossover (R : Type) where
  first : TPTSVF R
  lowSecond : TPTSVF R
  highSecond : TPTSVF R

namespace LinkwitzRileyCrossover

variable {R : Type} [Add R] [Sub R] [Mul R] [OfNat R 2]

def step (f : LinkwitzRileyCrossover R) (input : Sample R) : LinkwitzRileyCrossover R :=
  let first := f.first.step input
  { first
    lowSecond := f.lowSecond.step ⟨first.lowPass⟩
    highSecond := f.highSecond.step ⟨first.highPass⟩ }

def stepFloat : Step (LinkwitzRileyCrossover Float) (Sample Float) := step

end LinkwitzRileyCrossover
end DSP

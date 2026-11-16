import DSP.Filters.TPTSVF

namespace DSP

/-- A `TPTSVF` with the app's equal-power morph, in the operation order of
`GetMorphedOutput`. The caller computes `cosine`, `sine` and `upperHalf` from
the blend: below one half the output mixes lowpass and bandpass, above it
bandpass and highpass. -/
structure MorphingTPTSVF (R : Type) where
  svf : TPTSVF R
  cosine : R
  sine : R
  upperHalf : Bool
  output : R

namespace MorphingTPTSVF

variable {R : Type} [Add R] [Sub R] [Mul R] [OfNat R 2]

def step (f : MorphingTPTSVF R) (input : Sample R) : MorphingTPTSVF R :=
  let svf := f.svf.step input
  { f with
    svf
    output := if f.upperHalf then svf.bandPass * f.cosine + svf.highPass * f.sine
      else svf.lowPass * f.cosine + svf.bandPass * f.sine }

def stepFloat : Step (MorphingTPTSVF Float) (Sample Float) := step

end MorphingTPTSVF
end DSP

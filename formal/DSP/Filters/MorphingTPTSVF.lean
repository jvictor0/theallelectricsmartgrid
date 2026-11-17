import DSP.Filters.TPTSVF
import DSP.IfLess

namespace DSP

/-- A `TPTSVF` with the app's equal-power morph from `GetMorphedOutput`,
using explicit fused multiply-adds. The caller stores the clamped blend and computes `cosine`
and `sine` from it. Both halves are mixed, and each channel selects bandpass and
highpass above one half, lowpass and bandpass otherwise. -/
structure MorphingTPTSVF (R : Type) where
  svf : TPTSVF R
  cosine : R
  sine : R
  blend : R
  output : R

namespace MorphingTPTSVF

variable {R : Type} [Add R] [Sub R] [Neg R] [Mul R] [FMA R] [OfNat R 2] [OfScientific R] [IfLess R]

def step (f : MorphingTPTSVF R) (input : Sample R) : MorphingTPTSVF R :=
  let svf := f.svf.step input
  let lowHalf := FMA.fma svf.bandPass f.sine (svf.lowPass * f.cosine)
  let highHalf := FMA.fma svf.highPass f.sine (svf.bandPass * f.cosine)
  { f with svf, output := IfLess.ifLess 0.5 f.blend highHalf lowHalf }

def stepFloat : Step (MorphingTPTSVF Float) (Sample Float) := step

end MorphingTPTSVF
end DSP

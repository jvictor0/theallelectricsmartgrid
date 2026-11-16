import DSP.Filters.OnePoleTPTHighPass
import DSP.Filters.OnePoleTPTLowPass.Proofs

namespace DSP.OnePoleTPTHighPass

open Polynomial

variable {K : Type} [Field K]

/-- The stored lowpass keeps its own specification inside the highpass. -/
theorem lowPass_realizes (f : OnePoleTPTHighPass K) (g : K) (hG : f.lowPass.G * (1 + g) = g)
    (hs : f.lowPass.s = 0) :
    Bilinear.Realizes step f (·.lowPass.y) (Bilinear.analog (C g) (X + C g)) :=
  Bilinear.Realizes.project step OnePoleTPTLowPass.step (·.lowPass) (fun _ _ => rfl)
    (OnePoleTPTLowPass.realizes f.lowPass g hG hs)

/-- From rest, the stored output realizes the bilinear image of s / (s + g). -/
theorem realizes (f : OnePoleTPTHighPass K) (g : K) (hG : f.lowPass.G * (1 + g) = g)
    (hs : f.lowPass.s = 0) :
    Bilinear.Realizes step f (·.y) (Bilinear.analog X (X + C g)) := by
  refine ((lowPass_realizes f g hG hs).complement (fun _ _ => rfl)).transfer_eq ?_
  rw [Bilinear.one_sub_analog _ _ (OnePoleTPTLowPass.denominator_ne_zero _ g hG)]
  congr 1
  ring

end DSP.OnePoleTPTHighPass

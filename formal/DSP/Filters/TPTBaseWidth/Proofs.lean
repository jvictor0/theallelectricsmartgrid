import DSP.Filters.TPTBaseWidth
import DSP.Filters.OnePoleTPTHighPass.Proofs

namespace DSP.TPTBaseWidth

open Polynomial

variable {K : Type} [Field K]

/-- From rest, the output realizes the bilinear image of the analog band
s / (s + g₁) · g₂ / (s + g₂), for base gain g₁ and upper gain g₂. -/
theorem realizes (f : TPTBaseWidth K) (g₁ g₂ : K)
    (hG₁ : f.highPass.lowPass.G * (1 + g₁) = g₁) (hs₁ : f.highPass.lowPass.s = 0)
    (hG₂ : f.lowPass.G * (1 + g₂) = g₂) (hs₂ : f.lowPass.s = 0) :
    Bilinear.Realizes step f (·.lowPass.y)
      (Bilinear.analog (C g₂ * X) ((X + C g₂) * (X + C g₁))) := by
  have highPass := Bilinear.Realizes.project step OnePoleTPTHighPass.step (·.highPass)
    (fun _ _ => rfl) (OnePoleTPTHighPass.realizes f.highPass g₁ hG₁ hs₁)
  refine (Bilinear.Realizes.cascade step OnePoleTPTLowPass.step (·.lowPass) (·.highPass.y)
    (fun _ _ => rfl) highPass (OnePoleTPTLowPass.realizes f.lowPass g₂ hG₂ hs₂)).transfer_eq ?_
  rw [Bilinear.analog_mul]

end DSP.TPTBaseWidth

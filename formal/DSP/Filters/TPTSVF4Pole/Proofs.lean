import DSP.Filters.TPTSVF4Pole
import DSP.Filters.TPTSVF.Proofs

noncomputable section

namespace DSP.TPTSVF4Pole

open Polynomial

variable {K : Type} [Field K]

/-! Each stage may have its own gain and damping; the app tunes both alike.
The stage transfers multiply, as series analog sections do. -/

theorem realizes_highPass (f : TPTSVF4Pole K) (g₁ g₂ : K) (ht₁ : f.stage1.Tuned g₁)
    (hr₁ : f.stage1.AtRest) (ht₂ : f.stage2.Tuned g₂) (hr₂ : f.stage2.AtRest) :
    Bilinear.Realizes stepHighPass f (·.stage2.highPass)
      (Bilinear.analog (TPTSVF.highPassNumerator * TPTSVF.highPassNumerator)
        (TPTSVF.denominator g₂ f.stage2.k * TPTSVF.denominator g₁ f.stage1.k)) := by
  have first := Bilinear.Realizes.project stepHighPass TPTSVF.step (·.stage1) (fun _ _ => rfl)
    (TPTSVF.realizes_highPass f.stage1 g₁ ht₁ hr₁)
  refine (Bilinear.Realizes.cascade stepHighPass TPTSVF.step (·.stage2) (·.stage1.highPass)
    (fun _ _ => rfl) first (TPTSVF.realizes_highPass f.stage2 g₂ ht₂ hr₂)).transfer_eq ?_
  rw [Bilinear.analog_mul]

theorem realizes_lowPass (f : TPTSVF4Pole K) (g₁ g₂ : K) (ht₁ : f.stage1.Tuned g₁)
    (hr₁ : f.stage1.AtRest) (ht₂ : f.stage2.Tuned g₂) (hr₂ : f.stage2.AtRest) :
    Bilinear.Realizes stepLowPass f (·.stage2.lowPass)
      (Bilinear.analog (TPTSVF.lowPassNumerator g₂ * TPTSVF.lowPassNumerator g₁)
        (TPTSVF.denominator g₂ f.stage2.k * TPTSVF.denominator g₁ f.stage1.k)) := by
  have first := Bilinear.Realizes.project stepLowPass TPTSVF.step (·.stage1) (fun _ _ => rfl)
    (TPTSVF.realizes_lowPass f.stage1 g₁ ht₁ hr₁)
  refine (Bilinear.Realizes.cascade stepLowPass TPTSVF.step (·.stage2) (·.stage1.lowPass)
    (fun _ _ => rfl) first (TPTSVF.realizes_lowPass f.stage2 g₂ ht₂ hr₂)).transfer_eq ?_
  rw [Bilinear.analog_mul]

theorem realizes_bandPass (f : TPTSVF4Pole K) (g₁ g₂ : K) (ht₁ : f.stage1.Tuned g₁)
    (hr₁ : f.stage1.AtRest) (ht₂ : f.stage2.Tuned g₂) (hr₂ : f.stage2.AtRest) :
    Bilinear.Realizes stepBandPass f (·.stage2.bandPass)
      (Bilinear.analog (TPTSVF.bandPassNumerator g₂ * TPTSVF.bandPassNumerator g₁)
        (TPTSVF.denominator g₂ f.stage2.k * TPTSVF.denominator g₁ f.stage1.k)) := by
  have first := Bilinear.Realizes.project stepBandPass TPTSVF.step (·.stage1) (fun _ _ => rfl)
    (TPTSVF.realizes_bandPass f.stage1 g₁ ht₁ hr₁)
  refine (Bilinear.Realizes.cascade stepBandPass TPTSVF.step (·.stage2) (·.stage1.bandPass)
    (fun _ _ => rfl) first (TPTSVF.realizes_bandPass f.stage2 g₂ ht₂ hr₂)).transfer_eq ?_
  rw [Bilinear.analog_mul]

end DSP.TPTSVF4Pole

end

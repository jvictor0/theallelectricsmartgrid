import DSP.Filters.LinkwitzRileyCrossover
import DSP.Filters.TPTSVF.Proofs
import Mathlib.Analysis.SpecialFunctions.Trigonometric.Basic

noncomputable section

namespace DSP.LinkwitzRileyCrossover

open Polynomial

variable {K : Type} [Field K]

/-- Both branches share the first section, each with its own second section. -/
structure Ready (f : LinkwitzRileyCrossover K) (g : K) : Prop where
  first : f.first.Tuned g
  lowSecond : f.lowSecond.Tuned g
  highSecond : f.highSecond.Tuned g
  firstRest : f.first.AtRest
  lowRest : f.lowSecond.AtRest
  highRest : f.highSecond.AtRest

theorem realizes_lowPass (f : LinkwitzRileyCrossover K) (g : K) (h : f.Ready g) :
    Bilinear.Realizes step f (·.lowSecond.lowPass)
      (Bilinear.analog (TPTSVF.lowPassNumerator g * TPTSVF.lowPassNumerator g)
        (TPTSVF.denominator g f.lowSecond.k * TPTSVF.denominator g f.first.k)) := by
  have first := Bilinear.Realizes.project step TPTSVF.step (·.first) (fun _ _ => rfl)
    (TPTSVF.realizes_lowPass f.first g h.first h.firstRest)
  refine (Bilinear.Realizes.cascade step TPTSVF.step (·.lowSecond) (·.first.lowPass)
    (fun _ _ => rfl) first (TPTSVF.realizes_lowPass f.lowSecond g h.lowSecond h.lowRest)).transfer_eq ?_
  rw [Bilinear.analog_mul]

theorem realizes_highPass (f : LinkwitzRileyCrossover K) (g : K) (h : f.Ready g) :
    Bilinear.Realizes step f (·.highSecond.highPass)
      (Bilinear.analog (TPTSVF.highPassNumerator * TPTSVF.highPassNumerator)
        (TPTSVF.denominator g f.highSecond.k * TPTSVF.denominator g f.first.k)) := by
  have first := Bilinear.Realizes.project step TPTSVF.step (·.first) (fun _ _ => rfl)
    (TPTSVF.realizes_highPass f.first g h.first h.firstRest)
  refine (Bilinear.Realizes.cascade step TPTSVF.step (·.highSecond) (·.first.highPass)
    (fun _ _ => rfl) first
    (TPTSVF.realizes_highPass f.highSecond g h.highSecond h.highRest)).transfer_eq ?_
  rw [Bilinear.analog_mul]

/-- Dampings with k₁² + k₂² = 4 and k₁² k₂² = 2 make each branch Butterworth:
the denominator B satisfies B(s) B(-s) = s⁸ + g⁸, so on the imaginary axis
|B(jω)|² = ω⁸ + g⁸. -/
theorem butterworth (g k₁ k₂ : K) (h₁ : k₁ ^ 2 + k₂ ^ 2 = 4) (h₂ : k₁ ^ 2 * k₂ ^ 2 = 2) :
    (TPTSVF.denominator g k₁ * TPTSVF.denominator g k₂) *
        (TPTSVF.denominator g k₁ * TPTSVF.denominator g k₂).comp (-X) =
      X ^ 8 + C g ^ 8 := by
  have c₁ : C k₁ ^ 2 + C k₂ ^ 2 = (4 : K[X]) := by
    rw [← map_pow, ← map_pow, ← map_add, h₁]
    rfl
  have c₂ : C k₁ ^ 2 * C k₂ ^ 2 = (2 : K[X]) := by
    rw [← map_pow, ← map_pow, ← map_mul, h₂]
    rfl
  simp only [TPTSVF.denominator, mul_comp, add_comp, pow_comp, X_comp, C_comp]
  linear_combination (-(C g ^ 2 * X ^ 2 * (X ^ 2 + C g ^ 2) ^ 2)) * c₁ +
    (C g ^ 2 * X ^ 2) ^ 2 * c₂

/-- The app's dampings 2 cos(π/8) and 2 cos(3π/8) satisfy the Butterworth conditions. -/
theorem app_dampings :
    (2 * Real.cos (Real.pi / 8)) ^ 2 + (2 * Real.cos (3 * Real.pi / 8)) ^ 2 = 4 ∧
      (2 * Real.cos (Real.pi / 8)) ^ 2 * (2 * Real.cos (3 * Real.pi / 8)) ^ 2 = 2 := by
  have hc : Real.cos (3 * Real.pi / 8) = Real.sin (Real.pi / 8) := by
    rw [← Real.cos_pi_div_two_sub]
    ring_nf
  have hd : 2 * Real.sin (Real.pi / 8) * Real.cos (Real.pi / 8) = Real.sqrt 2 / 2 := by
    rw [← Real.sin_two_mul, ← Real.sin_pi_div_four]
    ring_nf
  have hs : Real.sqrt 2 ^ 2 = 2 := Real.sq_sqrt (by norm_num)
  have hp := Real.sin_sq_add_cos_sq (Real.pi / 8)
  rw [hc]
  constructor
  · linear_combination 4 * hp
  · have : (2 * Real.sin (Real.pi / 8) * Real.cos (Real.pi / 8)) ^ 2 = 1 / 2 := by
      rw [hd]
      linear_combination hs / 4
    linear_combination 4 * this

end DSP.LinkwitzRileyCrossover

end

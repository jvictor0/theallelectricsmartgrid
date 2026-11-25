import DSP.Filters.ButterworthDownsampler.Transfer
import DSP.Filters.Tuning.Proofs

noncomputable section

namespace DSP.ButterworthDownsampler

theorem section_response (g k f : ℝ) (hg : 0 < g) (hk : 0 < k) :
    ((prepareSection g k).transfer).response f =
      Response.svf (g : ℂ) (k : ℂ) 1 0 0
        (1 - ResponseFrequency.unitDelay f) (1 + ResponseFrequency.unitDelay f) := by
  simp [Transfer.response, Coefficients.transfer, IIR.numerator, IIR.denominator, Coefficients.toIIR,
    Fin.sum_univ_two, prepareSection, Response.svf]
  have ha₀ : (1 + (k : ℂ) * (g : ℂ) + (g : ℂ) * (g : ℂ)) ≠ 0 := by
    exact_mod_cast (show (1 + k * g + g * g : ℝ) ≠ 0 from ne_of_gt (by positivity))
  conv_rhs => rw [← div_div_div_cancel_right₀ ha₀]
  generalize ha : (1 + (k : ℂ) * (g : ℂ) + (g : ℂ) * (g : ℂ)) = a at ha₀ ⊢
  congr 1 <;> field_simp [ha₀] <;> subst a <;> ring

private theorem half_angle_power (g angle f : ℝ) (hg : 0 < g)
    (hc : 0 < Real.cos (Real.pi * f)) :
    Complex.normSq (Response.svf (g : ℂ) ((2 * Real.cos angle : ℝ) : ℂ) 1 0 0
      (⟨0, Real.sin (Real.pi * f)⟩ : ℂ) (Real.cos (Real.pi * f) : ℂ)) =
      1 / sectionPowerDenominator angle (Tuning.warp f / g) := by
  let w := g * Real.cos (Real.pi * f)
  let s := Real.sin (Real.pi * f)
  have hw : w ≠ 0 := ne_of_gt (mul_pos hg hc)
  have hx : Tuning.warp f / g = s / w := by
    rw [Tuning.warp, Real.tan_eq_sin_div_cos, div_div, mul_comm _ g]

  rw [hx, Response.svf, Complex.normSq_div]
  simp [Complex.normSq_apply, Complex.mul_re, Complex.mul_im, Complex.add_re, Complex.add_im, -Complex.ofReal_mul]
  change (w * w) * (w * w) / ((-(s * s) + w * w) * (-(s * s) + w * w) +
    (2 * Real.cos angle * w * s) * (2 * Real.cos angle * w * s)) = _
  rw [← one_div]
  have hw₄ : w ^ 4 ≠ 0 := pow_ne_zero 4 hw
  conv_rhs => rw [← div_div_div_cancel_right₀ (inv_ne_zero hw₄)]
  simp only [div_inv_eq_mul, one_mul, sectionPowerDenominator, Real.cos_two_mul]
  congr 1
  · ring
  · field_simp [hw]
    ring

theorem section_power (g angle f : ℝ) (hg : 0 < g) (ha : 0 < Real.cos angle)
    (hf₀ : 0 ≤ f) (hf₁ : f < 1 / 2) :
    ((prepareSection g (2 * Real.cos angle)).transfer).power f =
      1 / sectionPowerDenominator angle (Tuning.warp f / g) := by
  have hk : 0 < 2 * Real.cos angle := by positivity
  have hc : 0 < Real.cos (Real.pi * f) := by
    exact Real.cos_pos_of_mem_Ioo ⟨by nlinarith [Real.pi_pos], by nlinarith [Real.pi_pos]⟩
  rw [Transfer.power, section_response _ _ _ hg hk,
    ← Response.frequency_eq_delay _ (Response.svf_scale _ _ _ _ _) f, half_angle_power _ _ _ hg hc]

end DSP.ButterworthDownsampler

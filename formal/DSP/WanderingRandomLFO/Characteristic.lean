import DSP.WanderingRandomLFO.SamplingProofs
import DSP.WanderingRandomLFO.UniformSinc
import Mathlib.Data.Fin.VecNotation
import Mathlib.MeasureTheory.Measure.CharacteristicFunction.Basic

namespace DSP.WanderingRandomLFO

open MeasureTheory Set
open scoped BigOperators

noncomputable section

/-- Weights of the four consecutive uniform knots at a given spline phase. -/
def splineWeight (t : ℝ) : Fin 4 → ℝ :=
  ![(1 - t)^3 / 6, (4 - 6*t^2 + 3*t^3) / 6, (1 + 3*t + 3*t^2 - 3*t^3) / 6, t^3 / 6]

theorem sum_splineWeight (t : ℝ) : ∑ j, splineWeight t j = 1 := by
  simp [Fin.sum_univ_four, splineWeight]
  ring

theorem normalizedSpline_eq_sum (t : ℝ) (p : Fin 4 → ℝ) :
    normalizedSpline t p = ∑ j, splineWeight t j * p j := by
  simp [normalizedSpline_eq, Fin.sum_univ_four, splineWeight]
  ring

theorem one_sixth_le_middleWeight (t : ℝ) (ht : t ∈ Icc 0 1) :
    1 / 6 ≤ splineWeight t 1 := by
  have ht' : 0 ≤ t * (1 - t) := mul_nonneg ht.1 (sub_nonneg.mpr ht.2)
  have h : 0 ≤ (1 - t) * (1 + t * (1 - t)) :=
    mul_nonneg (sub_nonneg.mpr ht.2) (by linarith)
  simp [splineWeight]
  nlinarith

instance : IsProbabilityMeasure stationaryAmplitudeLaw := by
  unfold stationaryAmplitudeLaw
  infer_instance

/-- The characteristic function of the bipolar stationary spline output. -/
def stationaryCharacteristic (s : ℝ) : ℂ := charFun stationaryAmplitudeLaw s

theorem norm_stationaryCharacteristic_le_one (s : ℝ) : ‖stationaryCharacteristic s‖ ≤ 1 :=
  norm_charFun_le_one s

/-- Represent the stationary law as a weighted sum of bipolar knot draws. -/
theorem stationaryAmplitudeLaw_eq_sum :
    stationaryAmplitudeLaw =
      (unitUniform.prod knotUniforms).map (fun x : ℝ × (Fin 4 → ℝ) =>
        ∑ j, splineWeight x.1 j * (2 * x.2 j - 1)) := by
  rw [stationaryAmplitudeLaw_eq]
  congr 1
  funext x
  rw [← normalizedSpline_eq x.1 x.2, normalizedSpline_eq_sum]
  have h (a b : ℝ) : a * (2 * b) = 2 * (a * b) := by ring
  simp_rw [mul_sub, mul_one, h, Finset.sum_sub_distrib, ← Finset.mul_sum, sum_splineWeight]

private theorem stationaryCharacteristic_eq_sinc_uniform (s : ℝ) :
    stationaryCharacteristic s =
      (↑(∫ t, ∏ j : Fin 4, Real.sinc (s * splineWeight t j) ∂unitUniform) : ℂ) := by
  let amplitude := fun x : ℝ × (Fin 4 → ℝ) => ∑ j, splineWeight x.1 j * (2*x.2 j-1)
  have ha : Measurable amplitude := by
    simp [amplitude, Fin.sum_univ_four, splineWeight]
    fun_prop
  let f := fun x : ℝ × (Fin 4 → ℝ) => Complex.exp ((↑(s * amplitude x) : ℂ) * Complex.I)
  have hm : Measurable f := by
    unfold f
    fun_prop
  have hf : Integrable f (unitUniform.prod knotUniforms) := by
    apply (integrable_const (1 : ℝ)).mono' hm.aestronglyMeasurable
    exact ae_of_all _ fun x => by simp [f, Complex.norm_exp, Complex.mul_re]
  rw [stationaryCharacteristic, charFun_apply_real, stationaryAmplitudeLaw_eq_sum]
  simp_rw [← Complex.ofReal_mul]
  change (∫ x : ℝ, Complex.exp ((↑(s*x) : ℂ) * Complex.I) ∂
    (Measure.map amplitude (unitUniform.prod knotUniforms))) = _
  rw [integral_map ha.aemeasurable (by fun_prop)]
  change (∫ x, f x ∂unitUniform.prod knotUniforms) = _
  rw [integral_prod f hf, ← integral_complex_ofReal (μ := unitUniform)
    (f := fun t => ∏ j : Fin 4, Real.sinc (s * splineWeight t j))]
  apply integral_congr_ae
  filter_upwards with t
  have hexp (p : Fin 4 → ℝ) :
      f (t, p) = ∏ j, Complex.exp ((↑((s * splineWeight t j) * (2*p j-1)) : ℂ) * Complex.I) := by
    rw [← Complex.exp_sum]
    unfold f amplitude
    congr 1
    simp only [Fin.sum_univ_four]
    push_cast
    ring
  simp_rw [hexp]
  unfold knotUniforms
  rw [integral_fintype_prod_eq_prod (fun j : Fin 4 => fun u : ℝ =>
    Complex.exp ((↑((s * splineWeight t j) * (2*u-1)) : ℂ) * Complex.I))]
  simp_rw [integral_uniform_bipolar_exp, Complex.ofReal_prod]


/-- The characteristic function reduces to one quadrature over the uniform
spline phase. Each knot contributes its own sinc factor. -/
theorem stationaryCharacteristic_eq_sinc (s : ℝ) :
    stationaryCharacteristic s =
      (↑(∫ t in (0 : ℝ)..1, ∏ j : Fin 4, Real.sinc (s * splineWeight t j)) : ℂ) := by
  rw [stationaryCharacteristic_eq_sinc_uniform, unitUniform, restrict_Ico_eq_restrict_Ioc,
    intervalIntegral.integral_of_le zero_le_one]

theorem stationaryCharacteristic_neg (s : ℝ) :
    stationaryCharacteristic (-s) = stationaryCharacteristic s := by
  simp_rw [stationaryCharacteristic_eq_sinc_uniform, neg_mul, Real.sinc_neg]

/-- One cubic weight is at least one sixth throughout the phase interval,
providing a uniform bound on the characteristic function away from zero. -/
theorem norm_stationaryCharacteristic_le_six_div_abs {s : ℝ} (hs : s ≠ 0) :
    ‖stationaryCharacteristic s‖ ≤ 6 / |s| := by
  rw [stationaryCharacteristic_eq_sinc_uniform, Complex.norm_real]
  have hb (t : ℝ) (ht : t ∈ Ico (0 : ℝ) 1) :
      ‖∏ j : Fin 4, Real.sinc (s * splineWeight t j)‖ ≤ 6 / |s| := by
    have hw := one_sixth_le_middleWeight t ⟨ht.1, ht.2.le⟩
    have hwp : 0 < splineWeight t 1 := (by norm_num : (0 : ℝ) < 1/6).trans_le hw
    have hmid : |Real.sinc (s * splineWeight t 1)| ≤ 6 / |s| := by
      rw [Real.sinc_of_ne_zero (mul_ne_zero hs hwp.ne'), abs_div,
        abs_mul, abs_of_pos hwp]
      have hsin : |Real.sin (s * splineWeight t 1)| ≤ 1 :=
        abs_le.mpr ⟨Real.neg_one_le_sin _, Real.sin_le_one _⟩
      calc
        _ ≤ 1 / (|s| * splineWeight t 1) :=
          div_le_div_of_nonneg_right hsin (mul_pos (abs_pos.mpr hs) hwp).le
        _ ≤ 1 / (|s| * (1/6)) :=
          div_le_div_of_nonneg_left zero_le_one (by positivity)
            (mul_le_mul_of_nonneg_left hw (abs_nonneg s))
        _ = _ := by ring
    have h0 := Real.abs_sinc_le_one (s * splineWeight t 0)
    have h2 := Real.abs_sinc_le_one (s * splineWeight t 2)
    have h3 := Real.abs_sinc_le_one (s * splineWeight t 3)
    simp only [Real.norm_eq_abs, Fin.prod_univ_four, abs_mul]
    calc
      _ ≤ 1 * (6 / |s|) * 1 * 1 := by gcongr
      _ = _ := by simp
  have hae : ∀ᵐ t ∂unitUniform, t ∈ Ico (0 : ℝ) 1 := ae_restrict_mem measurableSet_Ico
  simpa using norm_integral_le_of_norm_le_const (hae.mono hb)

end

end DSP.WanderingRandomLFO

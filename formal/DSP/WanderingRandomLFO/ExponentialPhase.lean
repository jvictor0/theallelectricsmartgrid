import DSP.WanderingRandomLFO.PhaseLaw

namespace DSP.WanderingRandomLFO

open MeasureTheory ProbabilityTheory Set Filter
open scoped ENNReal

theorem uniformFraction_smul_volume_le (lambda c : ℝ) (hc : 1 ≤ c)
    (hc' : c ≤ (frameSamples : ℝ)) :
    ENNReal.ofReal (uniformFraction lambda) • (volume : Measure Phase) ≤ phaseStepLaw lambda c := by
  let r := max 1 lambda
  let s := Ioc (0 : ℝ) c⁻¹
  let t := Ioc (0 : ℝ) 1
  let scale := fun x : ℝ => c * x
  let wrap := fun x : ℝ => (x : Phase)
  let step := fun x : ℝ => (c * min 1 x : Phase)
  have hr : 0 ≤ r := zero_le_one.trans (le_max_left _ _)
  have hcpos : 0 < c := zero_lt_one.trans_le hc
  have hcap : c⁻¹ ≤ 1 := inv_le_one_of_one_le₀ hc
  have hs : MeasurableSet s := measurableSet_Ioc
  have ht : MeasurableSet t := measurableSet_Ioc
  have hscale : Measurable scale := by dsimp [scale]; fun_prop
  have hwrap : Measurable wrap := AddCircle.measurable_mk'
  have hstep : Measurable step := hwrap.comp (by fun_prop : Measurable (fun x : ℝ => c * min 1 x))
  have hdensity : ENNReal.ofReal (r * Real.exp (-r)) • (volume : Measure ℝ).restrict s ≤
      expMeasure r := by
    change _ ≤ (volume : Measure ℝ).withDensity (exponentialPDF r)
    refine le_trans ?_ (Measure.restrict_le_self (s := s))
    rw [restrict_withDensity hs, ← withDensity_const]
    apply withDensity_mono
    filter_upwards [ae_restrict_mem hs] with x hx
    rw [exponentialPDF_of_nonneg hx.1.le]
    apply ENNReal.ofReal_le_ofReal
    apply mul_le_mul_of_nonneg_left _ hr
    apply Real.exp_le_exp.mpr
    have hxle : x ≤ 1 := hx.2.trans hcap
    simpa using neg_le_neg (mul_le_mul_of_nonneg_left hxle hr)
  have hpreimage : scale ⁻¹' t = s := by
    ext x
    change (0 < c * x ∧ c * x ≤ 1) ↔ (0 < x ∧ x ≤ c⁻¹)
    rw [mul_pos_iff_of_pos_left hcpos, ← one_div c, le_div_iff₀ hcpos, mul_comm x c]
  have hscaled : ((volume : Measure ℝ).restrict s).map scale =
      ENNReal.ofReal c⁻¹ • (volume : Measure ℝ).restrict t := by
    rw [← hpreimage, ← Measure.restrict_map hscale ht]
    change ((volume : Measure ℝ).map (fun x => c * x)).restrict t = _
    rw [Real.map_volume_mul_left hcpos.ne', abs_of_pos (inv_pos.mpr hcpos), Measure.restrict_smul]
  have hwrapped : ((volume : Measure ℝ).restrict t).map wrap = (volume : Measure Phase) := by
    simpa [t, wrap] using (AddCircle.measurePreserving_mk 1 0).map_eq
  have hmap : ((volume : Measure ℝ).restrict s).map step =
      ENNReal.ofReal c⁻¹ • (volume : Measure Phase) := by
    calc
      _ = ((volume : Measure ℝ).restrict s).map (wrap ∘ scale) := by
        apply Measure.map_congr
        filter_upwards [ae_restrict_mem hs] with x hx
        have hxle : x ≤ 1 := hx.2.trans hcap
        simp [step, wrap, scale, min_eq_right hxle]
      _ = _ := by rw [← Measure.map_map hwrap hscale, hscaled, Measure.map_smul, hwrapped]
  have hbound : uniformFraction lambda ≤ (r * Real.exp (-r)) * c⁻¹ := by
    calc
      _ ≤ r / (frameSamples : ℝ) * Real.exp (-r) := min_le_right _ _
      _ ≤ r / c * Real.exp (-r) :=
        mul_le_mul_of_nonneg_right (div_le_div_of_nonneg_left hr hcpos hc') (Real.exp_pos _).le
      _ = _ := by rw [div_eq_mul_inv]; ring
  have hlaw := Measure.map_mono hdensity hstep
  rw [Measure.map_smul, hmap, smul_smul, ← ENNReal.ofReal_mul (mul_nonneg hr (Real.exp_pos _).le)]
    at hlaw
  refine (Measure.le_iff.mpr fun u _ => ?_).trans hlaw
  exact mul_le_mul_left (ENNReal.ofReal_le_ofReal hbound) (volume u)

end DSP.WanderingRandomLFO

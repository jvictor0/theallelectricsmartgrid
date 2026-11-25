import DSP.WanderingRandomLFO.ClockProofs
import DSP.WanderingRandomLFO.ExponentialPhase
import DSP.WanderingRandomLFO.IndependentPhaseSteps
import DSP.WanderingRandomLFO.PhaseConvolution
import Mathlib.MeasureTheory.Measure.Real

namespace DSP.WanderingRandomLFO

open MeasureTheory Filter Set
open scoped BigOperators ENNReal Topology

private theorem samplePhaseLaw_conv (lambda alpha : ℝ) (ha : alpha ≠ 1) (n : ℕ) (i : ℝ) :
    samplePhaseLaw lambda alpha n i =
      convolvedPhase (fun j => phaseStepLaw lambda (clockCoefficient alpha n i j)) (n+1) ∗
        Measure.dirac (clockConstant lambda alpha n i : Phase) := by
  rw [← phaseDraws_map_sum]
  unfold samplePhaseLaw samplePhaseAngle samplePhaseTime
  congr 1
  funext draw
  rw [exactSamplePhase_eq lambda alpha ha]
  congr 2
  apply Finset.sum_congr rfl
  intro j _
  rw [Fin.ofNat_eq_cast, Fin.natCast_eq_mk j.isLt]

private theorem phase_mixes (lambda : ℝ) :
    ∀ ε : ℝ, 0 < ε → ε < 1 → ∃ N : ℕ, ∀ n ≥ N, ∀ i : Fin frameSamples,
      ENNReal.ofReal (1-ε) • (volume : Measure Phase) ≤ samplePhaseLaw lambda exactAlpha n i := by
  intro ε hε _
  let q := uniformFraction lambda
  have hq : 0 < q := uniformFraction_pos lambda
  have hq' : q < 1 := uniformFraction_lt_one lambda
  obtain ⟨m, hm⟩ := (Metric.tendsto_atTop.mp
    (tendsto_pow_atTop_nhds_zero_of_lt_one (sub_nonneg.mpr hq'.le) (by linarith : 1-q < 1))) ε hε
  have hsmall : (1-q)^m ≤ ε := by
    have h := hm m le_rfl
    rw [Real.dist_eq, sub_zero, abs_of_nonneg (pow_nonneg (sub_nonneg.mpr hq'.le) _)] at h
    exact h.le
  obtain ⟨k, hk⟩ := (Metric.tendsto_atTop.mp
    (tendsto_pow_atTop_nhds_zero_of_lt_one exactAlpha_pos.le exactAlpha_lt_one)) (1/2) (by norm_num)
  refine ⟨m + k, fun n hn i => ?_⟩
  have hf : 0 < (frameSamples : ℝ) := by norm_num [frameSamples]
  have hi : (i : ℝ) ≤ (frameSamples : ℝ) := by exact_mod_cast i.isLt.le
  have hin : 0 ≤ (i : ℝ) := by positivity
  have hc : 0 ≤ (frameSamples : ℝ) - i * (1 - exactAlpha) := by
    have : (i : ℝ) * (1 - exactAlpha) ≤ (i : ℝ) := by
      simpa using mul_le_mul_of_nonneg_left (show 1 - exactAlpha ≤ 1 by linarith [exactAlpha_pos]) hin
    linarith
  have hc' : (frameSamples : ℝ) - i * (1 - exactAlpha) ≤ (frameSamples : ℝ) :=
    sub_le_self _ (mul_nonneg hin (sub_nonneg.mpr exactAlpha_lt_one.le))
  have good (j : ℕ) (hj : j < m) :
      ENNReal.ofReal q • (volume : Measure Phase) ≤ phaseStepLaw lambda (clockCoefficient exactAlpha n i j) := by
    have hpow := hk (n-j) (by omega)
    rw [Real.dist_eq, sub_zero, abs_of_nonneg (pow_nonneg exactAlpha_pos.le _)] at hpow
    have hprod := mul_le_mul hc' hpow.le (pow_nonneg exactAlpha_pos.le _) hf.le
    apply uniformFraction_smul_volume_le
    · unfold clockCoefficient
      norm_num [frameSamples] at hprod ⊢
      linarith
    · exact sub_le_self _ (mul_nonneg hc (pow_nonneg exactAlpha_pos.le _))
  rw [samplePhaseLaw_conv lambda exactAlpha exactAlpha_lt_one.ne]
  calc
    _ ≤ ENNReal.ofReal (1-(1-q)^m) • (volume : Measure Phase) := by
      gcongr
    _ ≤ _ := convolvedPhase_volume_le _ q hq.le hq'.le m (n+1) (by omega) good _

private theorem abs_prob_sub_le_of_smul_le {μ ν : Measure ℝ} [IsProbabilityMeasure μ]
    [IsProbabilityMeasure ν] {ε : ℝ} (hε : 0 ≤ ε) (hε' : ε ≤ 1)
    (h : ENNReal.ofReal (1 - ε) • ν ≤ μ) {s : Set ℝ} (hs : MeasurableSet s) :
    |μ.real s - ν.real s| ≤ ε := by
  have low (t : Set ℝ) : (1-ε) * ν.real t ≤ μ.real t := by
    have ht := ENNReal.toReal_mono (measure_ne_top μ t) (h t)
    simpa only [Measure.smul_apply, smul_eq_mul, ENNReal.toReal_mul,
      ENNReal.toReal_ofReal (sub_nonneg.mpr hε'), measureReal_def] using ht
  have hc := low sᶜ
  rw [probReal_compl_eq_one_sub hs, probReal_compl_eq_one_sub hs] at hc
  have hl := low s
  have ht := (measureReal_mono (μ := ν) (subset_univ s)).trans_eq probReal_univ
  have hn : 0 ≤ ν.real s := by positivity
  apply abs_le.mpr
  constructor <;> nlinarith [mul_nonneg hε hn, mul_nonneg hε (sub_nonneg.mpr ht)]

/-- At any fixed finite rate the marginal probability of every measurable output
set converges to the same law, including every offset of the 32-sample clock. -/
theorem everySample_tendsto (lambda : ℝ)
    (s : Set ℝ) (hs : MeasurableSet s) :
    Tendsto (fun n => (everySampleLaw lambda n).real s) atTop
      (𝓝 (stationaryAmplitudeLaw.real s)) := by
  letI (n : ℕ) : IsProbabilityMeasure (everySampleLaw lambda n) := by
    unfold everySampleLaw
    infer_instance
  letI : IsProbabilityMeasure stationaryAmplitudeLaw := by
    unfold stationaryAmplitudeLaw
    infer_instance
  apply Metric.tendsto_atTop.mpr
  intro ε hε
  have hδ : 0 < min (ε / 2) (1 / 2) := lt_min (by positivity) (by norm_num)
  have hδ' : min (ε / 2) (1 / 2) < 1 := (min_le_right _ _).trans_lt (by norm_num)
  obtain ⟨N, hN⟩ := phase_mixes lambda _ hδ hδ'
  refine ⟨N * frameSamples, fun n hn => ?_⟩
  have hf : 0 < frameSamples := by decide
  have hframe := (Nat.le_div_iff_mul_le hf).2 hn
  let i : Fin frameSamples := ⟨n % frameSamples, Nat.mod_lt n hf⟩
  have h := Measure.map_mono
    (Measure.prod_mono (hN _ hframe i) (le_rfl (a := knotUniforms))) measurable_bipolarSample
  rw [Measure.prod_smul_left, Measure.map_smul] at h
  rw [Real.dist_eq]
  exact (abs_prob_sub_le_of_smul_le (μ := everySampleLaw lambda n)
    (ν := stationaryAmplitudeLaw) hδ.le hδ'.le h hs).trans_lt
    ((min_le_left _ _).trans_lt (by linarith))

end DSP.WanderingRandomLFO

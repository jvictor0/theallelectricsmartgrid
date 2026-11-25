import DSP.WanderingRandomLFO.PhaseLaw
import Mathlib.MeasureTheory.Measure.Sub
import Mathlib.MeasureTheory.Measure.WithDensity

namespace DSP.WanderingRandomLFO

open MeasureTheory
open scoped ENNReal

noncomputable section

private theorem conv_phase_volume (μ : Measure Phase) :
    μ ∗ (volume : Measure Phase) = μ Set.univ • (volume : Measure Phase) := by
  ext s hs
  have hadd : Measurable (fun z : Phase × Phase => z.1 + z.2) := by fun_prop
  rw [Measure.conv, Measure.map_apply hadd hs, Measure.prod_apply (hadd hs)]
  simp only [Set.preimage_preimage, Measure.smul_apply, smul_eq_mul]
  have h (x : Phase) :
      (volume : Measure Phase) ((fun y => x + y) ⁻¹' s) = volume s :=
    measure_preimage_add _ _ _
  simp [h, mul_comm]

private theorem phase_volume_conv_probability (μ : Measure Phase) [IsProbabilityMeasure μ] :
    (volume : Measure Phase) ∗ μ = volume := by
  rw [Measure.conv_comm, conv_phase_volume, measure_univ, one_smul]

private theorem phase_conv_mono {μ ν μ' ν' : Measure Phase} [SFinite ν']
    (hμ : μ ≤ μ') (hν : ν ≤ ν') : μ ∗ ν ≤ μ' ∗ ν' := by
  exact Measure.map_mono (Measure.prod_mono hμ hν) (by fun_prop)

instance (steps : ℕ → Measure Phase) [∀ j, IsProbabilityMeasure (steps j)] (n : ℕ) :
    IsProbabilityMeasure (convolvedPhase steps n) := by
  induction n with
  | zero => unfold convolvedPhase; infer_instance
  | succ n ih =>
    letI := ih
    unfold convolvedPhase
    infer_instance

private theorem phase_volume_le_conv_probability {μ ν : Measure Phase} [IsProbabilityMeasure ν]
    (a : ℝ≥0∞) (hμ : a • (volume : Measure Phase) ≤ μ) :
    a • (volume : Measure Phase) ≤ μ ∗ ν := by
  simpa only [Measure.conv_smul_left, phase_volume_conv_probability] using
    phase_conv_mono hμ (le_refl ν)

private theorem phase_volume_le_conv_good {μ ν : Measure Phase}
    [IsProbabilityMeasure μ] [IsProbabilityMeasure ν]
    {q r : ℝ} (hq : 0 ≤ q) (hr : 0 ≤ r) (hr' : r ≤ 1)
    (hμ : ENNReal.ofReal (1 - r) • (volume : Measure Phase) ≤ μ)
    (hν : ENNReal.ofReal q • (volume : Measure Phase) ≤ ν) :
    ENNReal.ofReal (1 - r * (1 - q)) • (volume : Measure Phase) ≤ μ ∗ ν := by
  let uniform : Measure Phase := ENNReal.ofReal (1 - r) • (volume : Measure Phase)
  have huniform : IsFiniteMeasure uniform := isFiniteMeasure_of_le μ hμ
  let residual := μ - uniform
  have hsplit : uniform + residual = μ := by
    rw [add_comm]
    exact Measure.sub_add_cancel_of_le hμ
  have hresidual : IsFiniteMeasure residual :=
    isFiniteMeasure_of_le μ Measure.sub_le
  have hmass : residual Set.univ = ENNReal.ofReal r := by
    have h1r : 0 ≤ 1 - r := sub_nonneg.mpr hr'
    rw [Measure.sub_apply MeasurableSet.univ hμ, measure_univ]
    change 1 - (ENNReal.ofReal (1 - r) • (volume : Measure Phase)) Set.univ = _
    simp only [Measure.smul_apply, smul_eq_mul, measure_univ, mul_one]
    rw [← ENNReal.ofReal_one, ← ENNReal.ofReal_sub 1 h1r]
    congr 1
    ring
  have hkeep : uniform ∗ ν = uniform := by
    simp [uniform, Measure.conv_smul_left, phase_volume_conv_probability]
  have hrecover :
      (ENNReal.ofReal q * ENNReal.ofReal r) • (volume : Measure Phase) ≤ residual ∗ ν := by
    simpa only [Measure.conv_smul_right, conv_phase_volume, hmass, smul_smul] using
      phase_conv_mono (le_refl residual) hν
  have hcoefficient : ENNReal.ofReal (1 - r * (1 - q)) =
      ENNReal.ofReal (1 - r) + ENNReal.ofReal q * ENNReal.ofReal r := by
    rw [← ENNReal.ofReal_mul hq, ← ENNReal.ofReal_add (sub_nonneg.mpr hr') (mul_nonneg hq hr)]
    congr 1
    ring
  rw [hcoefficient, add_smul, ← hsplit, Measure.add_conv, hkeep]
  exact add_le_add le_rfl hrecover

theorem convolvedPhase_volume_le (steps : ℕ → Measure Phase)
    [∀ j, IsProbabilityMeasure (steps j)] (q : ℝ) (hq : 0 ≤ q) (hq' : q ≤ 1)
    (m len : ℕ) (hm : m ≤ len)
    (hgood : ∀ j < m, ENNReal.ofReal q • (volume : Measure Phase) ≤ steps j) (shift : Phase) :
    ENNReal.ofReal (1 - (1 - q)^m) • (volume : Measure Phase) ≤
      convolvedPhase steps len ∗ Measure.dirac shift := by
  have hprefix (j : ℕ) (hj : j ≤ m) :
      ENNReal.ofReal (1 - (1 - q)^j) • (volume : Measure Phase) ≤ convolvedPhase steps j := by
    induction j with
    | zero =>
      simp only [pow_zero, sub_self, ENNReal.ofReal_zero, zero_smul]
      exact Measure.zero_le _
    | succ j ih =>
      have hbase : 0 ≤ 1 - q := sub_nonneg.mpr hq'
      have hbase' : 1 - q ≤ 1 := by linarith
      rw [convolvedPhase, pow_succ]
      exact phase_volume_le_conv_good hq (pow_nonneg hbase j)
        (pow_le_one₀ hbase hbase') (ih (Nat.le_of_succ_le hj)) (hgood j hj)
  have htail : ENNReal.ofReal (1 - (1 - q)^m) • (volume : Measure Phase) ≤
      convolvedPhase steps len := by
    induction len, hm using Nat.le_induction with
    | base => exact hprefix m le_rfl
    | succ n hn ih =>
      rw [convolvedPhase]
      exact phase_volume_le_conv_probability _ ih
  exact phase_volume_le_conv_probability _ htail

end

end DSP.WanderingRandomLFO

import Mathlib.MeasureTheory.Integral.Lebesgue.Markov
import Mathlib.Probability.Independence.Integration

namespace DSP.Survey

open MeasureTheory ProbabilityTheory
open scoped ENNReal

private noncomputable def wagerFactor (q : ℝ≥0∞) (pass : Bool) : ℝ≥0∞ :=
  if pass = true then (1 + q) / (2 * q) else 1 / 2

/-- Product of the wagers for testing an asserted lower bound on the pass probability. -/
noncomputable def wagerProduct {Ω : Type*} {n : ℕ} (q : ℝ≥0∞) (trial : Fin n → Ω → Bool)
    (x : Ω) : ℝ≥0∞ :=
  ∏ i, wagerFactor q (trial i x)

/-- The product depends only on the number of passing trials. -/
theorem wagerProduct_eq_pow_hits {Ω : Type*} {n : ℕ} (q : ℝ≥0∞)
    (trial : Fin n → Ω → Bool) (x : Ω) :
    let hits := (Finset.univ.filter fun i => trial i x = true).card
    wagerProduct q trial x = ((1 + q) / (2 * q)) ^ hits * (1 / 2) ^ (n - hits) := by
  simp only [wagerProduct, wagerFactor, Finset.prod_ite, Finset.prod_const]
  congr 2
  have hc := Finset.card_filter_add_card_filter_not
    (s := (Finset.univ : Finset (Fin n))) (fun i => trial i x = true)
  simp only [Finset.card_univ, Fintype.card_fin] at hc
  omega

private theorem lintegral_wagerFactor_le_one {Ω : Type*} [MeasurableSpace Ω]
    (μ : Measure Ω) [IsProbabilityMeasure μ] (Y : Ω → Bool) (hY : Measurable Y)
    {q : ℝ≥0∞} (hq : 0 < q) (hq1 : q ≤ 1) (hPass : μ {x | Y x = true} ≤ q) :
    (∫⁻ x, wagerFactor q (Y x) ∂μ) ≤ 1 := by
  let passes : Set Ω := {x | Y x = true}
  have hm : MeasurableSet passes := hY (measurableSet_singleton true)
  have hqtop : q ≠ ∞ := ne_top_of_le_ne_top ENNReal.one_ne_top hq1
  have hcancel : (1 : ℝ≥0∞) / (2 * q) * q = 1 / 2 := by
    rw [ENNReal.div_mul _ (Or.inr hq.ne') (Or.inr hqtop),
      ENNReal.mul_div_cancel_right hq.ne' hqtop]
  have hhalf : q / (2 * q) = 1 / 2 := by
    simpa using ENNReal.mul_div_mul_right 1 2 hq.ne' hqtop
  have heq : (fun x => wagerFactor q (Y x)) =
      fun x => 1 / 2 + passes.indicator (fun _ => 1 / (2 * q)) x := by
    ext x
    by_cases hx : x ∈ passes
    · simp [wagerFactor, show Y x = true from hx, Set.indicator_of_mem hx,
        ENNReal.add_div, hhalf, add_comm]
    · simp [wagerFactor, show Y x ≠ true from hx, Set.indicator_of_notMem hx]
  rw [heq, lintegral_add_left measurable_const, lintegral_const,
    lintegral_indicator_const hm, measure_univ, mul_one]
  calc
    1 / 2 + 1 / (2 * q) * μ passes ≤ 1 / 2 + 1 / (2 * q) * q :=
      add_le_add_right (mul_le_mul_right hPass _) _
    _ = 1 := by
      rw [hcancel, ← ENNReal.add_div, one_add_one_eq_two, ENNReal.div_self two_ne_zero ENNReal.ofNat_ne_top]

/-- A fixed cell's wager crosses the reciprocal risk with probability at most that risk
when every trial has pass probability at most the tested level. Independent trials with a
common pass probability below that level satisfy this hypothesis. -/
theorem measure_wagerProduct_ge_inv_le {Ω : Type*} [MeasurableSpace Ω] {n : ℕ}
    (μ : Measure Ω) (trial : Fin n → Ω → Bool)
    (hTrial : ∀ i, Measurable (trial i)) (hIndep : iIndepFun trial μ)
    {q δ : ℝ≥0∞} (hq : 0 < q) (hq1 : q ≤ 1) (hδ : δ ≠ 0) (hδtop : δ ≠ ∞)
    (hPass : ∀ i, μ {x | trial i x = true} ≤ q) :
    μ {x | δ⁻¹ ≤ wagerProduct q trial x} ≤ δ := by
  letI : IsProbabilityMeasure μ := hIndep.isProbabilityMeasure
  have hFactor : Measurable (wagerFactor q) := measurable_of_countable _
  have hMeas : ∀ i, Measurable (fun x => wagerFactor q (trial i x)) :=
    fun i => hFactor.comp (hTrial i)
  have hIndepFactor : iIndepFun (fun i x => wagerFactor q (trial i x)) μ :=
    hIndep.comp (fun _ => wagerFactor q) (fun _ => hFactor)
  have hMean : (∫⁻ x, wagerProduct q trial x ∂μ) ≤ 1 := by
    change (∫⁻ x, ∏ i : Fin n, wagerFactor q (trial i x) ∂μ) ≤ 1
    rw [lintegral_prod_eq_prod_lintegral_of_indepFun _ _ hIndepFactor hMeas]
    exact Finset.prod_le_one' (fun i _ =>
      lintegral_wagerFactor_le_one μ (trial i) (hTrial i) hq hq1 (hPass i))
  have hMeasProd : Measurable (wagerProduct q trial) :=
    Finset.univ.measurable_prod (fun i _ => hMeas i)
  calc
    μ {x | δ⁻¹ ≤ wagerProduct q trial x} ≤ (∫⁻ x, wagerProduct q trial x ∂μ) / δ⁻¹ :=
      meas_ge_le_lintegral_div hMeasProd.aemeasurable
        (ENNReal.inv_ne_zero.mpr hδtop) (ENNReal.inv_ne_top.mpr hδ)
    _ ≤ 1 / δ⁻¹ := ENNReal.div_le_div_right hMean _
    _ = δ := by simp [div_eq_mul_inv]

end DSP.Survey

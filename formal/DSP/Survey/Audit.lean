import DSP.Survey.Bernoulli
import DSP.Survey.Certificate
import DSP.Survey.Regional

open MeasureTheory
open scoped BigOperators ENNReal

namespace DSP.Survey

/-- The fixed test accepts a claim when its wager reaches the reciprocal of
the assigned risk. -/
def wagerAccepted {Ω C T Q : Type*} {n : C → ℕ}
    (trial : (c : C) → T → Fin (n c) → Ω → Bool)
    (level : Q → ENNReal) (risk : C × T × Q → ENNReal)
    (x : Ω) (c : C) (t : T) (q : Q) : Prop :=
  (risk (c, t, q))⁻¹ ≤ wagerProduct (level q) (trial c t) x

/-- A finite family of fixed Bernoulli tests certifies every accepted regional
selection, including its conditional coverage when the region has positive
mass. The same audit can include successful and complementary trials as
separate thresholds in a single family. -/
theorem measure_wagerRegionalFailure_le_sum {Ω C T Q : Type*}
    [MeasurableSpace Ω] [Fintype C] [Fintype T] [Fintype Q]
    (μ : Measure Ω) {n : C → ℕ}
    (trial : (c : C) → T → Fin (n c) → Ω → Bool)
    (trial_measurable : ∀ c t i, Measurable (trial c t i))
    (trial_indep : ∀ c t, ProbabilityTheory.iIndepFun (trial c t) μ)
    (truth : C → T → ℝ)
    (truth_nonneg : ∀ c t, 0 ≤ truth c t)
    (trial_mean : ∀ c t i, μ {x | trial c t i x = true} ≤ ENNReal.ofReal (truth c t))
    (level : Q → ENNReal) (level_pos : ∀ q, 0 < level q) (level_le_one : ∀ q, level q ≤ 1)
    (risk : C × T × Q → ENNReal) (risk_ne_zero : ∀ i, risk i ≠ 0)
    (risk_ne_top : ∀ i, risk i ≠ ∞)
    (w : C → ℝ) (w_nonneg : ∀ c, 0 ≤ w c) :
    μ (regionalFailure truth (fun q => (level q).toReal)
      (wagerAccepted trial level risk) w) ≤ ∑ i, risk i := by
  refine measure_regionalFailure_le_sum μ truth (fun q => (level q).toReal)
    (wagerAccepted trial level risk) truth_nonneg risk ?_ w w_nonneg
  intro c t q
  by_cases overstates : truth c t < (level q).toReal
  · have mean_le (i : Fin (n c)) : μ {x | trial c t i x = true} ≤ level q :=
      (trial_mean c t i).trans ((ENNReal.ofReal_le_ofReal overstates.le).trans
        ENNReal.ofReal_toReal_le)
    exact (measure_mono (fun _ hx => hx.1)).trans
      (measure_wagerProduct_ge_inv_le μ (trial c t) (trial_measurable c t) (trial_indep c t)
        (level_pos q) (level_le_one q) (risk_ne_zero (c, t, q)) (risk_ne_top (c, t, q)) mean_le)
  · have empty_event :
        {x | wagerAccepted trial level risk x c t q ∧ truth c t < (level q).toReal} = ∅ := by
      ext x
      simp [overstates]
    rw [empty_event, measure_empty]
    exact zero_le

/-- The exact hit-count checker gives the same regional guarantee for rational
candidate levels and risks as the mathematical wager test. -/
theorem measure_certificateRegionalFailure_le_sum {Ω C T Q : Type*}
    [MeasurableSpace Ω] [Fintype C] [Fintype T] [Fintype Q]
    (μ : Measure Ω) {n : C → ℕ}
    (trial : (c : C) → T → Fin (n c) → Ω → Bool)
    (trial_measurable : ∀ c t i, Measurable (trial c t i))
    (trial_indep : ∀ c t, ProbabilityTheory.iIndepFun (trial c t) μ)
    (truth : C → T → ℝ)
    (truth_nonneg : ∀ c t, 0 ≤ truth c t)
    (trial_mean : ∀ c t i, μ {x | trial c t i x = true} ≤ ENNReal.ofReal (truth c t))
    (a b : Q → ℕ) (a_pos : ∀ q, 0 < a q) (a_lt_b : ∀ q, a q < b q)
    (d e : C × T × Q → ℕ) (d_pos : ∀ i, 0 < d i) (d_lt_e : ∀ i, d i < e i)
    (w : C → ℝ) (w_nonneg : ∀ c, 0 ≤ w c) :
    let level (q : Q) : ENNReal := (a q : ENNReal) / b q
    let risk (i : C × T × Q) : ENNReal := (d i : ENNReal) / e i
    μ (regionalFailure truth (fun q => (level q).toReal)
      (fun x c t q => Certificate.accepts (a q) (b q) (d (c, t, q)) (e (c, t, q)) (n c)
        (Finset.univ.filter (fun i => trial c t i x = true)).card = true) w) ≤ ∑ i, risk i := by
  intro level risk
  have decisions :
      (fun x c t q => Certificate.accepts (a q) (b q) (d (c, t, q)) (e (c, t, q)) (n c)
        (Finset.univ.filter (fun i => trial c t i x = true)).card = true) =
      wagerAccepted trial level risk := by
    funext x c t q
    apply propext
    have hits_le :
        (Finset.univ.filter (fun i => trial c t i x = true)).card ≤ n c := by
      simpa using Finset.card_filter_le (s := Finset.univ) (p := fun i => trial c t i x = true)
    rw [Certificate.accepts_iff, wagerAccepted, wagerProduct_eq_pow_hits]
    simp [level, risk, a_pos, a_lt_b, d_pos, d_lt_e, hits_le]
  rw [decisions]
  apply measure_wagerRegionalFailure_le_sum μ trial trial_measurable trial_indep truth
    truth_nonneg trial_mean level _ _ risk _ _ w w_nonneg
  · intro q
    exact ENNReal.div_pos (by exact_mod_cast (a_pos q).ne') (by simp)
  · intro q
    exact (ENNReal.div_le_div_right (by exact_mod_cast (a_lt_b q).le) (b q : ENNReal)).trans
      ENNReal.div_self_le_one
  · intro i
    exact (ENNReal.div_pos (by exact_mod_cast (d_pos i).ne') (by simp)).ne'
  · intro i
    exact ENNReal.div_ne_top (by simp) (by exact_mod_cast ((d_pos i).trans (d_lt_e i)).ne')

end DSP.Survey

import Mathlib.Analysis.Asymptotics.SpecificAsymptotics
import Mathlib.Tactic

open scoped BigOperators Topology
open Filter Finset

namespace NoiseDisplay.BlockCorrection

/-- The residual against the clipped mean at a common terminal age.
The finite prefix uses the expected powers at ages below seven directly. -/
noncomputable def finiteResidual (m Δ : ℕ → ℝ) (clip : ℝ) (N : ℕ) : ℝ :=
  if N < 7 then m N - clip else
    (m 4 + m 5 + m 6 + m 7) / 4 - clip +
      Δ 4 / 4 + Δ 5 / 2 + 3 * Δ 6 / 4 +
      ∑ j ∈ range (N - 7), Δ (j + 7)

theorem finiteResidual_eq (m Δ : ℕ → ℝ) (clip : ℝ)
    (hΔ : ∀ j, Δ j = m (j + 1) - m j) (N : ℕ) :
    finiteResidual m Δ clip N = m N - clip := by
  unfold finiteResidual
  split_ifs with h
  · rfl
  · have ht : (∑ j ∈ range (N - 7), Δ (j + 7)) = m N - m 7 := by
      have ha : N - 7 + 7 = N := by omega
      simpa only [hΔ, Nat.add_right_comm, Nat.zero_add, ha] using
        (Finset.sum_range_sub (fun j => m (j + 7)) (N - 7))
    rw [ht, hΔ 4, hΔ 5, hΔ 6]
    ring

theorem finite_residual_limit (m Δ : ℕ → ℝ) (clip : ℝ)
    (hΔ : ∀ j, Δ j = m (j + 1) - m j) {P : ℝ}
    (hP : Tendsto (fun n : ℕ => (n⁻¹ : ℝ) * ∑ j ∈ range n, m j) atTop (𝓝 P)) :
    Tendsto (fun n : ℕ => (n⁻¹ : ℝ) * ∑ j ∈ range n, finiteResidual m Δ clip j)
      atTop (𝓝 (P - clip)) := by
  apply (hP.sub_const clip).congr'
  filter_upwards [eventually_ge_atTop 1] with N hN
  have hN' : (N : ℝ) ≠ 0 := by exact_mod_cast (show N ≠ 0 by omega)
  simp only [finiteResidual_eq m Δ clip hΔ, sum_sub_distrib, sum_const, card_range, nsmul_eq_mul]
  field_simp

/-- A bound uniform in terminal age carries to the stationary power bias. -/
theorem finite_horizon_bound (m Δ : ℕ → ℝ) (hΔ : ∀ j, Δ j = m (j + 1) - m j)
    {P t : ℝ}
    (hP : Tendsto (fun n : ℕ => (n⁻¹ : ℝ) * ∑ j ∈ range n, m j) atTop (𝓝 P))
    (ht : 0 ≤ t)
    (bound : ∀ N : ℕ, 7 ≤ N →
      |Δ 4 / 4 + Δ 5 / 2 + 3 * Δ 6 / 4 + ∑ j ∈ range (N - 7), Δ (j + 7)| ≤ t) :
    |P - (m 4 + m 5 + m 6 + m 7) / 4| ≤ t := by
  let base := (m 4 + m 5 + m 6 + m 7) / 4
  let d := finiteResidual m Δ base
  let short (n : ℕ) := if n < 7 then d n else 0
  let rest (n : ℕ) := d n - short n
  have hz : Tendsto short atTop (𝓝 0) := by
    apply tendsto_const_nhds.congr'
    filter_upwards [eventually_ge_atTop 7] with n hn
    simp [short, not_lt.mpr hn]
  have hr : Tendsto (fun n : ℕ => (n⁻¹ : ℝ) * ∑ j ∈ range n, rest j)
      atTop (𝓝 (P - base)) := by
    simpa only [rest, sum_sub_distrib, mul_sub, sub_zero] using
      (finite_residual_limit m Δ base hΔ hP).sub hz.cesaro
  have pointwise (n : ℕ) : |rest n| ≤ t := by
    by_cases hn : n < 7
    · simpa [rest, short, hn] using ht
    · simpa [rest, short, hn, d, finiteResidual, base] using bound n (le_of_not_gt hn)
  have avg_bound : ∀ᶠ n : ℕ in atTop, |(n⁻¹ : ℝ) * ∑ j ∈ range n, rest j| ≤ t := by
    filter_upwards [eventually_ge_atTop 1] with n hn
    have hn' : (n : ℝ) ≠ 0 := by exact_mod_cast (show n ≠ 0 by omega)
    rw [abs_mul, abs_of_nonneg (by positivity : 0 ≤ (n⁻¹ : ℝ))]
    calc
      _ ≤ (n⁻¹ : ℝ) * ∑ j ∈ range n, |rest j| :=
        mul_le_mul_of_nonneg_left (abs_sum_le_sum_abs _ _) (by positivity)
      _ ≤ (n⁻¹ : ℝ) * ∑ _j ∈ range n, t :=
        mul_le_mul_of_nonneg_left (sum_le_sum fun j _ => pointwise j) (by positivity)
      _ = t := by simp [hn']
  exact le_of_tendsto hr.abs avg_bound

end NoiseDisplay.BlockCorrection

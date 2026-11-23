import Mathlib.Algebra.BigOperators.Group.Finset.Basic
import Mathlib.Algebra.Order.BigOperators.Group.Finset
import Mathlib.Data.Real.Basic
import Mathlib.Tactic

namespace DSP.NaiveReuppingLadder.NoiseResponse

/-- A sequence that can rise abruptly but falls by at most `step` per sample
    has total variation bounded by twice its possible falls and its net rise. -/
theorem log_variation_bound (f : ℕ → ℝ) (step : ℝ) (n : ℕ)
    (hs : 0 ≤ step) (h : ∀ i < n, f i - f (i + 1) ≤ step) :
    (∑ i ∈ Finset.range n, |f (i + 1) - f i|) ≤
      2 * (n : ℝ) * step + (f n - f 0) := by
  induction n with
  | zero => simp
  | succ n ih =>
    rw [Finset.sum_range_succ]
    have pre := ih (fun i hi => h i (Nat.lt_succ_of_lt hi))
    have last := h n (Nat.lt_succ_self n)
    have point : |f (n + 1) - f n| ≤ 2 * step + (f (n + 1) - f n) := by
      rcases le_total (f n) (f (n + 1)) with forward | backward
      · rw [abs_of_nonneg (sub_nonneg.mpr forward)]
        linarith
      · rw [abs_of_nonpos (sub_nonpos.mpr backward)]
        linarith
    rw [Nat.cast_succ]
    linarith

/-- A gain in [0,K] has squared variation at most K times its variation.
    The log-increment hypothesis is supplied by log(k/K) = log A - log H
    and the unit Lipschitz constant of exp on the nonpositive real axis. -/
theorem gain_difference_energy (gain a h : ℕ → ℝ) (K da dh : ℝ) (n : ℕ)
    (hda : 0 ≤ da) (hdh : 0 ≤ dh)
    (ha : ∀ i < n, a i - a (i+1) ≤ da)
    (hh : ∀ i < n, h i - h (i+1) ≤ dh)
    (gain_range : ∀ i ≤ n, 0 ≤ gain i ∧ gain i ≤ K)
    (gain_step : ∀ i < n, |gain (i+1) - gain i| ≤
      K * (|a (i+1) - a i| + |h (i+1) - h i|)) :
    (∑ i ∈ Finset.range n, (gain (i+1) - gain i)^2) ≤
      K^2 * (2 * (n : ℝ) * (da + dh) + (a n-a 0) + (h n-h 0)) := by
  have K0 : 0 ≤ K := (gain_range 0 (Nat.zero_le n)).1.trans
    (gain_range 0 (Nat.zero_le n)).2
  have square : ∀ i ∈ Finset.range n, (gain (i+1) - gain i)^2 ≤
      K^2 * (|a (i+1) - a i| + |h (i+1) - h i|) := by
    intro i hi
    have hin := Finset.mem_range.mp hi
    obtain ⟨gi0, giK⟩ := gain_range i (Nat.le_of_lt hin)
    obtain ⟨gj0, gjK⟩ := gain_range (i+1) hin
    have absK : |gain (i+1) - gain i| ≤ K := abs_le.mpr ⟨by linarith, by linarith⟩
    calc
      _ = |gain (i+1) - gain i|^2 := (sq_abs _).symm
      _ ≤ K * |gain (i+1) - gain i| := by
        rw [pow_two]
        exact mul_le_mul_of_nonneg_right absK (abs_nonneg _)
      _ ≤ _ := by
        calc
          _ ≤ K * (K * (|a (i+1)-a i| + |h (i+1)-h i|)) :=
            mul_le_mul_of_nonneg_left (gain_step i hin) K0
          _ = _ := by ring
  refine (Finset.sum_le_sum square).trans ?_
  simp_rw [mul_add, Finset.sum_add_distrib, ← Finset.mul_sum]
  have va := mul_le_mul_of_nonneg_left (log_variation_bound a da n hda ha) (sq_nonneg K)
  have vh := mul_le_mul_of_nonneg_left (log_variation_bound h dh n hdh hh) (sq_nonneg K)
  linarith

end DSP.NaiveReuppingLadder.NoiseResponse

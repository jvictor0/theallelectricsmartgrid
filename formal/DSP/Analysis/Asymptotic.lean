import DSP.Filters.OnePoleZOHLowPass.Ripple
import Mathlib.Analysis.Complex.Basic
import Mathlib.Analysis.SpecificLimits.Normed

noncomputable section
namespace DSP.Asymptotic
open Filter
open scoped Topology

def majorant (r : ℝ) (forcing : ℕ → ℝ) (initial : ℝ) : ℕ → ℝ
  | 0 => initial
  | n + 1 => r * majorant r forcing initial n + forcing n

/-- Stable scalar recurrences reject disturbances that tend to zero. -/
theorem majorant_tendsto (r : ℝ) (hr : 0 ≤ r) (hr1 : r < 1)
    (forcing : ℕ → ℝ) (initial : ℝ) (hf : Tendsto forcing atTop (𝓝 0)) :
    Tendsto (majorant r forcing initial) atTop (𝓝 0) := by
  have ha : 0 < 1 - r := sub_pos.mpr hr1
  have h := OnePoleZOHLowPass.Ripple.tendsto_tracking (1 - r) ha (by linarith)
    (fun n => forcing n / (1 - r)) (fun _ => 0)
    (majorant r forcing initial) (fun _ => 0)
    (fun n => by
      simp only [majorant, OnePoleZOHLowPass.relax]
      field_simp
      ring)
    (fun _ => by simp [OnePoleZOHLowPass.relax])
    (by simpa using hf.div_const (1 - r))
  simpa using h

/-- A contraction plus a vanishing disturbance converges to zero. -/
theorem tendsto_zero_of_norm_step_le (r : ℝ) (hr : 0 ≤ r) (hr1 : r < 1)
    (error : ℕ → ℂ) (forcing : ℕ → ℝ)
    (hstep : ∀ n, ‖error (n + 1)‖ ≤ r * ‖error n‖ + forcing n)
    (hf : Tendsto forcing atTop (𝓝 0)) : Tendsto error atTop (𝓝 0) := by
  have hb : ∀ n, ‖error n‖ ≤ majorant r forcing ‖error 0‖ n := by
    intro n
    induction n with
    | zero => exact le_rfl
    | succ n ih =>
      exact (hstep n).trans (add_le_add (mul_le_mul_of_nonneg_left ih hr) le_rfl)
  apply tendsto_zero_iff_norm_tendsto_zero.mpr
  exact squeeze_zero (fun n => norm_nonneg _) hb (majorant_tendsto r hr hr1 _ _ hf)

theorem stable_recurrence (pole : ℂ) (hp : ‖pole‖ < 1)
    (error forcing : ℕ → ℂ)
    (hstep : ∀ n, error (n + 1) = pole * error n + forcing n)
    (hf : Tendsto forcing atTop (𝓝 0)) : Tendsto error atTop (𝓝 0) := by
  apply tendsto_zero_of_norm_step_le ‖pole‖ (norm_nonneg _) hp error (fun n => ‖forcing n‖)
  · intro n
    rw [hstep]
    simpa only [norm_mul] using norm_add_le (pole * error n) (forcing n)
  · simpa using hf.norm

/-- Squaring preserves a vanishing tracking error when the reference is bounded. -/
theorem square_tracking (signal reference : ℕ → ℝ) (bound : ℝ)
    (hb : ∀ n, |reference n| ≤ bound)
    (h : Tendsto (fun n => signal n - reference n) atTop (𝓝 0)) :
    Tendsto (fun n => signal n ^ 2 - reference n ^ 2) atTop (𝓝 0) := by
  have he := h.abs
  have ht := he.mul (he.add_const (2 * bound))
  simp only [abs_zero, zero_add, zero_mul] at ht
  apply tendsto_zero_iff_norm_tendsto_zero.mpr
  apply squeeze_zero (fun n => norm_nonneg _) _ ht
  intro n
  rw [Real.norm_eq_abs, sq_sub_sq, abs_mul]
  rw [mul_comm |signal n + reference n| |signal n - reference n|]
  apply mul_le_mul_of_nonneg_left _ (abs_nonneg _)
  calc
    |signal n + reference n| = |(signal n - reference n) + 2 * reference n| := by congr 1; ring
    _ ≤ |signal n - reference n| + |2 * reference n| := abs_add_le _ _
    _ ≤ |signal n - reference n| + 2 * bound := by
      rw [abs_mul, abs_of_pos (by norm_num : (0 : ℝ) < 2)]
      linarith [hb n]

end DSP.Asymptotic

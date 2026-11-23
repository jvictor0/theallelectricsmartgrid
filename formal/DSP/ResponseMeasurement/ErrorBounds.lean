import DSP.ResponseMeasurement.Periodic
import DSP.Analysis.PhasePower

/-! Finite-period error bounds for the actual coherent and mean-square
measurements. A coherent amplitude is added as a complex number before power
is taken. These estimates allow nonuniform errors across the sample phases. -/

noncomputable section
namespace DSP.ResponseMeasurement
open Finset

/-- Error in a coherent peak phasor, including its factor of two. -/
theorem coherent_error_bound (z : ℂ) (hz : ‖z‖ = 1) (signal reference error : ℕ → ℝ)
    (count : ℕ) (he : ∀ n < count, |signal n-reference n| ≤ error n) :
    ‖coherent z signal count-coherent z reference count‖ ≤
      2*(count:ℝ)⁻¹*∑ n ∈ range count, error n := by
  rw [← coherent_sub, coherent, average, norm_mul, norm_smul]
  rw [show ‖(2:ℂ)‖ = (2:ℝ) by norm_num]
  simp only [Real.norm_of_nonneg (inv_nonneg.mpr (Nat.cast_nonneg count))]
  rw [mul_assoc]
  apply mul_le_mul_of_nonneg_left _ (by norm_num)
  apply mul_le_mul_of_nonneg_left _ (inv_nonneg.mpr (Nat.cast_nonneg count))
  calc
    _ ≤ ∑ n ∈ range count, ‖((signal n-reference n:ℝ):ℂ)*star z^n‖ := norm_sum_le _ _
    _ ≤ _ := by
      apply sum_le_sum
      intro n hn
      simpa only [norm_mul, norm_pow, norm_star, hz, one_pow, mul_one, Complex.norm_real, Real.norm_eq_abs] using he n (mem_range.mp hn)

theorem square_error_bound (x reference error : ℝ) (h : |x-reference| ≤ error) :
    |x^2-reference^2| ≤ error*(2*|reference|+error) := by
  have he := (abs_nonneg _).trans h
  have hx : |x| ≤ |reference|+error := by
    have ht := abs_add_le (x-reference) reference
    simp only [sub_add_cancel] at ht
    linarith
  rw [show x^2-reference^2 = (x-reference)*(x+reference) by ring, abs_mul]
  calc
    _ ≤ error*(|x|+|reference|) := mul_le_mul h (abs_add_le _ _) (abs_nonneg _) he
    _ ≤ _ := by nlinarith

/-- Total mean-square error retains every harmonic, with no square root. -/
theorem meanSquare_error_bound (signal reference error : ℕ → ℝ) (count : ℕ)
    (he : ∀ n < count, |signal n-reference n| ≤ error n) :
    |meanSquare signal count-meanSquare reference count| ≤
      (count:ℝ)⁻¹*∑ n ∈ range count, error n*(2*|reference n|+error n) := by
  unfold meanSquare
  rw [← mul_sub, ← sum_sub_distrib, abs_mul,
    abs_of_nonneg (inv_nonneg.mpr (Nat.cast_nonneg count))]
  apply mul_le_mul_of_nonneg_left _ (inv_nonneg.mpr (Nat.cast_nonneg count))
  calc
    _ ≤ ∑ n ∈ range count, |signal n^2-reference n^2| := abs_sum_le_sum_abs _ _
    _ ≤ _ := sum_le_sum (fun n hn => square_error_bound _ _ _ (he n (mem_range.mp hn)))

/-- Coherent component power is half the squared peak phasor magnitude. -/
theorem coherentPower_error_bound (actual reference : ℂ) (error : ℝ)
    (h : ‖actual-reference‖ ≤ error) :
    |‖actual‖^2/2-‖reference‖^2/2| ≤ error*(2*‖reference‖+error)/2 := by
  rw [← sub_div, abs_div, abs_of_pos (by norm_num : (0:ℝ)<2)]
  exact div_le_div_of_nonneg_right (DSP.PhasePower.complex_power_error _ _ _ h) (by norm_num)

/-- Scaling a certified power error to input-relative squared gain. A positive
input amplitude is explicit; no epsilon floor or clipping is inserted. -/
theorem normalizedPower_error_bound (actual reference error amplitude : ℝ)
    (hA : 0 < amplitude) (h : |actual-reference| ≤ error) :
    |2*actual/amplitude^2-2*reference/amplitude^2| ≤ 2*error/amplitude^2 := by
  rw [← sub_div, ← mul_sub, abs_div, abs_mul, abs_of_pos (by norm_num : (0:ℝ)<2),
    abs_of_pos (sq_pos_of_pos hA)]
  gcongr

end DSP.ResponseMeasurement

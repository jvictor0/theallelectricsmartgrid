import DSP.ResponseMeasurement.Approximation

/-! Explicit Taylor bounds for finite complex polynomials on the unit disk.
These are algebraic finite-difference bounds, with no quadrature or derivative
enclosure supplied by a numerical solver. -/

noncomputable section
namespace DSP.ComplexPolynomialBounds

theorem norm_nat_add_one (n : ℕ) : ‖(n:ℂ)+1‖ = (n:ℝ)+1 := by
  rw [show (n:ℂ)+1 = ((n+1:ℕ):ℂ) by simp,Complex.norm_natCast]
  simp

theorem power_remainder (n : ℕ) (z w : ℂ) (hz : ‖z‖ ≤ 1) (hw : ‖w‖ ≤ 1) :
    ‖w^(n+1)-z^(n+1)-((n:ℂ)+1)*z^n*(w-z)‖ ≤
      ((n:ℝ)*((n:ℝ)+1)/2)*‖w-z‖^2 := by
  induction n with
  | zero => simp
  | succ n ih =>
    have he : w^(n+1+1)-z^(n+1+1)-(((n+1:ℕ):ℂ)+1)*z^(n+1)*(w-z) =
        w*(w^(n+1)-z^(n+1)-((n:ℂ)+1)*z^n*(w-z))+
          ((n:ℂ)+1)*z^n*(w-z)^2 := by
      push_cast
      simp only [pow_succ]
      ring
    have hn : ‖z^n‖ ≤ 1 := by simpa only [norm_pow] using pow_le_one₀ (norm_nonneg z) hz
    rw [he]
    have hnorm : ‖((n:ℂ)+1)*z^n*(w-z)^2‖ = ((n:ℝ)+1)*‖z^n‖*‖w-z‖^2 := by
      rw [norm_mul,norm_mul,norm_nat_add_one,norm_pow,norm_pow]
    calc
      _ ≤ ‖w‖*‖w^(n+1)-z^(n+1)-((n:ℂ)+1)*z^n*(w-z)‖+
          (((n:ℝ)+1)*‖z^n‖)*‖w-z‖^2 := by
        have h := norm_add_le (w*(w^(n+1)-z^(n+1)-((n:ℂ)+1)*z^n*(w-z)))
          (((n:ℂ)+1)*z^n*(w-z)^2)
        rw [hnorm,norm_mul] at h
        exact h
      _ ≤ ((n:ℝ)*((n:ℝ)+1)/2)*‖w-z‖^2+((n:ℝ)+1)*‖w-z‖^2 := by
        apply add_le_add
        · exact (mul_le_mul_of_nonneg_right hw (norm_nonneg _)).trans (by simpa using ih)
        · exact mul_le_mul_of_nonneg_right
            (by nlinarith : ((n:ℝ)+1)*‖z^n‖ ≤ (n:ℝ)+1) (sq_nonneg _)
      _ = _ := by push_cast; ring

def primitiveTerm (a : ℂ) (n : ℕ) (z : ℂ) : ℂ := a*z^(n+1)/((n:ℂ)+1)

theorem primitiveTerm_bound (a : ℂ) (n : ℕ) (z : ℂ) (hz : ‖z‖ ≤ 1) :
    ‖primitiveTerm a n z‖ ≤ ‖a‖/((n:ℝ)+1) := by
  have hn : ‖z‖^(n+1) ≤ 1 := pow_le_one₀ (norm_nonneg z) hz
  simp only [primitiveTerm,norm_div,norm_mul,norm_pow,norm_nat_add_one]
  exact div_le_div_of_nonneg_right (by nlinarith [norm_nonneg a]) (by positivity)

theorem primitiveTerm_remainder (a : ℂ) (n : ℕ) (z w : ℂ)
    (hz : ‖z‖ ≤ 1) (hw : ‖w‖ ≤ 1) :
    ‖primitiveTerm a n w-primitiveTerm a n z-a*z^n*(w-z)‖ ≤
      (‖a‖*(n:ℝ)/2)*‖w-z‖^2 := by
  have hn : (n:ℂ)+1 ≠ 0 := by exact_mod_cast (by positivity : (n:ℝ)+1 ≠ 0)
  have he : primitiveTerm a n w-primitiveTerm a n z-a*z^n*(w-z) =
      a/((n:ℂ)+1)*(w^(n+1)-z^(n+1)-((n:ℂ)+1)*z^n*(w-z)) := by
    unfold primitiveTerm
    field_simp
  rw [he,norm_mul,norm_div]
  have hnorm : ‖(n:ℂ)+1‖ = (n:ℝ)+1 := by
    exact norm_nat_add_one n
  rw [hnorm]
  calc
    _ ≤ (‖a‖/((n:ℝ)+1))*(((n:ℝ)*((n:ℝ)+1)/2)*‖w-z‖^2) :=
      mul_le_mul_of_nonneg_left (power_remainder n z w hz hw) (by positivity)
    _ = _ := by field_simp

end DSP.ComplexPolynomialBounds

import DSP.Analysis.PhasePower

/-! Error propagation for stationary running-phase averages. These contracts
need a positive speed margin and established moment/integral errors. They do
not certify an approximate ladder orbit or identify phase and time averages. -/

noncomputable section
namespace DSP.PhasePower

/-- Inverse-speed weights are well-conditioned away from a locking boundary.
Both positive speeds must have a validated lower bound. -/
theorem inverse_speed_error (v w margin ε : ℝ) (hm : 0 < margin)
    (hv : margin ≤ v) (hw : margin ≤ w) (he : |v-w| ≤ ε) :
    |v⁻¹-w⁻¹| ≤ ε/margin^2 := by
  have hvp : 0 < v := lt_of_lt_of_le hm hv
  have hwp : 0 < w := lt_of_lt_of_le hm hw
  have he0 : 0 ≤ ε := (abs_nonneg _).trans he
  have hid : v⁻¹-w⁻¹ = (w-v)/(v*w) := by
    field_simp
  rw [hid, abs_div, abs_of_pos (mul_pos hvp hwp), abs_sub_comm w v]
  calc
    _ ≤ ε/(v*w) := div_le_div_of_nonneg_right he (le_of_lt (mul_pos hvp hwp))
    _ ≤ ε/margin^2 := by
      apply div_le_div_of_nonneg_left he0 (sq_pos_of_pos hm)
      nlinarith [mul_nonneg (sub_nonneg.mpr hv) (sub_nonneg.mpr hw)]

/-- Normalize two unnormalized complex moments. A unit-norm observable has
numerator norm at most its positive total weight. This separates normalization
error from any proof of the underlying inverse-speed integral estimates. -/
theorem normalized_moment_error (a b : ℂ) (mass referenceMass ε : ℝ)
    (hm : 0 < mass) (hr : 0 < referenceMass)
    (hn : ‖a-b‖ ≤ ε) (hd : |mass-referenceMass| ≤ ε)
    (hb : ‖b‖ ≤ referenceMass) :
    ‖a/(mass:ℂ)-b/(referenceMass:ℂ)‖ ≤ 2*ε/mass := by
  have hm0 : (mass:ℂ) ≠ 0 := by exact_mod_cast ne_of_gt hm
  have hr0 : (referenceMass:ℂ) ≠ 0 := by exact_mod_cast ne_of_gt hr
  have hid : a/(mass:ℂ)-b/(referenceMass:ℂ) =
      (a-b)/(mass:ℂ)+(b/(referenceMass:ℂ))*((referenceMass-mass)/mass:ℝ) := by
    push_cast
    field_simp
    ring
  have hbn : ‖b/(referenceMass:ℂ)‖ ≤ 1 := by
    rw [norm_div, Complex.norm_real, Real.norm_eq_abs, abs_of_pos hr]
    exact (div_le_one hr).mpr hb
  rw [hid]
  calc
    _ ≤ ‖(a-b)/(mass:ℂ)‖ + ‖b/(referenceMass:ℂ)*((referenceMass-mass)/mass:ℝ)‖ := norm_add_le _ _
    _ = ‖a-b‖/mass + ‖b/(referenceMass:ℂ)‖*(|mass-referenceMass|/mass) := by
      rw [norm_div, norm_mul, Complex.norm_real, Complex.norm_real, Real.norm_eq_abs,
        Real.norm_eq_abs, abs_div, abs_of_pos hm, abs_sub_comm referenceMass mass]
    _ ≤ ε/mass + 1*(ε/mass) := by
      gcongr
    _ = _ := by ring

end DSP.PhasePower

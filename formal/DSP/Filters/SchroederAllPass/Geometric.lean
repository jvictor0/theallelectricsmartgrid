import DSP.Filters.SchroederAllPass.Proofs
import Mathlib.Algebra.Ring.GeomSum

namespace DSP.SchroederAllPass

theorem norm_one_sub_gain_sq {gain : ℝ} (hg : |gain| < 1) :
    ‖1 - (gain : ℂ) ^ 2‖ = (1 + |gain|) * (1 - |gain|) := by
  have hs := (sq_lt_one_iff_abs_lt_one gain).mpr hg
  have hc : (1 - (gain : ℂ) ^ 2) = ((1 - gain ^ 2 : ℝ) : ℂ) := by push_cast; rfl
  rw [hc, Complex.norm_real, Real.norm_eq_abs, abs_of_nonneg (sub_nonneg.mpr hs.le)]
  nlinarith [sq_abs gain]

/-- The tail of the one-sample response on the unit circle. -/
theorem response_truncation_le {gain : ℝ} (hg : |gain| < 1) (q : ℂ)
    (hq : ‖q‖ = 1) (terms : ℕ) :
    ‖response (gain : ℂ) q 1 -
      (-(gain : ℂ) + (1 - (gain : ℂ)^2) *
        ∑ n ∈ Finset.range terms, (gain : ℂ)^n * q^(n+1))‖ ≤
      (1 + |gain|) * |gain|^terms := by
  let g : ℂ := gain
  change ‖response g q 1 -
    (-g + (1 - g ^ 2) * ∑ n ∈ Finset.range terms, g ^ n * q ^ (n + 1))‖ ≤ _
  have hnorm : ‖g * q‖ = |gain| := by
    rw [norm_mul, hq, mul_one, Complex.norm_real, Real.norm_eq_abs]
  have hlower : 1 - |gain| ≤ ‖1 - g * q‖ := by
    simpa only [norm_one, hnorm] using norm_sub_norm_le (1 : ℂ) (g * q)
  have hpos : 0 < ‖1 - g * q‖ := (sub_pos.mpr hg).trans_le hlower
  have hden : 1 - g * q ≠ 0 := norm_pos_iff.mp hpos
  have hsum : (∑ n ∈ Finset.range terms, g ^ n * q ^ (n + 1)) =
      q * ∑ n ∈ Finset.range terms, (g * q) ^ n := by
    rw [Finset.mul_sum]
    apply Finset.sum_congr rfl
    intro n _
    rw [mul_pow, pow_succ]
    ring
  have hgeom : (∑ n ∈ Finset.range terms, (g * q) ^ n) * (1 - g * q) =
      1 - (g * q) ^ terms := geom_sum_mul_neg (g * q) terms
  have htail : response g q 1 -
      (-g + (1 - g ^ 2) * q * (∑ n ∈ Finset.range terms, (g * q) ^ n)) =
      ((1 - g ^ 2) * q * (g * q) ^ terms) / (1 - g * q) := by
    simp only [response, pow_one]
    apply (eq_div_iff hden).2
    rw [sub_mul, div_mul_cancel₀ _ hden]
    rw [add_mul, mul_assoc ((1 - g ^ 2) * q), hgeom]
    ring
  rw [hsum, ← mul_assoc, htail, norm_div, norm_mul, norm_mul, hq, mul_one,
    norm_pow, hnorm, norm_one_sub_gain_sq hg, div_le_iff₀ hpos]
  calc
    (1 + |gain|) * (1 - |gain|) * |gain| ^ terms =
        ((1 + |gain|) * |gain| ^ terms) * (1 - |gain|) := by ring
    _ ≤ ((1 + |gain|) * |gain| ^ terms) * ‖1 - g * q‖ := by gcongr

end DSP.SchroederAllPass

import DSP.Filters.SquiggleBoyQuadReverb.Frozen.Complexification

namespace DSP.SquiggleBoyQuadReverb.Frozen

open scoped BigOperators

noncomputable section

/-- The four cubic weights with the newest sample at exponent zero. -/
def cubicStencilResponse (t : ℝ) (q : ℂ) : ℂ :=
  ∑ j : Fin 4, ((cubicWeights t j : ℝ) : ℂ) * q ^ (3 - j.val)

set_option maxRecDepth 1800 in
theorem normSq_cubicStencilResponse (t : ℝ) (q : ℂ) (hq : ‖q‖ = 1) :
    Complex.normSq (cubicStencilResponse t q) =
      1 - (t * (1 - t)) ^ 2 * (1 - q.re) ^ 2 *
        (3 + 2 * (t * (1 - t)) * (1 - q.re)) := by
  have hunit : q.re ^ 2 + q.im ^ 2 = 1 := by
    have hs : Complex.normSq q = 1 := by simp [Complex.normSq_eq_norm_sq, hq]
    simpa only [Complex.normSq_apply, pow_two] using hs
  have hi2 : q.im ^ 2 = 1 - q.re ^ 2 := by linarith
  have hi4 : q.im ^ 4 = (1 - q.re ^ 2) ^ 2 := by rw [← hi2]; ring
  have hi6 : q.im ^ 6 = (1 - q.re ^ 2) ^ 3 := by rw [← hi2]; ring
  have hf : cubicStencilResponse t q =
      ((cubicWeights t 0 : ℝ) : ℂ) * (q*q*q) + ((cubicWeights t 1 : ℝ) : ℂ) * (q*q) +
        ((cubicWeights t 2 : ℝ) : ℂ) * q + ((cubicWeights t 3 : ℝ) : ℂ) := by
    norm_num [cubicStencilResponse, Fin.sum_univ_four]
    ring
  simp only [hf, Complex.normSq_apply, Complex.add_re, Complex.add_im, Complex.mul_re,
    Complex.mul_im, Complex.ofReal_re, Complex.ofReal_im, zero_mul, sub_zero, add_zero]
  simp [cubicWeights, Matrix.cons_val_two, Matrix.cons_val_three]
  ring_nf
  simp only [hi6, hi4, hi2]
  ring

/-- Cubic interpolation of four consecutive taps is passive on the unit circle. -/
theorem norm_cubicStencilResponse_le_one {t : ℝ} {q : ℂ} (ht : t ∈ Set.Icc 0 1)
    (hq : ‖q‖ = 1) : ‖cubicStencilResponse t q‖ ≤ 1 := by
  have hw : 0 ≤ t * (1 - t) := mul_nonneg ht.1 (sub_nonneg.mpr ht.2)
  have hr : 0 ≤ 1 - q.re := sub_nonneg.mpr
    ((le_abs_self _).trans ((Complex.abs_re_le_norm _).trans_eq hq))
  have hle : Complex.normSq (cubicStencilResponse t q) ≤ 1 := by
    rw [normSq_cubicStencilResponse t q hq]
    exact sub_le_self _ (by positivity)
  rw [Complex.normSq_eq_norm_sq] at hle
  nlinarith [norm_nonneg (cubicStencilResponse t q)]

theorem norm_tapResponse_le_one_of_consecutive {n : ℕ} (p : Tap n ℝ) (q : ℂ) (i : Fin 4)
    (hq : ‖q‖ = 1) (ht : p.fraction i ∈ Set.Icc 0 1) (base : ℕ)
    (hage : ∀ j : Fin 4, (p.age i j).val + 1 = base + (3 - j.val)) :
    ‖tapResponse p.complex q i‖ ≤ 1 := by
  have hfactor : tapResponse p.complex q i = q ^ base * cubicStencilResponse (p.fraction i) q := by
    simp only [tapResponse, Tap.complex, cubicStencilResponse, Finset.mul_sum]
    apply Finset.sum_congr rfl
    intro j _
    rw [hage j, pow_add, cubicWeights_ofReal]
    ring
  rw [hfactor, norm_mul, norm_pow, hq, one_pow, one_mul]
  exact norm_cubicStencilResponse_le_one ht hq

end

end DSP.SquiggleBoyQuadReverb.Frozen

import DSP.Filters.SquiggleBoyQuadReverb.Frozen.Expected

namespace DSP.SquiggleBoyQuadReverb.Frozen

open Matrix
open scoped Matrix.Norms.L2Operator

noncomputable section

private theorem norm_schroeder_le_one (gain : ℝ) (z : ℂ)
    (hg : |gain| < 1) (hz : ‖z‖ ≤ 1) :
    ‖(z - (gain : ℂ)) / (1 - (gain : ℂ) * z)‖ ≤ 1 := by
  have hmul : ‖(gain : ℂ) * z‖ < 1 := by
    rw [norm_mul, Complex.norm_real, Real.norm_eq_abs]
    exact (mul_le_of_le_one_right (abs_nonneg gain) hz).trans_lt hg
  have hd : 1 - (gain : ℂ) * z ≠ 0 := by
    intro h
    rw [← sub_eq_zero.mp h, norm_one] at hmul
    exact (lt_irrefl _) hmul
  have hzSq : Complex.normSq z ≤ 1 := by
    rw [Complex.normSq_eq_norm_sq]
    nlinarith [norm_nonneg z]
  have hdifference : Complex.normSq (1 - (gain : ℂ) * z) - Complex.normSq (z - (gain : ℂ)) =
      (1 - gain ^ 2) * (1 - Complex.normSq z) := by
    simp only [Complex.normSq_apply, Complex.sub_re, Complex.sub_im, Complex.mul_re, Complex.mul_im,
      Complex.one_re, Complex.one_im, Complex.ofReal_re, Complex.ofReal_im]
    ring
  have hsq : Complex.normSq (z - (gain : ℂ)) ≤ Complex.normSq (1 - (gain : ℂ) * z) := by
    apply sub_nonneg.mp
    rw [hdifference]
    exact mul_nonneg (sub_nonneg.mpr ((sq_lt_one_iff_abs_lt_one gain).mpr hg).le) (sub_nonneg.mpr hzSq)
  rw [norm_div, div_le_one (norm_pos_iff.mpr hd)]
  rw [Complex.normSq_eq_norm_sq, Complex.normSq_eq_norm_sq] at hsq
  nlinarith [norm_nonneg (z - (gain : ℂ)), norm_nonneg (1 - (gain : ℂ) * z)]

theorem norm_allPassResponse_le_one {n : ℕ} {p : Tap n ℂ} {q : ℂ} {i : Fin 4}
    (h : ‖tapResponse p q i‖ ≤ 1) : ‖allPassResponse p q i‖ ≤ 1 := by
  have hg : |(allPassGain : ℝ)| < 1 := by norm_num [allPassGain]
  have hc : ((allPassGain : ℝ) : ℂ) = (allPassGain : ℂ) := by norm_num [allPassGain]
  simpa only [allPassResponse, SchroederAllPass.response, pow_one, hc] using
    norm_schroeder_le_one (allPassGain : ℝ) (tapResponse p q i) hg h

theorem norm_introResponse_le_one (q : ℂ) (order : List (Fin 8)) (taps : IntroTaps order ℂ)
    (h : ∀ j i, ‖tapResponse (taps j) q i‖ ≤ 1) : ‖introResponse q order taps‖ ≤ 1 := by
  induction order with
  | nil => simp [introResponse]
  | cons i order ih =>
    rw [introResponse, pi_norm_le_iff_of_nonneg zero_le_one]
    intro k
    have ht : ‖introResponse q order (fun j => taps j.succ) k‖ ≤ 1 :=
      (norm_le_pi_norm _ k).trans (ih _ (fun j l => h j.succ l))
    rw [Pi.mul_apply, norm_mul]
    exact mul_le_one₀ (norm_allPassResponse_le_one (h 0 k)) (norm_nonneg _) ht

/-- A frequency-local bound on both the return and the single-pass drive.
The damping bound can be tighter than one inside or outside the passband. -/
theorem transfer_bounds_of_passive_taps (p : Spec ℂ) (q : ℂ) {d : ℝ}
    (hq : ‖q‖ = 1) (hd0 : 0 ≤ d)
    (hmix : ‖(LinearMap.toMatrix' (mix : Lanes ℂ →ₗ[ℂ] Lanes ℂ))‖ ≤ 1)
    (hmain : ∀ i, ‖tapResponse p.main q i‖ ≤ 1)
    (hfirst : ∀ i, ‖tapResponse p.first q i‖ ≤ 1)
    (hsecond : ∀ i, ‖tapResponse p.second q i‖ ≤ 1)
    (hintro : ∀ j i, ‖tapResponse (p.intro j) q i‖ ≤ 1)
    (hdamp : ∀ i, ‖dampingResponse p q i‖ ≤ d) :
    ‖forward p q‖ ≤ d * ‖p.satGain‖ ∧
    ‖loopReturn p q‖ ≤ d * ‖p.satGain‖ * ‖p.feedback‖ ∧
      ‖directTransfer p q‖ ≤ d * ‖p.satGain‖ := by
  let out (i : Fin 4) := allPassResponse p.second q i * dampingResponse p q i *
    p.satGain i * allPassResponse p.first q i
  have hOut : ‖diagonal out‖ ≤ d * ‖p.satGain‖ := by
    rw [Matrix.l2_opNorm_diagonal, pi_norm_le_iff_of_nonneg (mul_nonneg hd0 (norm_nonneg _))]
    intro i
    calc
      ‖out i‖ = ‖allPassResponse p.second q i‖ * ‖dampingResponse p q i‖ *
          ‖p.satGain i‖ * ‖allPassResponse p.first q i‖ := by simp only [out, norm_mul]
      _ ≤ 1 * d * ‖p.satGain‖ * 1 := by
        gcongr
        · exact norm_allPassResponse_le_one (hsecond i)
        · exact hdamp i
        · exact norm_le_pi_norm _ _
        · exact norm_allPassResponse_le_one (hfirst i)
      _ = _ := by simp
  have hMain : ‖diagonal (q⁻¹ • tapResponse p.main q)‖ ≤ 1 := by
    rw [Matrix.l2_opNorm_diagonal, pi_norm_le_iff_of_nonneg zero_le_one]
    intro i
    simpa only [Pi.smul_apply, norm_smul, norm_inv, hq, inv_one, one_mul] using hmain i
  have hForward : ‖forward p q‖ ≤ d * ‖p.satGain‖ := by
    change ‖diagonal out * LinearMap.toMatrix' mix * diagonal (q⁻¹ • tapResponse p.main q)‖ ≤ _
    calc
      _ ≤ ‖diagonal out * LinearMap.toMatrix' (mix : Lanes ℂ →ₗ[ℂ] Lanes ℂ)‖ *
          ‖diagonal (q⁻¹ • tapResponse p.main q)‖ := norm_mul_le _ _
      _ ≤ (d * ‖p.satGain‖ * 1) * 1 := mul_le_mul
        ((norm_mul_le _ _).trans (mul_le_mul hOut hmix (norm_nonneg _) (by positivity)))
        hMain (norm_nonneg _) (by positivity)
      _ = _ := by simp
  refine ⟨hForward, ?_, ?_⟩
  · unfold loopReturn
    rw [norm_smul, hq, one_mul]
    exact (norm_mul_le _ _).trans (by rw [Matrix.l2_opNorm_diagonal]; gcongr)
  · unfold directTransfer
    refine (norm_mul_le _ _).trans ?_
    rw [Matrix.l2_opNorm_diagonal, ← mul_one (d * ‖p.satGain‖)]
    exact mul_le_mul hForward (norm_introResponse_le_one q _ p.intro hintro) (norm_nonneg _) (by positivity)

end

end DSP.SquiggleBoyQuadReverb.Frozen

import DSP.Filters.NaiveReuppingLadder.HarmonicBalance
import DSP.Analysis.NormalizedTanhDerivatives
import DSP.Analysis.ResidualValidation

/-! Full harmonic-balance equation residuals and a posteriori waveform error.
The equation space contains ALL modes. Contraction of a preconditioned root
solver does not assert physical attraction or selection of this solution from
reset. Autonomous phases require the separate phase-fixed formulation. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance
open DSP.Harmonics Metric
open scoped NNReal Matrix
variable {d : Type*} [Fintype d]

abbrev WaveState (d : Type*) := Fin 4 → Waveform d

def WaveState.orbit (q : WaveState d) : Orbit d := ⟨q 0,q 1,q 2,q 3⟩

def residual (s : NaiveReuppingLadder ℝ) (ω : Phase d) (u : Waveform d)
    (q : WaveState d) : WaveState d :=
  ![q 0-((1-s.alpha) • delay ω (q 0)+s.alpha • (u-s.baseFeedback • delay ω (q 3))),
    q 1-((1-s.alpha) • delay ω (q 1)+s.alpha • saturated s (q 0)),
    q 2-((1-s.alpha) • delay ω (q 2)+s.alpha • saturated s (q 1)),
    q 3-((1-s.alpha) • delay ω (q 3)+s.alpha • saturated s (q 2))]

theorem residual_zero_iff (s : NaiveReuppingLadder ℝ) (ω : Phase d) (u : Waveform d)
    (q : WaveState d) : residual s ω u q = 0 ↔ Balance s ω u q.orbit := by
  rw [← equations_iff_balance]
  simp only [residual,Equations,WaveState.orbit,funext_iff,Fin.forall_fin_succ,
    Fin.forall_fin_zero,Matrix.cons_val_zero,Matrix.cons_val_succ,Pi.zero_apply,sub_eq_zero,and_true]

/-- A small preconditioned FULL residual plus a uniform inverse-Jacobian
defect proves a nearby exact HB solution. The bound remains a formula in the
parameters; no single numerical fixture is hardwired. -/
theorem exists_balance_near (s : NaiveReuppingLadder ℝ) (ω : Phase d) (u : Waveform d)
    (inverse : WaveState d →L[ℝ] WaveState d) (hi : Function.Injective inverse)
    (candidate : WaveState d) (radius defect : ℝ) (q : ℝ≥0) (hr : 0 ≤ radius) (hq : q < 1)
    (hc : LipschitzOnWith q (ResidualValidation.correction (residual s ω u) inverse)
      (closedBall candidate radius))
    (hd : ‖inverse (residual s ω u candidate)‖ ≤ defect) (htrap : defect+q*radius ≤ radius) :
    ∃ solution ∈ closedBall candidate radius, Balance s ω u solution.orbit ∧
      dist candidate solution ≤ defect/(1-q) := by
  obtain ⟨solution,hs,he,_,herr⟩ :=
    ResidualValidation.exists_solution (residual s ω u) inverse hi candidate radius defect q hr hq hc hd htrap
  exact ⟨solution,hs,(residual_zero_iff s ω u solution).mp he,herr⟩

def outputLipschitz (s : NaiveReuppingLadder ℝ) : ℝ :=
  |s.baseFeedback+1| * (|s.saturation.gain|/|s.saturation.tanhGain|)

theorem saturation_slope_bound (c : NormalizedTanh.Coefficients ℝ) (x : ℝ) :
    |NormalizedTanh.slope c x| ≤ |c.gain|/|c.tanhGain| := by
  have ht := (Real.tanh_sq_lt_one (x*c.gain)).le
  have h0 := sq_nonneg (Real.tanh (x*c.gain))
  rw [NormalizedTanh.slope,abs_div,abs_mul,abs_of_nonneg (by linarith : 0 ≤ 1-Real.tanh (x*c.gain)^2)]
  exact div_le_div_of_nonneg_right (by nlinarith [abs_nonneg c.gain]) (abs_nonneg c.tanhGain)

theorem saturation_difference_bound (c : NormalizedTanh.Coefficients ℝ) (x y : ℝ) :
    |NormalizedTanh.apply c x-NormalizedTanh.apply c y| ≤
      (|c.gain|/|c.tanhGain|)*|x-y| := by
  exact Convex.norm_image_sub_le_of_norm_hasDerivWithin_le
    (fun z (_ : z ∈ Set.univ) => (NormalizedTanh.hasDerivAt_apply c z).hasDerivWithinAt)
    (fun z _ => saturation_slope_bound c z) convex_univ (Set.mem_univ y) (Set.mem_univ x)

omit [Fintype d] in
theorem output_difference_bound (s : NaiveReuppingLadder ℝ) (a b : WaveState d) (θ : Phase d) :
    |output s a.orbit θ-output s b.orbit θ| ≤ outputLipschitz s*dist a b := by
  have hs := saturation_difference_bound s.saturation (a 3 θ) (b 3 θ)
  have hw : |a 3 θ-b 3 θ| ≤ dist a b := by
    change ‖(a 3-b 3) θ‖ ≤ dist a b
    apply (ContinuousMap.norm_coe_le_norm _ _).trans
    exact (norm_le_pi_norm (a-b) 3).trans_eq (dist_eq_norm a b).symm
  have hL : 0 ≤ |s.saturation.gain|/|s.saturation.tanhGain| := div_nonneg (abs_nonneg _) (abs_nonneg _)
  change |(s.baseFeedback+1)*NormalizedTanh.apply s.saturation (a 3 θ)-
    (s.baseFeedback+1)*NormalizedTanh.apply s.saturation (b 3 θ)| ≤ _
  rw [← mul_sub,abs_mul]
  exact (mul_le_mul_of_nonneg_left (hs.trans (mul_le_mul_of_nonneg_left hw hL)) (abs_nonneg _)).trans_eq
    (by unfold outputLipschitz; ring)

end DSP.NaiveReuppingLadder.HarmonicBalance

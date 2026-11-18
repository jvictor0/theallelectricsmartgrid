import DSP.Filters.ConservativeReuppingLadder.Transfer
import DSP.Filters.NormalizedTanh.Proofs

/-! Differentiate the actual nonlinear trajectory with respect to stimulus
amplitude. The existing normalized-tanh derivative is reused; none of the
adaptive ladder's dynamics participate in this proof. This file proves the
finite-time derivative; Uniform.lean and Convergence.lean subsequently establish
uniform-in-time approximation and the time-first small-signal response. -/

noncomputable section
namespace DSP.ConservativeReuppingLadder

abbrev trajectory (s : ConservativeReuppingLadder ℝ) (u : ℕ → ℝ) :=
  Step.states step (fun i => ⟨u i⟩) s

abbrev linearTrajectory (s : ConservativeReuppingLadder ℝ) (u : ℕ → ℝ) :=
  Step.states linearStep (fun i => ⟨u i⟩) s

theorem trajectory_coefficients (s : ConservativeReuppingLadder ℝ) (u : ℕ → ℝ) (n : ℕ) :
    (trajectory s u n).alpha = s.alpha ∧ (trajectory s u n).k = s.k ∧
      (trajectory s u n).saturation = s.saturation := by
  induction n with
  | zero => exact ⟨rfl,rfl,rfl⟩
  | succ n ih => exact ih

theorem step_zero_reset (s : ConservativeReuppingLadder ℝ) : step (reset s) ⟨0⟩ = reset s := by
  simp [step, stepWith, reset, NormalizedTanh.apply, OnePoleZOHLowPass.onePoleStep_eq, fma_eq]

theorem trajectory_zero (s : ConservativeReuppingLadder ℝ) (n : ℕ) :
    trajectory (reset s) (fun _ => 0) n = reset s := by
  induction n with
  | zero => rfl
  | succ n ih =>
    change step (trajectory (reset s) (fun _ => 0) n) ⟨0⟩ = _
    rw [ih, step_zero_reset]

theorem onePole_derivative {input history : ℝ → ℝ} {di dh : ℝ}
    (alpha : ℝ) (hi : HasDerivAt input di 0) (hh : HasDerivAt history dh 0) :
    HasDerivAt (fun e => OnePoleZOHLowPass.onePoleStep alpha (input e) (history e))
      (OnePoleZOHLowPass.onePoleStep alpha di dh) 0 := by
  simp only [OnePoleZOHLowPass.onePoleStep_eq, fma_eq]
  exact (hi.const_mul alpha).fun_add (hh.const_mul (1-alpha))

theorem saturation_derivative (c : NormalizedTanh.Coefficients ℝ)
    {f : ℝ → ℝ} {d : ℝ} (hzero : f 0 = 0) (h : HasDerivAt f d 0) :
    HasDerivAt (fun e => NormalizedTanh.apply c (f e)) (NormalizedTanh.slopeZero c*d) 0 := by
  have ht := NormalizedTanh.hasDerivAt_zero c
  rw [← hzero] at ht
  exact ht.comp 0 h

theorem trajectory_hasDerivAt_zero (s : ConservativeReuppingLadder ℝ) (u : ℕ → ℝ) (n : ℕ) :
    let actual := fun e => trajectory (reset s) (fun i => e*u i) n
    let tangent := linearTrajectory (reset s) u n
    HasDerivAt (fun e => (actual e).stage1) tangent.stage1 0 ∧
    HasDerivAt (fun e => (actual e).stage2) tangent.stage2 0 ∧
    HasDerivAt (fun e => (actual e).stage3) tangent.stage3 0 ∧
    HasDerivAt (fun e => (actual e).stage4) tangent.stage4 0 ∧
    HasDerivAt (fun e => (actual e).output) tangent.output 0 := by
  induction n with
  | zero => exact ⟨hasDerivAt_const _ _, hasDerivAt_const _ _, hasDerivAt_const _ _,
      hasDerivAt_const _ _, hasDerivAt_const _ _⟩
  | succ n ih =>
    dsimp only at ih ⊢
    let actual := fun e => trajectory (reset s) (fun i => e*u i) n
    let tangent := linearTrajectory (reset s) u n
    have hc (e : ℝ) := trajectory_coefficients (reset s) (fun i => e*u i) n
    have hl := linear_coefficients (reset s) u n
    have hz : actual 0 = reset s := by
      simpa only [actual, zero_mul] using trajectory_zero s n
    let d := fun e => e*u n-s.k*(actual e).stage4
    let f1 := fun e => OnePoleZOHLowPass.onePoleStep s.alpha (d e) (actual e).stage1
    let f2 := fun e => OnePoleZOHLowPass.onePoleStep s.alpha (NormalizedTanh.apply s.saturation (f1 e)) (actual e).stage2
    let f3 := fun e => OnePoleZOHLowPass.onePoleStep s.alpha (NormalizedTanh.apply s.saturation (f2 e)) (actual e).stage3
    let f4 := fun e => OnePoleZOHLowPass.onePoleStep s.alpha (NormalizedTanh.apply s.saturation (f3 e)) (actual e).stage4
    have hzero : f1 0 = 0 ∧ f2 0 = 0 ∧ f3 0 = 0 ∧ f4 0 = 0 := by
      simp [f1,f2,f3,f4,d,hz,reset,OnePoleZOHLowPass.onePoleStep_eq,fma_eq,NormalizedTanh.apply]
    have hd := ((hasDerivAt_id (0 : ℝ)).mul_const (u n)).sub (ih.2.2.2.1.const_mul s.k)
    have h1 := onePole_derivative s.alpha hd ih.1
    have h2 := onePole_derivative s.alpha (saturation_derivative s.saturation hzero.1 h1) ih.2.1
    have h3 := onePole_derivative s.alpha (saturation_derivative s.saturation hzero.2.1 h2) ih.2.2.1
    have h4 := onePole_derivative s.alpha (saturation_derivative s.saturation hzero.2.2.1 h3) ih.2.2.2.1
    have ho := (saturation_derivative s.saturation hzero.2.2.2 h4).mul_const (s.k+1)
    have he (e : ℝ) : trajectory (reset s) (fun i => e*u i) (n+1) =
        {reset s with
          stage1 := f1 e, stage2 := f2 e, stage3 := f3 e, stage4 := f4 e,
          input := d e, output := NormalizedTanh.apply s.saturation (f4 e)*(s.k+1)} := by
      change step (actual e) ⟨e*u n⟩ = _
      have hcoeff := hc e
      change (actual e).alpha = s.alpha ∧ (actual e).k = s.k ∧ (actual e).saturation = s.saturation at hcoeff
      simp only [step,stepWith,hcoeff.1,hcoeff.2.1,hcoeff.2.2,f1,f2,f3,f4,d,reset]
    simp only [he]
    simp only [linearTrajectory, Step.states_succ, linearStep, stepWith]
    rw [hl.1, hl.2.1, hl.2.2]
    dsimp only [f1,f2,f3,f4,d,actual,linearTrajectory,reset,Pi.sub_apply,id_eq] at h1 h2 h3 h4 ho ⊢
    simp only [one_mul] at h1 h2 h3 h4 ho
    exact ⟨h1,h2,h3,h4,ho⟩

/-- At every sample, the derivative of the actual output is exactly the
output of the tangent recurrence whose transfer is proved in realizes_linear. -/
theorem output_hasDerivAt_zero (s : ConservativeReuppingLadder ℝ) (u : ℕ → ℝ) (n : ℕ) :
    HasDerivAt (fun e => (trajectory (reset s) (fun i => e*u i) (n+1)).output)
      (linearTrajectory (reset s) u (n+1)).output 0 :=
  (trajectory_hasDerivAt_zero s u (n+1)).2.2.2.2

/-- The samplewise amplitude derivative of the actual nonlinear output has
the stated transfer for every input stream. No infinite-time limit is taken. -/
theorem derivative_stream (s : ConservativeReuppingLadder ℝ) (u : ℕ → ℝ) :
    Bilinear.ser (fun n => deriv
      (fun e => (trajectory (reset s) (fun i => e*u i) (n+1)).output) 0) =
      transfer (Bilinear.const s.alpha) (Bilinear.const (NormalizedTanh.slopeZero s.saturation))
        (Bilinear.const s.k) Bilinear.delay * Bilinear.ser u := by
  rw [Bilinear.ser_congr (fun n => (output_hasDerivAt_zero s u n).deriv)]
  exact realizes_linear (reset s) ⟨rfl,rfl,rfl,rfl⟩ u

end DSP.ConservativeReuppingLadder

import DSP.Filters.ConservativeReuppingLadder.StateSpace
import DSP.Analysis.LocalDynamics

/-! Uniform-in-time small-signal approximation for the actual ladder. -/

noncomputable section
namespace DSP.ConservativeReuppingLadder

theorem histories_trajectory (s : ConservativeReuppingLadder ℝ) (u : ℕ → ℝ) (n : ℕ) :
    histories (trajectory (reset s) u n) = LocalDynamics.orbit (stateUpdate s) u 0 n := by
  induction n with
  | zero => ext i; fin_cases i <;> rfl
  | succ n ih =>
    have hc := trajectory_coefficients (reset s) u n
    change (trajectory (reset s) u n).alpha = s.alpha ∧
      (trajectory (reset s) u n).k = s.k ∧ (trajectory (reset s) u n).saturation = s.saturation at hc
    simp only [LocalDynamics.orbit, ← ih]
    change histories (step (trajectory (reset s) u n) ⟨u n⟩) = _
    ext i
    fin_cases i <;> simp [stateUpdate,histories,step,stepWith,withHistory,hc.1,hc.2.1,hc.2.2]

theorem histories_linearTrajectory (s : ConservativeReuppingLadder ℝ) (u : ℕ → ℝ) (n : ℕ) :
    histories (linearTrajectory (reset s) u n) = LocalDynamics.orbit
      (linearUpdate s).toContinuousLinearMap u 0 n := by
  induction n with
  | zero => ext i; fin_cases i <;> rfl
  | succ n ih =>
    have hc := linear_coefficients (reset s) u n
    change (linearTrajectory (reset s) u n).alpha = s.alpha ∧
      (linearTrajectory (reset s) u n).k = s.k ∧
      (linearTrajectory (reset s) u n).saturation = s.saturation at hc
    simp only [LocalDynamics.orbit, ← ih]
    change histories (linearStep (linearTrajectory (reset s) u n) ⟨u n⟩) = _
    ext i
    fin_cases i <;> simp [linearUpdate,histories,linearStep,stepWith,withHistory,hc.1,hc.2.1,hc.2.2]

def observation (s : ConservativeReuppingLadder ℝ) (x : History ℝ) : ℝ :=
  NormalizedTanh.apply s.saturation (x 3)*(s.k+1)

def linearObservation (s : ConservativeReuppingLadder ℝ) : History ℝ →L[ℝ] ℝ :=
  ((s.k+1)*NormalizedTanh.slopeZero s.saturation) • ContinuousLinearMap.proj 3

theorem observation_zero (s : ConservativeReuppingLadder ℝ) : observation s 0 = 0 := by
  simp [observation,NormalizedTanh.apply]

theorem observation_contDiff (s : ConservativeReuppingLadder ℝ) : ContDiff ℝ 1 (observation s) := by
  unfold observation NormalizedTanh.apply
  simp only [Tanh.tanh]
  fun_prop

theorem observation_hasFDerivAt (s : ConservativeReuppingLadder ℝ) :
    HasFDerivAt (observation s) (linearObservation s) 0 := by
  let P : History ℝ →L[ℝ] ℝ := ContinuousLinearMap.proj 3
  have h := (saturation_fderiv s.saturation (f := P) (map_zero P) P.hasFDerivAt).mul_const (s.k+1)
  convert! h using 1
  ext x
  simp [linearObservation,P]
  ring

theorem output_eq_observation (s : ConservativeReuppingLadder ℝ) (u : ℕ → ℝ) (n : ℕ) :
    (trajectory (reset s) u (n+1)).output = observation s
      (LocalDynamics.orbit (stateUpdate s) u 0 (n+1)) := by
  rw [← histories_trajectory]
  have hc := trajectory_coefficients (reset s) u n
  change (NormalizedTanh.apply (trajectory (reset s) u n).saturation
    (trajectory (reset s) u (n+1)).stage4)*((trajectory (reset s) u n).k+1) = _
  rw [hc.2.1,hc.2.2]
  rfl

theorem linearOutput_eq_observation (s : ConservativeReuppingLadder ℝ) (u : ℕ → ℝ) (n : ℕ) :
    (linearTrajectory (reset s) u (n+1)).output = linearObservation s
      (LocalDynamics.orbit (linearUpdate s).toContinuousLinearMap u 0 (n+1)) := by
  rw [← histories_linearTrajectory]
  have hc := linear_coefficients (reset s) u n
  change (NormalizedTanh.slopeZero (linearTrajectory (reset s) u n).saturation *
    (linearTrajectory (reset s) u (n+1)).stage4)*((linearTrajectory (reset s) u n).k+1) = _
  rw [hc.2.1,hc.2.2]
  simp [linearObservation,histories,reset]
  ring

/-- One amplitude threshold works for every sample and every input sequence
bounded by one, for the actual prepared nonlinear processor and its output. -/
theorem prepared_uniform_output (spec : LogicalSpec ℝ) (s : ConservativeReuppingLadder ℝ)
    (hc : 0 < spec.cutoff) (hr0 : 0 ≤ spec.resonance) (hr1 : spec.resonance ≤ 1)
    (hg : 0 < spec.gain) :
    let f := computeCoefficients spec s
    ∀ eta > 0, ∃ delta > 0, ∀ epsilon : ℝ, |epsilon| < delta →
      ∀ u : ℕ → ℝ, (∀ n, |u n| ≤ 1) → ∀ n,
        |(trajectory (reset f) (fun i => epsilon*u i) (n+1)).output -
          epsilon*(linearTrajectory (reset f) u (n+1)).output| ≤ eta*|epsilon| := by
  dsimp only
  let f := computeCoefficients spec s
  have h := LocalDynamics.uniform_observation (stateUpdate_zero f)
    (linearUpdate f).toContinuousLinearMap (stateUpdate_hasStrictFDerivAt f)
    (preparedGauge spec s hc hr0 hr1 hg) (observation_zero f) (linearObservation f)
    (observation_hasFDerivAt f)
  intro eta heta
  obtain ⟨delta,hd,hbound⟩ := h eta heta
  refine ⟨delta,hd,?_⟩
  intro epsilon he u hu n
  rw [output_eq_observation,linearOutput_eq_observation]
  exact hbound epsilon he u hu (n+1)

end DSP.ConservativeReuppingLadder

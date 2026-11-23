import DSP.Analysis.PhasePower.UnitPhase

/-! A closed, parameterized approximation theorem for the discrete reduced
phase system itself. No residual or contraction certificate is assumed here.
The companion perturbed theorem exposes the remaining phase-model error when
this reduction is applied to a full HB waveform or physical trajectory. -/

noncomputable section
namespace DSP.PhasePower.Discrete
open DSP.ResponseMeasurement

def phaseErrorBudget (q : Quadratic) (c : ℝ) (N : ℕ) (h phaseError : ℝ) : ℝ :=
  let D := h*(1+c)^2+phaseError
  error q c N h (phaseError+D^2/2) D

def samplingError (q : Quadratic) (c : ℝ) (N : ℕ) (h : ℝ) : ℝ :=
  tail q c N+(size1 q c N+size2 q c N)*h*(1+c)^4/2

theorem zero_phase_error (q : Quadratic) (c : ℝ) (N : ℕ) (h : ℝ) (hh : 0 < h) :
    phaseErrorBudget q c N h 0 = samplingError q c N h := by
  unfold phaseErrorBudget error samplingError
  field_simp
  ring

theorem perturbed_phase_curve (q : Quadratic) (c : ℝ) (hc : 0 ≤ c) (N : ℕ)
    (h : ℝ) (hh : 0 < h) (angle : ℕ → ℝ) (observation : ℕ → ℂ)
    (phaseError observationError : ℝ)
    (he : ∀ n, |angle (n+1)-angle n-h*velocity c (rotation (angle n))| ≤ phaseError)
    (ho : ∀ n, ‖observation n-q.eval (rotation (angle n))‖ ≤ observationError) :
    EventuallyNear (DSP.Coboundary.average observation) (q.eval (center c))
      (observationError+phaseErrorBudget q c N h phaseError) := by
  have hs n := phase_step c hc h hh.le (angle n) (angle (n+1)) phaseError (he n)
  exact approximates_observable q c hc N h hh (fun n => rotation (angle n)) observation
    (phaseError+(h*(1+c)^2+phaseError)^2/2) (h*(1+c)^2+phaseError) observationError
    (fun n => rotation_norm _) (fun n => (hs n).2) (fun n => (hs n).1) ho

def trajectory (c h initial : ℝ) : ℕ → ℝ
  | 0 => initial
  | n+1 => trajectory c h initial n+h*velocity c (rotation (trajectory c h initial n))

/-- Every initial phase, every positive step, and every finite correction
order. For 0 <= c < 1 the geometric tail decays as the order increases.
No rational/irrational rotation distinction or average-convergence assumption. -/
theorem reduced_model_curve (q : Quadratic) (c : ℝ) (hc : 0 ≤ c) (N : ℕ)
    (h : ℝ) (hh : 0 < h) (initial : ℝ) :
    EventuallyNear (DSP.Coboundary.average
      (fun n => q.eval (rotation (trajectory c h initial n)))) (q.eval (center c))
      (samplingError q c N h) := by
  have hp := perturbed_phase_curve q c hc N h hh (trajectory c h initial)
    (fun n => q.eval (rotation (trajectory c h initial n))) 0 0
    (fun n => by rw [trajectory]; simp) (fun n => by simp)
  simpa only [zero_phase_error q c N h hh,zero_add] using hp

/-- The center used in the exact finite-correction identity is precisely
the closed-form running moment at the specified detuning ratio. -/
theorem running_curve (q : Quadratic) (ratio : ℝ) (hr : 1 < ratio) (N : ℕ)
    (h : ℝ) (hh : 0 < h) (initial : ℝ) :
    EventuallyNear (DSP.Coboundary.average
      (fun n => q.eval (rotation (trajectory (runningCoherence ratio) h initial n))))
      (q.eval (runningMoment (runningCoherence ratio)))
      (samplingError q (runningCoherence ratio) N h) :=
  reduced_model_curve q _ (running_parameters ratio hr).1.le N h hh initial

end DSP.PhasePower.Discrete

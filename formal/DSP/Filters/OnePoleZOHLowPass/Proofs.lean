import DSP.Filter
import DSP.Filters.OnePoleZOHLowPass.Reference
import DSP.Discretization.ZOH
import DSP.Transfer.Delay
import Mathlib.Analysis.Calculus.Deriv.Prod
import Mathlib.Analysis.Calculus.Deriv.MeanValue
import Mathlib.Analysis.Calculus.Deriv.Shift
import Mathlib.Analysis.SpecialFunctions.ExpDeriv
import Mathlib.Tactic.Linarith
import Mathlib.Tactic.LinearCombination
import Mathlib.Tactic.Ring

namespace DSP.OnePoleZOHLowPass

/-! Exact discretization of y' = (u - y) / τ under a zero-order hold.
The coefficient alpha is the input weight 1 - exp(-T / τ), matching the app's
`OPLowPassFilter`. These proofs specialize the shared executable step and
state update to ℝ.
-/

noncomputable def digital (alpha : ℝ) : DigitalFilter ℝ where
  step := fun input state => (process { alpha := alpha, y := state } input).y
  output := id

noncomputable def digitalRun (alpha : ℝ) (u : DigitalSignal)
    (initial : ℝ) : DigitalSignal :=
  (digital alpha).run u initial

@[simp] theorem digitalRun_zero (alpha : ℝ) (u : DigitalSignal) (initial : ℝ) :
    digitalRun alpha u initial 0 = initial := rfl

theorem digitalRun_succ (alpha : ℝ) (u : DigitalSignal) (initial : ℝ) (n : ℕ) :
    digitalRun alpha u initial (n + 1) = onePoleStep alpha (u n) (digitalRun alpha u initial n) :=
  rfl

/-- The shared state-only step follows the scalar digital recurrence, including
its initial sample before any input interval has been processed. -/
theorem states_digitalRun (f : OnePoleZOHLowPass ℝ) (u : DigitalSignal) (n : ℕ) :
    (Step.states step (fun k => ⟨u k⟩) f n).y = digitalRun f.alpha u f.y n := by
  have ha := fun k => Step.states_invariant step (·.alpha) (fun _ _ => rfl)
    (fun k => ⟨u k⟩) f k
  induction n with
  | zero => rfl
  | succ n ih => rw [Step.states_succ, step_y, ha, ih, digitalRun_succ]

/-- Input u n is held on the nth sampling interval. Continuity includes the
boundaries, but the ODE is required only in each open interval: the held input
may jump at a sample time, so no derivative is assumed there. -/
def AnalogOnePoleLowPass (T τ : ℝ) (u : DigitalSignal) (initial : ℝ)
    (y : AnalogSignal) : Prop :=
  y 0 = initial ∧
  ContinuousOn y (Set.Ici 0) ∧
  ∀ (n : ℕ) (t : ℝ),
    sampleTime T n < t → t < sampleTime T (n + 1) →
      HasDerivAt y ((u n - y t) / τ) t

def analog (τ : ℝ) : AnalogFilter ℝ :=
  fun T u initial y => AnalogOnePoleLowPass T τ u initial y

theorem coefficient_bounds (T τ : ℝ) (hT : 0 < T) (hτ : 0 < τ) :
    0 < 1 - Real.exp (-T / τ) ∧ 1 - Real.exp (-T / τ) < 1 := by
  have hlt := Real.exp_lt_one_iff.mpr (div_neg_of_neg_of_pos (neg_neg_of_pos hT) hτ)
  have hpos := Real.exp_pos (-T / τ)
  exact ⟨by linarith, by linarith⟩

/-- Continuous dynamics y' = (u-y)/τ. -/
noncomputable def continuousSystem (τ : ℝ) : StateSpace 1 ℝ where
  A _ _ := -1 / τ
  B _ := 1 / τ
  C _ := 1
  D := 0

theorem continuousSystem_derivative (τ u : ℝ) (q : Fin 1 → ℝ) (i : Fin 1) :
    ((continuousSystem τ).operator q + u • (continuousSystem τ).B) i = (u - q i) / τ := by
  fin_cases i
  simp [continuousSystem, StateSpace.operator_apply, Matrix.mulVec, dotProduct]
  ring

/-- Identify the generic matrix-exponential ZOH with the scalar coefficient. -/
theorem zoh_advance_eq (T τ input initial : ℝ) (hT : 0 ≤ T) :
    ((continuousSystem τ).zoh T).advance (fun _ => initial) input =
      fun _ => onePoleStep (1 - Real.exp (-T / τ)) input initial := by
  let q : ℝ → Fin 1 → ℝ := fun t _ => input + Real.exp (-t / τ) * (initial - input)
  have hd : ∀ t, HasDerivAt q ((continuousSystem τ).operator (q t) + input • (continuousSystem τ).B) t := by
    intro t
    apply hasDerivAt_pi.mpr
    intro i
    rw [continuousSystem_derivative]
    exact (((((hasDerivAt_id t).neg).div_const τ).exp).mul_const (initial - input)).const_add input
      |>.congr_deriv (by dsimp [q]; ring)
  have hc : ContinuousOn q (Set.Icc 0 T) :=
    (show Differentiable ℝ q from fun t => (hd t).differentiableAt).continuous.continuousOn
  have he := (continuousSystem τ).zoh_interval T hT input q hc (fun t _ => hd t)
  have hi : q 0 = fun _ => initial := by funext i; simp [q]
  rw [hi] at he
  rw [← he]
  funext i
  simp only [q, onePoleStep_eq, fma_eq]
  ring

theorem onePole_implements_interval (T τ input initial : ℝ)
    (hT : 0 < T) (y : AnalogSignal)
    (hcont : ContinuousOn y (Set.Icc 0 T)) (hinit : y 0 = initial)
    (hode : ∀ t ∈ Set.Ioo 0 T, HasDerivAt y ((input - y t) / τ) t) :
    y T = onePoleStep (1 - Real.exp (-T / τ)) input initial := by
  have hc : ContinuousOn (fun t => fun _ : Fin 1 => y t) (Set.Icc 0 T) :=
    continuousOn_pi.mpr (fun _ => hcont)
  have hd : ∀ t ∈ Set.Ioo 0 T,
      HasDerivAt (fun r => fun _ : Fin 1 => y r)
        ((continuousSystem τ).operator (fun _ => y t) + input • (continuousSystem τ).B) t := by
    intro t ht
    exact hasDerivAt_pi.mpr (fun i => by rw [continuousSystem_derivative]; exact hode t ht)
  have he := (continuousSystem τ).zoh_interval T hT.le input _ hc hd
  rw [hinit, zoh_advance_eq T τ input initial hT.le] at he
  exact congrFun he 0

/-- The exported step's real specialization returns the analog endpoint. -/
theorem step_matches_interval (T τ input initial : ℝ)
    (hT : 0 < T) (y : AnalogSignal)
    (hcont : ContinuousOn y (Set.Icc 0 T)) (hinit : y 0 = initial)
    (hode : ∀ t ∈ Set.Ioo 0 T, HasDerivAt y ((input - y t) / τ) t) :
    (step { alpha := 1 - Real.exp (-T / τ), y := initial } { x := input }).y = y T := by
  exact (onePole_implements_interval T τ input initial hT y hcont hinit hode).symm

/-- Translate the nth interval to [0, T], preserving continuity and the ODE. -/
theorem onePole_step_at (T τ : ℝ) (hT : 0 < T)
    (u : DigitalSignal) (y : AnalogSignal)
    (hcont : ContinuousOn y (Set.Ici 0))
    (hode : ∀ (n : ℕ) (t : ℝ),
      sampleTime T n < t → t < sampleTime T (n + 1) →
        HasDerivAt y ((u n - y t) / τ) t)
    (n : ℕ) :
    y (sampleTime T (n + 1)) =
      onePoleStep (1 - Real.exp (-T / τ)) (u n) (y (sampleTime T n)) := by
  let start := sampleTime T n
  let shifted : AnalogSignal := fun t => y (start + t)
  have hstart : 0 ≤ start := mul_nonneg (Nat.cast_nonneg n) (le_of_lt hT)
  have hc : ContinuousOn shifted (Set.Icc 0 T) := by
    apply hcont.comp' (continuous_const.add continuous_id).continuousOn
    intro t ht
    exact add_nonneg hstart ht.1
  have hd : ∀ t ∈ Set.Ioo 0 T, HasDerivAt shifted ((u n - shifted t) / τ) t := by
    intro t ht
    have hlo : sampleTime T n < start + t := lt_add_of_pos_right start ht.1
    have hhi : start + t < sampleTime T (n + 1) := by
      rw [sampleTime_succ]
      simpa [start, add_comm] using add_lt_add_left ht.2 start
    simpa [shifted] using (hode n (start + t) hlo hhi).comp_const_add start t
  have hstep := onePole_implements_interval T τ (u n) (y start) hT shifted hc
    (by simp [shifted]) hd
  simpa [shifted, start, sampleTime_succ] using hstep

/-- Every analog solution with the specified initial state has exactly the
same samples as the recurrence, for arbitrary (including changing) input. -/
theorem analog_matches_digital (T τ : ℝ) (hT : 0 < T)
    (u : DigitalSignal) (initial : ℝ) (y : AnalogSignal)
    (hy : AnalogOnePoleLowPass T τ u initial y) :
    ExactAtSamples T y (digitalRun (1 - Real.exp (-T / τ)) u initial) := by
  rcases hy with ⟨hinit, hcont, hode⟩
  intro n
  change y (sampleTime T n) = digitalRun (1 - Real.exp (-T / τ)) u initial n
  induction n with
  | zero => simpa using hinit
  | succ n ih =>
      rw [onePole_step_at T τ hT u y hcont hode n, ih, digitalRun_succ]

theorem implements (T τ : ℝ) (hT : 0 < T) :
    Implements T (analog τ) (digital (1 - Real.exp (-T / τ))) := by
  intro u initial y hy
  exact analog_matches_digital T τ hT u initial y hy

end DSP.OnePoleZOHLowPass

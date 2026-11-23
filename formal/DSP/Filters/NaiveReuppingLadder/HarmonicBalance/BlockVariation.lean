import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.SecondVariation
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Attraction

/-! Compose first and mixed second variations over the original sample run.
The derivative frame carries two directions and their mixed variation, so
curvature of the initial chart is retained. This is proof-side bookkeeping;
the value component calls the existing step, including its feedback delay. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance

def Variation.history (v : Variation) : Fin 4 → ℝ := ![v.stage1,v.stage2,v.stage3,v.stage4]

structure DerivativeFrame where
  value : Fin 4 → ℝ
  left : Fin 4 → ℝ
  right : Fin 4 → ℝ
  mixed : Fin 4 → ℝ

namespace DerivativeFrame

def advance (s : NaiveReuppingLadder ℝ) (frame : DerivativeFrame) (u : Sample ℝ) : DerivativeFrame :=
  let next := step (withHistory s frame.value) u
  let left := firstVariation s next frame.left 0
  let right := firstVariation s next frame.right 0
  let mixed := mixedVariation s next left right frame.mixed 0
  ⟨audioHistory next, left.history, right.history, mixed.history⟩

/-- The value trajectory is exactly the audio projection of the sample map. -/
theorem run_value (s : NaiveReuppingLadder ℝ) (u : ℕ → Sample ℝ)
    (frame : DerivativeFrame) (n : ℕ) :
    (Step.states (advance s) u frame n).value =
      Step.states (audioStep s) u frame.value n := by
  induction n with
  | zero => rfl
  | succ n ih =>
    change audioStep s (Step.states (advance s) u frame n).value (u n) = _
    rw [ih, Step.states_succ]

/-- Along a parameter curve, value differentiates to left, and right
differentiates to mixed. No derivative of the left field is assumed. -/
def Differentiates (frame : ℝ → DerivativeFrame) (t : ℝ) : Prop :=
  (∀ j, HasDerivAt (fun r => (frame r).value j) ((frame t).left j) t) ∧
  (∀ j, HasDerivAt (fun r => (frame r).right j) ((frame t).mixed j) t)

theorem advance_differentiates (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (u : Sample ℝ) (frame : ℝ → DerivativeFrame) (t : ℝ) (h : Differentiates frame t) :
    Differentiates (fun r => advance s (frame r) u) t := by
  have hfirst := step_firstVariation s hd (fun r => (frame r).value) (fun _ => u.x)
    t (frame t).left 0 h.1 (hasDerivAt_const t u.x)
  have hsecond := step_mixedVariation s hd (fun r => (frame r).value)
    (fun r => (frame r).right) (fun _ => u.x) (fun _ => 0) t
    (frame t).left (frame t).mixed 0 0 h.1 (hasDerivAt_const t u.x)
    h.2 (hasDerivAt_const t 0)
  constructor
  · intro j
    fin_cases j
    · exact hfirst.1
    · exact hfirst.2.1
    · exact hfirst.2.2.1
    · exact hfirst.2.2.2.1
  · intro j
    fin_cases j
    · exact hsecond.1
    · exact hsecond.2.1
    · exact hsecond.2.2.1
    · exact hsecond.2.2.2.1

/-- Any finite block inherits the exact derivatives, including a curved
initial profile. The block length and fixed forcing sequence are arbitrary. -/
theorem run_differentiates (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (u : ℕ → Sample ℝ) (frame : ℝ → DerivativeFrame) (t : ℝ)
    (h : Differentiates frame t) (n : ℕ) :
    Differentiates (fun r => Step.states (advance s) u (frame r) n) t := by
  induction n with
  | zero => exact h
  | succ n ih => exact advance_differentiates s hd (u n) _ t ih

theorem run_hasDerivAt (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (u : ℕ → Sample ℝ) (frame : ℝ → DerivativeFrame) (t : ℝ)
    (h : Differentiates frame t) (n : ℕ) (j : Fin 4) :
    HasDerivAt (fun r => Step.states (audioStep s) u (frame r).value n j)
      ((Step.states (advance s) u (frame t) n).left j) t := by
  simpa only [run_value] using (run_differentiates s hd u frame t h n).1 j

def observationFirst (s : NaiveReuppingLadder ℝ) (value direction : Fin 4 → ℝ) : ℝ :=
  (s.baseFeedback+1)*NormalizedTanh.slope s.saturation (value 3)*direction 3

def observationMixed (s : NaiveReuppingLadder ℝ) (frame : DerivativeFrame) : ℝ :=
  (s.baseFeedback+1)*(NormalizedTanh.curvature s.saturation (frame.value 3)*
    frame.left 3*frame.right 3+NormalizedTanh.slope s.saturation (frame.value 3)*frame.mixed 3)

theorem observation_hasDerivAt (s : NaiveReuppingLadder ℝ) (frame : ℝ → DerivativeFrame)
    (t : ℝ) (h : Differentiates frame t) :
    HasDerivAt (fun r => audioObservation s (frame r).value)
      (observationFirst s (frame t).value (frame t).left) t := by
  exact (((NormalizedTanh.hasDerivAt_apply s.saturation ((frame t).value 3)).comp t
    (h.1 3)).mul_const (s.baseFeedback+1)).congr_deriv
    (by dsimp [observationFirst]; ring)

theorem observationFirst_hasDerivAt (s : NaiveReuppingLadder ℝ) (frame : ℝ → DerivativeFrame)
    (t : ℝ) (h : Differentiates frame t) :
    HasDerivAt (fun r => observationFirst s (frame r).value (frame r).right)
      (observationMixed s (frame t)) t := by
  exact ((((NormalizedTanh.hasDerivAt_slope s.saturation ((frame t).value 3)).comp t
    (h.1 3)).const_mul (s.baseFeedback+1)).mul (h.2 3)).congr_deriv
    (by dsimp [observationMixed]; ring)

end DerivativeFrame
end DSP.NaiveReuppingLadder.HarmonicBalance

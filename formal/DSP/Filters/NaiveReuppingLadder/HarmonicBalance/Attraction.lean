import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Variation
import DSP.Analysis.PeriodicOrbit

/-! Proof-side audio projection of the EXISTING sample step. The detector is
disabled but still advances in the full processor; projecting it away does not
change any of the four audio histories. A block-map contraction certificate
therefore concerns the sample implementation, not a separate oscillator model. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance

def audioHistory (s : NaiveReuppingLadder ℝ) : Fin 4 → ℝ :=
  ![s.stage1, s.stage2, s.stage3, s.stage4]

def audioStep (s : NaiveReuppingLadder ℝ) (x : Fin 4 → ℝ) (u : Sample ℝ) : Fin 4 → ℝ :=
  audioHistory (step (withHistory s x) u)

/-- Exactly the coefficient information needed by the audio projection. -/
def SameAudioParameters (s t : NaiveReuppingLadder ℝ) : Prop :=
  t.alpha = s.alpha ∧ t.saturation = s.saturation ∧
    t.baseFeedback = s.baseFeedback ∧ t.dampingScale = 0

theorem step_sameAudioParameters (s t : NaiveReuppingLadder ℝ) (u : Sample ℝ)
    (h : SameAudioParameters s t) : SameAudioParameters s (step t u) := h

theorem states_sameAudioParameters (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (u : ℕ → Sample ℝ) (n : ℕ) : SameAudioParameters s (Step.states step u s n) := by
  induction n with
  | zero => exact ⟨rfl, rfl, rfl, hd⟩
  | succ n ih => exact step_sameAudioParameters s _ _ ih

theorem audioStep_projection (s t : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (h : SameAudioParameters s t) (u : Sample ℝ) :
    audioHistory (step t u) = audioStep s (audioHistory t) u := by
  rcases h with ⟨ha, hs, hk, ht⟩
  simp [audioHistory, audioStep, step, withHistory, effectiveFeedback, hd, ht, ha, hs, hk]

/-- Full sample histories agree for every finite time, even though the
original implementation continues updating its unused detector histories. -/
theorem states_audioHistory (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (u : ℕ → Sample ℝ) (n : ℕ) :
    audioHistory (Step.states step u s n) = Step.states (audioStep s) u (audioHistory s) n := by
  induction n with
  | zero => rfl
  | succ n ih =>
    rw [Step.states_succ, audioStep_projection s _ hd (states_sameAudioParameters s hd u n), ih, Step.states_succ]

/-- The final saturator and output gain belong to the measured response. -/
def audioObservation (s : NaiveReuppingLadder ℝ) (x : Fin 4 → ℝ) : ℝ :=
  NormalizedTanh.apply s.saturation (x 3) * (s.baseFeedback+1)

theorem audioObservation_continuous (s : NaiveReuppingLadder ℝ) :
    Continuous (audioObservation s) :=
  (((NormalizedTanh.contDiff_apply s.saturation).continuous).comp
    (continuous_apply 3)).mul_const _

theorem step_output_projection (s t : NaiveReuppingLadder ℝ)
    (h : SameAudioParameters s t) (u : Sample ℝ) :
    (step t u).output = audioObservation s (audioHistory (step t u)) := by
  rcases h with ⟨_, hs, hk, hd⟩
  simp [step, audioObservation, audioHistory, effectiveFeedback, hs, hk, hd]

/-- Output sample n observes the history AFTER input sample n. This identity
prevents a one-sample phase mistake when using periodic measurement bounds. -/
theorem states_output_projection (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (u : ℕ → Sample ℝ) (n : ℕ) :
    (Step.states step u s (n+1)).output =
      audioObservation s (Step.states (audioStep s) u (audioHistory s) (n+1)) := by
  rw [← states_audioHistory s hd u (n+1), Step.states_succ]
  exact step_output_projection s _ (states_sameAudioParameters s hd u n) _

theorem audioStep_continuous (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (u : Sample ℝ) : Continuous (fun x => audioStep s x u) := by
  unfold audioStep audioHistory
  simp only [step, withHistory, effectiveFeedback, hd, zero_mul, zero_add, div_one,
    OnePoleZOHLowPass.onePoleStep_eq, fma_eq]
  have hs : Continuous (NormalizedTanh.apply s.saturation) :=
    (NormalizedTanh.contDiff_apply s.saturation).continuous
  fun_prop

theorem audioPrefix_continuous (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (u : ℕ → Sample ℝ) (n : ℕ) :
    Continuous (fun x => Step.states (audioStep s) u x n) := by
  induction n with
  | zero => exact continuous_id
  | succ n ih => exact (audioStep_continuous s hd (u n)).comp ih

end DSP.NaiveReuppingLadder.HarmonicBalance

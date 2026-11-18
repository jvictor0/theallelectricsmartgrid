import DSP.Filters.NaiveReuppingLadder.FloorFree
import DSP.Filters.BandPassedEnvelopeFollower.Convergence

/-! A proof-only floor-free trajectory. All component steps are the existing
Lean implementations. The equality with the production step below pins down
the sample timing and every operation except the choice of feedback coefficient.
This module is not exported to C++ and does not assert closed-loop convergence. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.FloorFree

/-- Removing the floor from the original excess-power expression. At exactly
zero input power its algebraic extension uses baseFeedback (proved below),
including at reset. This is an explicit analysis-model convention only. -/
def effectiveFeedback (state : NaiveReuppingLadder ℝ) : ℝ :=
  state.baseFeedback / (1 + state.dampingScale *
    (max 0 (state.feedbackEnvelope.power - state.referencePowerGain * state.inputEnvelope.power) /
      state.inputEnvelope.power))

theorem effectiveFeedback_eq_fromPowers (state : NaiveReuppingLadder ℝ)
    (hp : 0 < state.inputEnvelope.power) :
    effectiveFeedback state = fromPowers state.baseFeedback state.dampingScale
      state.referencePowerGain state.inputEnvelope.power state.feedbackEnvelope.power :=
  (fromPowers_eq _ _ _ _ _ hp).symm

theorem effectiveFeedback_zero_power (state : NaiveReuppingLadder ℝ)
    (hp : state.inputEnvelope.power = 0) : effectiveFeedback state = state.baseFeedback := by
  simp [effectiveFeedback, hp]

/-- The unchanged sample operations with an explicit feedback coefficient.
The fourth-stage history and detector powers belong to the previous sample. -/
def advance (state : NaiveReuppingLadder ℝ) (kEff : ℝ) (input : Sample ℝ) : NaiveReuppingLadder ℝ :=
  let drive := input.x - kEff * state.stage4
  let stage1 := OnePoleZOHLowPass.onePoleStep state.alpha drive state.stage1
  let stage2 := OnePoleZOHLowPass.onePoleStep state.alpha (NormalizedTanh.apply state.saturation stage1) state.stage2
  let stage3 := OnePoleZOHLowPass.onePoleStep state.alpha (NormalizedTanh.apply state.saturation stage2) state.stage3
  let stage4 := OnePoleZOHLowPass.onePoleStep state.alpha (NormalizedTanh.apply state.saturation stage3) state.stage4
  {state with
    stage1, stage2, stage3, stage4, input := drive, kEff
    inputEnvelope := state.inputEnvelope.step input
    feedbackEnvelope := state.feedbackEnvelope.step ⟨stage4⟩
    output := NormalizedTanh.apply state.saturation stage4 * (kEff + 1)}

theorem runtime_step_eq_advance (state : NaiveReuppingLadder ℝ) (input : Sample ℝ) :
    NaiveReuppingLadder.step state input =
      advance state (NaiveReuppingLadder.effectiveFeedback state) input := rfl

def step (state : NaiveReuppingLadder ℝ) (input : Sample ℝ) : NaiveReuppingLadder ℝ :=
  advance state (effectiveFeedback state) input

abbrev trajectory (state : NaiveReuppingLadder ℝ) (input : ℕ → ℝ) :=
  Step.states step (fun n => ⟨input n⟩) state

def output (state : NaiveReuppingLadder ℝ) (input : ℕ → ℝ) (n : ℕ) : ℝ :=
  (trajectory state input (n + 1)).output

def inputPowerOutput (state : NaiveReuppingLadder ℝ) (input : ℕ → ℝ) (n : ℕ) : ℝ :=
  (trajectory state input (n + 1)).inputEnvelope.power

/-- The input detector is independent of the adaptive loop. Its convergence
proof therefore applies here without a hypothesis about the ladder's output. -/
theorem trajectory_inputEnvelope (state : NaiveReuppingLadder ℝ) (input : ℕ → ℝ) (n : ℕ) :
    (trajectory state input n).inputEnvelope =
      BandPassedEnvelopeFollower.trajectory state.inputEnvelope input n := by
  induction n with
  | zero => rfl
  | succ n ih =>
    change BandPassedEnvelopeFollower.step (trajectory state input n).inputEnvelope ⟨input n⟩ = _
    rw [ih]
    rfl

theorem inputPowerOutput_eq (state : NaiveReuppingLadder ℝ) (input : ℕ → ℝ) (n : ℕ) :
    inputPowerOutput state input n = BandPassedEnvelopeFollower.powerOutput state.inputEnvelope input n := by
  simp only [inputPowerOutput, BandPassedEnvelopeFollower.powerOutput, trajectory_inputEnvelope]

end DSP.NaiveReuppingLadder.FloorFree

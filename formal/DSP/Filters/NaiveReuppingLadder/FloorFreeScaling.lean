import DSP.Filters.NaiveReuppingLadder.FloorFreeDynamics

/-! Amplitude scaling of the full floor-free state. Audio and bandpass histories
scale by a; detector powers by a^2; coefficients do not scale. The model with
saturators replaced by their zero-input slope remains an adaptive nonlinear
system. No long-time limit exchange is assumed here. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.FloorFree

def scaleSVF (a : ℝ) (s : TPTSVF ℝ) : TPTSVF ℝ :=
  {s with
    ic1 := a * s.ic1, ic2 := a * s.ic2,
    lowPass := a * s.lowPass, bandPass := a * s.bandPass,
    highPass := a * s.highPass, notch := a * s.notch, allPass := a * s.allPass}

def scaleEnvelope (a : ℝ) (s : BandPassedEnvelopeFollower ℝ) : BandPassedEnvelopeFollower ℝ :=
  {s with stage1 := scaleSVF a s.stage1, stage2 := scaleSVF a s.stage2, power := a ^ 2 * s.power}

def scaleHistory (a : ℝ) (s : NaiveReuppingLadder ℝ) : NaiveReuppingLadder ℝ :=
  {s with
    stage1 := a * s.stage1, stage2 := a * s.stage2, stage3 := a * s.stage3,
    stage4 := a * s.stage4, input := a * s.input, output := a * s.output,
    inputEnvelope := scaleEnvelope a s.inputEnvelope,
    feedbackEnvelope := scaleEnvelope a s.feedbackEnvelope}

theorem svf_step_scale (a : ℝ) (s : TPTSVF ℝ) (input : ℝ) :
    TPTSVF.step (scaleSVF a s) ⟨a * input⟩ = scaleSVF a (TPTSVF.step s ⟨input⟩) := by
  simp only [scaleSVF, TPTSVF.step, fma_eq]
  congr 1 <;> ring

theorem envelope_step_scale (a : ℝ) (s : BandPassedEnvelopeFollower ℝ) (input : ℝ) :
    BandPassedEnvelopeFollower.step (scaleEnvelope a s) ⟨a * input⟩ =
      scaleEnvelope a (BandPassedEnvelopeFollower.step s ⟨input⟩) := by
  have hb (v : TPTSVF ℝ) : (scaleSVF a v).bandPass = a * v.bandPass := rfl
  simp only [BandPassedEnvelopeFollower.step, TPTSVF4Pole.stepBandPass, scaleEnvelope,
    svf_step_scale, hb, OnePoleZOHLowPass.relax]
  congr 1
  ring

theorem onePole_scale (a alpha input previous : ℝ) :
    OnePoleZOHLowPass.onePoleStep alpha (a * input) (a * previous) =
      a * OnePoleZOHLowPass.onePoleStep alpha input previous := by
  simp only [OnePoleZOHLowPass.onePoleStep_eq, fma_eq]
  ring

theorem effectiveFeedback_scale (a : ℝ) (ha : a ≠ 0) (s : NaiveReuppingLadder ℝ) :
    effectiveFeedback (scaleHistory a s) = effectiveFeedback s := by
  have hexcess : a ^ 2 * s.feedbackEnvelope.power - s.referencePowerGain * (a ^ 2 * s.inputEnvelope.power) =
      a ^ 2 * (s.feedbackEnvelope.power - s.referencePowerGain * s.inputEnvelope.power) := by ring
  simp only [effectiveFeedback, scaleHistory, scaleEnvelope, hexcess]
  rw [← mul_zero (a ^ 2), ← mul_max_of_nonneg _ _ (sq_nonneg a),
    mul_div_mul_left _ _ (pow_ne_zero 2 ha)]
  simp only [mul_zero]

/-- General conjugacy, also usable with amplitude-normalized tanh. -/
theorem advanceWith_scale (a : ℝ) (s : NaiveReuppingLadder ℝ) (k input : ℝ)
    (saturate scaledSaturate : ℝ → ℝ)
    (hs : ∀ x, scaledSaturate (a * x) = a * saturate x) :
    advanceWith scaledSaturate (scaleHistory a s) k ⟨a * input⟩ =
      scaleHistory a (advanceWith saturate s k ⟨input⟩) := by
  have hd : a * input - k * (a * s.stage4) = a * (input - k * s.stage4) := by ring
  simp only [advanceWith, scaleHistory, hd, onePole_scale, hs, envelope_step_scale]
  congr 1
  ring

theorem scaleHistory_reset (a : ℝ) (s : NaiveReuppingLadder ℝ) :
    scaleHistory a (NaiveReuppingLadder.reset s) = NaiveReuppingLadder.reset s := by
  simp [scaleHistory, scaleEnvelope, scaleSVF, NaiveReuppingLadder.reset,
    BandPassedEnvelopeFollower.reset, BandPassedEnvelopeFollower.resetSVF]

namespace SlopeModel

def step (s : NaiveReuppingLadder ℝ) (input : Sample ℝ) : NaiveReuppingLadder ℝ :=
  advanceWith (fun x => NormalizedTanh.slopeZero s.saturation * x) s (effectiveFeedback s) input

abbrev trajectory (s : NaiveReuppingLadder ℝ) (input : ℕ → ℝ) :=
  Step.states step (fun n => ⟨input n⟩) s

def output (s : NaiveReuppingLadder ℝ) (input : ℕ → ℝ) (n : ℕ) : ℝ :=
  (trajectory s input (n + 1)).output

theorem step_scale (a : ℝ) (ha : a ≠ 0) (s : NaiveReuppingLadder ℝ) (input : ℝ) :
    step (scaleHistory a s) ⟨a * input⟩ = scaleHistory a (step s ⟨input⟩) := by
  unfold step
  rw [effectiveFeedback_scale a ha]
  exact advanceWith_scale a s _ input _ _ (fun x => by dsimp only [scaleHistory]; ring)

theorem trajectory_scale (a : ℝ) (ha : a ≠ 0) (s : NaiveReuppingLadder ℝ)
    (input : ℕ → ℝ) (n : ℕ) :
    trajectory (scaleHistory a s) (fun n => a * input n) n =
      scaleHistory a (trajectory s input n) := by
  induction n with
  | zero => rfl
  | succ n ih =>
    change step (trajectory (scaleHistory a s) (fun n => a * input n) n) ⟨a * input n⟩ = _
    rw [ih, step_scale a ha]
    rfl

theorem output_scale (a : ℝ) (ha : a ≠ 0) (s : NaiveReuppingLadder ℝ)
    (input : ℕ → ℝ) (n : ℕ) :
    output (scaleHistory a s) (fun n => a * input n) n = a * output s input n := by
  unfold output
  rw [trajectory_scale a ha]
  rfl

/-- Shrinking a stimulus does not reduce the relative modulation, harmonics,
or oscillations of this adaptive slope model. This is exact at every sample. -/
theorem output_scale_from_reset (a : ℝ) (ha : a ≠ 0) (s : NaiveReuppingLadder ℝ)
    (input : ℕ → ℝ) (n : ℕ) :
    output (NaiveReuppingLadder.reset s) (fun n => a * input n) n =
      a * output (NaiveReuppingLadder.reset s) input n := by
  simpa only [scaleHistory_reset] using output_scale a ha (NaiveReuppingLadder.reset s) input n

end SlopeModel
end DSP.NaiveReuppingLadder.FloorFree

import DSP.Filters.NaiveReuppingLadder
import DSP.Filters.Tuning.History
import DSP.Nonlinear.Real

/-! Structural contracts for the implementation. No transfer, small-signal,
nonlinear stability, or rational/transcendental equivalence is asserted here. -/
namespace DSP
open Channels

/-- Algebraic equivalence only: the two evaluation orders can round differently. -/
theorem OnePoleZOHLowPass.relax_eq_onePoleStep {R : Type} [Field R]
    (alpha input state : R) :
    OnePoleZOHLowPass.relax alpha input state = OnePoleZOHLowPass.onePoleStep alpha input state := by
  simp only [OnePoleZOHLowPass.relax, OnePoleZOHLowPass.onePoleStep_eq, fma_eq]
  ring

namespace NormalizedTanh
deriving instance Functor for Coefficients
instance logicalSpecFunctor : Functor LogicalSpec where
  map f s := ⟨f s.gain⟩
end NormalizedTanh
namespace BandPassedEnvelopeFollower
instance logicalSpecFunctor : Functor LogicalSpec where
  map f s := ⟨f s.integratorGain, f s.powerAlpha⟩
end BandPassedEnvelopeFollower
namespace NaiveReuppingLadder
instance logicalSpecFunctor : Functor LogicalSpec where
  map f s := ⟨f s.cutoff, f s.resonance, f s.gain⟩
end NaiveReuppingLadder
deriving instance Functor for NormalizedTanh
deriving instance Functor for BandPassedEnvelopeFollower
deriving instance Functor for NaiveReuppingLadder

namespace NormalizedTanh
theorem apply_real (gain x : ℝ) :
    apply (prepare ⟨gain⟩) x = Real.tanh (x * gain) / Real.tanh gain := rfl

theorem apply_zero (c : Coefficients ℝ) : apply c 0 = 0 := by
  simp [apply]

theorem apply_one (gain : ℝ) (h : Real.tanh gain ≠ 0) :
    apply (prepare ⟨gain⟩) 1 = 1 := by
  simp [apply, prepare, h]

theorem computeCoefficients_preserves_output {R : Type} [Tanh R]
    (spec : LogicalSpec R) (s : NormalizedTanh R) :
    (computeCoefficients spec s).output = s.output := rfl

theorem computeCoefficients_idempotent {R : Type} [Tanh R]
    (spec : LogicalSpec R) (s : NormalizedTanh R) :
    computeCoefficients spec (computeCoefficients spec s) = computeCoefficients spec s := rfl

theorem step_channel {R : Type} [Mul R] [Div R] [Tanh R] {n : ℕ}
    (s : NormalizedTanh (Fin n → R)) (x : Sample (Fin n → R)) (i : Fin n) :
    channel i (step s x) = step (channel i s) ⟨x.x i⟩ := rfl

theorem computeCoefficients_channel {R : Type} [Tanh R] {n : ℕ}
    (spec : LogicalSpec (Fin n → R)) (s : NormalizedTanh (Fin n → R)) (i : Fin n) :
    channel i (computeCoefficients spec s) = computeCoefficients (channel i spec) (channel i s) := rfl
end NormalizedTanh

namespace BandPassedEnvelopeFollower
def history {R : Type} (s : BandPassedEnvelopeFollower R) : List R :=
  TPTSVF.history s.stage1 ++ TPTSVF.history s.stage2 ++ [s.power]

def coefficientFields {R : Type} (s : BandPassedEnvelopeFollower R) : List R :=
  [s.stage1.k, s.stage1.a1, s.stage1.a2, s.stage1.a3,
    s.stage2.k, s.stage2.a1, s.stage2.a2, s.stage2.a3, s.powerAlpha]

variable {R : Type}

theorem computeCoefficients_preserves_history [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] (spec : LogicalSpec R) (s : BandPassedEnvelopeFollower R) :
    history (computeCoefficients spec s) = history s := rfl

theorem computeCoefficients_idempotent [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] (spec : LogicalSpec R) (s : BandPassedEnvelopeFollower R) :
    computeCoefficients spec (computeCoefficients spec s) = computeCoefficients spec s := rfl

theorem reset_preserves_coefficients [OfNat R 0] (s : BandPassedEnvelopeFollower R) :
    coefficientFields (reset s) = coefficientFields s := rfl

theorem reset_clears_history [OfNat R 0] (s : BandPassedEnvelopeFollower R) :
    history (reset s) = (history s).map (fun _ => 0) := rfl

theorem reset_idempotent [OfNat R 0] (s : BandPassedEnvelopeFollower R) : reset (reset s) = reset s := rfl

theorem step_channel [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R] {n : ℕ} (s : BandPassedEnvelopeFollower (Fin n → R))
    (x : Sample (Fin n → R)) (i : Fin n) :
    channel i (step s x) = step (channel i s) ⟨x.x i⟩ := rfl

theorem computeCoefficients_channel [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] {n : ℕ} (spec : LogicalSpec (Fin n → R))
    (s : BandPassedEnvelopeFollower (Fin n → R)) (i : Fin n) :
    channel i (computeCoefficients spec s) = computeCoefficients (channel i spec) (channel i s) := rfl

theorem reset_channel [OfNat R 0] {n : ℕ} (s : BandPassedEnvelopeFollower (Fin n → R)) (i : Fin n) :
    channel i (reset s) = reset (channel i s) := rfl
end BandPassedEnvelopeFollower

namespace NaiveReuppingLadder
def history {R : Type} (s : NaiveReuppingLadder R) : List R :=
  [s.stage1, s.stage2, s.stage3, s.stage4, s.input, s.output, s.kEff] ++
    s.inputEnvelope.history ++ s.feedbackEnvelope.history

def coefficientFields {R : Type} (s : NaiveReuppingLadder R) : List R :=
  [s.alpha, s.saturation.gain, s.saturation.tanhGain, s.cutoff, s.feedback,
    s.criticalFeedback, s.kSafe, s.baseFeedback, s.referencePowerGain, s.dampingScale] ++
    s.inputEnvelope.coefficientFields ++ s.feedbackEnvelope.coefficientFields

variable {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfScientific R]
  [Add R] [Sub R] [Neg R] [Mul R] [Div R] [FMA R] [IfLess R] [Tanh R]

theorem step_feedback_delay (s : NaiveReuppingLadder R) (x : Sample R) :
    (step s x).input = x.x - effectiveFeedback s * s.stage4 := rfl

theorem step_uses_previous_powers (s : NaiveReuppingLadder R) (x : Sample R) :
    (step s x).kEff = s.baseFeedback / (s.dampingScale *
      (maxValue 0 (s.feedbackEnvelope.power - s.referencePowerGain * s.inputEnvelope.power) /
        (s.inputEnvelope.power + powerFloor)) + 1) := rfl

theorem step_input_detector (s : NaiveReuppingLadder R) (x : Sample R) :
    (step s x).inputEnvelope = s.inputEnvelope.step x := rfl

theorem step_feedback_detector (s : NaiveReuppingLadder R) (x : Sample R) :
    (step s x).feedbackEnvelope = s.feedbackEnvelope.step ⟨(step s x).stage4⟩ := rfl

theorem step_preserves_coefficients (s : NaiveReuppingLadder R) (x : Sample R) :
    coefficientFields (step s x) = coefficientFields s := rfl

omit [OfNat R 0] [Neg R] in
theorem computeCoefficients_preserves_history [Trig R] [SquareRoot R]
    (spec : LogicalSpec R) (s : NaiveReuppingLadder R) :
    history (computeCoefficients spec s) = history s := rfl

omit [OfNat R 0] [Neg R] in
theorem computeCoefficients_idempotent [Trig R] [SquareRoot R]
    (spec : LogicalSpec R) (s : NaiveReuppingLadder R) :
    computeCoefficients spec (computeCoefficients spec s) = computeCoefficients spec s := rfl

omit [OfNat R 1] [OfNat R 2] [OfScientific R] [Add R] [Sub R] [Neg R] [Mul R]
  [Div R] [FMA R] [IfLess R] [Tanh R] in
theorem reset_preserves_coefficients (s : NaiveReuppingLadder R) :
    coefficientFields (reset s) = coefficientFields s := rfl

omit [OfNat R 1] [OfNat R 2] [OfScientific R] [Add R] [Sub R] [Neg R] [Mul R]
  [Div R] [FMA R] [IfLess R] [Tanh R] in
theorem reset_clears_history (s : NaiveReuppingLadder R) :
    history (reset s) = (history s).map (fun _ => 0) := rfl

omit [OfNat R 1] [OfNat R 2] [OfScientific R] [Add R] [Sub R] [Neg R] [Mul R]
  [Div R] [FMA R] [IfLess R] [Tanh R] in
theorem reset_idempotent (s : NaiveReuppingLadder R) : reset (reset s) = reset s := rfl

theorem step_channel {n : ℕ} (s : NaiveReuppingLadder (Fin n → R))
    (x : Sample (Fin n → R)) (i : Fin n) :
    channel i (step s x) = step (channel i s) ⟨x.x i⟩ := rfl

omit [OfNat R 0] [Neg R] in
theorem computeCoefficients_channel [Trig R] [SquareRoot R] {n : ℕ}
    (spec : LogicalSpec (Fin n → R)) (s : NaiveReuppingLadder (Fin n → R)) (i : Fin n) :
    channel i (computeCoefficients spec s) = computeCoefficients (channel i spec) (channel i s) := rfl

omit [OfNat R 1] [OfNat R 2] [OfScientific R] [Add R] [Sub R] [Neg R] [Mul R]
  [Div R] [FMA R] [IfLess R] [Tanh R] in
theorem reset_channel {n : ℕ} (s : NaiveReuppingLadder (Fin n → R)) (i : Fin n) :
    channel i (reset s) = reset (channel i s) := rfl
end NaiveReuppingLadder
end DSP

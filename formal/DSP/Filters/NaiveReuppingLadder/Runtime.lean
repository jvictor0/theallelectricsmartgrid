import DSP.Filters.NaiveReuppingLadder
import DSP.Filters.TPTSVF.Runtime

namespace DSP.NaiveReuppingLadder.Runtime

variable {R : Type}

structure DetectorState (R : Type) where
  stage1 : TPTSVF.Runtime.State R
  stage2 : TPTSVF.Runtime.State R
  power : R
  powerAlpha : R

def DetectorState.save (s : BandPassedEnvelopeFollower R) : DetectorState R :=
  ⟨TPTSVF.Runtime.save s.stage1, TPTSVF.Runtime.save s.stage2, s.power, s.powerAlpha⟩

def DetectorState.restore [OfNat R 0] (s : DetectorState R) : BandPassedEnvelopeFollower R :=
  ⟨TPTSVF.Runtime.restore s.stage1, TPTSVF.Runtime.restore s.stage2, s.power, s.powerAlpha⟩

structure State (R : Type) where
  alpha : R
  saturation : NormalizedTanh.Coefficients R
  stage1 : R
  stage2 : R
  stage3 : R
  stage4 : R
  inputEnvelope : DetectorState R
  feedbackEnvelope : DetectorState R
  baseFeedback : R
  referencePowerGain : R
  dampingScale : R

def save (s : NaiveReuppingLadder R) : State R :=
  ⟨s.alpha, s.saturation, s.stage1, s.stage2, s.stage3, s.stage4,
    DetectorState.save s.inputEnvelope, DetectorState.save s.feedbackEnvelope,
    s.baseFeedback, s.referencePowerGain, s.dampingScale⟩

def restore [OfNat R 0] (s : State R) : NaiveReuppingLadder R :=
  ⟨s.alpha, s.saturation, 0, 0, s.stage1, s.stage2, s.stage3, s.stage4, 0, 0, 0,
    s.inputEnvelope.restore, s.feedbackEnvelope.restore, 0, 0,
    s.baseFeedback, s.referencePowerGain, s.dampingScale⟩

section Processing
variable [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfScientific R]
  [Add R] [Sub R] [Neg R] [Mul R] [Div R] [FMA R] [IfLess R] [Tanh R]

def model : Processor (NaiveReuppingLadder R) (Sample R) (Sample R) :=
  fun s x => let next := s.step x; (next, ⟨next.output⟩)

def process : Processor (State R) (Sample R) (Sample R) :=
  Processor.compact model save restore

theorem simulation : Processor.Simulation (model (R := R)) process (fun s m => m = save s) := by
  intro s m hm x
  subst m
  exact ⟨rfl, rfl⟩

/-- Capture the applied controls for the finite noise response. The table is
indexed by requested resonance, since detector feedback evolves during the trial. -/
def prepareCoefficients [Trig R] [SquareRoot R] (spec : LogicalSpec R) (s : State R) :
    State R × LogicalSpec R :=
  let next := NaiveReuppingLadder.computeCoefficients spec (restore s)
  (save next, {spec with cutoff := next.cutoff})

omit [Neg R] in
theorem prepare_save [Trig R] [SquareRoot R] (spec : LogicalSpec R) (s : NaiveReuppingLadder R) :
    prepareCoefficients spec (save s) =
      let next := NaiveReuppingLadder.computeCoefficients spec s
      (save next, {spec with cutoff := next.cutoff}) := rfl
end Processing

def reset [OfNat R 0] (s : State R) : State R × Sample R :=
  let next := NaiveReuppingLadder.reset (restore s)
  (save next, ⟨next.output⟩)

theorem reset_save [OfNat R 0] (s : NaiveReuppingLadder R) :
    reset (save s) = (save (NaiveReuppingLadder.reset s), ⟨0⟩) := rfl

end DSP.NaiveReuppingLadder.Runtime

import DSP.Filters.NormalizedTanh
import DSP.Filters.BandPassedEnvelopeFollower
import DSP.Filters.OnePoleZOHLowPass

namespace DSP

/-- The app's discrete nonlinear ladder. Stage fields are UNSATURATED Euler
states; feedback reads the OLD fourth stage. Detector powers are also read
before either detector advances. This is not a linear transfer model. -/
structure NaiveReuppingLadder (R : Type) where
  alpha : R
  saturation : NormalizedTanh.Coefficients R
  cutoff : R
  feedback : R
  stage1 : R
  stage2 : R
  stage3 : R
  stage4 : R
  input : R
  output : R
  kEff : R
  inputEnvelope : BandPassedEnvelopeFollower R
  feedbackEnvelope : BandPassedEnvelopeFollower R
  criticalFeedback : R
  kSafe : R
  baseFeedback : R
  referencePowerGain : R
  dampingScale : R

namespace NaiveReuppingLadder

structure LogicalSpec (R : Type) where
  cutoff : R
  resonance : R
  gain : R

variable {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfScientific R]
  [Add R] [Sub R] [Neg R] [Mul R] [Div R] [FMA R] [IfLess R] [Tanh R]

/-- Preserve the handwritten float constant even in a double specialization. -/
def dampingEpsilon : R := 0.0000999999974737875163555145263671875
/-- The app squares its float epsilon before widening to Number. -/
def powerFloor : R := 0.0000000099999990510468705906532704830169677734375

def effectiveFeedback (state : NaiveReuppingLadder R) : R :=
  let excess := maxValue 0
    (state.feedbackEnvelope.power - state.referencePowerGain * state.inputEnvelope.power)
    / (state.inputEnvelope.power + powerFloor)
  state.baseFeedback / (state.dampingScale * excess + 1)

def step (state : NaiveReuppingLadder R) (input : Sample R) : NaiveReuppingLadder R :=
  let kEff := effectiveFeedback state
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

def updateFeedbackCoefficients (state : NaiveReuppingLadder R) : NaiveReuppingLadder R :=
  let slope := NormalizedTanh.slopeZero state.saturation
  let perStage := state.alpha / (2 - state.alpha) * slope
  let squared := perStage * perStage
  let kSafe := 0.9 / (squared * squared + 0.00000001)
  let baseFeedback := minValue state.feedback kSafe
  let reference := 1 / (state.criticalFeedback - baseFeedback)
  {state with
    kSafe, baseFeedback, referencePowerGain := reference * reference
    dampingScale := slope * slope * slope * dampingEpsilon}

/-- Valid inputs: positive cutoff, resonance in [0,1], positive gain. Cutoff is
the stage half-power corner, capped at engine Nyquist. Setters preserve history;
callers can cache unchanged specs, just as for the other generated filters. -/
def computeCoefficients [Trig R] [SquareRoot R]
    (spec : LogicalSpec R) (state : NaiveReuppingLadder R) : NaiveReuppingLadder R :=
  let cutoff := minValue 0.5 spec.cutoff
  let sine := Trig.sin2pi (cutoff * 0.5)
  let root := SquareRoot.sqrt (1 + sine * sine)
  let alpha := sine * 2 * (root - sine)
  let poleRatio := sine * 2 * (root + sine)
  let detectorSine := minValue (poleRatio * 0.5) 1
  let detectorCosineSquared := (1 - detectorSine) * (1 + detectorSine)
  let maxDetectorGain : R := 318.30883898555044
  let inverseMaxGainSquared := 1 / (maxDetectorGain * maxDetectorGain)
  let minimumCosineSquared := detectorSine * detectorSine * inverseMaxGainSquared
  let detectorGain := detectorSine / SquareRoot.sqrt (maxValue detectorCosineSquared minimumCosineSquared)
  let crossingGain := poleRatio + 2
  let nyquistGain := 1 + 2 / maxValue poleRatio 2
  let nyquistSquared := nyquistGain * nyquistGain
  let criticalFeedback := IfLess.ifLess poleRatio 2
    (crossingGain * crossingGain) (nyquistSquared * nyquistSquared)
  let envelopeAlphaScale : R := 0.43497944204608224 / 6.283185307179586476925286766559
  let detector : BandPassedEnvelopeFollower.LogicalSpec R := ⟨detectorGain, sine * envelopeAlphaScale⟩
  updateFeedbackCoefficients {state with
    alpha, cutoff, feedback := spec.resonance * 3.9
    saturation := NormalizedTanh.prepare ⟨spec.gain⟩, criticalFeedback
    inputEnvelope := BandPassedEnvelopeFollower.computeCoefficients detector state.inputEnvelope
    feedbackEnvelope := BandPassedEnvelopeFollower.computeCoefficients detector state.feedbackEnvelope}

def reset (state : NaiveReuppingLadder R) : NaiveReuppingLadder R :=
  {state with
    stage1 := 0, stage2 := 0, stage3 := 0, stage4 := 0, input := 0, output := 0, kEff := 0
    inputEnvelope := state.inputEnvelope.reset, feedbackEnvelope := state.feedbackEnvelope.reset}

def stepFloat := step (R := Float)
def computeCoefficientsFloat := computeCoefficients (R := Float)
def resetFloat := reset (R := Float)
end NaiveReuppingLadder
end DSP

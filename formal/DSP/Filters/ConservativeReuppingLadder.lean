import DSP.Filters.NormalizedTanh
import DSP.Filters.OnePoleZOHLowPass
import DSP.Filters.ReuppingLadder.Tuning

namespace DSP

/-- Fixed-feedback sibling of the handwritten Naive Reupping Ladder, selectable
in the app for A/B testing. Four Euler states, three in-loop normalized saturators,
one output saturator, and feedback from the previous fourth-stage state. -/
structure ConservativeReuppingLadder (R : Type) where
  alpha : R
  saturation : NormalizedTanh.Coefficients R
  k : R
  stage1 : R
  stage2 : R
  stage3 : R
  stage4 : R
  input : R
  output : R

namespace ConservativeReuppingLadder

structure LogicalSpec (R : Type) where
  cutoff : R
  resonance : R
  gain : R

/-- Fraction of the critical loop gain at maximum resonance, 3.9/4. -/
def resonanceMargin {R : Type} [OfScientific R] : R := 0.975

def stepWith {R : Type} [OfNat R 0] [OfNat R 1] [Add R] [Sub R] [Mul R] [FMA R]
    (saturate : R → R) (state : ConservativeReuppingLadder R) (input : Sample R) :
    ConservativeReuppingLadder R :=
  let drive := input.x - state.k * state.stage4
  let stage1 := OnePoleZOHLowPass.onePoleStep state.alpha drive state.stage1
  let stage2 := OnePoleZOHLowPass.onePoleStep state.alpha (saturate stage1) state.stage2
  let stage3 := OnePoleZOHLowPass.onePoleStep state.alpha (saturate stage2) state.stage3
  let stage4 := OnePoleZOHLowPass.onePoleStep state.alpha (saturate stage3) state.stage4
  {state with
    stage1, stage2, stage3, stage4, input := drive,
    output := saturate stage4 * (state.k + 1)}

def step {R : Type} [OfNat R 0] [OfNat R 1] [Add R] [Sub R] [Mul R] [Div R] [FMA R] [Tanh R]
    (state : ConservativeReuppingLadder R) (input : Sample R) : ConservativeReuppingLadder R :=
  stepWith (NormalizedTanh.apply state.saturation) state input

/-- Positive cutoff/gain and resonance in [0,1] are the tuning contract.
Preparation preserves histories. No power detector or adaptive coefficient. -/
def computeCoefficients {R : Type} [OfNat R 1] [OfNat R 2] [OfScientific R]
    [Add R] [Sub R] [Mul R] [Div R] [IfLess R] [Trig R] [SquareRoot R] [Tanh R]
    (spec : LogicalSpec R) (state : ConservativeReuppingLadder R) : ConservativeReuppingLadder R :=
  let tuning := ReuppingLadder.prepareTuning spec.cutoff
  let saturation := NormalizedTanh.prepare ⟨spec.gain⟩
  let slope := NormalizedTanh.slopeZero saturation
  let k := spec.resonance * resonanceMargin * tuning.criticalFeedback / (slope * slope * slope)
  {state with alpha := tuning.alpha, saturation, k}

def reset {R : Type} [OfNat R 0] (state : ConservativeReuppingLadder R) : ConservativeReuppingLadder R :=
  {state with stage1 := 0, stage2 := 0, stage3 := 0, stage4 := 0, input := 0, output := 0}

def stepFloat := step (R := Float)
def computeCoefficientsFloat := computeCoefficients (R := Float)
def resetFloat := reset (R := Float)
end ConservativeReuppingLadder
end DSP

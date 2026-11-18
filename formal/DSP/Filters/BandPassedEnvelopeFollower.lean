import DSP.Filters.Capture

namespace DSP

/-- Two unity-center-gain Q=1 SVF bandpasses, followed by mean-square smoothing.
The full child states are shared with the existing generated SVF recurrence. -/
structure BandPassedEnvelopeFollower (R : Type) where
  stage1 : TPTSVF R
  stage2 : TPTSVF R
  power : R
  powerAlpha : R

namespace BandPassedEnvelopeFollower
structure LogicalSpec (R : Type) where
  integratorGain : R
  powerAlpha : R

def computeCoefficients {R : Type} [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R]
    (spec : LogicalSpec R) (state : BandPassedEnvelopeFollower R) : BandPassedEnvelopeFollower R :=
  {state with
    stage1 := (TPTSVF.prepareFromSnapshot ⟨spec.integratorGain, 1⟩ state.stage1).1
    stage2 := (TPTSVF.prepareFromSnapshot ⟨spec.integratorGain, 1⟩ state.stage2).1
    powerAlpha := spec.powerAlpha}

def step {R : Type} [Add R] [Sub R] [Neg R] [Mul R] [FMA R] [OfNat R 2]
    (state : BandPassedEnvelopeFollower R) (input : Sample R) : BandPassedEnvelopeFollower R :=
  let cascade := TPTSVF4Pole.stepBandPass ⟨state.stage1, state.stage2⟩ input
  {state with
    stage1 := cascade.stage1, stage2 := cascade.stage2
    power := OnePoleZOHLowPass.relax state.powerAlpha
      (cascade.stage2.bandPass * cascade.stage2.bandPass) state.power}

def resetSVF {R : Type} [OfNat R 0] (state : TPTSVF R) : TPTSVF R :=
  {state with
    ic1 := 0, ic2 := 0, lowPass := 0, bandPass := 0, highPass := 0
    notch := 0, allPass := 0}

def reset {R : Type} [OfNat R 0] (state : BandPassedEnvelopeFollower R) : BandPassedEnvelopeFollower R :=
  {state with stage1 := resetSVF state.stage1, stage2 := resetSVF state.stage2, power := 0}

def stepFloat := step (R := Float)
def computeCoefficientsFloat := computeCoefficients (R := Float)
def resetFloat := reset (R := Float)
end BandPassedEnvelopeFollower
end DSP

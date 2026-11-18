import DSP.Filters.NaiveReuppingLadder

namespace DSP.Tests.NaiveReuppingLadder

def svfFields {R : Type} (s : TPTSVF R) : List R :=
  [s.k, s.a1, s.a2, s.a3, s.ic1, s.ic2, s.lowPass, s.bandPass, s.highPass, s.notch, s.allPass]

def envelopeFields {R : Type} (s : BandPassedEnvelopeFollower R) : List R :=
  svfFields s.stage1 ++ svfFields s.stage2 ++ [s.power, s.powerAlpha]

def fields {R : Type} (s : DSP.NaiveReuppingLadder R) : List R :=
  [s.alpha, s.saturation.gain, s.saturation.tanhGain, s.cutoff, s.feedback,
    s.stage1, s.stage2, s.stage3, s.stage4, s.input, s.output, s.kEff] ++
  envelopeFields s.inputEnvelope ++ envelopeFields s.feedbackEnvelope ++
  [s.criticalFeedback, s.kSafe, s.baseFeedback, s.referencePowerGain, s.dampingScale]

def seededSVF {R : Type} (x : R) : TPTSVF R := ⟨x,x,x,x,x,x,x,x,x,x,x⟩

def seeded {R : Type} [Mul R] (x : R) : DSP.NaiveReuppingLadder R :=
  let env := BandPassedEnvelopeFollower.mk (seededSVF x) (seededSVF x) (x*x) x
  ⟨x,⟨x,x⟩,x,x,x,x,x,x,x,x,x,env,env,x,x,x,x,x⟩

end DSP.Tests.NaiveReuppingLadder

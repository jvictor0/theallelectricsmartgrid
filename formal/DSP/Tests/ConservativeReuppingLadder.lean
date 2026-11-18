import DSP.Filters.ConservativeReuppingLadder

namespace DSP.Tests.ConservativeReuppingLadder

def fields {R : Type} (s : DSP.ConservativeReuppingLadder R) : List R :=
  [s.alpha,s.saturation.gain,s.saturation.tanhGain,s.k,
    s.stage1,s.stage2,s.stage3,s.stage4,s.input,s.output]

def seeded {R : Type} (x : R) : DSP.ConservativeReuppingLadder R :=
  ⟨x,⟨x,x⟩,x,x,x,x,x,x,x⟩

end DSP.Tests.ConservativeReuppingLadder

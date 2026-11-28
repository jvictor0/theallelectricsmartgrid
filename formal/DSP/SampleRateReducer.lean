import DSP.DelayMath
import DSP.IfLess

namespace DSP.SampleRateReducer

structure State (R : Type) where
  phase : R
  output : R

structure Input (R : Type) where
  freq : R
  sample : R

/-- Advance in cycles per processing sample and capture on wrap. An in-range
phase and its held sample stay frozen at full-rate bypass. -/
def process {R : Type} [OfNat R 0] [OfNat R 1] [Add R] [Sub R] [DelayMath R] [IfLess R]
    (input : Input R) : StateM (State R) R :=
  modifyGet fun state =>
    let phase := state.phase + IfLess.ifLess input.freq 1 input.freq 0
    let output := IfLess.ifLess phase 1 state.output input.sample
    let nextPhase := IfLess.ifLess phase 1 phase (phase - DelayMath.floor phase)
    (IfLess.ifLess input.freq 1 output input.sample, ⟨nextPhase, output⟩)

end DSP.SampleRateReducer

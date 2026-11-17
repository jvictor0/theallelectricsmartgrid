import DSP.Filters.TPTSVF

namespace DSP

/-- Two `TPTSVF` stages in series. Each variant feeds one response of the
first stage into the second and reads the same response of the second:
`stepHighPass` is the app's former handwritten four-pole HP. -/
structure TPTSVF4Pole (R : Type) where
  stage1 : TPTSVF R
  stage2 : TPTSVF R

namespace TPTSVF4Pole

variable {R : Type} [Add R] [Sub R] [Neg R] [Mul R] [FMA R] [OfNat R 2]

def stepHighPass (f : TPTSVF4Pole R) (input : Sample R) : TPTSVF4Pole R :=
  let stage1 := f.stage1.step input
  { stage1, stage2 := f.stage2.step ⟨stage1.highPass⟩ }

def stepLowPass (f : TPTSVF4Pole R) (input : Sample R) : TPTSVF4Pole R :=
  let stage1 := f.stage1.step input
  { stage1, stage2 := f.stage2.step ⟨stage1.lowPass⟩ }

def stepBandPass (f : TPTSVF4Pole R) (input : Sample R) : TPTSVF4Pole R :=
  let stage1 := f.stage1.step input
  { stage1, stage2 := f.stage2.step ⟨stage1.bandPass⟩ }

def stepHighPassFloat : Step (TPTSVF4Pole Float) (Sample Float) := stepHighPass

def stepLowPassFloat : Step (TPTSVF4Pole Float) (Sample Float) := stepLowPass

def stepBandPassFloat : Step (TPTSVF4Pole Float) (Sample Float) := stepBandPass

end TPTSVF4Pole
end DSP

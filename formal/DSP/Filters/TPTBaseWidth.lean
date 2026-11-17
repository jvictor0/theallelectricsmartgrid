import DSP.Filters.OnePoleTPTHighPass

namespace DSP

/-- A TPT highpass at the base frequency into a TPT lowpass at base × width,
the trapezoidal counterpart of the app's `OPBaseWidthFilter`. The output is
`lowPass.y`. -/
structure TPTBaseWidth (R : Type) where
  highPass : OnePoleTPTHighPass R
  lowPass : OnePoleTPTLowPass R

namespace TPTBaseWidth

variable {R : Type} [Sub R] [FMA R]

def step (f : TPTBaseWidth R) (input : Sample R) : TPTBaseWidth R :=
  let highPass := f.highPass.step input
  { highPass, lowPass := f.lowPass.step ⟨highPass.y⟩ }

def stepFloat : Step (TPTBaseWidth Float) (Sample Float) := step

end TPTBaseWidth
end DSP

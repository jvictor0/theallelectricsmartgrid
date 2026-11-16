import DSP.Filters.OnePoleTPTLowPass

namespace DSP

/-- Trapezoidal one-pole highpass: the input minus a TPT lowpass, whose
state, coefficient and output it stores. -/
structure OnePoleTPTHighPass (R : Type) where
  lowPass : OnePoleTPTLowPass R
  y : R

namespace OnePoleTPTHighPass

variable {R : Type} [Add R] [Sub R] [Mul R]

def step (f : OnePoleTPTHighPass R) (input : Sample R) : OnePoleTPTHighPass R :=
  let lowPass := f.lowPass.step input
  { lowPass, y := input.x - lowPass.y }

def stepFloat : Step (OnePoleTPTHighPass Float) (Sample Float) := step

@[simp] theorem step_lowPass (f : OnePoleTPTHighPass R) (input : Sample R) :
    (step f input).lowPass = f.lowPass.step input := rfl

@[simp] theorem step_y (f : OnePoleTPTHighPass R) (input : Sample R) :
    (step f input).y = input.x - (f.lowPass.step input).y := rfl

end OnePoleTPTHighPass
end DSP

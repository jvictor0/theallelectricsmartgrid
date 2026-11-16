import DSP.Step

namespace DSP

/-- Trapezoidal (TPT) one-pole lowpass. `G` is the instantaneous response
g / (1 + g), where g is the normalized integrator gain, ideally tan(π f) for
cutoff f in cycles per sample. `s` is the trapezoidal integrator's state and
`y` stores the most recent output. -/
structure OnePoleTPTLowPass (R : Type) where
  G : R
  s : R
  y : R

namespace OnePoleTPTLowPass

variable {R : Type} [Add R] [Sub R] [Mul R]

/-! The zero-delay feedback loop y = s + g (x - y) is solved by the
coefficient G, so each sample is explicit: v = (x - s) G, y = s + v, and the
trapezoidal integrator advances to y + v. The real-number proofs interpret
this exact operation order; Float need not satisfy their algebraic laws.
-/

/-- The output this filter would produce for input x, without advancing. -/
def respond (f : OnePoleTPTLowPass R) (x : R) : R :=
  (x - f.s) * f.G + f.s

def step (f : OnePoleTPTLowPass R) (input : Sample R) : OnePoleTPTLowPass R :=
  let v := (input.x - f.s) * f.G
  let y := v + f.s
  { f with s := y + v, y := y }

def stepFloat : Step (OnePoleTPTLowPass Float) (Sample Float) := step

@[simp] theorem step_G (f : OnePoleTPTLowPass R) (input : Sample R) :
    (step f input).G = f.G := rfl

@[simp] theorem step_s (f : OnePoleTPTLowPass R) (input : Sample R) :
    (step f input).s = ((input.x - f.s) * f.G + f.s) + (input.x - f.s) * f.G := rfl

@[simp] theorem step_y (f : OnePoleTPTLowPass R) (input : Sample R) :
    (step f input).y = f.respond input.x := rfl

theorem respond_eq (f : OnePoleTPTLowPass R) (x : R) : f.respond x = (x - f.s) * f.G + f.s := rfl

end OnePoleTPTLowPass
end DSP

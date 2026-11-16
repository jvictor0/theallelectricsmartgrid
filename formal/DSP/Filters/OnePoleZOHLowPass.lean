import DSP.StateSpace.Basic
import DSP.Step

namespace DSP

/-- The input-weight coefficient and the current output. As in the app's
`OPLowPassFilter`, `alpha` weights the new input, ideally 1 - exp(-T/τ). -/
structure OnePoleZOHLowPass (R : Type) where
  alpha : R
  y : R

namespace OnePoleZOHLowPass

variable {R : Type} [OfNat R 0] [OfNat R 1] [Add R] [Sub R] [Mul R]

/-! The same arithmetic is interpreted by the chosen number type. These
definitions require operations, not algebraic laws: Float need not satisfy
the identities used by the separate real-number proofs. This module has no
Mathlib dependency and can be compiled on its own.
-/

def discrete (alpha : R) : StateSpace 1 R where
  A _ _ := 1 - alpha
  B _ := alpha
  C _ := 1
  D := 0

@[inline] def onePoleStep (alpha input state : R) : R :=
  (discrete alpha).advance (fun _ => state) input 0

@[simp] theorem onePoleStep_eq (alpha input state : R) :
    onePoleStep alpha input state = (1 - alpha) * state + alpha * input := rfl

@[inline] def process (f : OnePoleZOHLowPass R) (input : R) : OnePoleZOHLowPass R :=
  { f with y := onePoleStep f.alpha input f.y }

@[simp] theorem process_alpha (f : OnePoleZOHLowPass R) (input : R) :
    (f.process input).alpha = f.alpha := rfl

@[simp] theorem process_y (f : OnePoleZOHLowPass R) (input : R) :
    (f.process input).y = onePoleStep f.alpha input f.y := rfl

/-- The stored output is the updated state, after this input interval. -/
def step (f : OnePoleZOHLowPass R) (input : Sample R) : OnePoleZOHLowPass R :=
  f.process input.x

def stepFloat : Step (OnePoleZOHLowPass Float) (Sample Float) := step

@[simp] theorem step_alpha (f : OnePoleZOHLowPass R) (input : Sample R) :
    (step f input).alpha = f.alpha := rfl

@[simp] theorem step_y (f : OnePoleZOHLowPass R) (input : Sample R) :
    (step f input).y = onePoleStep f.alpha input.x f.y := rfl

end OnePoleZOHLowPass
end DSP

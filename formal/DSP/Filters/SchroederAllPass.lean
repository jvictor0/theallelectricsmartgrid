import DSP.DelayBuffer.Basic
import DSP.FMA
import DSP.Step

namespace DSP.SchroederAllPass

structure Coefficients (capacity : Nat) (R : Type) where
  age : Fin capacity
  gain : R

structure State (capacity : Nat) (R : Type) where
  buffer : DelayBuffer capacity R
  y : R

variable {capacity : Nat} {R : Type}

/-- The raw-delay Schroeder recurrence in AllPassFilter.hpp. -/
def step [Neg R] [FMA R] (c : Coefficients capacity R) (s : State capacity R)
    (input : Sample R) : State capacity R :=
  let y := FMA.fma (-c.gain) input.x (s.buffer.read c.age)
  let written := FMA.fma c.gain y input.x
  ⟨s.buffer.push written, y⟩

end DSP.SchroederAllPass

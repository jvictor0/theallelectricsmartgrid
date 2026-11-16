import DSP.StateSpace.Basic

namespace DSP.Coupled

/-- A two-state example with runtime coefficients, coupling and feedthrough. -/
structure State (R : Type) where
  model : StateSpace 2 R
  q : Fin 2 → R

structure Input (R : Type) where
  x : R

structure Output (R : Type) where
  y : R

def processor {R : Type} [OfNat R 0] [Add R] [Mul R]
    (state : State R) (input : Input R) : State R × Output R :=
  let result := state.model.processor state.q input.x
  ({ state with q := result.1 }, { y := result.2 })

def processorFloat : Processor (State Float) (Input Float) (Output Float) := processor

@[simp] theorem processor_state {R : Type} [OfNat R 0] [Add R] [Mul R]
    (state : State R) (input : Input R) :
    (processor state input).1.q = state.model.advance state.q input.x := by
  simp [processor, StateSpace.processor]

@[simp] theorem processor_output {R : Type} [OfNat R 0] [Add R] [Mul R]
    (state : State R) (input : Input R) :
    (processor state input).2.y = state.model.observe state.q input.x := rfl

end DSP.Coupled

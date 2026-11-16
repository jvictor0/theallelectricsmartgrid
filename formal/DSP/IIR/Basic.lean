import DSP.StateSpace.Basic

namespace DSP.IIR

structure Coefficients (m n : Nat) (R : Type) where
  b0 : R
  b : Fin m → R
  a : Fin n → R

structure State (m n : Nat) (R : Type) where
  inputs : Fin m → R
  outputs : Fin n → R

/-- Insert a sample at the front of a fixed-size history. -/
def shift {R : Type} (x : R) : {n : Nat} → (Fin n → R) → Fin n → R
  | 0, _ => Fin.elim0
  | _ + 1, h =>
    Fin.cases x (fun j => h j.castSucc)

variable {m n : Nat} {R : Type} [OfNat R 0] [Add R] [Sub R] [Mul R]

def processor (c : Coefficients m n R) : Processor (State m n R) R R := fun s x =>
  let y := (c.b0 * x + StateSpace.dot c.b s.inputs) - StateSpace.dot c.a s.outputs
  let inputs := StateSpace.snapshot m (shift x s.inputs)
  let outputs := StateSpace.snapshot n (shift y s.outputs)
  ({ inputs := inputs.values, outputs := outputs.values }, y)

def zeroState : State m n R := { inputs := fun _ => 0, outputs := fun _ => 0 }

end DSP.IIR

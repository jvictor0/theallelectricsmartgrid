import DSP.Processor.Run

namespace DSP

/-- Coefficients only; continuous and discrete interpretations live separately. -/
structure StateSpace (n : Nat) (R : Type) where
  A : Fin n → Fin n → R
  B : Fin n → R
  C : Fin n → R
  D : R

namespace StateSpace

/-- Ascending, left-associated sum; a singleton introduces no extra zero. -/
def orderedSum {R : Type} [OfNat R 0] [Add R] : (n : Nat) → (Fin n → R) → R
  | 0, _ => 0
  | 1, f => f 0
  | n + 2, f => orderedSum (n + 1) (fun i => f i.castSucc) + f (Fin.last (n + 1))

/-- A data result keeps Lean from eta-expanding the snapshot into deferred work. -/
structure Snapshot (n : Nat) (R : Type) where
  values : Fin n → R

/-- Evaluate all coordinates before returning the data wrapper. -/
def snapshot {R : Type} : (n : Nat) → (Fin n → R) → Snapshot n R
  | 0, _ => ⟨Fin.elim0⟩
  | n + 1, f =>
    let head := f 0
    let tail := snapshot n (fun i => f i.succ)
    ⟨Fin.cases head tail.values⟩

@[simp] theorem snapshot_values {R : Type} (n : Nat) (f : Fin n → R) : (snapshot n f).values = f := by
  induction n with
  | zero => funext i; exact Fin.elim0 i
  | succ n ih =>
    funext i
    refine Fin.cases ?_ (fun j => ?_) i
    · rfl
    · exact congrFun (ih (fun i => f i.succ)) j

variable {n : Nat} {R : Type} [OfNat R 0] [Add R] [Mul R]

def dot (a b : Fin n → R) : R := orderedSum n (fun i => a i * b i)

def advance (s : StateSpace n R) (q : Fin n → R) (u : R) : Fin n → R :=
  fun i => dot (s.A i) q + s.B i * u

def observe (s : StateSpace n R) (q : Fin n → R) (u : R) : R := dot s.C q + s.D * u

def processor (s : StateSpace n R) : Processor (Fin n → R) R R :=
  fun q u =>
    let next := snapshot n (s.advance q u)
    (next.values, s.observe q u)

def processorNext (s : StateSpace n R) : Processor (Fin n → R) R R :=
  fun q u =>
    let next := snapshot n (s.advance q u)
    (next.values, s.observe next.values u)

end StateSpace
end DSP

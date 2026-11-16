import DSP.Processor.Run

namespace DSP.Processor

variable {S T I O : Type}

def Simulation (p : Processor S I O) (q : Processor T I O) (rel : S → T → Prop) : Prop :=
  ∀ s t, rel s t → ∀ x, rel (p s x).1 (q t x).1 ∧ (p s x).2 = (q t x).2

theorem runState_simulation (p : Processor S I O) (q : Processor T I O)
    (rel : S → T → Prop) (h : Simulation p q rel) (u : Nat → I)
    (s : S) (t : T) (hinit : rel s t) (n : Nat) :
    rel (runState p u s n) (runState q u t n) := by
  induction n with
  | zero => exact hinit
  | succ n ih => exact (h _ _ ih (u n)).1

theorem run_simulation (p : Processor S I O) (q : Processor T I O)
    (rel : S → T → Prop) (h : Simulation p q rel) (u : Nat → I)
    (s : S) (t : T) (hinit : rel s t) : run p u s = run q u t := by
  funext n
  exact (h _ _ (runState_simulation p q rel h u s t hinit n) (u n)).2

end DSP.Processor

import DSP.Processor

namespace DSP.Processor

variable {S I O : Type}

def runState (p : Processor S I O) (input : Nat → I) (initial : S) : Nat → S
  | 0 => initial
  | n + 1 => (p (runState p input initial n) (input n)).1

def run (p : Processor S I O) (input : Nat → I) (initial : S) (n : Nat) : O :=
  (p (runState p input initial n) (input n)).2

@[simp] theorem runState_zero (p : Processor S I O) (u : Nat → I) (s : S) :
    runState p u s 0 = s := rfl

@[simp] theorem runState_succ (p : Processor S I O) (u : Nat → I) (s : S) (n : Nat) :
    runState p u s (n + 1) = (p (runState p u s n) (u n)).1 := rfl

theorem runState_unique (p : Processor S I O) (u : Nat → I) (s : S) (q : Nat → S)
    (hzero : q 0 = s) (hstep : ∀ n, q (n + 1) = (p (q n) (u n)).1) :
    q = runState p u s := by
  funext n
  induction n with
  | zero => exact hzero
  | succ n ih => rw [hstep, ih]; rfl

end DSP.Processor

import Init

namespace DSP

/-- State-only processing: every observable value is a field of the state. -/
abbrev Step (State Input : Type) := State → Input → State

/-- The shared single-sample input record. -/
structure Sample (R : Type) where
  x : R

namespace Step

variable {S T I J : Type}

def states (p : Step S I) (u : Nat → I) (initial : S) : Nat → S
  | 0 => initial
  | n + 1 => p (states p u initial n) (u n)

@[simp] theorem states_zero (p : Step S I) (u : Nat → I) (s : S) : states p u s 0 = s := rfl

@[simp] theorem states_succ (p : Step S I) (u : Nat → I) (s : S) (n : Nat) :
    states p u s (n + 1) = p (states p u s n) (u n) := rfl

/-- A component of the state evolves as its own step, driven by any input
computed from the composite's old state and current input. -/
theorem states_map (p : Step S I) (q : Step T J) (π : S → T) (input : S → I → J)
    (h : ∀ s x, π (p s x) = q (π s) (input s x)) (u : Nat → I) (s : S) (n : Nat) :
    π (states p u s n) = states q (fun k => input (states p u s k) (u k)) (π s) n := by
  induction n with
  | zero => rfl
  | succ n ih => rw [states_succ, h, ih]; rfl

theorem states_invariant {α : Type} (p : Step S I) (f : S → α) (h : ∀ s x, f (p s x) = f s)
    (u : Nat → I) (s : S) (n : Nat) : f (states p u s n) = f s := by
  induction n with
  | zero => rfl
  | succ n ih => rw [states_succ, h, ih]

end Step
end DSP

namespace DSP

/-- Run at most `fuel` steps. A `done` step returns its value immediately. -/
def foldUntil {A : Type} : Nat → (A → ForInStep A) → A → A
  | 0, _, initial => initial
  | fuel + 1, step, initial =>
    match step initial with
    | .done result => result
    | .yield result => foldUntil fuel step result

def foldUntilM {M : Type → Type} {A : Type} [Monad M] :
    Nat → (A → M (ForInStep A)) → A → M A
  | 0, _, initial => pure initial
  | fuel + 1, step, initial => do
    match ← step initial with
    | .done result => pure result
    | .yield result => foldUntilM fuel step result

end DSP

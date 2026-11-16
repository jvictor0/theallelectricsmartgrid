import DSP.IIR.Basic
import DSP.StateSpace.Digital
import Mathlib.Data.Nat.Basic

namespace DSP.IIR

variable {m n : ℕ} {R : Type} [Ring R]

def delay (k : ℕ) (u : ℕ → R) (t : ℕ) : R := if k ≤ t then u (t - k) else 0

def history (u : ℕ → R) (t : ℕ) : Fin n → R := fun i => delay (i.val + 1) u t

@[simp] theorem history_zero (u : ℕ → R) : (history u 0 : Fin n → R) = fun _ => 0 := by
  funext i
  simp [history, delay]

theorem history_succ (u : ℕ → R) (t : ℕ) :
    (history u (t + 1) : Fin n → R) = shift (u t) (history u t) := by
  cases n with
  | zero => funext i; exact Fin.elim0 i
  | succ n =>
      funext i
      refine Fin.cases ?_ (fun j => ?_) i
      · simp [history, delay, shift]
      · simp only [history, shift, Fin.cases_succ, Fin.val_succ, Fin.val_castSucc, delay]
        split_ifs with h h'
        · congr 1; omega
        · omega
        · omega
        · rfl

def SatisfiesDifferenceEquation (c : Coefficients m n R) (u y : ℕ → R) : Prop :=
  ∀ t, y t = c.b0 * u t + ∑ i, c.b i * delay (i.val + 1) u t -
    ∑ i, c.a i * delay (i.val + 1) y t

theorem history_invariant (c : Coefficients m n R) (u : ℕ → R) (t : ℕ) :
    let s := Processor.runState (processor c) u zeroState t
    s.inputs = history u t ∧ s.outputs = history (Processor.run (processor c) u zeroState) t := by
  induction t with
  | zero => simp [Processor.runState, zeroState]
  | succ t ih =>
      dsimp only at ih ⊢
      constructor
      · rw [Processor.runState_succ]
        simp only [processor, StateSpace.snapshot_values]
        rw [ih.1, history_succ]
      · rw [Processor.runState_succ]
        simp only [processor, StateSpace.snapshot_values]
        change shift (Processor.run (processor c) u zeroState t) _ = _
        rw [ih.2, history_succ]

theorem iir_run_correct (c : Coefficients m n R) (u : ℕ → R) :
    SatisfiesDifferenceEquation c u (Processor.run (processor c) u zeroState) := by
  intro t
  have h := history_invariant c u t
  change c.b0 * u t + StateSpace.dot c.b _ - StateSpace.dot c.a _ = _
  rw [h.1, h.2]
  simp only [StateSpace.dot_eq_dotProduct, dotProduct, history]

theorem recurrence_unique (c : Coefficients m n R) (u y w : ℕ → R)
    (hy : SatisfiesDifferenceEquation c u y) (hw : SatisfiesDifferenceEquation c u w) : y = w := by
  funext t
  induction t using Nat.strong_induction_on with
  | h t ih =>
      rw [hy t, hw t]
      congr 1
      apply Finset.sum_congr rfl
      intro i _
      congr 1
      unfold delay
      split_ifs with h
      · exact ih _ (by omega)
      · rfl

theorem iir_run_iff_recurrence (c : Coefficients m n R) (u y : ℕ → R) :
    y = Processor.run (processor c) u zeroState ↔ SatisfiesDifferenceEquation c u y := by
  constructor
  · rintro rfl; exact iir_run_correct c u
  · intro h; exact recurrence_unique c u y _ h (iir_run_correct c u)

end DSP.IIR

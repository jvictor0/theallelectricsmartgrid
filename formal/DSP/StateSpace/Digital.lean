import DSP.FMA.Algebra
import DSP.StateSpace.Basic
import Mathlib.LinearAlgebra.Matrix.ToLin
import Mathlib.Data.Fin.VecNotation
import Mathlib.Tactic.Ring

namespace DSP.StateSpace

open Matrix

variable {n : ℕ} {R : Type}

theorem orderedSum_eq_sum [AddCommMonoid R] (n : ℕ) (f : Fin n → R) :
    orderedSum n f = ∑ i, f i := by
  induction n using Nat.twoStepInduction with
  | zero => simp [orderedSum]
  | one => simp [orderedSum]
  | more n _ ih =>
      rw [orderedSum, ih]
      exact (Fin.sum_univ_castSucc f).symm

theorem dot_eq_dotProduct [Semiring R] (a b : Fin n → R) : dot a b = a ⬝ᵥ b := by
  induction n using Nat.twoStepInduction with
  | zero => simp [dot, dotProduct]
  | one => simp [dot, dotProduct]
  | more n _ ih =>
      rw [dot, fma_eq, ih]
      simpa [dotProduct, add_comm] using (Fin.sum_univ_castSucc (fun i => a i * b i)).symm

theorem advance_eq [Semiring R] (s : StateSpace n R) (q : Fin n → R) (u : R) :
    s.advance q u = s.A *ᵥ q + (fun i => s.B i * u) := by
  funext i
  simp [advance, dot_eq_dotProduct, mulVec, add_comm]

theorem observe_eq [Semiring R] (s : StateSpace n R) (q : Fin n → R) (u : R) :
    s.observe q u = s.C ⬝ᵥ q + s.D * u := by simp [observe, dot_eq_dotProduct, add_comm]

theorem advance_smul [CommSemiring R] (s : StateSpace n R) (c : R) (q : Fin n → R) (u : R) :
    s.advance (c • q) (c * u) = c • s.advance q u := by
  simp only [advance_eq, mulVec_smul, smul_add]
  congr 1
  funext i
  simp [mul_left_comm]

theorem observe_smul [CommSemiring R] (s : StateSpace n R) (c : R) (q : Fin n → R) (u : R) :
    s.observe (c • q) (c * u) = c * s.observe q u := by
  simp only [observe_eq, dotProduct_smul, smul_eq_mul]
  ring

def DigitalRealization [Semiring R] (s : StateSpace n R) (u : ℕ → R)
    (initial : Fin n → R) (q : ℕ → Fin n → R) (y : ℕ → R) : Prop :=
  q 0 = initial ∧ (∀ k, q (k + 1) = s.advance (q k) (u k)) ∧
    ∀ k, y k = s.observe (q k) (u k)

theorem processor_run_correct [Semiring R] (s : StateSpace n R)
    (u : ℕ → R) (initial : Fin n → R) :
    DigitalRealization s u initial (Processor.runState s.processor u initial)
      (Processor.run s.processor u initial) := by
  refine ⟨rfl, fun k => ?_, fun _ => rfl⟩
  simp [Processor.runState_succ, processor]

theorem digitalRealization_unique [Semiring R] (s : StateSpace n R)
    (u : ℕ → R) (initial : Fin n → R) (q : ℕ → Fin n → R) (y : ℕ → R)
    (h : DigitalRealization s u initial q y) :
    q = Processor.runState s.processor u initial ∧ y = Processor.run s.processor u initial := by
  have hq := Processor.runState_unique s.processor u initial q h.1 (fun k => by simpa [processor] using h.2.1 k)
  refine ⟨hq, ?_⟩
  funext k
  rw [h.2.2 k, hq]
  rfl

/-- Standard observation of this model equals observation after advancing s. -/
def observeNext [Semiring R] (s : StateSpace n R) : StateSpace n R :=
  { s with C := s.C ᵥ* Matrix.of s.A, D := s.C ⬝ᵥ s.B + s.D }

theorem observeNext_equiv [CommSemiring R] (s : StateSpace n R) (q : Fin n → R) (u : R) :
    (s.observeNext).processor q u = s.processorNext q u := by
  simp only [processor, processorNext, snapshot_values, observeNext, observe_eq, advance_eq]
  congr 1
  have hm : s.C ⬝ᵥ (fun i => s.B i * u) = (s.C ⬝ᵥ s.B) * u := by
    simp only [dotProduct, Finset.sum_mul, mul_assoc]
  simp only [dotProduct_add, dotProduct_mulVec, hm]
  change (s.C ᵥ* s.A) ⬝ᵥ q + (s.C ⬝ᵥ s.B + s.D) * u =
    (s.C ᵥ* s.A) ⬝ᵥ q + (s.C ⬝ᵥ s.B) * u + s.D * u
  ring

end DSP.StateSpace

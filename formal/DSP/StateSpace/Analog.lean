import DSP.StateSpace.Digital
import DSP.ODE.LinearFlow
import Mathlib.Analysis.Normed.Module.FiniteDimension

namespace DSP.StateSpace

open Matrix

variable {n : ℕ}

noncomputable def operator (s : StateSpace n ℝ) : (Fin n → ℝ) →L[ℝ] (Fin n → ℝ) :=
  (Matrix.toLin' s.A).toContinuousLinearMap

@[simp] theorem operator_apply (s : StateSpace n ℝ) (q : Fin n → ℝ) :
    s.operator q = s.A *ᵥ q := rfl

theorem advance_operator (s : StateSpace n ℝ) (q : Fin n → ℝ) (u : ℝ) :
    s.advance q u = s.operator q + u • s.B := by
  rw [advance_eq, operator_apply]
  congr 1
  funext i
  simp [mul_comm]

/-- A continuous state trajectory represented by adjoining closed held intervals.
This representation avoids choosing a value for the derivative at input jumps. -/
structure HeldRealization (s : StateSpace n ℝ) (T : ℝ) (u : ℕ → ℝ) (initial : Fin n → ℝ) where
  segment : ℕ → ℝ → Fin n → ℝ
  starts : segment 0 0 = initial
  continuous : ∀ k, ContinuousOn (segment k) (Set.Icc 0 T)
  derivative : ∀ k t, t ∈ Set.Ioo 0 T →
    HasDerivAt (segment k) (s.operator (segment k t) + u k • s.B) t
  joins : ∀ k, segment k T = segment (k + 1) 0

def HeldRealization.sample {s : StateSpace n ℝ} {T : ℝ} {u : ℕ → ℝ}
    {initial : Fin n → ℝ} (q : s.HeldRealization T u initial) (k : ℕ) : Fin n → ℝ :=
  q.segment k 0

end DSP.StateSpace

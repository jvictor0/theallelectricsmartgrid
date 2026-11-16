import DSP.StateSpace.Digital
import Mathlib.LinearAlgebra.Matrix.NonsingularInverse
import Mathlib.Data.Complex.Basic

namespace DSP.StateSpace

open Matrix

variable {n : ℕ} {K : Type} [Field K]

def resolvent (s : StateSpace n K) (z : K) : Matrix (Fin n) (Fin n) K :=
  z • (1 : Matrix (Fin n) (Fin n) K) - Matrix.of s.A

def RegularAt (s : StateSpace n K) (z : K) : Prop := (s.resolvent z).det ≠ 0

noncomputable def modeState (s : StateSpace n K) (z : K) : Fin n → K :=
  (s.resolvent z)⁻¹ *ᵥ s.B

noncomputable def transfer (s : StateSpace n K) (z : K) : K := s.C ⬝ᵥ s.modeState z + s.D

theorem resolvent_equation (s : StateSpace n K) (z : K) (v : Fin n → K) :
    s.resolvent z *ᵥ v = s.B ↔ z • v = s.A *ᵥ v + s.B := by
  simp only [resolvent, sub_mulVec, smul_mulVec, one_mulVec]
  rw [sub_eq_iff_eq_add]
  rw [add_comm]
  rfl

theorem modeState_equation (s : StateSpace n K) (z : K) (hz : s.RegularAt z) :
    z • s.modeState z = s.A *ᵥ s.modeState z + s.B := by
  apply (s.resolvent_equation z _).mp
  rw [modeState, mulVec_mulVec, mul_nonsing_inv _ (isUnit_iff_ne_zero.mpr hz), one_mulVec]

theorem modeState_unique (s : StateSpace n K) (z : K) (hz : s.RegularAt z) (v : Fin n → K)
    (hv : z • v = s.A *ᵥ v + s.B) : v = s.modeState z := by
  have h := congrArg (fun w => (s.resolvent z)⁻¹ *ᵥ w) ((s.resolvent_equation z v).mpr hv)
  simpa [mulVec_mulVec, nonsing_inv_mul _ (isUnit_iff_ne_zero.mpr hz), modeState] using h

/-- Algebra shared by the derivative and geometric-mode proofs. -/
def ModeEquations (s : StateSpace n K) (z gain : K) : Prop :=
  ∃ v, z • v = s.A *ᵥ v + s.B ∧ gain = s.C ⬝ᵥ v + s.D

theorem modeEquations_iff_transfer (s : StateSpace n K) (z gain : K) (hz : s.RegularAt z) :
    s.ModeEquations z gain ↔ gain = s.transfer z := by
  constructor
  · rintro ⟨v, hv, hy⟩
    rw [s.modeState_unique z hz v hv] at hy
    exact hy
  · intro h
    exact ⟨s.modeState z, s.modeState_equation z hz, h⟩

end DSP.StateSpace

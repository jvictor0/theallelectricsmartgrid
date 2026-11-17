import DSP.FMA
import Mathlib.Algebra.Ring.Defs
import Mathlib.Algebra.Group.Pi.Basic

namespace DSP

/-- Exact arithmetic for the algebraic proofs. The pointwise instance takes
priority for channel functions, preserving the scalar interpretation. -/
instance (priority := low) {R : Type} [Semiring R] : FMA R :=
  ⟨fun a b c => a * b + c⟩

instance (priority := low) {R : Type} [Semiring R] : LawfulFMA R := ⟨fun _ _ _ => rfl⟩

instance {ι R : Type} [Mul R] [Add R] [FMA R] [LawfulFMA R] : LawfulFMA (ι → R) :=
  ⟨fun a b c => funext fun i => fma_eq (a i) (b i) (c i)⟩

end DSP

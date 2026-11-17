import Init

namespace DSP

/-- Multiply-add, fused for floating point and pointwise for channels. -/
class FMA (R : Type) where
  fma : R → R → R → R

/-- Native IEEE multiply-add, with one rounding of the exact product plus addend.
Like Lean's built-in floating-point primitives, its kernel value is opaque. -/
@[extern "fma"] opaque FMA.float : Float → Float → Float → Float

@[extern "fmaf"] opaque FMA.float32 : Float32 → Float32 → Float32 → Float32

instance : FMA Float := ⟨FMA.float⟩
instance : FMA Float32 := ⟨FMA.float32⟩

instance {ι R : Type} [FMA R] : FMA (ι → R) :=
  ⟨fun a b c i => FMA.fma (a i) (b i) (c i)⟩

/-- The exact-arithmetic interpretation. Floating-point FMA does not satisfy
this law with rounded multiplication and addition. -/
class LawfulFMA (R : Type) [Mul R] [Add R] [FMA R] : Prop where
  fma_eq : ∀ a b c : R, FMA.fma a b c = a * b + c

@[simp] theorem fma_eq {R : Type} [Mul R] [Add R] [FMA R] [LawfulFMA R]
    (a b c : R) : FMA.fma a b c = a * b + c := LawfulFMA.fma_eq a b c

end DSP

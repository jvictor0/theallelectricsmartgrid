import Init

namespace DSP

/-- `a < b ? x : y`, per channel: the app's `MultiChannelNumber::IfLess`. -/
class IfLess (R : Type) where
  ifLess : R → R → R → R → R

instance : IfLess Float := ⟨fun a b x y => if a < b then x else y⟩

instance : IfLess Float32 := ⟨fun a b x y => if a < b then x else y⟩

/-- Pointwise selection: every channel chooses its own branch. -/
instance {ι R : Type} [IfLess R] : IfLess (ι → R) :=
  ⟨fun a b x y i => IfLess.ifLess (a i) (b i) (x i) (y i)⟩

/-- A scalar selection whose branch depends only on the compared values. -/
class LawfulIfLess (R : Type) [IfLess R] where
  less : R → R → Bool
  ifLess_eq : ∀ a b x y : R, IfLess.ifLess a b x y = if less a b then x else y

end DSP

import Mathlib.Algebra.Notation.Pi.Defs

/-! Function-backed channels need numeric literals without requiring ring laws
from Float or Float32. Every literal is broadcast, as with Vector::Splat. -/
namespace DSP.Channels

instance (priority := low) {ι R : Type} {n : Nat} [OfNat R n] : OfNat (ι → R) n :=
  ⟨fun _ => OfNat.ofNat n⟩

instance {ι R : Type} [OfScientific R] : OfScientific (ι → R) :=
  ⟨fun m s e _ => OfScientific.ofScientific m s e⟩

end DSP.Channels

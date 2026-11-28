import DSP.FMA
import DSP.IfLess

namespace DSP

/-- A fixed group of independent numeric lanes. The common emitter represents
these values as native packed numbers and ignores any padded lanes. -/
structure Packed (R : Type) (n : Nat) where
  lane : Fin n → R

namespace Packed

def splat {R : Type} {n : Nat} (x : R) : Packed R n :=
  ⟨fun _ => x⟩

def add {R : Type} {n : Nat} [Add R] (a b : Packed R n) : Packed R n :=
  ⟨fun i => a.lane i + b.lane i⟩

def sub {R : Type} {n : Nat} [Sub R] (a b : Packed R n) : Packed R n :=
  ⟨fun i => a.lane i - b.lane i⟩

def mul {R : Type} {n : Nat} [Mul R] (a b : Packed R n) : Packed R n :=
  ⟨fun i => a.lane i * b.lane i⟩

def div {R : Type} {n : Nat} [Div R] (a b : Packed R n) : Packed R n :=
  ⟨fun i => a.lane i / b.lane i⟩

def neg {R : Type} {n : Nat} [Neg R] (a : Packed R n) : Packed R n :=
  ⟨fun i => -a.lane i⟩

def ifLess {R : Type} {n : Nat} [IfLess R] (a b yes no : Packed R n) : Packed R n :=
  ⟨fun i => IfLess.ifLess (a.lane i) (b.lane i) (yes.lane i) (no.lane i)⟩

def fma {R : Type} {n : Nat} [FMA R] (a b c : Packed R n) : Packed R n :=
  ⟨fun i => FMA.fma (a.lane i) (b.lane i) (c.lane i)⟩

def anyDifferent {R : Type} {n : Nat} [BEq R] (a b : Packed R n) : Bool :=
  Fin.foldl n (fun changed i => changed || a.lane i != b.lane i) false

def abs32 {n : Nat} (a : Packed Float32 n) : Packed Float32 n :=
  ⟨fun i => (a.lane i).abs⟩

def floor32 {n : Nat} (a : Packed Float32 n) : Packed Float32 n :=
  ⟨fun i => (a.lane i).floor⟩

def floor64 {n : Nat} (a : Packed Float n) : Packed Float n :=
  ⟨fun i => (a.lane i).floor⟩

def widen {n : Nat} (a : Packed Float32 n) : Packed Float n :=
  ⟨fun i => (a.lane i).toFloat⟩

def narrow {n : Nat} (a : Packed Float n) : Packed Float32 n :=
  ⟨fun i => (a.lane i).toFloat32⟩

instance {R : Type} {n k : Nat} [OfNat R k] : OfNat (Packed R n) k := ⟨splat (OfNat.ofNat k)⟩
instance {R : Type} {n : Nat} [OfScientific R] : OfScientific (Packed R n) :=
  ⟨fun mantissa sign exponent => splat (OfScientific.ofScientific mantissa sign exponent)⟩
instance {R : Type} {n : Nat} [Add R] : Add (Packed R n) := ⟨add⟩
instance {R : Type} {n : Nat} [Sub R] : Sub (Packed R n) := ⟨sub⟩
instance {R : Type} {n : Nat} [Mul R] : Mul (Packed R n) := ⟨mul⟩
instance {R : Type} {n : Nat} [Div R] : Div (Packed R n) := ⟨div⟩
instance {R : Type} {n : Nat} [Neg R] : Neg (Packed R n) := ⟨neg⟩
instance {R : Type} {n : Nat} [IfLess R] : IfLess (Packed R n) := ⟨ifLess⟩
instance {R : Type} {n : Nat} [FMA R] : FMA (Packed R n) := ⟨fma⟩

end Packed
end DSP

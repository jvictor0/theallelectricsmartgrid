import Init.Data.SInt.Float

namespace DSP.Codegen.Index

/-- The range of a native signed index. The surrounding arithmetic stays in Int. -/
def Fits (value : Int) : Prop := -(2 ^ 63) ≤ value ∧ value < 2 ^ 63

def add (left right : Int) (_ : Fits (left + right)) : Int := left + right

def sub (left right : Int) (_ : Fits (left - right)) : Int := left - right

def ofNat (value : Nat) (_ : Fits (Int.ofNat value)) : Int := Int.ofNat value

/-- Size indices use a native size_t with at least 64 bits. -/
def toNat (value : Int) (_ : 0 ≤ value) : Nat := value.toNat

def addNat (left right : Nat) (_ : left + right < 2 ^ 64) : Nat := left + right

/-- Finite inputs whose truncation is strictly inside the saturation endpoints.
    On this domain Lean's saturating conversion agrees with a native cast. -/
def FloatFits (value : Float) : Prop :=
  value.isFinite = true ∧ -(2 ^ 63) < value.toInt64.toInt ∧ value.toInt64.toInt < 2 ^ 63 - 1

def fromFloat (value : Float) (_ : FloatFits value) : Int := value.toInt64.toInt

end DSP.Codegen.Index

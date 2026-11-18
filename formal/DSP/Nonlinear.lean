import DSP.FMA
import DSP.IfLess

namespace DSP

/-- Numeric interpretation of tanh. Reals use the transcendental; executable
instances use the app's clamped cubic rational approximation. These are not
asserted to be equal. -/
class Tanh (R : Type) where
  tanh : R → R

class SquareRoot (R : Type) where
  sqrt : R → R

/-- Match `MultiChannelNumber::Min/Max`, including their operand ordering. -/
def minValue {R : Type} [IfLess R] (a b : R) : R := IfLess.ifLess b a b a
def maxValue {R : Type} [IfLess R] (a b : R) : R := IfLess.ifLess a b b a

/-- The executable approximation, also available over real arithmetic for
future approximation-specific analysis. Preserve the app's explicit FMA. -/
def Tanh.rational {R : Type} [OfNat R 1] [OfNat R 9] [OfNat R 27]
    [Neg R] [Add R] [Mul R] [Div R] [FMA R] [IfLess R] (x : R) : R :=
  let x2 := x * x
  let y := x * (x2 + 27) / FMA.fma (x * 9) x 27
  minValue 1 (maxValue (-1) y)

instance : Tanh Float := ⟨Tanh.rational⟩
instance : Tanh Float32 := ⟨Tanh.rational⟩
instance : SquareRoot Float := ⟨Float.sqrt⟩
instance : SquareRoot Float32 := ⟨Float32.sqrt⟩
instance {ι R : Type} [Tanh R] : Tanh (ι → R) := ⟨fun x i => Tanh.tanh (x i)⟩
instance {ι R : Type} [SquareRoot R] : SquareRoot (ι → R) :=
  ⟨fun x i => SquareRoot.sqrt (x i)⟩

end DSP

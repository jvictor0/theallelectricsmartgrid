import DSP.Transcendental

namespace DSP

/-- Additional preparation operations for integer-delay filters. Phase is in
turns, and `decay x` means exp(-log(1000)*x), keeping exact constants in the
interpretation rather than pretending a decimal literal equals pi or log. -/
class DelayMath (R : Type) where
  exp2 : R → R
  floor : R → R
  phaseTurns : R → R → R
  decay : R → R

def DelayMath.exp2Float (x : Float) : Float := Float.exp2 x
def DelayMath.floorFloat (x : Float) : Float := Float.floor x
def DelayMath.phaseFloat (y x : Float) : Float := Float.atan2 y x / 6.283185307179586476925286766559
def DelayMath.decayFloat (x : Float) : Float := Float.exp (-(6.907755278982137052053974364053 * x))
def DelayMath.exp2Float32 (x : Float32) : Float32 := Float32.exp2 x
def DelayMath.floorFloat32 (x : Float32) : Float32 := Float32.floor x
def DelayMath.phaseFloat32 (y x : Float32) : Float32 := Float32.atan2 y x / 6.283185307179586476925286766559
def DelayMath.decayFloat32 (x : Float32) : Float32 := Float32.exp (-(6.907755278982137052053974364053 * x))

instance : DelayMath Float := ⟨DelayMath.exp2Float, DelayMath.floorFloat, DelayMath.phaseFloat, DelayMath.decayFloat⟩
instance : DelayMath Float32 := ⟨DelayMath.exp2Float32, DelayMath.floorFloat32, DelayMath.phaseFloat32, DelayMath.decayFloat32⟩

end DSP

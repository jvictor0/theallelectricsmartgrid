import Init

namespace DSP

/-- Phase-based operations. Exact proofs use real transcendentals; C++ lowering
uses Math4096. Float instances are libm references, not lookup-table emulation. -/
class Trig (R : Type) where
  sin2pi : R → R
  cos2pi : R → R
  tanPi : R → R

class ExpDecay (R : Type) where
  expNegTwoPi : R → R

def Trig.sinFloat (x : Float) : Float := Float.sin (6.283185307179586476925286766559 * x)
def Trig.cosFloat (x : Float) : Float := Float.cos (6.283185307179586476925286766559 * x)
def Trig.tanFloat (x : Float) : Float := Float.tan (3.1415926535897932384626433832795 * x)
def Trig.sinFloat32 (x : Float32) : Float32 := Float32.sin (6.283185307179586476925286766559 * x)
def Trig.cosFloat32 (x : Float32) : Float32 := Float32.cos (6.283185307179586476925286766559 * x)
def Trig.tanFloat32 (x : Float32) : Float32 := Float32.tan (3.1415926535897932384626433832795 * x)
def ExpDecay.float (x : Float) : Float := Float.exp (-(6.283185307179586476925286766559 * x))
def ExpDecay.float32 (x : Float32) : Float32 := Float32.exp (-(6.283185307179586476925286766559 * x))

instance : Trig Float := ⟨Trig.sinFloat, Trig.cosFloat, Trig.tanFloat⟩
instance : Trig Float32 := ⟨Trig.sinFloat32, Trig.cosFloat32, Trig.tanFloat32⟩
instance : ExpDecay Float := ⟨ExpDecay.float⟩
instance : ExpDecay Float32 := ⟨ExpDecay.float32⟩
instance {ι R : Type} [Trig R] : Trig (ι → R) :=
  ⟨fun x i => Trig.sin2pi (x i), fun x i => Trig.cos2pi (x i), fun x i => Trig.tanPi (x i)⟩
instance {ι R : Type} [ExpDecay R] : ExpDecay (ι → R) :=
  ⟨fun x i => ExpDecay.expNegTwoPi (x i)⟩

end DSP

import DSP.DelayBuffer.Basic
import DSP.Filters.NormalizedAllPass

namespace DSP.TunedDecayComb

/-- Prepared coefficients for one channel of TunedDecayComb.hpp. The tuner
supplies delayGain=r^N, a²+b²=1 and h*(1+k*g+g²)=1. -/
structure Coefficients (capacity : Nat) (R : Type) where
  age : Fin capacity
  a : R
  b : R
  r : R
  delayGain : R
  g : R
  k : R
  h : R
  gain : R
  sign : R
  bypass : Bool

def Coefficients.map {capacity : Nat} {R S : Type} (f : R → S) (c : Coefficients capacity R) :
    Coefficients capacity S :=
  ⟨c.age, f c.a, f c.b, f c.r, f c.delayGain, f c.g, f c.k, f c.h,
    f c.gain, f c.sign, c.bypass⟩

structure Branch (R : Type) where
  allpass : R
  ic1 : R
  ic2 : R
  y : R

structure State (capacity : Nat) (R : Type) where
  buffer : DelayBuffer capacity R
  first : Branch R
  second : Branch R
  y : R

variable {capacity : Nat} {R : Type}

def branchStep [Add R] [Sub R] [Mul R] [OfNat R 0] [OfNat R 2]
    (c : Coefficients capacity R) (s : Branch R) (input delayed : R) : Branch R :=
  let ap := NormalizedAllPass.step ⟨c.a, c.b, c.r, s.allpass, 0⟩
    ⟨delayed * c.delayGain⟩
  let excitation := input + c.sign * ap.y
  let ic1 := c.r * s.ic1
  let ic2 := c.r * s.ic2
  let v1 := c.h * (ic1 + c.g * (excitation - ic2))
  let v2 := ic2 + c.g * v1
  if c.bypass then ⟨ap.s, 0, 0, excitation⟩
  else ⟨ap.s, v1 * 2 - ic1, v2 * 2 - ic2, c.gain * v2⟩

/-- None is settled operation; some mix is a crossfade sample. Both branches
read the old buffer before the one shared write, exactly as in the C++ core.
The external retuning/crossfade scheduler is not part of this sample kernel. -/
def step [Add R] [Sub R] [Mul R] [OfNat R 0] [OfNat R 2]
    (c₀ c₁ : Coefficients capacity R) (mix : Option R)
    (s : State capacity R) (input : Sample R) : State capacity R :=
  let first := branchStep c₀ s.first input.x (s.buffer.read c₀.age)
  match mix with
  | none => ⟨s.buffer.push first.y, first, s.second, first.y⟩
  | some m =>
    let second := branchStep c₁ s.second input.x (s.buffer.read c₁.age)
    let y := first.y + (second.y - first.y) * m
    ⟨s.buffer.push y, first, second, y⟩

end DSP.TunedDecayComb

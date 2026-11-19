import DSP.Filters.SchroederAllPass
import DSP.Filters.TunedDecayComb

namespace DSP

namespace TunedDecayComb.Channels

structure State (capacity : Nat) (ι R : Type) where
  buffer : DelayBuffer capacity (ι → R)
  first : ι → Branch R
  second : ι → Branch R
  y : ι → R

variable {capacity : Nat} {ι R : Type}

def project (ch : ι) (s : State capacity ι R) : TunedDecayComb.State capacity R :=
  ⟨s.buffer.map (fun v => v ch), s.first ch, s.second ch, s.y ch⟩

def step [Add R] [Sub R] [Mul R] [OfNat R 0] [OfNat R 2]
    (c₀ c₁ : ι → Coefficients capacity R) (mix : Option R)
    (s : State capacity ι R) (x : Sample (ι → R)) : State capacity ι R :=
  let first := fun ch => branchStep (c₀ ch) (s.first ch) (x.x ch) (s.buffer.read (c₀ ch).age ch)
  match mix with
  | none => ⟨s.buffer.push (fun ch => (first ch).y), first, s.second, fun ch => (first ch).y⟩
  | some m =>
    let second := fun ch => branchStep (c₁ ch) (s.second ch) (x.x ch) (s.buffer.read (c₁ ch).age ch)
    let y := fun ch => (first ch).y + ((second ch).y - (first ch).y) * m
    ⟨s.buffer.push y, first, second, y⟩

end TunedDecayComb.Channels

namespace SchroederAllPass.Channels

structure Coefficients (capacity : Nat) (ι R : Type) where
  age : ι → Fin capacity
  gain : ι → R

variable {capacity : Nat} {ι R : Type}

def project (ch : ι) (s : State capacity (ι → R)) : State capacity R :=
  ⟨s.buffer.map (fun v => v ch), s.y ch⟩

def step [Neg R] [FMA R] (c : Coefficients capacity ι R) (s : State capacity (ι → R))
    (x : Sample (ι → R)) : State capacity (ι → R) :=
  let y := fun ch => FMA.fma (-(c.gain ch)) (x.x ch) (s.buffer.read (c.age ch) ch)
  let written := fun ch => FMA.fma (c.gain ch) (y ch) (x.x ch)
  ⟨s.buffer.push written, y⟩

end SchroederAllPass.Channels
end DSP

import Init.Data.Vector.Basic

namespace DSP

/-- Fixed-capacity circular storage. `head` is the next slot to overwrite.
An age `i : Fin capacity` denotes a delay of `i.val+1` samples. Reads are pure;
clients may read several taps before committing one new sample with `push`. -/
structure DelayBuffer (capacity : Nat) (R : Type) where
  data : Vector R capacity
  head : Fin capacity

namespace DelayBuffer

variable {capacity : Nat} {R : Type}

/-- Construct a reader from its delay in samples. Zero is intentionally absent. -/
def ofDelay (n : Nat) (hpositive : 0 < n) (hbounded : n ≤ capacity) : Fin capacity :=
  ⟨n - 1, by omega⟩

def empty [OfNat R 0] (h : 0 < capacity) : DelayBuffer capacity R :=
  ⟨Vector.replicate capacity 0, ⟨0, h⟩⟩

/-- Modular subtraction expressed without an overflowing intermediate sum. -/
def address (head age : Fin capacity) : Fin capacity :=
  if h : age.val < head.val then
    ⟨head.val - (age.val + 1), by omega⟩
  else
    ⟨head.val + (capacity - (age.val + 1)), by omega⟩

def advance (head : Fin capacity) : Fin capacity :=
  if h : head.val + 1 < capacity then ⟨head.val + 1, h⟩ else ⟨0, by omega⟩

def read (s : DelayBuffer capacity R) (age : Fin capacity) : R :=
  s.data[(address s.head age).val]

/-- One slot changes, then the head advances. No other stored sample is shifted. -/
def push (s : DelayBuffer capacity R) (x : R) : DelayBuffer capacity R :=
  ⟨s.data.set s.head.val x s.head.isLt, advance s.head⟩

/-- Logical history, newest first; this is a view, not another stored buffer. -/
def history (s : DelayBuffer capacity R) : Fin capacity → R := s.read

def map {S : Type} (f : R → S) (s : DelayBuffer capacity R) : DelayBuffer capacity S :=
  ⟨s.data.map f, s.head⟩

/-- Interleaved channels can select different taps in the same physical ring. -/
def readChannels {ι : Type} (s : DelayBuffer capacity (ι → R))
    (ages : ι → Fin capacity) : ι → R := fun ch => s.read (ages ch) ch

end DelayBuffer
end DSP

import DSP.DelayBuffer.Basic
import DSP.DelayMath
import DSP.FMA
import DSP.Nonlinear

namespace DSP.DelayBuffer

/-- Convert between sample lengths and nonnegative whole-sample indices. -/
class SampleIndex (R : Type) where
  ofNat : Nat → R
  toNat : R → Nat

instance : SampleIndex Float := ⟨Float.ofNat, fun samples => samples.toUInt64.toNat⟩
instance : SampleIndex Float32 := ⟨Float32.ofNat, fun samples => samples.toUInt64.toNat⟩

private structure CubicTap (capacity : Nat) (R : Type) where
  age : Fin capacity
  fraction : R

def fractionalPosition {R : Type} [Neg R] [Sub R] [DelayMath R] (samples : R) : R × R :=
  let rounded := -DelayMath.floor (-samples)
  (rounded, rounded - samples)

def cubicPosition {R : Type} [Neg R] [Sub R] [Mul R] [Div R]
    [IfLess R] [DelayMath R] (period samples : R) : R × R :=
  let magnitude := maxValue samples (-samples)
  let wrapped := magnitude - period * DelayMath.floor (magnitude / period)
  let reflected := minValue wrapped (period - wrapped)
  fractionalPosition reflected

private def cubicTap {capacity : Nat} {R : Type} [OfNat R 2] [Neg R] [Sub R] [Mul R] [Div R]
    [IfLess R] [DelayMath R] [SampleIndex R] (positive : 0 < capacity) (samples : R) :
    CubicTap capacity R :=
  let position := cubicPosition (2 * SampleIndex.ofNat capacity) samples
  let whole := SampleIndex.toNat position.1
  ⟨⟨(whole + capacity - 1) % capacity, Nat.mod_lt _ positive⟩, position.2⟩

private def olderAge {capacity : Nat} (age : Fin capacity) : Fin capacity :=
  ⟨(age.val + 1) % capacity, Nat.mod_lt _ (Nat.zero_lt_of_lt age.isLt)⟩

private def newerAge {capacity : Nat} (age : Fin capacity) : Fin capacity :=
  ⟨(age.val + capacity - 1) % capacity, Nat.mod_lt _ (Nat.zero_lt_of_lt age.isLt)⟩

def cubicInterpolate {R : Type} [OfNat R 2] [OfNat R 3] [OfNat R 4] [OfNat R 5]
    [OfScientific R] [Neg R] [Sub R] [Mul R] [FMA R] (ySub1 y0 y1 y2 a : R) : R :=
  let c := y1 - ySub1
  let d := FMA.fma ySub1 2 (FMA.fma y0 (-5) (y1 * 4)) - y2
  let e := FMA.fma (y0 - y1) 3 (y2 - ySub1)
  FMA.fma (a * 0.5) (FMA.fma a (FMA.fma a e d) c) y0

/-- The nominal tap has room for both the excursion and the cubic read stencil. -/
abbrev CubicOffsetFits (capacity tap depth : Nat) : Prop :=
  depth + 3 ≤ tap ∧ tap + depth + 1 ≤ capacity

private def interpolateAt {capacity : Nat} {ι R : Type}
    [OfNat R 2] [OfNat R 3] [OfNat R 4] [OfNat R 5] [OfScientific R] [Neg R] [Sub R] [Mul R] [FMA R]
    (buffer : DelayBuffer capacity (ι → R)) (tap : CubicTap capacity R) (ch : ι) : R :=
  let next := newerAge tap.age
  cubicInterpolate (buffer.read (olderAge tap.age) ch) (buffer.read tap.age ch)
    (buffer.read next ch) (buffer.read (newerAge next) ch) tap.fraction

/-- Read a common fractional offset from positive integral base taps. The
caller provides space for the interpolation neighbors on both sides. -/
def readCubicOffset {capacity : Nat} {ι R : Type} [OfNat R 2] [OfNat R 3] [OfNat R 4] [OfNat R 5]
    [OfScientific R] [Neg R] [Sub R] [Mul R] [FMA R] [DelayMath R] [SampleIndex R]
    (buffer : DelayBuffer capacity (ι → R)) (base : ι → Nat) (offset : R) : ι → R :=
  let (rounded, fraction) := fractionalPosition offset
  let whole := SampleIndex.toNat rounded
  fun ch =>
    let positive := Nat.zero_lt_of_lt buffer.head.isLt
    let age := ⟨(base ch + whole + capacity - 1) % capacity, Nat.mod_lt _ positive⟩
    interpolateAt buffer ⟨age, fraction⟩ ch

/-- Read fractional delays in samples after writing the current input. Delays
reflect at zero and at capacity before Catmull–Rom interpolation. -/
def readCubic {capacity : Nat} {ι R : Type} [OfNat R 2] [OfNat R 3] [OfNat R 4] [OfNat R 5]
    [OfScientific R] [Neg R] [Sub R] [Mul R] [Div R] [FMA R] [IfLess R] [DelayMath R] [SampleIndex R]
    (buffer : DelayBuffer capacity (ι → R)) (delaySamples : ι → R) : ι → R :=
  fun ch => interpolateAt buffer
    (cubicTap (Nat.zero_lt_of_lt buffer.head.isLt) (delaySamples ch)) ch

end DSP.DelayBuffer

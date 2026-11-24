import DSP.DelayBuffer.Cubic

namespace DSP.DelayBuffer

/-- Four independently addressed ring ages with a packed unsigned lowering. -/
structure QuadAges where
  lane : Fin 4 → UInt32

def QuadAges.pack (first second third fourth : UInt64) : QuadAges :=
  ⟨fun ch =>
    match ch.val with
    | 0 => UInt32.ofNat first.toNat
    | 1 => UInt32.ofNat second.toNat
    | 2 => UInt32.ofNat third.toNat
    | _ => UInt32.ofNat fourth.toNat⟩

/-- Convert nonnegative, representable whole samples independently in each lane. -/
class SampleAges (R : Type) where
  toAges : R → QuadAges

instance {R : Type} [SampleIndex R] : SampleAges (Fin 4 → R) :=
  ⟨fun x => ⟨fun ch => UInt32.ofNat (SampleIndex.toNat (x ch))⟩⟩

/-- Assemble one quad sample by taking each lane from its addressed frame. -/
class Gather (R : Type) where
  gather : (Fin 4 → R) → R

instance {R : Type} : Gather (Fin 4 → R) := ⟨fun values ch => values ch ch⟩

/-- Select one channel and broadcast it to the result channels. -/
class BroadcastLane (R : Type) where
  lane : R → Fin 4 → R

instance {R : Type} : BroadcastLane (Fin 4 → R) := ⟨fun values ch _ => values ch⟩

def pushFrame {R : Type} {capacity : Nat} (sample : R) : StateM (DelayBuffer capacity R) PUnit :=
  modify (·.push sample)

def clear {R : Type} {capacity : Nat} [OfNat R 0] : StateM (DelayBuffer capacity R) PUnit :=
  modify fun s => empty (Nat.zero_lt_of_lt s.head.isLt)

abbrev History (R : Type) := Σ capacity : Nat, DelayBuffer capacity R

class HistoryView (S R : Type) where
  history : S → History R

instance {R : Type} {capacity : Nat} : HistoryView (DelayBuffer capacity R) R :=
  ⟨fun ring => ⟨capacity, ring⟩⟩

private def readWrapped {R : Type} (history : History R) (age : Nat) : R :=
  let ⟨capacity, ring⟩ := history
  ring.read ⟨age % capacity, Nat.mod_lt _ (Nat.zero_lt_of_lt ring.head.isLt)⟩

def readWrappedAge {S R : Type} [HistoryView S R] (age : UInt64) : StateM S R :=
  fun state => (readWrapped (HistoryView.history state) age.toNat, state)

/-- Gather a group of lane-indexed samples from circular storage. The offsets
select frames forward from each lane's starting position. -/
def readAgeOffsets {S R : Type} [HistoryView S R] [Gather R] {count : Nat}
    (ages : QuadAges) (offsets : Fin count → UInt64) : StateM S (Fin count → R) :=
  fun state =>
    let history := HistoryView.history (R := R) state
    let capacity := history.1
    let gather tap := Gather.gather fun ch =>
      readWrapped history ((ages.lane ch).toNat + capacity - (offsets tap).toNat % capacity)
    (gather, state)

/-- Read and interpolate four consecutive ages, from oldest to newest. The
address type can be a shared scalar age or a packed age for each lane. -/
def readCubicAt {S R A : Type} [OfNat R 2] [OfNat R 3] [OfNat R 4] [OfNat R 5]
    [OfScientific R] [Neg R] [Sub R] [Mul R] [FMA R]
    (read : A → StateM S R) (subtract : A → UInt64 → A) (oldest : A) (fraction : R) : StateM S R := do
  let older ← read oldest
  let center ← read (subtract oldest 1)
  let newer ← read (subtract oldest 2)
  let newest ← read (subtract oldest 3)
  pure (cubicInterpolate older center newer newest fraction)

def readCubicAges {S R : Type} [HistoryView S R] [Gather R]
    [OfNat R 2] [OfNat R 3] [OfNat R 4] [OfNat R 5] [OfScientific R] [Neg R] [Sub R] [Mul R] [FMA R]
    (oldest : QuadAges) (fraction : R) : StateM S R := do
  let taps ← readAgeOffsets oldest (fun i : Fin 4 => UInt64.ofNat i.val)
  pure (cubicInterpolate (taps 0) (taps 1) (taps 2) (taps 3) fraction)

end DSP.DelayBuffer

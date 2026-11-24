import PhaseTime.InverseStorage

namespace PhaseTime.InverseRuntime

open DSP.Codegen

/-- Grid endpoints use at most 52 magnitude bits. The adjacent integer grid
    lines still convert exactly to double and the signed arithmetic has headroom. -/
def gridLimit : Int := 4503599627370496

def GridBound (index : Int) : Prop := -gridLimit ≤ index ∧ index ≤ gridLimit

abbrev GridIndex := { index : Int // GridBound index }

/-- Supplied at the native entry after scaling. Within ±2^52 both rounded
    grid endpoints are finite and fall in GridBound. -/
structure PhaseInGrid (phase : Float) : Prop where
  floorFits : Index.FloatFits phase.floor
  ceilFits : Index.FloatFits phase.ceil
  floorBound : GridBound phase.floor.toInt64.toInt
  ceilBound : GridBound phase.ceil.toInt64.toInt

def gridFloor (phase : Float) (bound : PhaseInGrid phase) : GridIndex :=
  ⟨Index.fromFloat phase.floor bound.floorFits, bound.floorBound⟩

def gridCeil (phase : Float) (bound : PhaseInGrid phase) : GridIndex :=
  ⟨Index.fromFloat phase.ceil bound.ceilFits, bound.ceilBound⟩

private theorem offsetFits (offset : Nat) (bound : offset ≤ 8) : Index.Fits (Int.ofNat offset) := by
  unfold Index.Fits
  change -(2 ^ 63 : Int) ≤ (offset : Int) ∧ (offset : Int) < 2 ^ 63
  omega

def gridNext (index : GridIndex) : Int :=
  have bound := index.property
  Index.add index.val 1 (by unfold GridBound gridLimit at bound; unfold Index.Fits; omega)

def gridLowerEdge (highest extra : GridIndex) : Int :=
  have high := highest.property
  have cap := extra.property
  Index.sub highest.val extra.val (by unfold GridBound gridLimit at high cap; unfold Index.Fits; omega)

variable {R : Type} [arithmetic : Arithmetic R] {n : Nat}

/-- The cursor may reach the exclusive right edge after its last batch. -/
abbrev GridCursor (first last : GridIndex) :=
  { index : Int // first.val ≤ index ∧ index ≤ last.val + 1 }

theorem cursorRoomFits (first last : GridIndex) (cursor : GridCursor first last) :
    Index.Fits (last.val - cursor.val) ∧ Index.Fits (last.val - cursor.val + 1) := by
  have hfirst := first.property
  have hlast := last.property
  have hcursor := cursor.property
  unfold GridBound gridLimit at hfirst hlast
  unfold Index.Fits
  omega

def gridRemaining (first last : GridIndex) (cursor : GridCursor first last) : Int :=
  let room := Index.sub last.val cursor.val (cursorRoomFits first last cursor).1
  Index.add room 1 (cursorRoomFits first last cursor).2

def boundedScatterStepM (left right : Point R) (first last : GridIndex)
    (cursor : GridCursor first last) : Action R n (ForInStep (GridCursor first last)) := do
  let remaining := gridRemaining first last cursor
  if active : remaining > 0 then
    let capacity : Int := Index.ofNat batchCapacity (offsetFits batchCapacity (by decide))
    let count := if remaining ≤ capacity then remaining else capacity
    have hcount : 1 ≤ count ∧ count ≤ 8 ∧ count ≤ remaining := by
      unfold count
      have hcapacity : capacity = 8 := rfl
      split <;> omega
    let rest := Index.sub count 1 (by unfold Index.Fits; omega)
    have hrest : 0 ≤ rest := by unfold rest Index.sub; omega
    let extra := Index.toNat rest hrest
    have bounded : extra < batchCapacity := by unfold extra Index.toNat rest Index.sub batchCapacity; omega
    have hc := cursor.property
    have hf := first.property
    have hl := last.property
    have hremaining : remaining = last.val - cursor.val + 1 := rfl
    have hpred : Index.Fits (cursor.val - 1) := by
      unfold GridBound gridLimit at hf hl
      unfold Index.Fits
      omega
    have hnext : Index.Fits (cursor.val + count) := by
      unfold GridBound gridLimit at hf hl
      unfold Index.Fits
      omega
    let predecessor := fun (_ : Unit) => Index.sub cursor.val 1 hpred
    let indexAt (offset : Nat) (beforeEnd : offset ≤ extra) :=
      have hoffset : offset < 8 := by unfold batchCapacity at bounded; omega
      let delta := Index.ofNat offset (offsetFits offset (Nat.le_of_lt hoffset))
      have hdelta : delta = (offset : Int) := rfl
      have inside : first.val ≤ cursor.val + delta ∧ cursor.val + delta ≤ last.val := by
        unfold extra Index.toNat rest Index.sub at beforeEnd
        omega
      Index.add cursor.val delta (by
        unfold GridBound gridLimit at hf hl
        unfold Index.Fits
        omega)
    commitBatchUsingM left right cursor.val extra bounded predecessor indexAt
    let next := Index.add cursor.val count hnext
    have hnextVal : next = cursor.val + count := rfl
    return .yield ⟨next, by omega⟩
  else
    return .done cursor

def boundedScatterM (left right : Point R) (first last : GridIndex) : Action R n PUnit := do
  if available : first.val ≤ last.val then
    let cursor : GridCursor first last := ⟨first.val, by omega⟩
    let remaining := gridRemaining first last cursor
    have hremaining : remaining = last.val - first.val + 1 := rfl
    let fuel := Index.toNat remaining (by omega)
    let _ ← DSP.foldUntilM fuel (boundedScatterStepM left right first last) cursor
    pure ()

end PhaseTime.InverseRuntime

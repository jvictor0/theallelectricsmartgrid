import PhaseTime.InverseRuntime
import DSP.Codegen.Control
import DSP.Codegen.Index
import DSP.Codegen.Scratch

namespace PhaseTime.InverseRuntime

variable {R : Type} [arithmetic : Arithmetic R] {n : Nat}

abbrev Action (R : Type) [a : Arithmetic R] (n : Nat) :=
  StateM (State R a.Delta n)

structure SeedEntry (R : Type) where
  phase : R
  index : Int

namespace Storage

def beginRevision : Action R n PUnit := pure ()
def endRevision : Action R n PUnit := pure ()

def capacityMinusOne : Action R n Int := pure (Int.ofNat n)

def gridResolution : Action Float n Float := fun s => (s.gridResolution, s)

def base (index : Int) : Action R n (Option R) :=
  fun s => ((s.payload.cells.get (wrap n index)).base, s)

def fallback (index : Int) : Action R n (Option R) :=
  fun s => ((s.payload.cells.get (wrap n index)).fallbackEnd, s)

def cut (index : Int) : Action R n (Option R) :=
  fun s => ((s.payload.cells.get (wrap n index)).replacement.map (·.cut), s)

def startDelta (index : Int) : Action R n (Option arithmetic.Delta) :=
  fun s => ((s.payload.cells.get (wrap n index)).replacement.map (·.start), s)

def endDelta (index : Int) : Action R n (Option arithmetic.Delta) :=
  fun s => ((s.payload.cells.get (wrap n index)).replacement.bind (·.stop), s)

def seed : Action R n (Option (SeedEntry R)) :=
  fun s => (s.payload.seed.map (fun phase => ⟨phase, arithmetic.floor phase⟩), s)
def frontier : Action R n (Option (Point R)) := fun s => (s.payload.frontier, s)
def previous : Action R n (Option (Point R)) := fun s => (s.previous, s)
def highest : Action R n (Option Int) := fun s => (s.highest, s)
def hasForward : Action R n Bool := fun s => (s.hasForward, s)
def descending : Action R n Bool := fun s => (s.descending, s)

def sameSlot (a b : Int) : Action R n Bool :=
  pure (wrap n a == wrap n b)

def clearSeed : Action R n PUnit :=
  fun s => (⟨⟩, { s with payload.seed := none })

def storeSeed (phase : R) (_index : {i : Int // i = arithmetic.floor phase}) : Action R n PUnit :=
  fun s => (⟨⟩, { s with payload.seed := some phase })

def storeFrontier (phase time : R) : Action R n PUnit :=
  fun s => (⟨⟩, { s with payload.frontier := some ⟨phase, time⟩ })

def storePrevious (phase time : R) : Action R n PUnit :=
  fun s => (⟨⟩, { s with previous := some ⟨phase, time⟩ })

/-- The previous observation used by the app's captured view is outside State. -/
def storeViewPrevious (_phase _time : R) : Action R n PUnit := pure ()

def storeHighest (index : Int) : Action R n PUnit :=
  fun s => (⟨⟩, { s with highest := some index })

/-- The normalized lower bound for the app's captured view is outside State. -/
def storeOldest (_phase : R) : Action R n PUnit := pure ()

def storeDirection (forward descending : Bool) : Action R n PUnit :=
  fun s => (⟨⟩, { s with hasForward := forward, descending })

def markDescending : Action R n PUnit :=
  fun s => (⟨⟩, { s with descending := true })

def incrementIslands : Action R n PUnit :=
  fun s => (⟨⟩, { s with islandReassignments := s.islandReassignments + 1 })

def clearCell (index : Int) : Action R n PUnit :=
  fun s => (⟨⟩, { s with payload.cells := s.payload.cells.set (wrap n index) {} })

/-- Store the new traversal's endpoints with no replacement. -/
def putCell (index : Int) (time : R) (closed : Bool) (stop : R) : Action R n PUnit :=
  fun s =>
    let cell : Cell R arithmetic.Delta := ⟨some time, if closed then some stop else none, none⟩
    (⟨⟩, { s with payload.cells := s.payload.cells.set (wrap n index) cell })

def putFallback (index : Int) (time : R) : Action R n PUnit :=
  fun s =>
    let slot := wrap n index
    let cell := s.payload.cells.get slot
    (⟨⟩, { s with payload.cells := s.payload.cells.set slot { cell with fallbackEnd := some time } })

def putReplacement (index : Int) (cut : R) (start : arithmetic.Delta) : Action R n PUnit :=
  fun s =>
    let slot := wrap n index
    let cell := s.payload.cells.get slot
    (⟨⟩, { s with payload.cells := s.payload.cells.set slot { cell with replacement := some ⟨cut, start, none⟩ } })

def putEndDelta (index : Int) (closed : Bool) (stop : arithmetic.Delta) : Action R n PUnit :=
  fun s =>
    let slot := wrap n index
    let cell := s.payload.cells.get slot
    let replacement := cell.replacement.map fun branch =>
      { branch with stop := if closed then some stop else none }
    (⟨⟩, { s with payload.cells := s.payload.cells.set slot { cell with replacement } })

end Storage

open Storage in
def loadIntervalM (index : Int) (origin query : R) (decode : R → arithmetic.Delta → Option R) :
    Action R n (R × Option R × Option R) := do
  let branch ← cut index
  if branch.any (fun phase => arithmetic.le phase query) then
    let start ← startDelta index
    let stop ← endDelta index
    return (branch.getD query, start.bind (decode origin), stop.bind (decode origin))
  else
    let entry ← seed
    let stop ← fallback index
    let low := entry.elim (arithmetic.ofIndex index) fun stored =>
      if stored.index = index then stored.phase else arithmetic.ofIndex index
    return (low, some origin, stop)

def closeInterval (leftPhase high : R) (stop : Option R) (last : Option (Point R)) : R × Option R :=
  if stop.isSome then (high, stop)
  else if last.any (fun edge => frontierCloses leftPhase high edge.phase) then
    (last.elim high (·.phase), last.map (·.time))
  else (high, none)

def readLoadedTo (high : R) (loaded : R × Option R × Option R)
    (last : Option (Point R)) (query cutoff : R) : Option R :=
  loaded.2.1.bind fun time =>
    let (rightPhase, rightTime) := closeInterval loaded.1 high loaded.2.2 last
    readInterval ⟨loaded.1, time⟩ rightPhase rightTime query cutoff

def readLoaded (index : Int) (loaded : R × Option R × Option R)
    (last : Option (Point R)) (query cutoff : R) : Option R :=
  readLoadedTo (arithmetic.ofIndex (index + 1)) loaded last query cutoff

open Storage in
def readPayloadUsingM (query cutoff : R) (indexAt : Unit → Int)
    (rightOf : Int → R) (decode : R → arithmetic.Delta → Option R) : Action R n (Option R) := do
  let last ← frontier
  if last.any (fun edge => query == edge.phase) then
    return last.bind fun edge => timeWithinCutoff edge.time cutoff
  else
    let index := indexAt ()
    let some origin ← base index | return none
    let loaded ← loadIntervalM index origin query decode
    return readLoadedTo (rightOf index) loaded last query cutoff

def readPayloadM (query cutoff : R) : Action R n (Option R) :=
  readPayloadUsingM query cutoff (fun _ => arithmetic.floor query)
    (fun index => arithmetic.ofIndex (index + 1)) arithmetic.decode

open Storage in
def retireSeedM (index : Int) : Action R n PUnit := do
  if let some entry ← seed then
    if ← sameSlot entry.index index then clearSeed

open Storage in
def assignCellM (index : Int) (time : R) (closed : Bool) (stop : R) : Action R n PUnit := do
  retireSeedM index
  putCell index time closed stop

open Storage in
def clearSeedCellM : Action R n PUnit := do
  if let some entry ← seed then clearCell entry.index

open Storage in
def seedCellAtM (edge : Point R) (index : Int)
    (hindex : index = arithmetic.floor edge.phase) : Action R n PUnit := do
  clearSeedCellM
  assignCellM index edge.time false edge.time
  storeSeed edge.phase ⟨index, hindex⟩

def seedCellM (edge : Point R) : Action R n PUnit :=
  seedCellAtM edge (arithmetic.floor edge.phase) rfl

open Storage in
def finishCellM (index : Int) (time : R) : Action R n PUnit := do
  let initialEnd ← fallback index
  if initialEnd.isNone then
    putFallback index time
  else if (← cut index).isSome then
    if let some origin ← base index then
      putEndDelta index true (arithmetic.encode origin time)
    else
      putEndDelta index false (arithmetic.encode time time)

open Storage in
def prepareBranchUsingM (edge : Point R) (index : Int) (hindex : index = arithmetic.floor edge.phase)
    (checkRetention : Action R n Bool) (rightPhase : Unit → R) : Action R n PUnit := do
  let retained ← checkRetention
  let origin ← base index
  let entry ← seed
  let belowSeed := entry.any fun stored =>
    if stored.index = index then arithmetic.lt edge.phase stored.phase else false
  if !retained || origin.isNone || belowSeed then
    seedCellAtM edge index hindex
  else if (← fallback index).isSome then
    if let some time := origin then
      let oldCut ← cut index
      let lastEdge ← frontier
      let keep := oldCut.any fun old =>
        lastEdge.any fun last => keepCut last.phase (rightPhase ()) edge.phase old
      if keep then
        putEndDelta index false (arithmetic.encode time time)
      else
        if oldCut.any (fun old => arithmetic.lt old edge.phase) then incrementIslands
        putReplacement index edge.phase (arithmetic.encode time edge.time)

open Storage in
def prepareBranchM (edge : Point R) : Action R n PUnit :=
  let index := arithmetic.floor edge.phase
  let checkRetention : Action R n Bool := do
    let last ← highest
    let extra ← capacityMinusOne
    return last.any fun hi =>
      if hi - extra ≤ index then
        if index ≤ hi then true else false
      else false
  prepareBranchUsingM edge index rfl checkRetention (fun _ => arithmetic.ofIndex (index + 1))

open Storage in
def beginBranchUsingM (edge : Point R) (index : Int) (prepare : Action R n PUnit) : Action R n PUnit := do
  if edge.phase == arithmetic.ofIndex index then return ()
  beginRevision
  prepare
  storeFrontier edge.phase edge.time
  endRevision

def beginBranchM (edge : Point R) : Action R n PUnit :=
  let index := arithmetic.floor edge.phase
  beginBranchUsingM edge index (prepareBranchM edge)


def batchTimesUsing (left right : Point R) (extra : Nat) (bounded : extra < batchCapacity)
    (indexAt : (offset : Nat) → offset ≤ extra → Int) :
    DSP.Codegen.Scratch R batchCapacity (fun i => i < extra + 1) :=
  let count := DSP.Codegen.Index.addNat extra 1 (by unfold batchCapacity at bounded; omega)
  DSP.Codegen.Scratch.ofFn batchCapacity count (by unfold count DSP.Codegen.Index.addNat; omega) fun i =>
    have hi : i.val ≤ extra := by
      have := i.isLt
      unfold count DSP.Codegen.Index.addNat at this
      omega
    (edgeAt left right (indexAt i.val hi)).time

def batchTimes (left right : Point R) (first : Int) (extra : Nat)
    (bounded : extra < batchCapacity) : DSP.Codegen.Scratch R batchCapacity (fun i => i < extra + 1) :=
  batchTimesUsing left right extra bounded (fun offset _ => first + Int.ofNat offset)

def finishPredecessorAtM (left : Point R) (first : Int) (time : R)
    (predecessor : Unit → Int) : Action R n PUnit :=
  if arithmetic.lt left.phase (arithmetic.ofIndex first) then finishCellM (predecessor ()) time
  else pure ()

def finishPredecessorM (left : Point R) (first : Int) (time : R) : Action R n PUnit :=
  finishPredecessorAtM left first time (fun _ => first - 1)

open Storage in
def commitBatchUsingM (left right : Point R) (first : Int) (extra : Nat)
    (bounded : extra < batchCapacity) (predecessor : Unit → Int)
    (indexAt : (offset : Nat) → offset ≤ extra → Int) : Action R n PUnit := do
  let times := batchTimesUsing left right extra bounded indexAt
  beginRevision
  finishPredecessorAtM left first (times.get ⟨0, by decide⟩ (Nat.zero_lt_succ _)) predecessor
  Fin.foldlM extra (fun () i =>
    have hi := i.isLt
    let next := DSP.Codegen.Index.addNat i.val 1 (by unfold batchCapacity at bounded; omega)
    have hnext : next < extra + 1 := by unfold next DSP.Codegen.Index.addNat; omega
    assignCellM (indexAt i.val (by omega))
      (times.get ⟨i.val, by omega⟩ (by change i.val < extra + 1; omega)) true
      (times.get ⟨next, by omega⟩ hnext)) ()
  let finalIndex := indexAt extra (Nat.le_refl extra)
  let finalTime := times.get ⟨extra, bounded⟩ (Nat.lt_succ_self _)
  assignCellM finalIndex finalTime false finalTime
  storeFrontier (arithmetic.ofIndex finalIndex) finalTime
  endRevision

def commitBatchM (left right : Point R) (first : Int) (extra : Nat)
    (bounded : extra < batchCapacity) : Action R n PUnit :=
  commitBatchUsingM left right first extra bounded (fun _ => first - 1)
    (fun offset _ => first + Int.ofNat offset)

structure BatchCursor where
  index : Int
  remaining : Int

def scatterStepM (left right : Point R) (cursor : BatchCursor) :
    Action R n (ForInStep BatchCursor) :=
  if cursor.remaining ≤ 0 then pure (.done cursor)
  else do
    let capacity := Int.ofNat batchCapacity
    let count := if cursor.remaining ≤ capacity then cursor.remaining else capacity
    have bounded : (count - 1).toNat < batchCapacity := by
      dsimp [count, capacity, batchCapacity]
      split <;> omega
    commitBatchM left right cursor.index (count - 1).toNat bounded
    pure (.yield ⟨cursor.index + count, cursor.remaining - count⟩)

def scatterM (left right : Point R) (index remaining : Int) : Action R n PUnit := do
  let _ ← DSP.foldUntilM remaining.toNat (scatterStepM left right) ⟨index, remaining⟩
  pure ()

open Storage in
def openVisitUsingM (openBranchAtStart : Action R n PUnit) : Action R n PUnit := do
  let openBranch ← do
    if ← hasForward then descending else pure true
  if openBranch then openBranchAtStart

def openVisitM (left : Point R) : Action R n PUnit :=
  openVisitUsingM (beginBranchM left)

open Storage in
def publishForwardM (right : Point R) (highest : Int) (oldest : R) : Action R n PUnit := do
  storeHighest highest
  storeOldest oldest
  beginRevision
  storeFrontier right.phase right.time
  endRevision
  storeDirection true false

open Storage in
def writeForwardM (resolution : R) (left right : Point R) : Action R n PUnit := do
  let high ← highest
  let extra ← capacityMinusOne
  openVisitM left
  let first := arithmetic.ceil left.phase
  let last := arithmetic.floor right.phase
  scatterM left right first (last - first + 1)
  let hi := high.getD last
  let newest := if hi ≤ last then last else hi
  publishForwardM right newest (arithmetic.ofIndex (newest - extra) / resolution)

open Storage in
def writeObservedM (observation old : Point R) (hasPrior : Bool)
    (forward : arithmetic.lt old.phase observation.phase = true → Action R n PUnit) : Action R n PUnit := do
  let preceding := if hasPrior then old else observation
  storeViewPrevious preceding.phase preceding.time
  storePrevious observation.phase observation.time
  if hasPrior then
    if advances : arithmetic.lt old.phase observation.phase then
      forward advances
    else if arithmetic.lt observation.phase old.phase then
      markDescending

open Storage in
def writeM (resolution : R) (observation : Point R) : Action R n PUnit := do
  let prior ← previous
  let old := prior.getD observation
  let left := { old with phase := old.phase * resolution }
  let right := { observation with phase := observation.phase * resolution }
  writeObservedM observation old prior.isSome (fun _ => writeForwardM resolution left right)

end PhaseTime.InverseRuntime

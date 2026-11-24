import Init.Data.SInt.Float
import Init.Data.Vector

namespace PhaseTime.InverseRuntime

/-- Phase and absolute sample times share a precision. Only the two replacement
    timestamps use a separate, base-relative representation. -/
class Arithmetic (R : Type) extends Add R, Sub R, Mul R, Div R, BEq R where
  Delta : Type
  lt : R → R → Bool
  le : R → R → Bool
  floor : R → Int
  ceil : R → Int
  ofIndex : Int → R
  encode : R → R → Delta
  decode : R → Delta → Option R

instance : Arithmetic Float where
  lt a b := decide (a < b)
  le a b := decide (a ≤ b)
  floor a := a.floor.toInt64.toInt
  ceil a := a.ceil.toInt64.toInt
  ofIndex i := (Int64.ofInt i).toFloat
  Delta := Float32
  encode base time := (time - base).toFloat32
  decode base delta :=
    let time := base + delta.toFloat
    if time.isFinite then some time else none

structure Point (R : Type) where
  phase : R
  time : R

structure Replacement (R D : Type) where
  cut : R
  start : D
  stop : Option D := none

structure Cell (R D : Type) where
  base : Option R := none
  fallbackEnd : Option R := none
  replacement : Option (Replacement R D) := none

structure Interval (R : Type) where
  left : Point R
  rightPhase : R
  rightTime : Option R

structure Published (R D : Type) (extraCapacity : Nat) where
  cells : Vector (Cell R D) (extraCapacity + 1) := Vector.replicate _ {}
  seed : Option R := none
  frontier : Option (Point R) := none

structure State (R D : Type) (extraCapacity : Nat) where
  payload : Published R D extraCapacity := {}
  highest : Option Int := none
  previous : Option (Point R) := none
  hasForward : Bool := false
  descending : Bool := false
  islandReassignments : Nat := 0

def wrap (extraCapacity : Nat) (index : Int) : Fin (extraCapacity + 1) :=
  let modulus : Int := (extraCapacity : Int) + 1
  have positive : 0 < modulus := by simp [modulus]
  let residue := index % modulus
  have nonnegative : 0 ≤ residue := Int.emod_nonneg index (by omega)
  have bounded : residue < modulus := Int.emod_lt_of_pos index positive
  ⟨residue.toNat, by dsimp only [modulus] at bounded; omega⟩

variable {R : Type} [arithmetic : Arithmetic R] {n : Nat}

def interpolate (left right : Point R) (query : R) : R :=
  let alpha := (query - left.phase) / (right.phase - left.phase)
  left.time + alpha * (right.time - left.time)

def timeWithinCutoff (time cutoff : R) : Option R :=
  if arithmetic.le time cutoff then some time else none

def entryInRange (left : Point R) (query cutoff : R) : Bool :=
  !arithmetic.lt query left.phase && !arithmetic.lt cutoff left.time

def interiorInRange (leftPhase rightPhase query : R) : Bool :=
  !arithmetic.lt rightPhase query && arithmetic.lt leftPhase rightPhase

def frontierCloses (leftPhase rightPhase frontier : R) : Bool :=
  arithmetic.le leftPhase frontier && arithmetic.lt frontier rightPhase

def keepCut (frontier rightPhase newCut oldCut : R) : Bool :=
  arithmetic.le frontier rightPhase && !arithmetic.lt newCut oldCut

def readIntervalWith {Output : Type} (missing : Output) (present : R → Output)
    (left right : Point R) (hasLeft hasRight : Bool) (query cutoff : R) : Output :=
  if !hasLeft || !entryInRange left query cutoff then missing
  else if query == left.phase then present left.time
  else if !hasRight || !interiorInRange left.phase right.phase query then missing
  else
    let time := interpolate left right query
    if arithmetic.le time cutoff then present time else missing

def readInterval (left : Point R) (rightPhase : R) (rightTime : Option R)
    (query cutoff : R) : Option R :=
  let right := ⟨rightPhase, rightTime.getD left.time⟩
  readIntervalWith none some left right true rightTime.isSome query cutoff

def selectInterval (cell : Cell R arithmetic.Delta) (seed : Option R)
    (frontier : Option (Point R)) (index : Int) (query : R) : Option (Interval R) := do
  let base ← cell.base
  let lowPhase := match seed with
    | some phase => if arithmetic.floor phase == index then phase else arithmetic.ofIndex index
    | none => arithmetic.ofIndex index
  let highPhase := arithmetic.ofIndex (index + 1)
  let (leftPhase, leftTime, stop) : R × Option R × Option R := match cell.replacement with
    | some branch =>
      if arithmetic.le branch.cut query then
        (branch.cut, arithmetic.decode base branch.start, branch.stop.bind (arithmetic.decode base))
      else (lowPhase, some base, cell.fallbackEnd)
    | none => (lowPhase, some base, cell.fallbackEnd)
  let leftTime ← leftTime
  let (rightPhase, rightTime) := match stop, frontier with
    | none, some edge =>
      if frontierCloses leftPhase highPhase edge.phase then
        (edge.phase, some edge.time)
      else (highPhase, none)
    | _, _ => (highPhase, stop)
  return ⟨⟨leftPhase, leftTime⟩, rightPhase, rightTime⟩

def readCell (cell : Cell R arithmetic.Delta) (seed : Option R)
    (frontier : Option (Point R)) (index : Int) (query cutoff : R) : Option R :=
  match frontier with
  | some edge =>
    if query == edge.phase then timeWithinCutoff edge.time cutoff
    else (selectInterval cell seed frontier index query).bind fun interval =>
      readInterval interval.left interval.rightPhase interval.rightTime query cutoff
  | none => (selectInterval cell seed frontier index query).bind fun interval =>
    readInterval interval.left interval.rightPhase interval.rightTime query cutoff

def read (s : Published R arithmetic.Delta n) (query cutoff : R) : Option R :=
  let index := arithmetic.floor query
  readCell (s.cells.get (wrap n index)) s.seed s.frontier index query cutoff

def retireSeed (seed : Option R) (slot : Fin (n + 1)) : Option R :=
  seed.filter (fun phase => wrap n (arithmetic.floor phase) != slot)

def assignCell (s : Published R arithmetic.Delta n) (index : Int)
    (time : R) (stop : Option R) : Published R arithmetic.Delta n :=
  let slot := wrap n index
  { s with cells := s.cells.set slot ⟨some time, stop, none⟩,
           seed := retireSeed s.seed slot }

def seedCell (s : Published R arithmetic.Delta n) (edge : Point R) :
    Published R arithmetic.Delta n :=
  let cleared := match s.seed with
    | some previous => { s with cells := s.cells.set (wrap n (arithmetic.floor previous)) {} }
    | none => s
  { (assignCell cleared (arithmetic.floor edge.phase) edge.time none) with seed := some edge.phase }

def finishCell (s : Published R arithmetic.Delta n) (index : Int) (time : R) :
    Published R arithmetic.Delta n :=
  let slot := wrap n index
  let cell := s.cells.get slot
  let finished := match cell.base, cell.fallbackEnd, cell.replacement with
    | _, none, _ => { cell with fallbackEnd := some time }
    | base, some _, some branch =>
      { cell with replacement := some { branch with stop := base.map (fun t => arithmetic.encode t time) } }
    | _, _, _ => cell
  { s with cells := s.cells.set slot finished }

def retained (highest : Option Int) (n : Nat) (index : Int) : Bool :=
  highest.any (fun last => last - Int.ofNat n ≤ index && index ≤ last)

def beginBranch (s : State R arithmetic.Delta n) (edge : Point R) :
    State R arithmetic.Delta n := Id.run do
  let index := arithmetic.floor edge.phase
  if edge.phase == arithmetic.ofIndex index then return s
  let slot := wrap n index
  let cell := s.payload.cells.get slot
  let belowSeed := s.payload.seed.any (fun phase =>
    arithmetic.floor phase == index && arithmetic.lt edge.phase phase)
  let mut payload := s.payload
  let mut islands := s.islandReassignments
  if !retained s.highest n index || cell.base.isNone || belowSeed then
    payload := seedCell payload edge
  else if let (some base, some _) := (cell.base, cell.fallbackEnd) then
    let next : Replacement R arithmetic.Delta := ⟨edge.phase, arithmetic.encode base edge.time, none⟩
    let branch := match cell.replacement with
      | none => next
      | some old =>
        let keep := payload.frontier.any (fun last =>
          keepCut last.phase (arithmetic.ofIndex (index + 1)) edge.phase old.cut)
        if keep then { old with stop := none }
        else next
    if cell.replacement.any (fun old => arithmetic.lt old.cut branch.cut) then
      islands := islands + 1
    payload := { payload with cells := payload.cells.set slot { cell with replacement := some branch } }
  return { s with payload := { payload with frontier := some edge }, islandReassignments := islands }

def batchCapacity : Nat := 8

def edgeAt (left right : Point R) (index : Int) : Point R :=
  let phase := arithmetic.ofIndex index
  ⟨phase, interpolate left right phase⟩

def commitTail (s : Published R arithmetic.Delta n) (left right edge : Point R)
    (index : Int) : Nat → Published R arithmetic.Delta n
  | 0 => { (assignCell s index edge.time none) with frontier := some edge }
  | count + 1 =>
    let next := edgeAt left right (index + 1)
    let completed := assignCell s index edge.time (some next.time)
    commitTail completed left right next (index + 1) count

/-- Commit a nonempty batch. The predecessor is finished before any aliasing
    slot is entered. Interior cells receive both endpoints in one update. -/
def commitBatch (s : Published R arithmetic.Delta n) (left right : Point R)
    (first : Int) (extraEdges : Nat) : Published R arithmetic.Delta n :=
  let edge := edgeAt left right first
  let ready := if arithmetic.lt left.phase edge.phase then finishCell s (first - 1) edge.time else s
  commitTail ready left right edge first extraEdges

def scatter (s : Published R arithmetic.Delta n) (left right : Point R)
    (index : Int) (remaining : Nat) : Published R arithmetic.Delta n :=
  if _ : remaining > 0 then
    let count := min batchCapacity remaining
    scatter (commitBatch s left right index (count - 1)) left right
      (index + Int.ofNat count) (remaining - count)
  else s
termination_by remaining
decreasing_by
  simp only [batchCapacity] at *
  omega

def write (s : State R arithmetic.Delta n) (resolution : R) (observation : Point R) :
    State R arithmetic.Delta n :=
  let observed := { s with previous := some observation }
  match s.previous with
  | none => observed
  | some previous =>
    if arithmetic.lt previous.phase observation.phase then
      let left := { previous with phase := previous.phase * resolution }
      let right := { observation with phase := observation.phase * resolution }
      let opened := if !s.hasForward || s.descending then beginBranch observed left else observed
      let first := arithmetic.ceil left.phase
      let last := arithmetic.floor right.phase
      let payload := scatter opened.payload left right first (last - first + 1).toNat
      { opened with payload := { payload with frontier := some right },
                    highest := some (max last (s.highest.getD last)),
                    hasForward := true, descending := false }
    else if arithmetic.lt observation.phase previous.phase then { observed with descending := true }
    else observed

end PhaseTime.InverseRuntime

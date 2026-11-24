import Batteries.Data.BinaryHeap
import Batteries.Data.Fin.Fold
import DSP.Codegen.Control
import DSP.Resynthesis.Types

namespace DSP.Resynthesis

inductive GateState where
  | low | triggered | high
  deriving BEq, DecidableEq, Inhabited

structure PVDRInput (R P : Type) (n : ℕ) where
  prevMagnitude : BinTable R n
  magnitude : BinTable R n
  phase : BinTable R n
  omega : BinTable P n

structure PhaseLink (P : Type) (n : ℕ) where
  root : ActiveBin n
  delta : P
  small : Bool

structure PVDRGraph (P : Type) (n : ℕ) where
  order : List (ActiveBin n)
  link : BinTable (PhaseLink P n) n

namespace PVDR

structure Entry (R : Type) (n : ℕ) where
  bin : ActiveBin n
  previous : Bool
  magnitude : R

/-- The queue is selected by the backend. Each entry reads its priority from
the immutable analysis input, and equal priorities may pop in either order. -/
structure Queue (R Q : Type) (n : ℕ) where
  empty : Q
  push : Q → Entry R n → Q
  pop : Q → Option (Entry R n) × Q

def makeEntry {R P : Type} {n : ℕ} (input : PVDRInput R P n) (bin : ActiveBin n)
    (previous : Bool) : Entry R n :=
  ⟨bin, previous, if previous then input.prevMagnitude.get bin else input.magnitude.get bin⟩

def binaryQueue {R : Type} {n : ℕ} (lt : R → R → Bool) :
    Queue R (Batteries.BinaryHeap (Entry R n) (fun a b => lt a.magnitude b.magnitude)) n :=
  ⟨Batteries.BinaryHeap.empty _, Batteries.BinaryHeap.insert, Batteries.BinaryHeap.extractMax⟩

def updateGate {R : Type} [OfNat R 0] [LT R] [LE R] [DecidableLT R] [DecidableLE R]
    (low trigger magnitude : R) (gate : GateState) : GateState :=
  if magnitude ≤ 0 ∨ magnitude < low then .low
  else match gate with
    | .low => if trigger < magnitude then .triggered else .low
    | .triggered => .high
    | .high => .high

structure Work (P Q : Type) (n : ℕ) where
  gates : BinTable GateState n
  computed : BinTable Bool n
  links : BinTable (PhaseLink P n) n
  reverseOrder : List (ActiveBin n)
  queue : Q

def record {P Q : Type} {n : ℕ} (work : Work P Q n) (bin : ActiveBin n)
    (link : PhaseLink P n) : Work P Q n :=
  { work with
    computed := work.computed.set bin true
    links := work.links.set bin link
    reverseOrder := bin :: work.reverseOrder }

def pushCurrent {R P Q : Type} {n : ℕ} (queue : Queue R Q n) (input : PVDRInput R P n)
    (work : Work P Q n) (bin : ActiveBin n) : Work P Q n :=
  { work with queue := queue.push work.queue (makeEntry input bin false) }

def attach {R P Q : Type} {n : ℕ} [Sub R] [Add P]
    (widen : R → P) (queue : Queue R Q n) (input : PVDRInput R P n) (parent : ActiveBin n)
    (neighbor : Option (ActiveBin n)) (work : Work P Q n) : Work P Q n :=
  match neighbor with
  | none => work
  | some bin =>
    if work.computed.get bin then work
    else
      let parentLink := work.links.get parent
      let offset := widen (input.phase.get bin - input.phase.get parent)
      let delta := if parentLink.root = parent then offset else offset + parentLink.delta
      pushCurrent queue input (record work bin ⟨parentLink.root, delta, false⟩) bin

def visit {R P Q : Type} {n : ℕ} [Sub R] [Add P] [Mul P]
    (widen : R → P) (hop : P) (queue : Queue R Q n) (input : PVDRInput R P n)
    (entry : Entry R n) (work : Work P Q n) : Work P Q n :=
  let bin := entry.bin
  if entry.previous then
    if work.computed.get bin then work
    else pushCurrent queue input (record work bin ⟨bin, input.omega.get bin * hop, false⟩) bin
  else
    let work := attach widen queue input bin bin.above work
    attach widen queue input bin bin.below work

def drainStep {R P Q : Type} {n : ℕ} [Sub R] [Add P] [Mul P]
    (widen : R → P) (hop : P) (queue : Queue R Q n) (input : PVDRInput R P n)
    (work : Work P Q n) : ForInStep (Work P Q n) :=
  match queue.pop work.queue with
  | (none, _) => .done work
  | (some entry, rest) => .yield (visit widen hop queue input entry { work with queue := rest })

/-- One previous entry and at most one current entry are pushed per bin. -/
def drain {R P Q : Type} {n : ℕ} [Sub R] [Add P] [Mul P]
    (widen : R → P) (hop : P) (queue : Queue R Q n) (input : PVDRInput R P n)
    (fuel : ℕ) (work : Work P Q n) : Work P Q n :=
  foldUntil fuel (drainStep widen hop queue input) work

theorem drain_succ {R P Q : Type} {n : ℕ} [Sub R] [Add P] [Mul P]
    (widen : R → P) (hop : P) (queue : Queue R Q n) (input : PVDRInput R P n)
    (fuel : ℕ) (work : Work P Q n) :
    drain widen hop queue input (fuel + 1) work = (match queue.pop work.queue with
      | (none, _) => work
      | (some entry, rest) =>
        drain widen hop queue input fuel (visit widen hop queue input entry { work with queue := rest })) := by
  rcases popped : queue.pop work.queue with ⟨entry, rest⟩
  cases entry <;> simp [drain, foldUntil, drainStep, popped]

def referenceMagnitude {R P : Type} {n : ℕ} [OfNat R 0] [Max R]
    (input : PVDRInput R P n) : R :=
  Fin.foldl (n / 2 - 1)
    (fun peak bin => max (max peak (input.magnitude.get bin)) (input.prevMagnitude.get bin)) 0

def analyze {R P Q RNG : Type} {n : ℕ} [OfNat R 0] [LT R] [LE R] [DecidableLT R]
    [DecidableLE R] [Max R] [Sub R] [OfNat P 0] [Add P] [Mul P]
    (widen : R → P) (hop : P) (thresholds : R → R × R) (queue : Queue R Q n)
    (nextPhase : StateM RNG R) (gates : BinTable GateState n) (input : PVDRInput R P n) :
    StateM RNG (BinTable GateState n × PVDRGraph P n) := do
  let (low, trigger) := thresholds (referenceMagnitude input)
  let initial : Work P Q n :=
    ⟨gates, BinTable.ofFn (fun _ => false), BinTable.ofFn (fun bin => ⟨bin, 0, true⟩), [], queue.empty⟩
  let work ← (List.finRange (n / 2 - 1)).foldlM (fun work bin => do
    let gate := updateGate low trigger (input.magnitude.get bin) (work.gates.get bin)
    let work := { work with gates := work.gates.set bin gate }
    if gate == .low then
      let delta ← nextPhase
      pure (record work bin ⟨bin, widen delta, true⟩)
    else
      pure { work with queue := queue.push work.queue (makeEntry input bin true) }) initial
  let work := drain widen hop queue input (2 * (n / 2 - 1)) work
  return (work.gates, ⟨work.reverseOrder.reverse, work.links⟩)

end PVDR
end DSP.Resynthesis

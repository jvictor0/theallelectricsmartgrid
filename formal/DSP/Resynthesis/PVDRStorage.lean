import DSP.Resynthesis.PVDR

namespace DSP.Resynthesis.PVDR

/-- The state of a single graph traversal, including its immutable input and
the interpretations of the queue and random source. -/
structure Storage (R P Q RNG : Type) (n : Nat) where
  input : PVDRInput R P n
  queue : Queue R Q n
  draw : StateM RNG R
  rng : RNG
  work : Work P Q n

namespace Storage

variable {R P Q RNG : Type} {n : Nat}

def magnitude (bin : ActiveBin n) : StateM (Storage R P Q RNG n) R :=
  fun state => (state.input.magnitude.get bin, state)

def previousMagnitude (bin : ActiveBin n) : StateM (Storage R P Q RNG n) R :=
  fun state => (state.input.prevMagnitude.get bin, state)

def phase (bin : ActiveBin n) : StateM (Storage R P Q RNG n) R :=
  fun state => (state.input.phase.get bin, state)

def omega (bin : ActiveBin n) : StateM (Storage R P Q RNG n) P :=
  fun state => (state.input.omega.get bin, state)

def gate (bin : ActiveBin n) : StateM (Storage R P Q RNG n) GateState :=
  fun state => (state.work.gates.get bin, state)

def setGate (bin : ActiveBin n) (gate : GateState) : StateM (Storage R P Q RNG n) PUnit :=
  fun state => (⟨⟩, { state with work.gates := state.work.gates.set bin gate })

def computed (bin : ActiveBin n) : StateM (Storage R P Q RNG n) Bool :=
  fun state => (state.work.computed.get bin, state)

def link (bin : ActiveBin n) : StateM (Storage R P Q RNG n) (PhaseLink P n) :=
  fun state => (state.work.links.get bin, state)

/-- Append one visited bin and its associated link to the traversal graph. -/
def put (bin : ActiveBin n) (link : PhaseLink P n) : StateM (Storage R P Q RNG n) PUnit :=
  fun state => (⟨⟩, { state with work := record state.work bin link })

def push (entry : Entry R n) : StateM (Storage R P Q RNG n) PUnit :=
  fun state => (⟨⟩, { state with work.queue := state.queue.push state.work.queue entry })

def pop : StateM (Storage R P Q RNG n) (Option (Entry R n)) :=
  fun state => match state.queue.pop state.work.queue with
  | (none, _) => (none, state)
  | (some entry, rest) => (some entry, { state with work.queue := rest })

def nextPhase : StateM (Storage R P Q RNG n) R :=
  fun state =>
    let next := state.draw.run state.rng
    (next.1, { state with rng := next.2 })

def attach [Sub R] [Add P] (widen : R → P) (parent : ActiveBin n)
    (neighbor : Option (ActiveBin n)) : StateM (Storage R P Q RNG n) PUnit := do
  match neighbor with
  | none => pure ⟨⟩
  | some bin =>
    if ← computed bin then pure ⟨⟩
    else
      let root ← link parent
      let newPhase ← phase bin
      let oldPhase ← phase parent
      let offset := widen (newPhase - oldPhase)
      let delta := if Nat.beq root.root.val parent.val then offset else offset + root.delta
      put bin ⟨root.root, delta, false⟩
      let priority ← magnitude bin
      push ⟨bin, false, priority⟩

def visit [Sub R] [Add P] [Mul P] (widen : R → P) (hop : P)
    (entry : Entry R n) : StateM (Storage R P Q RNG n) PUnit := do
  let bin := entry.bin
  if entry.previous then
    if ← computed bin then pure ⟨⟩
    else
      let frequency ← omega bin
      put bin ⟨bin, frequency * hop, false⟩
      let priority ← magnitude bin
      push ⟨bin, false, priority⟩
  else
    attach widen bin bin.above
    attach widen bin bin.below

def initializeBinWith (classify : R → R → R → GateState → GateState)
    (widen : R → P) (low trigger : R) (bin : ActiveBin n) : StateM (Storage R P Q RNG n) PUnit := do
  let current ← magnitude bin
  let old ← gate bin
  let next := classify low trigger current old
  setGate bin next
  match next with
  | .low =>
    let delta ← nextPhase
    put bin ⟨bin, widen delta, true⟩
  | .triggered | .high =>
    let priority ← previousMagnitude bin
    push ⟨bin, true, priority⟩

def initializeBin [OfNat R 0] [LT R] [LE R] [DecidableLT R] [DecidableLE R]
    (widen : R → P) (low trigger : R) (bin : ActiveBin n) : StateM (Storage R P Q RNG n) PUnit :=
  initializeBinWith updateGate widen low trigger bin

def drain [Sub R] [Add P] [Mul P] (widen : R → P) (hop : P) (fuel : Nat) :
    StateM (Storage R P Q RNG n) PUnit :=
  foldUntilM fuel (fun acc => do
    match ← pop with
    | none => pure (.done acc)
    | some entry =>
      visit widen hop entry
      pure (.yield acc)) (⟨⟩ : PUnit)

def analyzeWith [OfNat R 0] [Max R] [Sub R] [Add P] [Mul P]
    (classify : R → R → R → GateState → GateState)
    (widen : R → P) (hop : P) (thresholds : R → R × R) :
    StateM (Storage R P Q RNG n) PUnit := do
  let peak ← Fin.foldlM (n / 2 - 1) (fun peak bin => do
    let current ← magnitude bin
    let previous ← previousMagnitude bin
    pure (max (max peak current) previous)) 0
  let limits := thresholds peak
  let _ ← Fin.foldlM (n / 2 - 1) (fun _ bin => initializeBinWith classify widen limits.1 limits.2 bin) (⟨⟩ : PUnit)
  drain widen hop (2 * (n / 2 - 1))

def analyze [OfNat R 0] [LT R] [LE R] [DecidableLT R] [DecidableLE R]
    [Max R] [Sub R] [Add P] [Mul P]
    (widen : R → P) (hop : P) (thresholds : R → R × R) :
    StateM (Storage R P Q RNG n) PUnit :=
  analyzeWith updateGate widen hop thresholds

structure Input where
  hop : Float
  schmitt : Bool
  fixedFloor : Float

/-- The historical fixed-floor experiment has no triggered state. -/
def classify [OfNat R 0] [LT R] [LE R] [DecidableLT R] [DecidableLE R]
    (schmitt : Bool) (low trigger magnitude : R) (gate : GateState) : GateState :=
  if schmitt then updateGate low trigger magnitude gate
  else if magnitude < low then .low else .high

abbrev Heap32 := Batteries.BinaryHeap (Entry Float32 4096)
  (fun a b => decide (a.magnitude < b.magnitude))

abbrev Heap64 := Batteries.BinaryHeap (Entry Float 4096)
  (fun a b => decide (a.magnitude < b.magnitude))

def analyze32 (input : Input) : StateM (Storage Float32 Float Heap32 Nat 4096) PUnit :=
  analyzeWith (classify input.schmitt) Float32.toFloat input.hop fun peak =>
    let lowRatio := if input.schmitt then 0.000251188643150958 else input.fixedFloor
    ((peak.toFloat * lowRatio).toFloat32, (peak.toFloat * 0.001).toFloat32)

def analyze64 (input : Input) : StateM (Storage Float Float Heap64 Nat 4096) PUnit :=
  analyzeWith (classify input.schmitt) id input.hop fun peak =>
    let lowRatio := if input.schmitt then 0.000251188643150958 else input.fixedFloor
    (peak * lowRatio, peak * 0.001)

end Storage
end DSP.Resynthesis.PVDR

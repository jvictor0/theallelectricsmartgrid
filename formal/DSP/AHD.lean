import DSP.Codegen.Control

namespace DSP.AHD

abbrev Stage := Fin 5

def idle : Stage := 0
def attack : Stage := 1
def hold : Stage := 2
def decay : Stage := 3
def release : Stage := 4

def isStage (current expected : Stage) : Bool :=
  if current.val = expected.val then true else false

structure Memory (R : Type) where
  stage : Stage
  position : R
  previousSamples : R
  startOutput : R
  rawOutput : R
  endSamples : R
  armed : Bool
  forwardOnly : Bool

structure Input (R : Type) where
  phaseDistance : R
  phaseMagnitude : R
  phaseRatio : R
  period : R
  attackIncrement : R
  holdLoops : R
  decayIncrement : R
  basicPhaseIncrement : R
  releaseFactor : R
  trigger : Bool
  reverseTrigger : Bool
  forwardStartOutput : R
  releaseRequested : Bool

structure Status where
  idle : Bool
  releasing : Bool

structure Travel (R : Type) where
  samples : R
  delta : R
  attackIncrement : R
  holdSamples : R
  decayIncrement : R

section Ordered

variable {R : Type} [OfNat R 0] [OfNat R 1] [Add R] [Sub R] [Mul R] [Div R] [Neg R]
  [LT R] [LE R] [DecidableLT R] [DecidableLE R]

def below (a b : R) : Bool := if a < b then true else false
def atMost (a b : R) : Bool := if a <= b then true else false

def elapsed (i : Input R) (forwardOnly : Bool) : R :=
  let directed := if forwardOnly then i.phaseDistance else i.phaseMagnitude
  directed * i.phaseRatio * i.period

/-- Length of an uninterrupted forward envelope with fixed settings. -/
def completion (i : Input R) (start : R) : R :=
  (1 - start) / i.attackIncrement + i.holdLoops * i.period + 1 / i.decayIncrement

def endDistance (s : Memory R) (samples : R) : R :=
  samples - s.endSamples

def finishDecay (s : Memory R) (samples remaining : R) : Memory R :=
  { s with
    stage := idle
    position := 0
    rawOutput := 0
    endSamples := samples - remaining
    armed := true }

def resumeDecay (s : Memory R) : Memory R :=
  { s with stage := decay, position := 1, rawOutput := 0, armed := false }

def prepareReverse (s : Memory R) (i : Input R) : Memory R :=
  { s with
    stage := idle
    rawOutput := 0
    startOutput := i.forwardStartOutput
    endSamples := completion i i.forwardStartOutput
    armed := true
    forwardOnly := true }

def stageOutput (s : Memory R) : R :=
  if isStage s.stage attack then s.position
  else if isStage s.stage hold then 1
  else if isStage s.stage decay then 1 - s.position
  else 0

def advanceOne (travel : Travel R) (delta : R) : StateM (Memory R) (ForInStep R) := do
  if atMost delta 0 && atMost 0 delta then
    return .done 0
  let s ← get
  let forward := below 0 delta
  let holdSamples := travel.holdSamples
  if isStage s.stage hold && atMost holdSamples 0 then
    set { s with stage := if forward then decay else attack
                 position := if forward then 0 else 1 }
    return .yield delta
  let increment := if isStage s.stage attack then travel.attackIncrement
    else if isStage s.stage hold then 1 / holdSamples else travel.decayIncrement
  if increment <= 0 then
    return .done 0
  let position := s.position + delta * increment
  if !forward && isStage s.stage attack then
    set { s with position := if position < s.startOutput then s.startOutput else position }
    return .done 0
  if (if forward then below position 1 else below 0 position) then
    set { s with position }
    return .done 0
  let boundary := if forward then 1 else 0
  let rest := delta - (boundary - s.position) / increment
  let remaining := if forward then (if rest < 0 then 0 else rest)
    else (if 0 < rest then 0 else rest)
  if forward && isStage s.stage decay then
    set (finishDecay s travel.samples remaining)
    return .done 0
  let next := if isStage s.stage hold then (if forward then decay else attack) else hold
  set { s with stage := next, position := if forward then 0 else 1 }
  return .yield remaining

def advance (round : R → R) (travel : Travel R) : StateM (Memory R) PUnit := do
  -- A signed step can visit at most three stages, including a skipped zero hold.
  let _ ← DSP.foldUntilM 3 (advanceOne travel) travel.delta
  modify fun s => { s with rawOutput := round (stageOutput s), previousSamples := travel.samples }

def advancePhase (step : Travel R → StateM (Memory R) PUnit) (i : Input R) :
    StateM (Memory R) PUnit := do
  let s ← get
  if isStage s.stage idle && !s.armed then
    return ⟨⟩
  let samples := elapsed i s.forwardOnly
  let travel := fun delta => ⟨samples, delta, i.attackIncrement, i.holdLoops * i.period,
    i.decayIncrement⟩
  if s.forwardOnly && below samples 0 then
    set { s with stage := idle, rawOutput := 0, armed := false }
  else if isStage s.stage idle then
    let delta := endDistance s samples
    if delta < 0 then
      set (resumeDecay s)
      step (travel delta)
  else
    step (travel (samples - s.previousSamples))

def advanceRelease (i : Input R) : StateM (Memory R) PUnit := do
  let s ← get
  let delta := i.basicPhaseIncrement * i.phaseRatio * i.period * i.releaseFactor
  let value := s.rawOutput - i.decayIncrement * delta
  if value <= 0 then
    set { s with stage := idle, rawOutput := 0 }
  else
    set { s with rawOutput := value }

def applyEvents (round : R → R) (i : Input R) (before : Memory R) : Memory R :=
  if i.trigger then
    let start := round before.rawOutput
    { before with stage := attack, position := start, startOutput := start
                  previousSamples := 0, armed := false, forwardOnly := false }
  else if i.reverseTrigger then
    prepareReverse before i
  else if i.releaseRequested then
    { before with armed := false
                  stage := if isStage before.stage idle then idle else release }
  else before

def process (round : R → R) (phaseStep : Input R → StateM (Memory R) PUnit) (i : Input R) :
    StateM (Memory R) Status := do
  if i.trigger || i.reverseTrigger || i.releaseRequested then
    modify (applyEvents round i)
  if isStage (← get).stage release then
    advanceRelease i
  else
    phaseStep i
  let after ← get
  return { idle := isStage after.stage idle, releasing := isStage after.stage release }

end Ordered

def roundOutput (x : Float) : Float := x.toFloat32.toFloat
def advanceFloat := advance roundOutput
def advancePhaseFloat := advancePhase advanceFloat
def processFloat := process roundOutput advancePhaseFloat

end DSP.AHD

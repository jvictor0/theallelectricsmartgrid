import DSP.Resynthesis.Grain
import DSP.Codegen.Control

namespace DSP.Resynthesis.SpectrumStorage

abbrev Lane := Fin 4
abbrev Bin := ActiveBin 4096
abbrev Oscillator := Fin 3

structure Visit (P : Type) where
  bin : Bin
  link : PhaseLink P 4096

structure Scene (R P : Type) where
  math : GrainMath R P
  controls : Lane → Controls R
  input : Lane → PVDRInput R P 4096
  gates : Lane → BinTable GateState 4096
  graph : Lane → PVDRGraph P 4096
  magnitudes : Lane → BinTable R 4096
  phases : Lane → Oscillator → BinTable P 4096
  output : Lane → BinTable (ComplexValue R) 4096
  drive : Lane → R

variable {R P : Type}

def controls (lane : Lane) : StateM (Scene R P) (Controls R) :=
  fun state => (state.controls lane, state)

def graphSize (lane : Lane) : StateM (Scene R P) (Fin 2048) :=
  fun state =>
    (⟨min (state.graph lane).order.length 2047, by omega⟩, state)

def visitAt (lane : Lane) (index : Bin) : StateM (Scene R P) (Visit P) :=
  fun state =>
    let graph := state.graph lane
    let bin := graph.order[index.val]?.getD 0
    (⟨bin, graph.link.get bin⟩, state)

def gate (lane : Lane) (bin : Bin) : StateM (Scene R P) GateState :=
  fun state => ((state.gates lane).get bin, state)

def analysisPhase (lane : Lane) (bin : Bin) : StateM (Scene R P) R :=
  fun state => ((state.input lane).phase.get bin, state)

def omega (lane : Lane) (bin : Bin) : StateM (Scene R P) P :=
  fun state => ((state.input lane).omega.get bin, state)

def setOmega (lane : Lane) (bin : Bin) (omega : P) : StateM (Scene R P) PUnit :=
  fun state =>
    let old := state.input lane
    (⟨⟩, {state with input := Function.update state.input lane {old with omega := old.omega.set bin omega}})

def synthesisMagnitude (lane : Lane) (bin : Bin) : StateM (Scene R P) R :=
  fun state => ((state.magnitudes lane).get bin, state)

def phase (lane : Lane) (osc : Oscillator) (bin : Bin) : StateM (Scene R P) P :=
  fun state => ((state.phases lane osc).get bin, state)

def setPhase (lane : Lane) (osc : Oscillator) (bin : Bin) (phase : P) :
    StateM (Scene R P) PUnit :=
  fun state =>
    let phases := state.phases lane
    let updated := Function.update phases osc ((phases osc).set bin phase)
    (⟨⟩, {state with phases := Function.update state.phases lane updated})

def clearOutput [OfNat R 0] : StateM (Scene R P) PUnit :=
  fun state => (⟨⟩, {state with output := fun _ => BinTable.ofFn fun _ => ⟨0, 0⟩})

def coefficient (lane : Lane) (bin : Bin) : StateM (Scene R P) (ComplexValue R) :=
  fun state => ((state.output lane).get bin, state)

def setCoefficient (lane : Lane) (bin : Bin) (value : ComplexValue R) : StateM (Scene R P) PUnit :=
  fun state =>
    (⟨⟩, {state with output := Function.update state.output lane ((state.output lane).set bin value)})

def polar (radius : R) (angle : P) : StateM (Scene R P) (ComplexValue R) :=
  fun state => (state.math.polar radius angle, state)

def round (frequency : P) : StateM (Scene R P) Int :=
  fun state => (state.math.phaseRound frequency, state)

def ratio [Div P] (shift : PitchRatio) : StateM (Scene R P) P :=
  fun state => (state.math.intPhase shift.numerator / state.math.intPhase shift.denominator, state)

def drive (lane : Lane) : StateM (Scene R P) R := fun state => (state.drive lane, state)

def forLanes {S : Type} (count : Fin 5) (step : Lane → StateM S PUnit) : StateM S PUnit :=
  Fin.foldlM count.val (fun _ lane =>
    step ⟨lane.val, by have h := lane.isLt; have k := count.isLt; omega⟩) ⟨⟩

def forVisits (lane : Lane) (step : Visit P → StateM (Scene R P) PUnit) : StateM (Scene R P) PUnit := do
  let count ← graphSize lane
  Fin.foldlM count.val (fun _ index => do
    let visit ← visitAt lane ⟨index.val, by have h := index.isLt; have k := count.isLt; omega⟩
    step visit) ⟨⟩

def fixup [Add P] [Mul P] (widen : R → P) (lane : Lane) (osc : Oscillator) (detune : R) :
    StateM (Scene R P) PUnit :=
  forVisits lane fun visit => do
    let bin := visit.bin
    let link := visit.link
    if Nat.beq link.root.val bin.val then
      let old ← phase lane osc bin
      let analysis ← analysisPhase lane bin
      let gate ← gate lane bin
      let triggered := match gate with
        | .triggered => true
        | _ => false
      setPhase lane osc bin (leaderPhase triggered old (widen analysis) link.delta (widen detune))
    else
      let rootPhase ← phase lane osc link.root
      let rootOmega ← omega lane link.root
      setOmega lane bin rootOmega
      setPhase lane osc bin (followerPhase rootPhase link.delta)

def scatter [Add R] (lane : Lane) (bin : Bin) (radius : R) (angle : P) :
    StateM (Scene R P) PUnit := do
  let prior ← coefficient lane bin
  let addend ← polar radius angle
  setCoefficient lane bin (ComplexValue.add prior addend)

def accumulateDry [Add R] [Mul R] (lane : Lane) (osc : Oscillator) (gain : R) :
    StateM (Scene R P) PUnit :=
  Fin.foldlM 2047 (fun _ bin => do
    let magnitude ← synthesisMagnitude lane bin
    let angle ← phase lane osc bin
    scatter lane bin (gain * magnitude) angle) ⟨⟩

section Shift
variable [Add P] [Sub P] [Mul P] [Div P] [Neg P] [OfScientific P] [LT P] [DecidableLT P]

def accumulateShifted [Add R] [Mul R] [Div R] (widen : R → P) (natSample : Nat → R) (natPhase : Nat → P)
    (lane : Lane) (osc : Oscillator) (gain : R) (shift : PitchRatio) :
    StateM (Scene R P) PUnit := do
  let scale ← ratio shift
  forVisits lane fun visit => do
    if visit.link.small then pure ⟨⟩
    else
      let root := visit.link.root
      let frequency ← omega lane root
      let rootOmega := frequency * scale
      let rounded ← round (rootOmega * natPhase 4096)
      let rootTarget := clampShiftedBin 4096 rounded
      let phase ← phase lane osc root
      let rootPhase := phase * scale
      let target := if Nat.beq root.val visit.bin.val then rootTarget
        else rootTarget - Int.ofNat root.coefficient.val + Int.ofNat visit.bin.coefficient.val
      let angle := if Nat.beq root.val visit.bin.val then rootPhase
        else followerPhase rootPhase visit.link.delta
      match acceptedBinWith (n := 4096) widen natSample natPhase rootOmega target with
      | none => pure ⟨⟩
      | some bin =>
        let magnitude ← synthesisMagnitude lane visit.bin
        scatter lane bin (gain * magnitude) angle

def accumulate [Add R] [Mul R] [Div R] (widen : R → P) (natSample : Nat → R) (natPhase : Nat → P)
    (lane : Lane) (osc : Oscillator) (gain : R) (shift : PitchRatio) : StateM (Scene R P) PUnit := do
  if shift.numerator = 1 ∧ shift.denominator = 1 then accumulateDry lane osc gain
  else accumulateShifted widen natSample natPhase lane osc gain shift

def synthesize [Add R] [Sub R] [Mul R] [Div R] [OfNat R 0] [OfNat R 1] [OfNat R 2]
    [OfNat R 3] [Max R] (widen : R → P) (natSample : Nat → R) (natPhase : Nat → P) (sqrt : R → R)
    (count : Fin 5) : StateM (Scene R P) PUnit := do
  clearOutput
  forLanes count fun lane => do
    let controls ← controls lane
    let _ ← Fin.foldlM 3 (fun _ osc =>
      fixup widen lane osc (oscillatorDetune controls osc)) (⟨⟩ : PUnit)
    let fade := fadeWeight controls.fade
    Fin.foldlM 3 (fun _ osc => do
      let gain := gainFor sqrt controls.unisonGain osc
      accumulateDry lane osc (max 0 (1 - fade) * gain)
      accumulate widen natSample natPhase lane osc (max 0 fade * gain) controls.shift) ⟨⟩

inductive Stage where
  | spectrum | fixup | dry

structure Input where
  stage : Stage
  count : Fin 5
  oscillator : Oscillator

def run [Add R] [Sub R] [Mul R] [Div R] [OfNat R 0] [OfNat R 1] [OfNat R 2]
    [OfNat R 3] [Max R] (widen : R → P) (natSample : Nat → R) (natPhase : Nat → P) (sqrt : R → R)
    (input : Input) : StateM (Scene R P) PUnit :=
  match input.stage with
  | .spectrum => synthesize widen natSample natPhase sqrt input.count
  | .fixup => forLanes input.count fun lane => do
    let detune ← drive lane
    fixup widen lane input.oscillator detune
  | .dry => forLanes input.count fun lane => do
    let gain ← drive lane
    accumulateDry lane input.oscillator gain

def synthesize32 (input : Input) : StateM (Scene Float32 Float) PUnit :=
  run Float32.toFloat Float32.ofNat Float.ofNat Float32.sqrt input

def synthesize64 (input : Input) : StateM (Scene Float Float) PUnit :=
  run id Float.ofNat Float.ofNat Float.sqrt input

end Shift
end DSP.Resynthesis.SpectrumStorage

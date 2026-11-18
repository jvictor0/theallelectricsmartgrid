import DSP.Filters.MultibandSaturator
import DSP.Tests.Splitter

namespace DSP.Tests.Multiband

def sectionFields {R : Type} (s : TPTSVF R) : List R :=
  [s.k,s.a1,s.a2,s.a3,s.ic1,s.ic2,s.lowPass,s.bandPass,s.highPass,s.notch,s.allPass]

def mapSection {R S : Type} (f : R → S) (s : TPTSVF R) : TPTSVF S :=
  ⟨f s.k,f s.a1,f s.a2,f s.a3,f s.ic1,f s.ic2,f s.lowPass,f s.bandPass,
    f s.highPass,f s.notch,f s.allPass⟩

def mapPhase {R S : Type} (f : R → S) : (t : LinkwitzRileySplitter.Topology) → LinkwitzRileySplitter.PhaseState R t → LinkwitzRileySplitter.PhaseState S t
  | .leaf, s => ⟨f s.x⟩
  | .branch low high, s => ⟨mapSection f s.svf, mapPhase f low s.low, mapPhase f high s.high⟩

def mapTree {R S : Type} (f : R → S) : (t : LinkwitzRileySplitter.Topology) → LinkwitzRileySplitter.TreeState R t → LinkwitzRileySplitter.TreeState S t
  | .leaf, s => ⟨f s.x⟩
  | .branch low high, s =>
    ⟨mapSection f s.first,mapSection f s.second,mapPhase f high s.lowPhase,
      mapPhase f low s.highPhase,mapTree f low s.low,mapTree f high s.high⟩

def mapCutoffs {R S : Type} (f : R → S) : (t : LinkwitzRileySplitter.Topology) → LinkwitzRileySplitter.TreeData R t → LinkwitzRileySplitter.TreeData S t
  | .leaf, _ => ⟨⟩
  | .branch low high, s => ⟨f s.value,mapCutoffs f low s.low,mapCutoffs f high s.high⟩

def snapshotFields {R : Type} : (t : LinkwitzRileySplitter.Topology) → LinkwitzRileySplitter.TreeData (TPTSVF.Snapshot R) t → List R
  | .leaf, _ => []
  | .branch low high, s => [s.value.g,s.value.k] ++ snapshotFields low s.low ++ snapshotFields high s.high

/-- Independent scalar expression for the app's unnormalized rational tanh. -/
def referenceTanh (x : Float) : Float :=
  max (-1) (min 1 (x * (27 + x*x) / (27 + 9*x*x)))

def checkNear (label : String) (tolerance a b : Float) : IO Unit :=
  unless (a-b).abs ≤ tolerance * (1+b.abs) do
    throw (IO.userError s!"{label}: got {a}, expected {b}")

def checkPrecision {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfNat R 8]
    [Add R] [Sub R] [Neg R] [Mul R] [Div R] [FMA R] [Trig R] [Tanh R]
    (name : String) (fromFloat : Float → R) (toFloat : R → Float) (bits : R → UInt64)
    (tolerance : Float) : IO Unit := do
  for extraBands in [0,1,2,3,4,6,7] do
    let n := extraBands + 1
    let topology := LinkwitzRileySplitter.Topology.balanced n
    let spec : MultibandSaturator.LogicalSpec n R :=
      ⟨⟨mapCutoffs fromFloat topology (Splitter.cutoffTree n topology 0)⟩,
        fun i => fromFloat (if i.val % 3 == 1 then 0 else 0.25 + Float.ofNat i.val / 2), fromFloat 1.7⟩
    let seeded := Splitter.seeded n (fromFloat 0.25)
    let prepared := MultibandSaturator.prepare spec
    let installed := LinkwitzRileySplitter.installPrepared prepared.splitter seeded
    let original := LinkwitzRileySplitter.computeCoefficients spec.splitter seeded
    unless (Splitter.fields sectionFields installed).map bits == (Splitter.fields sectionFields original).map bits do
      throw (IO.userError s!"{name}/{n}: prepared installation differs from the existing LR tuner")
    unless (snapshotFields topology prepared.splitter.snapshot.tree).map bits ==
        (snapshotFields topology (LinkwitzRileySplitter.makeSnapshot spec.splitter).tree).map bits do
      throw (IO.userError s!"{name}/{n}: prepared snapshot differs from logical tuning")
    let reset := LinkwitzRileySplitter.reset installed
    let fresh := LinkwitzRileySplitter.computeCoefficients spec.splitter (LinkwitzRileySplitter.zero n)
    unless (Splitter.fields sectionFields reset).map bits == (Splitter.fields sectionFields fresh).map bits do
      throw (IO.userError s!"{name}/{n}: reset did not clear all histories while retaining coefficients")
    let initial := MultibandSaturator.installPrepared prepared MultibandSaturator.zero
    let mut state := initial
    let mut reference := fresh
    for sample in [:256] do
      let x := fromFloat (if sample == 0 then 3.0 else 2.0*(Float.ofNat sample*0.17).sin)
      let (next, out) := MultibandSaturator.step state ⟨x⟩
      state := next
      reference := LinkwitzRileySplitter.step reference ⟨x⟩
      unless (Splitter.fields sectionFields state.splitter).map bits ==
          (Splitter.fields sectionFields reference).map bits do
        throw (IO.userError s!"{name}/{n}: saturation affected the splitter state")
      let mut combined := 0.0
      for band in List.finRange n do
        let before := toFloat (reference.bands band * spec.bandGain band)
        let after := referenceTanh before
        checkNear s!"{name}/{n}: band input" tolerance (toFloat (out.bands band).before) before
        checkNear s!"{name}/{n}: band output" tolerance (toFloat (out.bands band).after) after
        combined := combined + after
      let beforeMaster := combined * toFloat spec.masterGain
      checkNear s!"{name}/{n}: master input" tolerance (toFloat out.master.before) beforeMaster
      checkNear s!"{name}/{n}: master output" tolerance (toFloat out.master.after) (referenceTanh beforeMaster)
      unless bits out.sub == bits (fromFloat 0) do
        throw (IO.userError s!"{name}/{n}: independent mode produced a sub output")
    let reset := MultibandSaturator.reset state
    unless (Splitter.fields sectionFields reset.splitter).map bits ==
        (Splitter.fields sectionFields initial.splitter).map bits &&
        (List.ofFn reset.bandGain).map bits == (List.ofFn initial.bandGain).map bits &&
        bits reset.masterGain == bits initial.masterGain do
      throw (IO.userError s!"{name}/{n}: multiband reset changed tuning")
    let changed : MultibandSaturator.LogicalSpec n R := {spec with masterGain := fromFloat 0}
    let retuned := MultibandSaturator.computeCoefficients changed state
    unless (Splitter.fields sectionFields retuned.splitter).map bits ==
        (Splitter.fields sectionFields state.splitter).map bits do
      throw (IO.userError s!"{name}/{n}: coefficient update changed history")
    let (_, muted) := MultibandSaturator.step retuned ⟨fromFloat 1⟩
    checkNear s!"{name}/{n}: master mute" 0 (toFloat muted.master.after) 0

/-- Materialize recurrence fields between samples, just as the generated target
stores concrete vector fields. Keep the data wrappers until after mapping the
whole tree: projecting inside the mapping lambda defers evaluation again. -/
def freeze {n c : Nat} (state : MultibandSaturator.State n (Fin c → Float)) : MultibandSaturator.State n (Fin c → Float) :=
  let tree := mapTree (StateSpace.snapshot c) _ state.splitter.tree
  {state with splitter := {state.splitter with tree := mapTree StateSpace.Snapshot.values _ tree}}

def checkChannels (c : Nat) [NeZero c] (mono : Bool) : IO Unit := do
  let scalar := Splitter.spec4 (0.12 : Float) 0.03 0.3
  let spec : MultibandSaturator.LogicalSpec 4 (Fin c → Float) :=
    ⟨⟨mapCutoffs (fun f lane => f * (if mono then 1 else 1 + 0.1*Float.ofNat lane.val)) _ scalar.tree⟩,
      fun band lane => (0.5 + Float.ofNat band.val / 2) *
        (if mono then 1 else 1 + Float.ofNat lane.val / 3), fun _ => 1.4⟩
  let mut state := freeze (MultibandSaturator.computeCoefficients spec MultibandSaturator.zero)
  let mut reference := (StateSpace.snapshot c fun lane =>
    LinkwitzRileySplitter.computeCoefficients ⟨mapCutoffs (· lane) _ spec.splitter.tree⟩ (LinkwitzRileySplitter.zero 4)).values
  for sample in [:256] do
    let input := (StateSpace.snapshot c fun lane =>
      (Float.ofNat (sample+1) * (0.13 + 0.03*Float.ofNat lane.val)).sin *
        (0.5 + Float.ofNat lane.val)).values
    let (next, out) := if mono then MultibandSaturator.stepMonoBass state ⟨input⟩ else MultibandSaturator.step state ⟨input⟩
    reference := (StateSpace.snapshot c fun lane => LinkwitzRileySplitter.step (reference lane) ⟨input lane⟩).values
    let rawBass := (List.ofFn fun lane => (reference lane).bands 0).foldl (·+·) 0 / Float.ofNat c
    for lane in List.finRange c do
      let mut combined := 0.0
      let mut sub := 0.0
      for band in List.finRange 4 do
        let raw := if mono && band.val == 0 then rawBass else (reference lane).bands band
        let before := raw * spec.bandGain band lane
        let after := referenceTanh before
        checkNear "channel band input" 1e-12 ((out.bands band).before lane) before
        checkNear "channel band output" 1e-12 ((out.bands band).after lane) after
        if mono && band.val == 0 then sub := after else combined := combined + after
      let beforeMaster := (combined+sub) * spec.masterGain lane
      checkNear "channel sub" 1e-12 (out.sub lane) sub
      checkNear "channel master input" 1e-12 (out.master.before lane) beforeMaster
      checkNear "channel master output" 1e-12 (out.master.after lane) (referenceTanh beforeMaster)
    state := freeze next

def checkPackedPreparation : IO Unit := do
  let cutoffs : Fin 3 → Float := fun i => if i.val == 0 then 0.001 else if i.val == 1 then 0.1 else 0.49
  let packed := LinkwitzRileySplitter.prepareSection cutoffs
  for lane in List.finRange 3 do
    let scalar := LinkwitzRileySplitter.prepareSection (cutoffs lane)
    unless [packed.snapshot.g lane,packed.snapshot.k lane,packed.a1 lane,packed.a2 lane,packed.a3 lane].map Float.toBits ==
        [scalar.snapshot.g,scalar.snapshot.k,scalar.a1,scalar.a2,scalar.a3].map Float.toBits do
      throw (IO.userError "packed cutoff preparation differs from scalar preparation")

def check : IO Unit := do
  checkPrecision "Float" id id Float.toBits 1e-12
  checkPrecision "Float32" Float.toFloat32 Float32.toFloat (fun x => x.toBits.toUInt64) 2e-6
  checkPackedPreparation
  checkChannels 1 false
  checkChannels 2 false
  checkChannels 3 false
  checkChannels 4 false
  checkChannels 2 true
  checkChannels 4 true
  IO.println "Multiband: prepared LR coefficients, Float/Float32 processing, meter taps, reset, packed cutoffs, independent lanes and mono bass passed."

end DSP.Tests.Multiband

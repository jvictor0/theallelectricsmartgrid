import DSP.Resynthesis.SpectrumFixupProof
import DSP.Resynthesis.SpectrumScatterProofs

namespace DSP.Resynthesis.SpectrumStorage

private theorem runStageBind {S A B : Type} (action : StateM S A) (next : A → StateM S B)
    (state : S) : (action >>= next).run state = (next (action.run state).1).run (action.run state).2 := rfl

private theorem fold_preserves {S I A : Type} (step : I → StateM S PUnit)
    (project : S → A) (agreement : ∀ state i, project ((step i).run state).2 = project state)
    (indices : List I) (state : S) :
    project (((indices.foldlM (fun _ i => step i) PUnit.unit).run state).2) = project state := by
  induction indices generalizing state with
  | nil => rfl
  | cons i indices ih =>
    simp only [List.foldlM_cons, runStageBind]
    rw [ih, agreement]

variable {R P : Type}

def fixOscillators [Add P] [Mul P] [OfNat R 1] [Sub R] [Div R]
    (widen : R → P) (lane : Lane) (control : Controls R) : StateM (Scene R P) PUnit :=
  Fin.foldlM 3 (fun _ osc => fixup widen lane osc (oscillatorDetune control osc)) PUnit.unit

private theorem fixup_preserves [Add P] [Mul P] (widen : R → P) (lane : Lane)
    (osc : Oscillator) (detune : R) {A : Type} (project : Scene R P → A)
    (phaseWrite : ∀ state bin value, project ((setPhase lane osc bin value).run state).2 = project state)
    (omegaWrite : ∀ state bin value, ((state.graph lane).link.get bin).root ≠ bin →
      project ((setOmega lane bin value).run state).2 = project state)
    (state : Scene R P) :
    project ((fixup widen lane osc detune).run state).2 = project state := by
  unfold fixup forVisits
  simp only [runStageBind]
  rw [Fin.foldlM_eq_foldlM_finRange]
  apply fold_preserves
  intro state index
  simp only [runStageBind]
  have hvisit : ∀ (index : Bin), (visitAt lane index).run state =
      (⟨(state.graph lane).order[index.val]?.getD 0,
        (state.graph lane).link.get ((state.graph lane).order[index.val]?.getD 0)⟩, state) :=
    fun _ => rfl
  simp only [hvisit, Nat.beq_eq, ← Fin.ext_iff]
  split
  · simp only [runStageBind, phaseWrite]
    rfl
  · rename_i follower
    simp only [runStageBind, phaseWrite]
    exact omegaWrite state _ _ follower

private theorem fixup_keeps_context [Add P] [Mul P] (widen : R → P) (lane : Lane)
    (osc : Oscillator) (detune : R) (state : Scene R P) :
    let next := ((fixup widen lane osc detune).run state).2
    (next.math, next.controls, next.gates, next.graph, next.magnitudes, next.output, next.drive,
      fun l => (next.input l).phase) =
    (state.math, state.controls, state.gates, state.graph, state.magnitudes, state.output, state.drive,
      fun l => (state.input l).phase) := by
  refine fixup_preserves widen lane osc detune (fun s =>
    (s.math, s.controls, s.gates, s.graph, s.magnitudes, s.output, s.drive,
      fun l => (s.input l).phase)) ?_ ?_ state
  · intros
    rfl
  · intros
    dsimp [setOmega, StateT.run]
    congr 7
    funext l
    by_cases h : l = lane
    · subst l
      rw [Function.update_self]
    · rw [Function.update_of_ne h]

private theorem fixup_keeps_other_phase [Add P] [Mul P] (widen : R → P) (lane target : Lane)
    (osc chosen : Oscillator) (detune : R) (state : Scene R P)
    (other : lane ≠ target ∨ osc ≠ chosen) :
    ((fixup widen lane osc detune).run state).2.phases target chosen = state.phases target chosen := by
  refine fixup_preserves widen lane osc detune (fun s => s.phases target chosen) ?_ ?_ state
  · intro state bin value
    dsimp [setPhase, StateT.run]
    rcases other with h | h
    · rw [Function.update_of_ne (Ne.symm h)]
    · by_cases hl : target = lane
      · subst target
        rw [Function.update_self, Function.update_of_ne (Ne.symm h)]
      · rw [Function.update_of_ne hl]
  · intros
    rfl


private theorem fold_fixup_phase [Add P] [Mul P] [OfNat R 1] [Sub R] [Div R]
    (widen : R → P) (lane : Lane) (control : Controls R) (chosen : Oscillator)
    (oscillators : List Oscillator) (unique : oscillators.Nodup) (state : Scene R P)
    (bound : (state.graph lane).order.length ≤ 2047) (sameWiden : state.math.widen = widen) :
    let next := ((oscillators.foldlM (fun _ osc =>
      fixup widen lane osc (oscillatorDetune control osc)) PUnit.unit).run state).2
    next.phases lane chosen =
      if chosen ∈ oscillators then
        fixupPhases state.math (oscillatorDetune control chosen) (state.input lane)
          (state.gates lane) (state.graph lane) (state.phases lane chosen)
      else state.phases lane chosen := by
  induction oscillators generalizing state with
  | nil => simp [List.foldlM_nil]; rfl
  | cons osc rest ih =>
    simp only [List.foldlM_cons, runStageBind]
    obtain ⟨absent, distinct⟩ := List.nodup_cons.mp unique
    let next := ((fixup widen lane osc (oscillatorDetune control osc)).run state).2
    change (StateT.run (rest.foldlM (fun _ osc =>
      fixup widen lane osc (oscillatorDetune control osc)) PUnit.unit) next).2.phases lane chosen = _
    have context := fixup_keeps_context widen lane osc (oscillatorDetune control osc) state
    change (next.math, next.controls, next.gates, next.graph, next.magnitudes, next.output,
      next.drive, fun l => (next.input l).phase) = _ at context
    simp only [Prod.mk.injEq] at context
    obtain ⟨math, _, gates, graph, _, _, _, analysis⟩ := context
    rw [ih distinct next (graph ▸ bound) (math ▸ sameWiden)]
    by_cases step : osc = chosen
    · subst osc
      simp only [absent, if_false, List.mem_cons, true_or, if_true]
      exact fixup_phases_eq widen lane chosen (oscillatorDetune control chosen) state bound sameWiden
    · have kept := fixup_keeps_other_phase widen lane lane osc chosen
        (oscillatorDetune control osc) state (Or.inr step)
      change next.phases lane chosen = _ at kept
      have analysis' := congrFun analysis lane
      simp only [List.mem_cons, Ne.symm step, false_or, math, gates, graph, kept]
      split
      · simp only [fixupPhases, analysis']
      · rfl

theorem fixOscillators_phases_eq [Add P] [Mul P] [OfNat R 1] [Sub R] [Div R]
    (widen : R → P) (lane : Lane) (control : Controls R) (state : Scene R P)
    (bound : (state.graph lane).order.length ≤ 2047) (sameWiden : state.math.widen = widen)
    (osc : Oscillator) :
    ((fixOscillators widen lane control).run state).2.phases lane osc =
      fixupPhases state.math (oscillatorDetune control osc) (state.input lane)
        (state.gates lane) (state.graph lane) (state.phases lane osc) := by
  rw [fixOscillators, Fin.foldlM_eq_foldlM_finRange]
  simpa only [List.mem_finRange, if_true] using
    fold_fixup_phase widen lane control osc (List.finRange 3) (List.nodup_finRange 3)
      state bound sameWiden


private theorem fixup_keeps_root_omega [Add P] [Mul P] (widen : R → P) (lane : Lane)
    (osc : Oscillator) (detune : R) (state : Scene R P) (root : Bin)
    (self : ((state.graph lane).link.get root).root = root) :
    (((fixup widen lane osc detune).run state).2.input lane).omega.get root =
      (state.input lane).omega.get root := by
  let project := fun s : Scene R P => if ((s.graph lane).link.get root).root = root
    then some ((s.input lane).omega.get root) else none
  have kept := fixup_preserves widen lane osc detune project (fun _ _ _ => rfl) (by
    intro s bin value follower
    dsimp [project, setOmega, StateT.run]
    rw [Function.update_self]
    split
    · rename_i self'
      have distinct : bin ≠ root := by
        intro eq
        exact follower (eq.symm ▸ self')
      congr 1
      unfold BinTable.get BinTable.set
      exact Vector.getElem_set_ne bin.isLt root.isLt (fun h => distinct (Fin.ext h))
    · rfl) state
  have context := fixup_keeps_context widen lane osc detune state
  simp only [Prod.mk.injEq] at context
  obtain ⟨_, _, _, graph, _, _, _, _⟩ := context
  dsimp [project] at kept
  simpa only [graph, self, if_true, Option.some.injEq] using kept

private theorem fixOscillators_keeps_context [Add P] [Mul P] [OfNat R 1] [Sub R] [Div R]
    (widen : R → P) (lane : Lane) (control : Controls R) (state : Scene R P) :
    let next := ((fixOscillators widen lane control).run state).2
    (next.math, next.controls, next.gates, next.graph, next.magnitudes, next.output, next.drive,
      fun l => (next.input l).phase) =
    (state.math, state.controls, state.gates, state.graph, state.magnitudes, state.output, state.drive,
      fun l => (state.input l).phase) := by
  rw [fixOscillators, Fin.foldlM_eq_foldlM_finRange]
  exact fold_preserves _ (fun s =>
    (s.math, s.controls, s.gates, s.graph, s.magnitudes, s.output, s.drive,
      fun l => (s.input l).phase))
    (fun s osc => fixup_keeps_context widen lane osc (oscillatorDetune control osc) s) _ state

private theorem fixOscillators_keeps_root_omega [Add P] [Mul P] [OfNat R 1] [Sub R] [Div R]
    (widen : R → P) (lane : Lane) (control : Controls R) (state : Scene R P) (root : Bin)
    (self : ((state.graph lane).link.get root).root = root) :
    (((fixOscillators widen lane control).run state).2.input lane).omega.get root =
      (state.input lane).omega.get root := by
  let project := fun s : Scene R P => if ((s.graph lane).link.get root).root = root
    then some ((s.input lane).omega.get root) else none
  have each : ∀ s osc,
      project ((fixup widen lane osc (oscillatorDetune control osc)).run s).2 = project s := by
    intro s osc
    have context := fixup_keeps_context widen lane osc (oscillatorDetune control osc) s
    simp only [Prod.mk.injEq] at context
    obtain ⟨_, _, _, graph, _, _, _, _⟩ := context
    dsimp [project]
    rw [graph]
    split
    · rename_i self'
      rw [fixup_keeps_root_omega widen lane osc (oscillatorDetune control osc) s root self']
    · rfl
  have kept := fold_preserves _ project each (List.finRange 3) state
  have context := fixOscillators_keeps_context widen lane control state
  simp only [Prod.mk.injEq] at context
  obtain ⟨_, _, _, graph, _, _, _, _⟩ := context
  rw [fixOscillators, Fin.foldlM_eq_foldlM_finRange] at graph ⊢
  dsimp [project] at kept
  simpa only [graph, self, if_true, Option.some.injEq] using kept

section Scatter

variable [Add R] [Sub R] [Mul R] [Div R] [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfNat R 3] [Max R]
variable [Add P] [Sub P] [Mul P] [Div P] [Neg P] [OfScientific P] [LT P] [DecidableLT P]

def accumulateOscillators (widen : R → P) (natSample : Nat → R) (natPhase : Nat → P)
    (sqrt : R → R) (lane : Lane) (control : Controls R) : StateM (Scene R P) PUnit :=
  let fade := fadeWeight control.fade
  Fin.foldlM 3 (fun _ osc => do
    let gain := gainFor sqrt control.unisonGain osc
    accumulateDry lane osc (max 0 (1 - fade) * gain)
    accumulate widen natSample natPhase lane osc (max 0 fade * gain) control.shift) PUnit.unit

private def spectrumUpdate (state : Scene R P) (lane : Lane) (control : Controls R)
    (output : BinTable (ComplexValue R) 4096) (osc : Oscillator) :
    BinTable (ComplexValue R) 4096 :=
  let fade := fadeWeight control.fade
  let gain := oscillatorGain state.math control osc
  let output := DSP.Resynthesis.accumulateDry state.math (max 0 (1 - fade) * gain)
    (state.magnitudes lane) (state.phases lane osc) output
  let shiftedGain := max 0 fade * gain
  if control.shift = .unity then
    DSP.Resynthesis.accumulateDry state.math shiftedGain (state.magnitudes lane)
      (state.phases lane osc) output
  else
    DSP.Resynthesis.accumulateShifted state.math shiftedGain control.shift (state.input lane).omega
      (state.magnitudes lane) (state.phases lane osc) (state.graph lane) output

private theorem accumulateOscillators_run (state : Scene R P) (lane : Lane) (control : Controls R)
    (bound : (state.graph lane).order.length ≤ 2047) :
    (accumulateOscillators state.math.widen state.math.natSample state.math.natPhase state.math.sqrt
      lane control).run state =
        (PUnit.unit, replaceOutput state lane ((List.finRange 3).foldl
          (spectrumUpdate state lane control) (state.output lane))) := by
  let step := fun osc => do
    let fade := fadeWeight control.fade
    let gain := oscillatorGain state.math control osc
    accumulateDry lane osc (max 0 (1 - fade) * gain)
    accumulate state.math.widen state.math.natSample state.math.natPhase lane osc
      (max 0 fade * gain) control.shift
  have each : ∀ output osc, (step osc).run (replaceOutput state lane output) =
      (PUnit.unit, replaceOutput state lane (spectrumUpdate state lane control output osc)) := by
    intro output osc
    have unity : (control.shift.numerator = 1 ∧ control.shift.denominator = 1) ↔
        control.shift = .unity := by cases control.shift; simp [PitchRatio.unity]
    simp only [step, runStageBind, accumulateDry_run, replaceOutput_here, replaceOutput_twice]
    unfold accumulate spectrumUpdate
    simp only [unity]
    split
    · simp only [accumulateDry_run, replaceOutput_here, replaceOutput_twice]
      rfl
    · let dry := DSP.Resynthesis.accumulateDry state.math
        (max 0 (1 - fadeWeight control.fade) * oscillatorGain state.math control osc)
        (state.magnitudes lane) (state.phases lane osc) output
      change (accumulateShifted state.math.widen state.math.natSample state.math.natPhase lane osc
        (max 0 (fadeWeight control.fade) * oscillatorGain state.math control osc) control.shift).run
        (replaceOutput state lane dry) = _
      rw [accumulateShifted_run (replaceOutput state lane dry) state.math.widen state.math.natSample
        state.math.natPhase lane osc (max 0 (fadeWeight control.fade) * oscillatorGain state.math control osc)
        control.shift bound rfl rfl rfl]
      simp only [replaceOutput_here, replaceOutput_twice]
      rfl
  have result := foldOutput_run (List.finRange 3) lane state (state.output lane)
    step (spectrumUpdate state lane control) each
  rw [replaceOutput_initial] at result
  rw [accumulateOscillators, Fin.foldlM_eq_foldlM_finRange]
  exact result

def outputDFT (table : BinTable (ComplexValue R) 4096) : DFT R 4096 :=
  ⟨fun k => if h : 0 < k.val then table.get ⟨k.val - 1, by have hk := k.isLt; omega⟩
    else ComplexValue.ofReal 0⟩

theorem accumulateOscillators_spectrum_eq (state : Scene R P) (lane : Lane) (control : Controls R)
    (bound : (state.graph lane).order.length ≤ 2047)
    (cleared : state.output lane = BinTable.ofFn (fun _ => ComplexValue.ofReal 0)) :
    outputDFT (((accumulateOscillators state.math.widen state.math.natSample state.math.natPhase
        state.math.sqrt lane control).run state).2.output lane) =
      synthesizeSpectrum state.math control (state.input lane).omega (state.magnitudes lane)
        (state.phases lane) (state.graph lane) := by
  rw [accumulateOscillators_run state lane control bound, replaceOutput_here, cleared]
  rfl

private theorem fold_same {A B : Type} (left right : A → B → A) (bins : List B)
    (same : ∀ acc bin, bin ∈ bins → left acc bin = right acc bin) (initial : A) :
    bins.foldl left initial = bins.foldl right initial := by
  induction bins generalizing initial with
  | nil => rfl
  | cons bin bins ih =>
    simp only [List.foldl_cons]
    rw [same initial bin (List.mem_cons_self ..)]
    exact ih (fun acc rest member => same acc rest (List.mem_cons_of_mem _ member)) _

private theorem spectrum_omega_eq (math : GrainMath R P) (control : Controls R)
    (first second : BinTable P 4096) (magnitude : BinTable R 4096)
    (phases : Oscillator → BinTable P 4096) (graph : PVDRGraph P 4096)
    (roots : ∀ bin ∈ graph.order, first.get (graph.link.get bin).root =
      second.get (graph.link.get bin).root) :
    synthesizeSpectrum math control first magnitude phases graph =
      synthesizeSpectrum math control second magnitude phases graph := by
  let update := fun omega initial osc =>
    let fade := fadeWeight control.fade
    let gain := oscillatorGain math control osc
    let dry := DSP.Resynthesis.accumulateDry math (max 0 (1 - fade) * gain) magnitude (phases osc) initial
    let wet := max 0 fade * gain
    if control.shift = .unity then DSP.Resynthesis.accumulateDry math wet magnitude (phases osc) dry
    else DSP.Resynthesis.accumulateShifted math wet control.shift omega magnitude (phases osc) graph dry
  let zero : BinTable (ComplexValue R) 4096 := BinTable.ofFn (fun _ => ComplexValue.ofReal 0)
  have same : ∀ initial osc, osc ∈ List.finRange 3 → update first initial osc = update second initial osc := by
    intro initial osc _
    simp only [update]
    split
    · rfl
    · unfold DSP.Resynthesis.accumulateShifted
      dsimp only
      apply fold_same
      intro acc bin member
      rw [roots bin member]
  exact congrArg outputDFT (fold_same (update first) (update second) (List.finRange 3) same zero)

def synthesizeLane (widen : R → P) (natSample : Nat → R) (natPhase : Nat → P)
    (sqrt : R → R) (lane : Lane) : StateM (Scene R P) PUnit := do
  let control ← controls lane
  fixOscillators widen lane control
  accumulateOscillators widen natSample natPhase sqrt lane control

private theorem synthesizeLane_run (state : Scene R P) (widen : R → P) (natSample : Nat → R)
    (natPhase : Nat → P) (sqrt : R → R) (lane : Lane) :
    (synthesizeLane widen natSample natPhase sqrt lane).run state =
      (accumulateOscillators widen natSample natPhase sqrt lane (state.controls lane)).run
        ((fixOscillators widen lane (state.controls lane)).run state).2 := by
  have readControl : (controls lane).run state = (state.controls lane, state) := rfl
  unfold synthesizeLane
  rw [runStageBind (controls lane), readControl]
  exact runStageBind (fixOscillators widen lane (state.controls lane))
    (fun _ => accumulateOscillators widen natSample natPhase sqrt lane (state.controls lane)) state

/-- A lane pass agrees with the reference when prior output is zero and each visited link points
to a self-root. Follower omega writes preserve the root frequencies read by synthesis. -/
theorem synthesizeLane_eq (state : Scene R P) (lane : Lane)
    (bound : (state.graph lane).order.length ≤ 2047)
    (flattened : ∀ bin ∈ (state.graph lane).order,
      let root := ((state.graph lane).link.get bin).root
      ((state.graph lane).link.get root).root = root)
    (cleared : state.output lane = BinTable.ofFn (fun _ => ComplexValue.ofReal 0)) :
    let next := ((synthesizeLane state.math.widen state.math.natSample state.math.natPhase
      state.math.sqrt lane).run state).2
    let phases := fun osc => fixupPhases state.math (oscillatorDetune (state.controls lane) osc)
      (state.input lane) (state.gates lane) (state.graph lane) (state.phases lane osc)
    next.gates lane = state.gates lane ∧ next.magnitudes lane = state.magnitudes lane ∧
      next.phases lane = phases ∧
      outputDFT (next.output lane) = synthesizeSpectrum state.math (state.controls lane)
        (state.input lane).omega (state.magnitudes lane) phases (state.graph lane) := by
  simp only [synthesizeLane_run]
  let control := state.controls lane
  generalize fixedEq : ((fixOscillators state.math.widen lane (state.controls lane)).run state).2 = fixed
  have phaseEq := funext (fixOscillators_phases_eq state.math.widen lane (state.controls lane) state bound rfl)
  rw [fixedEq] at phaseEq
  have context := fixOscillators_keeps_context state.math.widen lane (state.controls lane) state
  simp only [fixedEq, Prod.mk.injEq] at context
  obtain ⟨math, _, gates, graph, magnitude, output, _, _⟩ := context
  have bound' : (fixed.graph lane).order.length ≤ 2047 := by simpa only [graph] using bound
  have cleared' : fixed.output lane = BinTable.ofFn (fun _ => ComplexValue.ofReal 0) := by
    simpa only [output] using cleared
  have outputEq := accumulateOscillators_spectrum_eq fixed lane control bound' cleared'
  have runEq := accumulateOscillators_run fixed lane control bound'
  simp only [math] at outputEq runEq
  refine ⟨?_, ?_, ?_, ?_⟩
  · exact (congrArg (fun result => result.2.gates lane) runEq).trans (congrFun gates lane)
  · exact (congrArg (fun result => result.2.magnitudes lane) runEq).trans (congrFun magnitude lane)
  · exact (congrArg (fun result => result.2.phases lane) runEq).trans phaseEq
  · refine outputEq.trans ?_
    rw [magnitude, graph, phaseEq]
    refine spectrum_omega_eq state.math control (fixed.input lane).omega (state.input lane).omega
      (state.magnitudes lane) _ (state.graph lane) ?_
    intro bin member
    have root := fixOscillators_keeps_root_omega state.math.widen lane (state.controls lane) state _
      (flattened bin member)
    simpa only [fixedEq] using root

/-- The single-lane entry point clears its own output before running the lane pass. -/
theorem synthesize_one_lane_eq (state : Scene R P)
    (bound : (state.graph 0).order.length ≤ 2047)
    (flattened : ∀ bin ∈ (state.graph 0).order,
      let root := ((state.graph 0).link.get bin).root
      ((state.graph 0).link.get root).root = root) :
    let next := ((synthesize state.math.widen state.math.natSample state.math.natPhase
      state.math.sqrt 1).run state).2
    let phases := fun osc => fixupPhases state.math (oscillatorDetune (state.controls 0) osc)
      (state.input 0) (state.gates 0) (state.graph 0) (state.phases 0 osc)
    next.gates 0 = state.gates 0 ∧ next.magnitudes 0 = state.magnitudes 0 ∧
      next.phases 0 = phases ∧ outputDFT (next.output 0) =
        synthesizeSpectrum state.math (state.controls 0) (state.input 0).omega
          (state.magnitudes 0) phases (state.graph 0) := by
  let cleared : Scene R P := { state with output := fun _ => BinTable.ofFn (fun _ => ComplexValue.ofReal 0) }
  have clearedEq : (clearOutput.run state).2 = cleared := rfl
  have runOne :
      (synthesize state.math.widen state.math.natSample state.math.natPhase state.math.sqrt 1).run state =
      (synthesizeLane state.math.widen state.math.natSample state.math.natPhase state.math.sqrt 0).run
        cleared := by
    have laneFold :
        forLanes (1 : Fin 5)
          (synthesizeLane state.math.widen state.math.natSample state.math.natPhase state.math.sqrt) =
        synthesizeLane state.math.widen state.math.natSample state.math.natPhase state.math.sqrt 0 := by
      unfold forLanes
      change Fin.foldlM (0 + 1) _ PUnit.unit = _
      rw [Fin.foldlM_succ, Fin.foldlM_zero]
      rfl
    change (clearOutput >>= fun _ => forLanes 1
      (synthesizeLane state.math.widen state.math.natSample state.math.natPhase state.math.sqrt)).run state = _
    rw [runStageBind, laneFold, clearedEq]
  rw [runOne]
  have result := synthesizeLane_eq cleared 0 bound flattened rfl
  simpa only [cleared] using result

end Scatter
end DSP.Resynthesis.SpectrumStorage

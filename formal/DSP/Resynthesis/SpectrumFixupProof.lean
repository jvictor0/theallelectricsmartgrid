import DSP.Resynthesis.SpectrumStorage

namespace DSP.Resynthesis.SpectrumStorage

variable {R P : Type} [Add P] [Mul P]

private theorem run_bind {S A B : Type} (action : StateM S A) (next : A → StateM S B)
    (state : S) : (action >>= next).run state = (next (action.run state).1).run (action.run state).2 := rfl

private theorem fold_projection {S I A : Type} (step : I → StateM S PUnit)
    (project : S → A) (update : A → I → A)
    (agreement : ∀ state i, project ((step i).run state).2 = update (project state) i)
    (indices : List I) (state : S) :
    project (((indices.foldlM (fun _ i => step i) PUnit.unit).run state).2) =
      indices.foldl update (project state) := by
  induction indices generalizing state with
  | nil => rfl
  | cons i indices ih =>
    simp only [List.foldlM_cons, List.foldl_cons, run_bind]
    rw [ih, agreement]

private def fixVisit (widen : R → P) (lane : Lane) (osc : Oscillator) (detune : R)
    (visit : Visit P) : StateM (Scene R P) PUnit := do
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

private def phaseStep (widen : R → P) (detune : R) (analysis : BinTable R 4096)
    (gates : BinTable GateState 4096) (graph : PVDRGraph P 4096)
    (phases : BinTable P 4096) (bin : Bin) : BinTable P 4096 :=
  let link := graph.link.get bin
  let next := if link.root = bin then
    leaderPhase (gates.get bin == .triggered) (phases.get bin) (widen (analysis.get bin))
      link.delta (widen detune)
    else followerPhase (phases.get link.root) link.delta
  phases.set bin next

private structure PhaseContext (R P : Type) where
  analysis : BinTable R 4096
  gates : BinTable GateState 4096
  graph : PVDRGraph P 4096
  phases : BinTable P 4096

private def project (lane : Lane) (osc : Oscillator) (state : Scene R P) : PhaseContext R P :=
  ⟨(state.input lane).phase, state.gates lane, state.graph lane, state.phases lane osc⟩

private def visitIndex (widen : R → P) (lane : Lane) (osc : Oscillator) (detune : R)
    (i : Bin) : StateM (Scene R P) PUnit := do
  let visit ← visitAt lane i
  fixVisit widen lane osc detune visit

private def updateAt (widen : R → P) (detune : R) (state : PhaseContext R P) (i : Bin) :
    PhaseContext R P :=
  let bin := state.graph.order[i.val]?.getD 0
  { state with phases := phaseStep widen detune state.analysis state.gates state.graph state.phases bin }

omit [Add P] [Mul P] in
private theorem visitAt_run (lane : Lane) (i : Bin) (state : Scene R P) :
    (visitAt lane i).run state =
      (⟨(state.graph lane).order[i.val]?.getD 0,
        (state.graph lane).link.get ((state.graph lane).order[i.val]?.getD 0)⟩, state) := rfl

private theorem visit_agrees (widen : R → P) (lane : Lane) (osc : Oscillator) (detune : R)
    (state : Scene R P) (i : Bin) :
    project lane osc (((visitIndex widen lane osc detune i).run state).2) =
      updateAt widen detune (project lane osc state) i := by
  simp only [visitIndex, run_bind, visitAt_run, fixVisit, updateAt, project, phaseStep]
  simp only [Nat.beq_eq, ← Fin.ext_iff]
  split
  · simp only [run_bind]
    dsimp [phase, analysisPhase, gate, setPhase, StateT.run]
    simp only [Function.update_self]
    congr 3
    cases (state.gates lane).get ((state.graph lane).order[↑i]?.getD 0) <;> rfl
  · simp only [run_bind]
    dsimp [phase, omega, setOmega, setPhase, StateT.run]
    simp only [Function.update_self]

private def binIndex {n : Nat} (bound : n ≤ 2047) (i : Fin n) : Bin :=
  ⟨i.val, by have := i.isLt; omega⟩

private theorem orderedBins (order : List Bin) (bound : order.length ≤ 2047) :
    ((List.finRange order.length).map fun i => order[(binIndex bound i).val]?.getD 0) = order := by
  apply List.ext_getElem
  · simp
  · intro i _ _
    simp [binIndex]

private def updateBin (widen : R → P) (detune : R) (state : PhaseContext R P) (bin : Bin) :
    PhaseContext R P :=
  { state with phases := phaseStep widen detune state.analysis state.gates state.graph state.phases bin }

private theorem fold_at_to_bins (widen : R → P) (detune : R) (indices : List Bin)
    (state : PhaseContext R P) :
    indices.foldl (updateAt widen detune) state =
      (indices.map fun i => state.graph.order[i.val]?.getD 0).foldl (updateBin widen detune) state := by
  induction indices generalizing state with
  | nil => rfl
  | cons i indices ih =>
    simp only [List.foldl_cons, List.map_cons, ih]
    rfl

private theorem fold_bin_phases (widen : R → P) (detune : R) (bins : List Bin)
    (state : PhaseContext R P) :
    (bins.foldl (updateBin widen detune) state).phases =
      bins.foldl (phaseStep widen detune state.analysis state.gates state.graph) state.phases := by
  induction bins generalizing state with
  | nil => rfl
  | cons i indices ih =>
    simp only [List.foldl_cons, ih]
    rfl

theorem fixup_phases_eq (widen : R → P) (lane : Lane) (osc : Oscillator) (detune : R)
    (state : Scene R P) (bound : (state.graph lane).order.length ≤ 2047)
    (sameWiden : state.math.widen = widen) :
    ((fixup widen lane osc detune).run state).2.phases lane osc =
      fixupPhases state.math detune (state.input lane) (state.gates lane) (state.graph lane)
        (state.phases lane osc) := by
  let count := min (state.graph lane).order.length 2047
  have countBound : count ≤ 2047 := Nat.min_le_right ..
  have countEq : count = (state.graph lane).order.length := Nat.min_eq_left bound
  change (project lane osc ((Fin.foldlM count
    (fun _ i => visitIndex widen lane osc detune (binIndex countBound i)) PUnit.unit).run state).2).phases = _
  rw [Fin.foldlM_eq_foldlM_finRange, ← List.foldlM_map,
    fold_projection (visitIndex widen lane osc detune) (project lane osc) (updateAt widen detune)
      (visit_agrees widen lane osc detune), fold_at_to_bins, List.map_map]
  have ordered :
      ((List.finRange count).map fun i => (state.graph lane).order[(binIndex countBound i).val]?.getD 0) =
        (state.graph lane).order := by
    clear_value count
    subst countEq
    exact orderedBins _ _
  rw [show (((List.finRange count).map
    ((fun i : Bin => (project lane osc state).graph.order[i.val]?.getD 0) ∘ binIndex countBound))) =
      (state.graph lane).order from ordered, fold_bin_phases]
  simp only [fixupPhases, project, sameWiden]
  rfl

end DSP.Resynthesis.SpectrumStorage

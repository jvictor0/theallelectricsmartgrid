import PhaseTime.InverseStorage
import PhaseTime.InverseRuntimeVisits

namespace PhaseTime.InverseRuntime

variable {R : Type} [a : Arithmetic R] {n : Nat}

open Storage

set_option linter.unusedSimpArgs false
set_option linter.unusedTactic false
set_option linter.unreachableTactic false

private theorem runBind {S A B : Type} (x : StateM S A) (f : A → StateM S B) (s : S) :
    (x >>= f).run s = (f (x.run s).1).run (x.run s).2 := by rfl

private theorem runPure {S A : Type} (x : A) (s : S) :
    (pure x : StateM S A).run s = (x, s) := rfl

private theorem runIf {S A : Type} (p : Prop) [Decidable p] (x y : StateM S A) (s : S) :
    (if p then x else y).run s = if p then x.run s else y.run s := by split <;> rfl

private theorem idPure {A : Type} (x : A) : (pure x : Id A) = x := rfl
private theorem idBind {A B : Type} (x : Id A) (f : A → Id B) : x >>= f = f x := rfl

private theorem idMap {A B : Type} (f : A → B) (x : Id A) : f <$> x = f x := rfl

private theorem runMap {S A B : Type} (f : A → B) (x : StateM S A) (s : S) :
    (f <$> x).run s = (f (x.run s).1, (x.run s).2) := by rfl

attribute [local simp] runBind runPure runIf runMap idPure idBind idMap

private def loadedInterval (cell : Cell R a.Delta) (entry : Option R)
    (index : Int) (origin query : R) : R × Option R × Option R :=
  match cell.replacement with
  | some branch =>
    if a.le branch.cut query then
      (branch.cut, a.decode origin branch.start, branch.stop.bind (a.decode origin))
    else (lowPhase, some origin, cell.fallbackEnd)
  | none => (lowPhase, some origin, cell.fallbackEnd)
where
  lowPhase := match entry with
    | some phase => if a.floor phase == index then phase else a.ofIndex index
    | none => a.ofIndex index

private theorem loadIntervalM_run (s : State R a.Delta n) (index : Int) (origin query : R) :
    (loadIntervalM index origin query a.decode).run s =
      (loadedInterval (s.payload.cells.get (wrap n index)) s.payload.seed index origin query, s) := by
  unfold loadIntervalM
  rcases hc : s.payload.cells.get (wrap n index) with ⟨baseTime, fallbackTime, branch⟩
  have hcut : (cut index).run s = (branch.map (·.cut), s) := by simp [StateT.run, cut, hc]
  have hstart : (startDelta index).run s = (branch.map (·.start), s) := by simp [StateT.run, startDelta, hc]
  have hend : (endDelta index).run s = (branch.bind (·.stop), s) := by simp [StateT.run, endDelta, hc]
  have hseed : (seed : Action R n (Option (SeedEntry R))).run s =
      (s.payload.seed.map (fun phase => ⟨phase, a.floor phase⟩), s) := rfl
  have hfallback : (fallback index).run s = (fallbackTime, s) := by simp [StateT.run, fallback, hc]
  cases branch with
  | none =>
    cases hs : s.payload.seed <;>
      simp [hcut, hseed, hs, hfallback, loadedInterval, loadedInterval.lowPhase, Option.elim]
  | some branch =>
    cases hs : s.payload.seed <;> by_cases hle : a.le branch.cut query = true <;>
      simp [hcut, hstart, hend, hseed, hs, hfallback, hle, loadedInterval, loadedInterval.lowPhase,
        Option.any, Option.elim]


private theorem closeInterval_eq (leftPhase high : R) (stop : Option R) (last : Option (Point R)) :
    closeInterval leftPhase high stop last = match stop, last with
      | none, some edge =>
        if frontierCloses leftPhase high edge.phase then (edge.phase, some edge.time) else (high, none)
      | _, _ => (high, stop) := by
  cases stop <;> cases last <;> simp [closeInterval, Option.any]

private theorem readLoaded_eq (cell : Cell R a.Delta) (entry : Option R) (last : Option (Point R))
    (index : Int) (origin query cutoff : R) (hb : cell.base = some origin) :
    readLoaded index (loadedInterval cell entry index origin query) last query cutoff =
      (selectInterval cell entry last index query).bind fun interval =>
        readInterval interval.left interval.rightPhase interval.rightTime query cutoff := by
  cases hs : cell.replacement with
  | none =>
    simp [readLoaded, readLoadedTo, loadedInterval, loadedInterval.lowPhase, selectInterval, hb, hs, closeInterval_eq]
    cases cell.fallbackEnd <;> cases last <;> simp <;> split <;> rfl
  | some branch =>
    by_cases hle : a.le branch.cut query = true
    · simp [readLoaded, readLoadedTo, loadedInterval, loadedInterval.lowPhase, selectInterval, hb, hs, hle, closeInterval_eq]
      cases a.decode origin branch.start <;> simp
      cases branch.stop.bind (a.decode origin) <;> cases last <;> simp <;> split <;> rfl
    · simp [readLoaded, readLoadedTo, loadedInterval, loadedInterval.lowPhase, selectInterval, hb, hs, hle, closeInterval_eq]
      cases cell.fallbackEnd <;> cases last <;> simp <;> split <;> rfl

theorem readPayloadM_run (s : State R a.Delta n) (query cutoff : R) :
    (readPayloadM query cutoff).run s = (read s.payload query cutoff, s) := by
  unfold readPayloadM readPayloadUsingM read
  have hfront : (Storage.frontier : Action R n (Option (Point R))).run s = (s.payload.frontier, s) := rfl
  have hbase : (base (a.floor query)).run s =
      ((s.payload.cells.get (wrap n (a.floor query))).base, s) := rfl
  simp only [runBind, hfront, runIf, hbase, Prod.fst, Prod.snd, ← readLoaded.eq_1]
  generalize hc : s.payload.cells.get (wrap n (a.floor query)) = cell
  cases hf : s.payload.frontier with
  | none =>
    cases hb : cell.base <;>
      simp [hb, readCell, loadIntervalM_run, hc, selectInterval, Option.any, readLoaded_eq]
  | some edge =>
    by_cases hit : (query == edge.phase) = true
    · simp [hit, readCell, Option.any]
    · cases hb : cell.base <;>
        simp [hit, hb, readCell, loadIntervalM_run, hc, selectInterval, Option.any, readLoaded_eq]

theorem assignCellM_run (s : State R a.Delta n) (index : Int) (time stop : R) (closed : Bool) :
    (assignCellM index time closed stop).run s =
      (⟨⟩, { s with payload := assignCell s.payload index time (if closed then some stop else none) }) := by
  have getSeed : (seed : Action R n (Option (SeedEntry R))).run s =
      (s.payload.seed.map (fun phase => ⟨phase, a.floor phase⟩), s) := rfl
  let cell : Cell R a.Delta := ⟨some time, if closed then some stop else none, none⟩
  have put (s' : State R a.Delta n) :
      (putCell index time closed stop).run s' =
        (⟨⟩, {s' with payload.cells := s'.payload.cells.set (wrap n index) cell}) := rfl
  unfold assignCellM retireSeedM
  cases hs : s.payload.seed with
  | none => simp [getSeed, hs, put, assignCell, retireSeed, cell]
  | some phase =>
    have slot : (sameSlot (a.floor phase) index).run s = (wrap n (a.floor phase) == wrap n index, s) := rfl
    have clear : (clearSeed : Action R n PUnit).run s = (⟨⟩, {s with payload.seed := none}) := rfl
    by_cases same : wrap n (a.floor phase) = wrap n index <;>
      simp [getSeed, hs, slot, clear, put, assignCell, retireSeed, Option.filter, same, bne, cell]

theorem seedCellM_run (s : State R a.Delta n) (edge : Point R) :
    (seedCellM edge).run s = (⟨⟩, { s with payload := seedCell s.payload edge }) := by
  unfold seedCellM seedCellAtM
  have hc (s' : State R a.Delta n) (index : Int) :
      (clearCell index).run s' =
        (⟨⟩, { s' with payload.cells := s'.payload.cells.set (wrap n index) {} }) := rfl
  have hs (s' : State R a.Delta n) :
      (storeSeed edge.phase ⟨a.floor edge.phase, rfl⟩).run s' =
        (⟨⟩, { s' with payload.seed := some edge.phase }) := rfl
  have hseed : (seed : Action R n (Option (SeedEntry R))).run s =
      (s.payload.seed.map (fun phase => ⟨phase, a.floor phase⟩), s) := rfl
  cases h : s.payload.seed <;>
    simp [clearSeedCellM, seedCell, hseed, hc, hs, h, assignCellM_run, assignCell]

theorem finishCellM_run (s : State R a.Delta n) (index : Int) (time : R) :
    (finishCellM index time).run s = (⟨⟩, { s with payload := finishCell s.payload index time }) := by
  unfold finishCellM
  let slot := wrap n index
  cases hc : s.payload.cells.get slot with
  | mk baseTime fallbackEnd replacement =>
    cases baseTime <;> cases fallbackEnd <;> cases replacement <;>
      simp [finishCell, fallback, base, cut, putFallback, putEndDelta, ← show
        slot = wrap n index from rfl, hc, StateT.run, StateT.bind, StateT.pure,
        Bind.bind, Pure.pure, Id.run] <;>
      first | rfl | rw [← hc, show s.payload.cells.set slot.val (s.payload.cells.get slot) = s.payload.cells from
        Vector.set_getElem_self slot.isLt]

theorem batchTimes_get (left right : Point R) (first : Int) (extra : Nat)
    (bounded : extra < batchCapacity) (i : Nat) (within : i ≤ extra) :
    (batchTimes left right first extra bounded).get ⟨i, by omega⟩ (Nat.lt_succ_of_le within) =
      (edgeAt left right (first + Int.ofNat i)).time :=
  rfl

private def assignEdgesM (left right : Point R) (first : Int) (extra : Nat) : Action R n PUnit :=
  Fin.foldlM extra (fun () i => do
    let index := first + Int.ofNat i.val
    assignCellM index (edgeAt left right index).time true (edgeAt left right (index + 1)).time) ()

private theorem assignEdgesM_succ (left right : Point R) (first : Int) (extra : Nat) :
    (assignEdgesM left right first (extra + 1) : Action R n PUnit) =
      (do
        assignCellM first (edgeAt left right first).time true (edgeAt left right (first + 1)).time
        assignEdgesM left right (first + 1) extra) := by
  let step (start : Int) (_ : PUnit.{1}) (i : Fin extra) : Action R n PUnit :=
    let index := start + Int.ofNat i.val
    assignCellM index (edgeAt left right index).time true (edgeAt left right (index + 1)).time
  have hstep : (fun (_ : PUnit.{1}) (i : Fin extra) =>
        let index := first + Int.ofNat i.succ.val
        assignCellM index (edgeAt left right index).time true (edgeAt left right (index + 1)).time) =
      step (first + 1) := by
    funext u i
    have h : first + Int.ofNat i.succ.val = first + 1 + Int.ofNat i.val := by
      change first + ((i.val + 1 : Nat) : Int) = first + 1 + (i.val : Int)
      omega
    simp only [step, h]
  unfold assignEdgesM
  rw [Fin.foldlM_succ]
  have hzero : first + Int.ofNat 0 = first := Int.add_zero first
  simp only [Fin.val_zero, hzero, hstep]
  rfl

private def publishLastM (left right : Point R) (index : Int) : Action R n PUnit := do
  let edge := edgeAt left right index
  assignCellM index edge.time false edge.time
  storeFrontier edge.phase edge.time

private theorem assignEdgesM_run (s : State R a.Delta n) (left right : Point R)
    (first : Int) (extra : Nat) :
    (do
      assignEdgesM left right first extra
      publishLastM left right (first + Int.ofNat extra) : Action R n PUnit).run s =
      (⟨⟩, {s with payload := commitTail s.payload left right (edgeAt left right first) first extra}) := by
  induction extra generalizing first s with
  | zero =>
    unfold assignEdgesM publishLastM
    have hzero : first + Int.ofNat 0 = first := Int.add_zero first
    simp only [Fin.foldlM_zero, hzero, runBind, runPure, assignCellM_run, Prod.fst, Prod.snd,
      Bool.false_eq_true, if_false, commitTail]
    rfl
  | succ extra ih =>
    rw [assignEdgesM_succ]
    have h : first + Int.ofNat (extra + 1) = first + 1 + Int.ofNat extra := by
      change first + ((extra + 1 : Nat) : Int) = first + 1 + (extra : Int)
      omega
    rw [h]
    let payload := assignCell s.payload first (edgeAt left right first).time
      (some (edgeAt left right (first + 1)).time)
    let next := { s with payload }
    simpa only [runBind, assignCellM_run, Prod.fst, Prod.snd, commitTail, if_true,
      Bool.true_eq, next, payload] using ih next (first + 1)

private theorem commitEdges_prepared (left right : Point R) (first : Int) (extra : Nat)
    (bounded : extra < batchCapacity) :
    (Fin.foldlM extra (fun () i =>
      have hi := i.isLt
      let times := batchTimes left right first extra bounded
      assignCellM (first + Int.ofNat i.val)
        (times.get ⟨i.val, by omega⟩ (by change i.val < extra + 1; omega)) true
        (times.get ⟨i.val + 1, by omega⟩ (Nat.succ_lt_succ hi))) () : Action R n PUnit) =
        assignEdgesM left right first extra := by
  unfold assignEdgesM
  congr 1
  funext u i
  cases u
  have hi := i.isLt
  dsimp only
  rw [batchTimes_get left right first extra bounded i.val (by omega),
    batchTimes_get left right first extra bounded (i.val + 1) (by omega)]
  have h : first + Int.ofNat (i.val + 1) = first + Int.ofNat i.val + 1 := by
    change first + ((i.val + 1 : Nat) : Int) = first + (i.val : Int) + 1
    omega
  rw [h]

theorem commitBatchM_run (s : State R a.Delta n) (left right : Point R)
    (first : Int) (extra : Nat) (bounded : extra < batchCapacity) :
    (commitBatchM left right first extra bounded).run s =
      (⟨⟩, {s with payload := commitBatch s.payload left right first extra}) := by
  unfold commitBatchM commitBatchUsingM
  dsimp only [DSP.Codegen.Index.addNat]
  rw [← batchTimes.eq_1]
  simp only [← finishPredecessorM.eq_1]
  simp only [batchTimes_get left right first extra bounded 0 (by omega),
    batchTimes_get left right first extra bounded extra (Nat.le_refl _),
    Int.ofNat_zero, Int.add_zero, commitEdges_prepared]
  have hbeg : (beginRevision : Action R n PUnit).run s = (⟨⟩, s) := rfl
  have hend (s' : State R a.Delta n) : (endRevision : Action R n PUnit).run s' = (⟨⟩, s') := rfl
  have hp (s' : State R a.Delta n) :
      (do
        assignEdgesM left right first extra
        assignCellM (first + Int.ofNat extra)
          (edgeAt left right (first + Int.ofNat extra)).time false
          (edgeAt left right (first + Int.ofNat extra)).time
        storeFrontier (a.ofIndex (first + Int.ofNat extra))
          (edgeAt left right (first + Int.ofNat extra)).time
        endRevision : Action R n PUnit).run s' =
        (⟨⟩, {s' with payload := commitTail s'.payload left right (edgeAt left right first) first extra}) := by
    have h := assignEdgesM_run s' left right first extra
    simp only [runBind, hend, publishLastM, edgeAt] at h ⊢
    simpa [storeFrontier, StateT.run] using h
  simp only [runBind, hbeg, Prod.fst, Prod.snd]
  unfold finishPredecessorM finishPredecessorAtM commitBatch
  simp only [runIf, runPure, finishCellM_run, edgeAt, runBind, hend, Prod.fst, Prod.snd] at hp ⊢
  split <;> simp_all


private theorem scatterStepM_run (s : State R a.Delta n) (left right : Point R) (index : Int)
    (remaining : Nat) (positive : 0 < remaining) :
    (scatterStepM left right ⟨index, remaining⟩).run s =
      let count := min batchCapacity remaining
      (.yield ⟨index + Int.ofNat count, (↑(remaining - count) : Int)⟩,
        { s with payload := commitBatch s.payload left right index (count - 1) }) := by
  let count := min batchCapacity remaining
  have hpositive : ¬ (remaining : Int) ≤ 0 := by omega
  have hcount : (if (remaining : Int) ≤ Int.ofNat batchCapacity then
      (remaining : Int) else Int.ofNat batchCapacity) = (count : Int) := by
    change (if (remaining : Int) ≤ (batchCapacity : Int) then
      (remaining : Int) else (batchCapacity : Int)) = (count : Int)
    by_cases h : remaining ≤ batchCapacity
    · rw [if_pos (Int.ofNat_le.mpr h), show count = remaining from Nat.min_eq_right h]
    · rw [if_neg (fun hi => h (Int.ofNat_le.mp hi)),
        show count = batchCapacity from Nat.min_eq_left (by omega)]
  unfold scatterStepM
  simp only [hpositive, BatchCursor.index, BatchCursor.remaining, if_false, runIf, runBind,
    commitBatchM_run, runPure, Prod.fst, Prod.snd, hcount]
  change (ForInStep.yield (BatchCursor.mk (index + (count : Int)) ((remaining : Int) - (count : Int))),
    {s with payload := commitBatch s.payload left right index ((↑count : Int) - 1).toNat}) = _
  have hstep : ((count : Int) - 1).toNat = count - 1 := Int.toNat_sub count 1
  rw [← Int.ofNat_sub (Nat.min_le_right batchCapacity remaining), hstep]
  rfl

private theorem scatterFuelM_run (s : State R a.Delta n) (left right : Point R) (index : Int)
    (fuel remaining : Nat) (enough : remaining ≤ fuel) :
    ((DSP.foldUntilM fuel (scatterStepM left right) ⟨index, (remaining : Int)⟩).run s).2 =
      {s with payload := scatter s.payload left right index remaining} := by
  induction fuel generalizing s index remaining with
  | zero =>
    have h : remaining = 0 := by omega
    subst remaining
    simp [DSP.foldUntilM, scatter]
  | succ fuel ih =>
    by_cases h : 0 < remaining
    · let count := min batchCapacity remaining
      have hcount : 0 < count := by simp [count, batchCapacity, h]
      have hfuel : remaining - count ≤ fuel := by omega
      simp only [DSP.foldUntilM, runBind, scatterStepM_run s left right index remaining h,
        Prod.fst, Prod.snd]
      rw [ih _ _ _ hfuel]
      conv => rhs; rw [scatter]
      simp only [if_pos h, dif_pos h, count]
    · have hz : remaining = 0 := by omega
      subst remaining
      simp [DSP.foldUntilM, scatterStepM, scatter]

theorem scatterM_run (s : State R a.Delta n) (left right : Point R) (index remaining : Int) :
    (scatterM left right index remaining).run s =
      (⟨⟩, {s with payload := scatter s.payload left right index remaining.toNat}) := by
  unfold scatterM
  by_cases h : 0 ≤ remaining
  · have hcast : (remaining.toNat : Int) = remaining := by omega
    simp only [runBind, runPure]
    rw [← scatterFuelM_run s left right index remaining.toNat remaining.toNat (Nat.le_refl _), hcast]
  · have hz : remaining.toNat = 0 := by omega
    simp [hz, DSP.foldUntilM, scatter]


theorem beginBranchM_run (s : State R a.Delta n) (edge : Point R)
    (irreflexive : ∀ x : R, a.lt x x = false) :
    (beginBranchM edge).run s = (⟨⟩, beginBranch s edge) := by
  let index := a.floor edge.phase
  have hindex : a.floor edge.phase = index := rfl
  have hsame : ((s.payload.seed.map fun phase => SeedEntry.mk phase (a.floor phase)).any fun entry =>
      if entry.index = index then a.lt edge.phase entry.phase else false) =
      s.payload.seed.any (fun phase => a.floor phase == index && a.lt edge.phase phase) := by
    cases hs : s.payload.seed with
    | none => rfl
    | some phase => by_cases h : a.floor phase = index <;> simp [h, Option.any]
  have hretain : (s.highest.any fun hi => if hi - Int.ofNat n ≤ index then
        (if index ≤ hi then true else false) else false) = retained s.highest n index := by
    cases hh : s.highest with
    | none => rfl
    | some hi =>
      by_cases h : hi - Int.ofNat n ≤ index <;> by_cases h' : index ≤ hi <;>
        simp [hh, h, h', retained, Option.any]
  have hhighest : (highest : Action R n (Option Int)).run s = (s.highest, s) := rfl
  have hcapacity : (capacityMinusOne : Action R n Int).run s = (Int.ofNat n, s) := rfl
  have hseed : (seed : Action R n (Option (SeedEntry R))).run s =
      (s.payload.seed.map (fun phase => ⟨phase, a.floor phase⟩), s) := rfl
  have hfront : (Storage.frontier : Action R n (Option (Point R))).run s = (s.payload.frontier, s) := rfl
  have hbeg : (beginRevision : Action R n PUnit).run s = (⟨⟩, s) := rfl
  have hend (t : State R a.Delta n) : (endRevision : Action R n PUnit).run t = (⟨⟩, t) := rfl
  have hpublish (t : State R a.Delta n) :
      (storeFrontier edge.phase edge.time).run t = (⟨⟩, {t with payload.frontier := some edge}) := rfl
  let cell := s.payload.cells.get (wrap n index)
  have hbase : (base index).run s = (cell.base, s) := rfl
  have hfallback : (fallback index).run s = (cell.fallbackEnd, s) := rfl
  have hcut : (cut index).run s = (cell.replacement.map (·.cut), s) := rfl
  have hseedAt : (seedCellAtM edge (a.floor edge.phase) rfl).run s =
      (⟨⟩, {s with payload := seedCell s.payload edge}) := seedCellM_run s edge
  have hcell : s.payload.cells.get (wrap n index) = cell := rfl
  unfold beginBranchM beginBranchUsingM beginBranch
  by_cases hg : (edge.phase == a.ofIndex index) = true
  · simp only [hindex, hg, if_true, dif_pos, runPure, Id.run]
    rfl
  · simp only [hindex, hg, if_neg hg, dif_neg hg, prepareBranchM, prepareBranchUsingM, runBind, runPure, runIf, hbeg, hend,
      hhighest, hcapacity, hseed, hfront, hbase, hfallback, hcut, hpublish, Prod.fst, Prod.snd,
      hsame, hretain, idBind, idPure, Id.run, hcell, Bool.false_eq_true, if_false]
    by_cases reset : (!retained s.highest n index || cell.base.isNone ||
        s.payload.seed.any (fun phase => a.floor phase == index && a.lt edge.phase phase)) = true
    · simp only [if_pos reset, hseedAt, Prod.snd]
    · simp only [if_neg reset]
      cases hb : cell.base with
      | none => simp [hb] at reset
      | some origin =>
        cases hs : cell.fallbackEnd with
        | none => simp only [hs, Option.isSome_none, Bool.false_eq_true, ↓reduceIte, hb]
        | some stop =>
          simp only [hb, hs, Option.isSome_some, ↓reduceIte, runBind, hcut, hfront, Prod.fst, Prod.snd]
          cases hc : cell.replacement with
          | none =>
            simp [hc, putReplacement, StateT.run, Option.any, hcell, hb, hs]
          | some old =>
            simp only [hc, Option.map_some, Option.any_some]
            by_cases keep : s.payload.frontier.any (fun last =>
                keepCut last.phase (a.ofIndex (index + 1)) edge.phase old.cut) = true <;>
              by_cases later : a.lt old.cut edge.phase = true <;>
              simp [hc, keep, later, irreflexive, putEndDelta, putReplacement, hcell, hb, hs,
                incrementIslands, StateT.run]
            simp [Bind.bind, StateT.bind, Id.instMonad, incrementIslands, putReplacement, hcell, hb, hs]

private theorem openVisitM_run (s : State R a.Delta n) (left : Point R)
    (irreflexive : ∀ x : R, a.lt x x = false) :
    (openVisitM left).run s =
      (⟨⟩, if !s.hasForward || s.descending then beginBranch s left else s) := by
  have hforward : (hasForward : Action R n Bool).run s = (s.hasForward, s) := rfl
  have hdesc : (descending : Action R n Bool).run s = (s.descending, s) := rfl
  unfold openVisitM openVisitUsingM
  cases hf : s.hasForward <;> cases hd : s.descending <;>
    simp [hforward, hdesc, hf, hd, beginBranchM_run, irreflexive]

private theorem writeForwardM_run (s : State R a.Delta n) (resolution : R) (left right : Point R)
    (irreflexive : ∀ x : R, a.lt x x = false) :
    (writeForwardM resolution left right).run s =
      let opened := if !s.hasForward || s.descending then beginBranch s left else s
      let first := a.ceil left.phase
      let last := a.floor right.phase
      let payload := scatter opened.payload left right first (last - first + 1).toNat
      (⟨⟩, { opened with
        payload := { payload with frontier := some right }
        highest := some (max last (s.highest.getD last))
        hasForward := true
        descending := false }) := by
  let opened := if !s.hasForward || s.descending then beginBranch s left else s
  let last := a.floor right.phase
  have hopen : (openVisitM left).run s = (⟨⟩, opened) := openVisitM_run s left irreflexive
  have hhighest : (highest : Action R n (Option Int)).run s = (s.highest, s) := rfl
  have hcapacity : (capacityMinusOne : Action R n Int).run s = (Int.ofNat n, s) := rfl
  have hmax : (if s.highest.getD last ≤ last then last else s.highest.getD last) =
      max last (s.highest.getD last) := by
    by_cases h : s.highest.getD last ≤ last
    · simp [h, max_eq_left h]
    · simp [h, max_eq_right (by omega : last ≤ s.highest.getD last)]
  unfold writeForwardM
  simp only [runBind, hhighest, hcapacity, hopen, Prod.fst, Prod.snd, scatterM_run]
  rw [show a.floor right.phase = last from rfl, hmax]
  rfl

theorem writeM_run (s : State R a.Delta n) (resolution : R) (observation : Point R)
    (irreflexive : ∀ x : R, a.lt x x = false) :
    (writeM resolution observation).run s = (⟨⟩, write s resolution observation) := by
  let observed := {s with previous := some observation}
  have hprev : (previous : Action R n (Option (Point R))).run s = (s.previous, s) := rfl
  have hview (p t : R) : (storeViewPrevious p t : Action R n PUnit).run s = (⟨⟩, s) := rfl
  have hobs : (storePrevious observation.phase observation.time).run s = (⟨⟩, observed) := rfl
  unfold writeM write writeObservedM
  cases hp : s.previous with
  | none => simp [hprev, hview, hobs, hp, observed]
  | some previous =>
    simp only [hprev, hview, hobs, hp, Option.getD_some, Option.isSome_some, if_true,
      runBind, Prod.fst, Prod.snd]
    by_cases hr : a.lt previous.phase observation.phase = true
    · simp [hr, writeForwardM_run, irreflexive, observed]
    · by_cases hd : a.lt observation.phase previous.phase = true
      · simp [hr, hd, markDescending, StateT.run, observed]
      · simp [hr, hd, StateT.run, observed]
        rfl

noncomputable section

attribute [local instance] exactArithmetic

theorem writeM_exact (s : State ℝ ℝ n) (resolution : ℝ) (observation : Point ℝ) :
    (writeM resolution observation).run s = (⟨⟩, write s resolution observation) :=
  writeM_run s resolution observation (fun x => by
    change decide (x < x) = false
    simp)

theorem SampledWrites.observeM {F : ℝ → ℝ} {resolution : ℝ} {s : State ℝ ℝ n}
    (history : SampledWrites F resolution n s) (observation : Point ℝ)
    (onTrajectory : F observation.time = observation.phase * resolution)
    (segment : ∀ previous ∈ s.previous, SampleSegment F resolution previous observation) :
    SampledWrites F resolution n ((writeM resolution observation).run s).2 := by
  rw [writeM_exact]
  exact history.observe observation onTrajectory segment

theorem reachable_readPayloadM_soundness {F : ℝ → ℝ} {resolution : ℝ} {s : State ℝ ℝ n}
    (positive : 0 < resolution) (continuous : Continuous F)
    (history : SampledWrites F resolution n s)
    {previous : Point ℝ} {query cutoff time : ℝ}
    (latest : s.previous = some previous) (retained : RetainedQuery s resolution query)
    (found : ((readPayloadM query cutoff).run s).1 = some time) :
    time ≤ previous.time ∧ time ≤ cutoff ∧ |F time - query| ≤ 1 ∧
      ∃ exactTime ≤ previous.time, F exactTime = query := by
  rw [readPayloadM_run] at found
  exact reachable_read_soundness positive continuous history latest retained found

end

end PhaseTime.InverseRuntime

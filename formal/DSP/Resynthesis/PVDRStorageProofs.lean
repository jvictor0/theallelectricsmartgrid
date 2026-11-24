import DSP.Resynthesis.PVDRStorage

namespace DSP.Resynthesis.PVDR.Storage

variable {R P Q RNG : Type} {n : Nat}

theorem foldlM_read_run {S A : Type} (k : Nat) (f : S → A → Fin k → A)
    (init : A) (state : S) :
    (Fin.foldlM (m := StateM S) k (fun acc bin => ((fun s => (f s acc bin, s)) : StateM S A)) init).run state =
      (Fin.foldl k (f state) init, state) := by
  induction k generalizing init with
  | zero => simp [Fin.foldlM_zero, Fin.foldl_zero, StateT.run, Pure.pure, StateT.pure]
  | succ k ih =>
    rw [Fin.foldlM_succ, Fin.foldl_succ]
    exact ih (fun s acc bin => f s acc bin.succ) (f state init 0)

theorem peak_run [OfNat R 0] [Max R] (state : Storage R P Q RNG n) :
    (Fin.foldlM (n / 2 - 1) (fun peak bin => do
      let current ← magnitude bin
      let previous ← previousMagnitude bin
      pure (max (max peak current) previous)) 0).run state =
        (referenceMagnitude state.input, state) := by
  exact foldlM_read_run _ (fun s peak bin =>
    max (max peak (s.input.magnitude.get bin)) (s.input.prevMagnitude.get bin)) 0 state

def modelSeedStep [OfNat R 0] [LT R] [LE R] [DecidableLT R] [DecidableLE R]
    (widen : R → P) (low trigger : R) (input : PVDRInput R P n) (queue : Queue R Q n)
    (draw : StateM RNG R) (work : Work P Q n) (bin : ActiveBin n) : StateM RNG (Work P Q n) := do
  let gate := updateGate low trigger (input.magnitude.get bin) (work.gates.get bin)
  let work := { work with gates := work.gates.set bin gate }
  if gate == .low then
    let delta ← draw
    pure (record work bin ⟨bin, widen delta, true⟩)
  else
    pure { work with queue := queue.push work.queue (makeEntry input bin true) }

theorem initializeBin_run [OfNat R 0] [LT R] [LE R] [DecidableLT R] [DecidableLE R]
    (widen : R → P) (low trigger : R) (bin : ActiveBin n) (state : Storage R P Q RNG n) :
    (initializeBin widen low trigger bin).run state =
      let result := (modelSeedStep widen low trigger state.input state.queue state.draw state.work bin).run state.rng
      (⟨⟩, { state with work := result.1, rng := result.2 }) := by
  cases next : updateGate low trigger (state.input.magnitude.get bin) (state.work.gates.get bin) <;>
    simp [initializeBin, initializeBinWith, modelSeedStep, magnitude, gate, setGate, nextPhase, put,
      previousMagnitude, push, makeEntry, StateT.run, Bind.bind, StateT.bind,
      Pure.pure, next] <;> rfl

theorem foldlM_finRange {m : Type → Type} [Monad m] [LawfulMonad m] {A : Type}
    (k : Nat) (f : A → Fin k → m A) (init : A) :
    Fin.foldlM k f init = (List.finRange k).foldlM f init := by
  induction k generalizing init with
  | zero => simp [Fin.foldlM_zero]
  | succ k ih =>
    rw [Fin.foldlM_succ, List.finRange_succ, List.foldlM_cons]
    simp only [List.foldlM_map]
    congr 1
    funext acc
    exact ih _ acc

theorem seedList_run [OfNat R 0] [LT R] [LE R] [DecidableLT R] [DecidableLE R]
    (widen : R → P) (low trigger : R) (bins : List (ActiveBin n))
    (state : Storage R P Q RNG n) :
    (bins.foldlM (fun _ bin => initializeBin widen low trigger bin) (⟨⟩ : PUnit)).run state =
      let result := (bins.foldlM (modelSeedStep widen low trigger state.input state.queue state.draw)
        state.work).run state.rng
      (⟨⟩, { state with work := result.1, rng := result.2 }) := by
  induction bins generalizing state with
  | nil => rfl
  | cons bin bins ih =>
    simp only [List.foldlM_cons, StateT.run_bind, initializeBin_run, ih]
    rfl

theorem seed_run [OfNat R 0] [LT R] [LE R] [DecidableLT R] [DecidableLE R]
    (widen : R → P) (low trigger : R) (state : Storage R P Q RNG n) :
    (Fin.foldlM (n / 2 - 1) (fun _ bin => initializeBin widen low trigger bin) (⟨⟩ : PUnit)).run state =
      let result := ((List.finRange (n / 2 - 1)).foldlM
        (modelSeedStep widen low trigger state.input state.queue state.draw) state.work).run state.rng
      (⟨⟩, { state with work := result.1, rng := result.2 }) := by
  rw [foldlM_finRange, seedList_run]


private theorem run_bind {S A B : Type} (action : StateM S A) (next : A → StateM S B) (state : S) :
    (action >>= next).run state = (next (action.run state).1).run (action.run state).2 := rfl

private theorem run_pure {S A : Type} (value : A) (state : S) :
    (pure value : StateM S A).run state = (value, state) := rfl

theorem attach_run [Sub R] [Add P] (widen : R → P) (parent : ActiveBin n)
    (neighbor : Option (ActiveBin n)) (state : Storage R P Q RNG n) :
    ((Storage.attach widen parent neighbor).run state).2 =
      { state with work := PVDR.attach widen state.queue state.input parent neighbor state.work } := by
  cases neighbor with
  | none => rfl
  | some bin =>
    simp only [Storage.attach, PVDR.attach, run_bind]
    change ((if state.work.computed.get bin then (pure PUnit.unit : StateM (Storage R P Q RNG n) PUnit) else _).run state).2 = _
    split
    · rfl
    · simp only [Nat.beq_eq, Fin.ext_iff]
      rfl

theorem visit_run [Sub R] [Add P] [Mul P] (widen : R → P) (hop : P) (entry : Entry R n)
    (state : Storage R P Q RNG n) :
    ((Storage.visit widen hop entry).run state).2 =
      { state with work := PVDR.visit widen hop state.queue state.input entry state.work } := by
  by_cases previous : entry.previous
  · simp only [Storage.visit, PVDR.visit, previous, if_true, run_bind]
    change ((if state.work.computed.get entry.bin then (pure PUnit.unit : StateM (Storage R P Q RNG n) PUnit) else _).run state).2 = _
    split <;> rfl
  · simp only [Storage.visit, PVDR.visit, previous]
    change ((Storage.attach widen entry.bin entry.bin.below).run
      ((Storage.attach widen entry.bin entry.bin.above).run state).2).2 = _
    rw [attach_run, attach_run]
    rfl

theorem drain_run [Sub R] [Add P] [Mul P] (widen : R → P) (hop : P) (fuel : Nat)
    (state : Storage R P Q RNG n) :
    ((Storage.drain widen hop fuel).run state).2 =
      { state with work := PVDR.drain widen hop state.queue state.input fuel state.work } := by
  induction fuel generalizing state with
  | zero => rfl
  | succ fuel ih =>
    rw [PVDR.drain_succ]
    rcases popped : state.queue.pop state.work.queue with ⟨entry, rest⟩
    cases entry with
    | none =>
      have popRun : (Storage.pop.run state) = (none, state) := by
        simp [Storage.pop, StateT.run, popped]
      simp only [Storage.drain, foldUntilM, run_bind, popRun, run_pure]
    | some entry =>
      simp only [Storage.drain, foldUntilM, run_bind]
      have popRun : (Storage.pop.run state) = (some entry, { state with work.queue := rest }) := by
        simp [Storage.pop, StateT.run, popped]
      rw [popRun]
      change ((Storage.drain widen hop fuel).run
        ((Storage.visit widen hop entry).run { state with work.queue := rest }).2).2 = _
      rw [ih, visit_run]

/-- The storage action computes the same gates, ordered links and RNG state as
the functional PVDR model, for the same queue and random-source semantics. -/
theorem analyze_run [OfNat R 0] [LT R] [LE R] [DecidableLT R] [DecidableLE R]
    [Max R] [Sub R] [OfNat P 0] [Add P] [Mul P]
    (widen : R → P) (hop : P) (thresholds : R → R × R)
    (queue : Queue R Q n) (draw : StateM RNG R) (rng : RNG)
    (input : PVDRInput R P n) (gates : BinTable GateState n) :
    let work : Work P Q n :=
      ⟨gates, BinTable.ofFn (fun _ => false), BinTable.ofFn (fun bin => ⟨bin, 0, true⟩), [], queue.empty⟩
    let storage : Storage R P Q RNG n := ⟨input, queue, draw, rng, work⟩
    let final := ((Storage.analyze widen hop thresholds).run storage).2
    (PVDR.analyze widen hop thresholds queue draw gates input).run rng =
      ((final.work.gates, ⟨final.work.reverseOrder.reverse, final.work.links⟩), final.rng) := by
  have seedEq (low trigger : R) (bin : ActiveBin n) :
      initializeBinWith (Q := Q) (RNG := RNG) updateGate widen low trigger bin =
        initializeBin widen low trigger bin := rfl
  simp only [Storage.analyze, Storage.analyzeWith, seedEq, run_bind, peak_run, seed_run, drain_run]
  rfl

end DSP.Resynthesis.PVDR.Storage

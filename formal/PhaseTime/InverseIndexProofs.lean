import PhaseTime.InverseCodegen
import PhaseTime.InverseStorageProofs

namespace PhaseTime.InverseRuntime

open DSP.Codegen Storage

set_option linter.unusedSimpArgs false

variable {R : Type} [a : Arithmetic R] {n : Nat}

private theorem runBind {S A B : Type} (x : StateM S A) (f : A → StateM S B) (s : S) :
    (x >>= f).run s = (f (x.run s).1).run (x.run s).2 := rfl

private theorem runPure {S A : Type} (x : A) (s : S) :
    (pure x : StateM S A).run s = (x, s) := rfl

private theorem runMap {S A B : Type} (f : A → B) (x : StateM S A) (s : S) :
    (f <$> x).run s = (f (x.run s).1, (x.run s).2) := rfl

private theorem gridRemaining_eq (first last : GridIndex) (cursor : GridCursor first last) :
    gridRemaining first last cursor = last.val - cursor.val + 1 := rfl

private theorem gridCount (first last : GridIndex) (cursor : GridCursor first last)
    (active : cursor.val ≤ last.val) :
    (if gridRemaining first last cursor ≤ Int.ofNat batchCapacity then
      gridRemaining first last cursor else Int.ofNat batchCapacity) =
      Int.ofNat (min batchCapacity (last.val - cursor.val + 1).toNat) := by
  let remaining := last.val - cursor.val + 1
  have nonnegative : 0 ≤ remaining := by dsimp [remaining]; omega
  change (if remaining ≤ (batchCapacity : Int) then remaining else (batchCapacity : Int)) =
    (↑(min batchCapacity remaining.toNat) : Int)
  have cast : (remaining.toNat : Int) = remaining := Int.toNat_of_nonneg nonnegative
  by_cases h : remaining ≤ (batchCapacity : Int)
  · have le : remaining.toNat ≤ batchCapacity := by omega
    rw [if_pos h, Nat.min_eq_right le, cast]
  · have le : batchCapacity ≤ remaining.toNat := by omega
    rw [if_neg h, Nat.min_eq_left le]


private theorem boundedStep_run (s : State R a.Delta n) (left right : Point R)
    (first last : GridIndex) (cursor : GridCursor first last) (active : cursor.val ≤ last.val) :
    let count := min batchCapacity (last.val - cursor.val + 1).toNat
    let next : GridCursor first last := ⟨cursor.val + (count : Int), by
      have := cursor.property
      have hcount := Nat.min_le_right batchCapacity (last.val - cursor.val + 1).toNat
      omega⟩
    (boundedScatterStepM left right first last cursor).run s =
      (.yield next, {s with payload := commitBatch s.payload left right cursor.val (count - 1)}) := by
  let count := min batchCapacity (last.val - cursor.val + 1).toNat
  have positive : gridRemaining first last cursor > 0 := by rw [gridRemaining_eq]; omega
  have hcount := gridCount first last cursor active
  unfold boundedScatterStepM
  simp only [dif_pos positive, Index.ofNat, Index.add, Index.sub, Index.toNat]
  -- Both index suppliers reduce to the same mathematical integer expression.
  have subtract : (Int.ofNat count - 1).toNat = count - 1 := Int.toNat_sub count 1
  simp only [hcount, show Int.ofNat (min batchCapacity (last.val - cursor.val + 1).toNat) =
    Int.ofNat count from rfl, subtract, ← commitBatchM.eq_1, runBind, runPure, commitBatchM_run, Prod.snd]
  have extra_eq : ((if gridRemaining first last cursor ≤ Int.ofNat batchCapacity then
      gridRemaining first last cursor else Int.ofNat batchCapacity) - 1).toNat =
      count - 1 := (congrArg (fun x : Int => (x - 1).toNat) hcount).trans subtract
  let next : GridCursor first last := ⟨cursor.val + (count : Int), by
    have := cursor.property
    have hle := Nat.min_le_right batchCapacity (last.val - cursor.val + 1).toNat
    omega⟩
  exact congrArg (fun extraEdges =>
    (ForInStep.yield next, {s with payload := commitBatch s.payload left right cursor.val extraEdges})) extra_eq

private theorem boundedFuel_run (s : State R a.Delta n) (left right : Point R)
    (first last : GridIndex) (cursor : GridCursor first last) (fuel : Nat)
    (enough : (last.val - cursor.val + 1).toNat ≤ fuel) :
    ((DSP.foldUntilM fuel (boundedScatterStepM left right first last) cursor).run s).2 =
      {s with payload := scatter s.payload left right cursor.val (last.val - cursor.val + 1).toNat} := by
  induction fuel generalizing s cursor with
  | zero =>
    have empty : (last.val - cursor.val + 1).toNat = 0 := by omega
    simp [DSP.foldUntilM, empty, scatter]
    rfl
  | succ fuel ih =>
    by_cases active : cursor.val ≤ last.val
    · let remaining := (last.val - cursor.val + 1).toNat
      let count := min batchCapacity remaining
      have positive : remaining > 0 := by dsimp [remaining]; omega
      have countPos : count > 0 := by simp [count, batchCapacity, positive]
      have countLe : count ≤ remaining := Nat.min_le_right _ _
      have cursorBound := cursor.property
      have enoughNext : remaining - count ≤ fuel := by omega
      have remainder : (last.val - (cursor.val + (count : Int)) + 1).toNat = remaining - count := by
        dsimp [remaining] at countLe ⊢
        omega
      simp only [DSP.foldUntilM, runBind, boundedStep_run s left right first last cursor active, Prod.fst, Prod.snd]
      rw [ih]
      · conv => rhs; rw [scatter]
        simp only [show (last.val - cursor.val + 1).toNat > 0 from positive, dif_pos, count, remaining, remainder]
        rfl
      · exact remainder ▸ enoughNext
    · have empty : (last.val - cursor.val + 1).toNat = 0 := by omega
      have inactive : ¬ gridRemaining first last cursor > 0 := by rw [gridRemaining_eq]; omega
      simp [DSP.foldUntilM, boundedScatterStepM, inactive, scatter, empty, runBind, runPure]
      rfl

theorem boundedScatterM_run (s : State R a.Delta n) (left right : Point R)
    (first last : GridIndex) :
    (boundedScatterM left right first last).run s =
      (⟨⟩, {s with payload := scatter s.payload left right first.val (last.val - first.val + 1).toNat}) := by
  unfold boundedScatterM
  by_cases available : first.val ≤ last.val
  · simp only [dif_pos available, runBind, runPure, Index.toNat, gridRemaining_eq]
    rw [boundedFuel_run]
    exact Nat.le_refl _
  · have empty : (last.val - first.val + 1).toNat = 0 := by omega
    simp [available, empty, scatter, runPure]
    rfl

theorem readBoundedM_eq (input : ReadInput) (bound : PhaseInGrid input.phase) :
    (readBoundedM input bound Arithmetic.decode : Action Float n (Option Float)) =
      readPayloadM input.phase input.cutoff :=
  rfl

private theorem runIf {S A : Type} (p : Prop) [Decidable p] (x y : StateM S A) (s : S) :
    (if p then x else y).run s = if p then x.run s else y.run s := by split <;> rfl

private theorem idBind {A B : Type} (x : Id A) (f : A → Id B) : (x >>= f) = f x := rfl
private theorem idMap {A B : Type} (f : A → B) (x : Id A) : f <$> x = f x := rfl

private def decodePresent (base : Float) (delta : Float32) : Option Float :=
  some (decodeTimestamp base delta)

private theorem loadIntervalM_decodePresent (s : State Float Float32 n)
    (safe : DecodablePayload s.payload) (index : Int) (origin query : Float)
    (loaded : (s.payload.cells.get (wrap n index)).base = some origin) :
    (loadIntervalM index origin query decodePresent).run s =
      (loadIntervalM index origin query Arithmetic.decode).run s := by
  let slot := wrap n index
  let replacement := (s.payload.cells.get slot).replacement
  have hcut : (cut index).run s = (replacement.map (·.cut), s) := rfl
  have hstart : (startDelta index).run s = (replacement.map (·.start), s) := rfl
  have hstop : (endDelta index).run s = (replacement.bind (·.stop), s) := rfl
  have hdecode (delta : Float32) (finite : (decodeTimestamp origin delta).isFinite = true) :
      Arithmetic.decode origin delta = decodePresent origin delta := by
    change (if (decodeTimestamp origin delta).isFinite then some (decodeTimestamp origin delta)
      else none) = some (decodeTimestamp origin delta)
    simp [finite]
  unfold loadIntervalM
  cases hr : replacement with
  | none => simp [runBind, runPure, runIf, idBind, idMap, hcut, hstart, hstop, hr, Option.any]
  | some branch =>
    have hb := safe slot origin (by simp [slot, loaded]) branch (by simp [← show
      replacement = (s.payload.cells.get slot).replacement from rfl, hr])
    cases hs : branch.stop with
    | none =>
      simp [runBind, runPure, runIf, idBind, idMap, hcut, hstart, hstop, hr, hs, hdecode branch.start hb.1]
    | some delta =>
      have hd := hdecode delta (hb.2 delta (by simp [hs]))
      simp [runBind, runPure, runIf, idBind, idMap, hcut, hstart, hstop, hr, hs, hdecode branch.start hb.1, hd]

theorem readBoundedM_decodePresent (s : State Float Float32 n) (input : ReadInput)
    (range : PhaseInGrid input.phase) (safe : DecodablePayload s.payload) :
    (readBoundedM input range decodePresent).run s =
      (readBoundedM input range Arithmetic.decode).run s := by
  let index := (gridFloor input.phase range).val
  have hfront : (Storage.frontier : Action Float n (Option (Point Float))).run s =
      (s.payload.frontier, s) := rfl
  have hbase : (base index).run s = ((s.payload.cells.get (wrap n index)).base, s) := rfl
  unfold readBoundedM readPayloadUsingM
  simp only [runBind, runIf, hfront, hbase, Prod.fst, Prod.snd,
    show (gridFloor input.phase range).val = index from rfl]
  split
  · rfl
  · cases hb : (s.payload.cells.get (wrap n index)).base with
    | none => rfl
    | some origin =>
      simp only [runBind, loadIntervalM_decodePresent s safe index origin input.phase hb]

theorem readPayloadFloat_run (s : State Float Float32 0) (input : ReadInput)
    (range : PhaseInGrid input.phase) (safe : DecodablePayload s.payload) :
    readPayloadFloat input range s safe = ((read s.payload input.phase input.cutoff).getD input.missing, s) := by
  unfold readPayloadFloat
  change ((·.getD input.missing) <$> readBoundedM input range decodePresent).run s = _
  simp only [runMap, readBoundedM_decodePresent s input range safe,
    readBoundedM_eq, readPayloadM_run]

private theorem beginGridBranchM_run (s : State Float Float32 n) (edge : Point Float)
    (bound : PhaseInGrid edge.phase) (high extra : GridIndex) (present : Bool)
    (hhigh : s.highest = if present then some high.val else none)
    (hcap : extra.val = Int.ofNat n) :
    (beginGridBranchM edge (gridFloor edge.phase bound) rfl high extra present).run s =
      (beginBranchM edge).run s := by
  let index := gridFloor edge.phase bound
  have idx : Arithmetic.floor edge.phase = index.val := rfl
  have hstep : (beginRevision : Action Float n PUnit).run s = (⟨⟩, s) := rfl
  have hhighest : (highest : Action Float n (Option Int)).run s = (s.highest, s) := rfl
  have hcapacity : (capacityMinusOne : Action Float n Int).run s = (Int.ofNat n, s) := rfl
  unfold beginGridBranchM beginBranchM beginBranchUsingM prepareBranchM
  simp only [idx, beginBranchUsingM, runIf, runBind, hstep, Prod.fst, Prod.snd]
  unfold prepareBranchUsingM
  simp only [runBind, runPure, hhighest, hcapacity, Prod.fst, Prod.snd, hhigh, ← hcap]
  have nextEq : (Int64.ofInt (gridNext (gridFloor edge.phase bound))).toFloat =
      Arithmetic.ofIndex (index.val + 1) := rfl
  have lowerEq : gridLowerEdge high extra = high.val - extra.val := rfl
  cases present <;>
    simp only [Bool.false_eq_true, if_false, if_true, Option.any_none, Option.any_some, nextEq, lowerEq] <;>
    rfl

private theorem writeGridForwardM_run (s : State Float Float32 n) (left right : Point Float)
    (resolution : Float) (lo : PhaseInGrid left.phase) (hi : PhaseInGrid right.phase)
    (width : 0 < right.phase - left.phase)
    (high extra : GridIndex) (present : Bool)
    (hhigh : s.highest = if present then some high.val else none)
    (hcap : extra.val = Int.ofNat n) :
    (writeGridForwardM left right resolution lo hi width high extra present).run s =
      (writeForwardM resolution left right).run s := by
  have hforward : (hasForward : Action Float n Bool).run s = (s.hasForward, s) := rfl
  have hdesc : (descending : Action Float n Bool).run s = (s.descending, s) := rfl
  have hopen : (openVisitUsingM (beginGridBranchM left (gridFloor left.phase lo) rfl high extra present)).run s =
      (openVisitM left).run s := by
    unfold openVisitM openVisitUsingM
    simp only [runBind, runIf, runPure, hforward, hdesc, beginGridBranchM_run s left lo high extra present hhigh hcap]
  have hhighest : (highest : Action Float n (Option Int)).run s = (s.highest, s) := rfl
  have hcapacity : (capacityMinusOne : Action Float n Int).run s = (Int.ofNat n, s) := rfl
  unfold writeGridForwardM writeForwardM
  simp only [runBind, runPure, hopen, boundedScatterM_run, scatterM_run, hhighest, hcapacity,
    Prod.fst, Prod.snd, hhigh, ← hcap]
  have hfloor : Arithmetic.floor right.phase = (gridFloor right.phase hi).val := rfl
  have hceil : Arithmetic.ceil left.phase = (gridCeil left.phase lo).val := rfl
  cases present with
  | false => simp [hfloor, hceil, gridLowerEdge, Index.sub]; rfl
  | true =>
    by_cases le : high.val ≤ (gridFloor right.phase hi).val <;>
      simp [hfloor, hceil, le, gridLowerEdge, Index.sub] <;> rfl

/-- Both writers use the initial observation and ring metadata from storage.
    The bounded writer additionally uses the configured resolution from storage. -/
theorem writeBoundedM_run (s : State Float Float32 n) (input : WriteInput)
    (range : input.InRange s) :
    writeBoundedM input s range = (writeM s.gridResolution input.observation).run s := by
  let observation := input.observation
  let old := s.previous.getD observation
  let observed := {s with previous := some observation}
  have hview (p t : Float) : (storeViewPrevious p t : Action Float n PUnit).run s = (⟨⟩, s) := rfl
  have hobs : (storePrevious observation.phase observation.time).run s = (⟨⟩, observed) := rfl
  let left : Point Float := ⟨old.phase * s.gridResolution, old.time⟩
  let right : Point Float := ⟨observation.phase * s.gridResolution, observation.time⟩
  let high : GridIndex := ⟨s.highest.getD 0, range.highest⟩
  let extra : GridIndex := ⟨Int.ofNat n, range.capacity⟩
  have hhigh : observed.highest = if s.highest.isSome then some high.val else none := by
    cases h : s.highest <;> simp [observed, high, h]
  have hgrid (width : 0 < right.phase - left.phase) :=
    writeGridForwardM_run observed left right s.gridResolution range.previous range.phase width high extra
      s.highest.isSome hhigh rfl
  have idPure {A : Type} (x : A) : (pure x : Id A) = x := rfl
  change
    (writeObservedM observation old s.previous.isSome fun advances =>
      writeGridForwardM left right s.gridResolution range.previous range.phase
        (range.forwardWidth advances) high extra s.highest.isSome).run s =
    (writeObservedM observation old s.previous.isSome fun _ =>
      writeForwardM s.gridResolution left right).run s
  unfold writeObservedM
  by_cases present : s.previous.isSome = true <;>
    by_cases advances : Arithmetic.lt old.phase observation.phase = true <;>
    simp [runBind, runPure, runIf, idBind, idPure, hview, hobs, present, advances, hgrid]


end PhaseTime.InverseRuntime

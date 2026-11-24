import DSP.Resynthesis.Coverage
import DSP.Resynthesis.Real
import Mathlib.Data.Real.Basic
import Mathlib.Tactic.Linarith

namespace DSP.Resynthesis.PVDR

variable {Q S : Type} {n : ℕ}

/-- Queue accounting together with the maximum-priority rule. -/
structure QueueMaximum (queue : Queue ℝ Q n) extends QueueAccounting queue where
  maximum : ∀ q entry rest, queue.pop q = (some entry, rest) →
    ∀ other ∈ contents q, other.magnitude ≤ entry.magnitude

private theorem get_set_same {A : Type} (table : BinTable A n)
    (bin : ActiveBin n) (value : A) : (table.set bin value).get bin = value := by
  simp [BinTable.set, BinTable.get]

private theorem get_set_other {A : Type} (table : BinTable A n)
    {bin other : ActiveBin n} (value : A) (different : other ≠ bin) :
    (table.set bin value).get other = table.get other := by
  have differentIndices : bin.val ≠ other.val := fun eq => different (Fin.ext eq.symm)
  simp [BinTable.set, BinTable.get, differentIndices]

private theorem get_ofFn {A : Type} (f : ActiveBin n → A) (bin : ActiveBin n) :
    (BinTable.ofFn f).get bin = f bin := by simp [BinTable.ofFn, BinTable.get]

private theorem id_pure {A : Type} (a : A) : (pure a : Id A) = a := rfl
private theorem id_bind {A B : Type} (a : Id A) (f : A → Id B) : a >>= f = f a := rfl
private theorem id_map {A B : Type} (f : A → B) (a : Id A) : f <$> a = f a := rfl

private noncomputable def prepare (low trigger : ℝ) (queue : Queue ℝ Q n) (nextPhase : StateM S ℝ)
    (input : PVDRInput ℝ ℝ n) (work : Work ℝ Q n) (bin : ActiveBin n) :
    StateM S (Work ℝ Q n) := do
  let gate := updateGate low trigger (input.magnitude.get bin) (work.gates.get bin)
  let work := { work with gates := work.gates.set bin gate }
  if gate == .low then
    let delta ← nextPhase
    pure (record work bin ⟨bin, delta, true⟩)
  else
    pure { work with queue := queue.push work.queue (makeEntry input bin true) }

private theorem prepare_at (low trigger : ℝ) {queue : Queue ℝ Q n}
    (law : QueueAccounting queue) (nextPhase : StateM S ℝ) (input : PVDRInput ℝ ℝ n)
    (work : Work ℝ Q n) (bin : ActiveBin n) (state : S) :
    let out := ((prepare low trigger queue nextPhase input work bin).run state).1
    let gate := updateGate low trigger (input.magnitude.get bin) (work.gates.get bin)
    out.gates.get bin = gate ∧
    (gate = .low → out.computed.get bin = true ∧ (out.links.get bin).small = true) ∧
    (gate ≠ .low → out.computed.get bin = work.computed.get bin) ∧
    (∀ other, other ≠ bin → out.gates.get other = work.gates.get other ∧
      out.computed.get other = work.computed.get other ∧ out.links.get other = work.links.get other) ∧
    (∀ entry, entry ∈ law.contents out.queue ↔ entry ∈ law.contents work.queue ∨
      (gate ≠ .low ∧ entry = makeEntry input bin true)) := by
  dsimp only
  unfold prepare
  generalize updateGate low trigger (input.magnitude.get bin) (work.gates.get bin) = gate
  have eqLow : (GateState.low == GateState.low) = true := rfl
  have eqTriggered : (GateState.triggered == GateState.low) = false := rfl
  have eqHigh : (GateState.high == GateState.low) = false := rfl
  cases gate <;> simp [eqLow, eqTriggered, eqHigh, record, get_set_same,
    StateT.run_pure, StateT.run_map, id_map, id_pure,
    fun entry => (law.push work.queue (makeEntry input bin true)).mem_iff (a := entry),
    List.mem_cons, or_comm] <;>
    (intro other ne; simp [get_set_other _ _ ne])

private theorem prepare_fold (low trigger : ℝ) {queue : Queue ℝ Q n}
    (law : QueueAccounting queue) (nextPhase : StateM S ℝ) (input : PVDRInput ℝ ℝ n)
    (bins : List (ActiveBin n)) (unique : bins.Nodup) (work : Work ℝ Q n) (state : S) :
    let out := ((bins.foldlM (prepare low trigger queue nextPhase input) work).run state).1
    let gate := fun bin => updateGate low trigger (input.magnitude.get bin) (work.gates.get bin)
    (∀ bin ∈ bins, out.gates.get bin = gate bin ∧
      (gate bin = .low → out.computed.get bin = true ∧ (out.links.get bin).small = true) ∧
      (gate bin ≠ .low → out.computed.get bin = work.computed.get bin)) ∧
    (∀ bin, bin ∉ bins → out.gates.get bin = work.gates.get bin ∧
      out.computed.get bin = work.computed.get bin ∧ out.links.get bin = work.links.get bin) ∧
    (∀ entry, entry ∈ law.contents out.queue ↔ entry ∈ law.contents work.queue ∨
      ∃ bin ∈ bins, gate bin ≠ .low ∧ entry = makeEntry input bin true) := by
  induction bins generalizing work state with
  | nil => simp [List.foldlM, StateT.run_pure, id_pure]
  | cons head tail ih =>
    obtain ⟨absent, tailUnique⟩ := List.nodup_cons.mp unique
    rw [List.foldlM_cons, StateT.run_bind]
    simp only [id_bind]
    obtain ⟨headGate, headLow, headActive, headOther, headQueue⟩ :=
      prepare_at low trigger law nextPhase input work head state
    obtain ⟨tailAt, tailOther, tailQueue⟩ := ih tailUnique
      ((prepare low trigger queue nextPhase input work head).run state).1
      ((prepare low trigger queue nextPhase input work head).run state).2
    dsimp only at *
    refine ⟨?_, ?_, ?_⟩
    · intro bin belongs
      rcases List.mem_cons.mp belongs with rfl | inTail
      · obtain ⟨gateEq, computedEq, linkEq⟩ := tailOther bin absent
        exact ⟨gateEq.trans headGate,
          fun isLow => ⟨computedEq.trans (headLow isLow).1,
            (congrArg PhaseLink.small linkEq).trans (headLow isLow).2⟩,
          fun active => computedEq.trans (headActive active)⟩
      · have different : bin ≠ head := fun eq => absent (eq ▸ inTail)
        obtain ⟨gateEq, computedEq, _⟩ := headOther bin different
        simpa only [gateEq, computedEq] using tailAt bin inTail
    · intro bin absentAll
      have different : bin ≠ head := fun eq => absentAll (List.mem_cons.mpr (Or.inl eq))
      obtain ⟨gateEq, computedEq, linkEq⟩ := tailOther bin (fun mem =>
        absentAll (List.mem_cons_of_mem head mem))
      obtain ⟨g, c, l⟩ := headOther bin different
      exact ⟨gateEq.trans g, computedEq.trans c, linkEq.trans l⟩
    · intro entry
      rw [tailQueue, headQueue]
      constructor
      · rintro ((old | ⟨active, eq⟩) | ⟨bin, inTail, active, eq⟩)
        · exact Or.inl old
        · exact Or.inr ⟨head, by simp, active, eq⟩
        · exact Or.inr ⟨bin, List.mem_cons_of_mem _ inTail,
            by simpa only [(headOther bin (fun e => absent (e ▸ inTail))).1] using active, eq⟩
      · rintro (old | ⟨bin, inAll, active, eq⟩)
        · exact Or.inl (Or.inl old)
        · rcases List.mem_cons.mp inAll with rfl | inTail
          · exact Or.inl (Or.inr ⟨active, eq⟩)
          · exact Or.inr ⟨bin, inTail,
              by simpa only [(headOther bin (fun e => absent (e ▸ inTail))).1] using active, eq⟩

private theorem prepare_queue_nodup (low trigger : ℝ) {queue : Queue ℝ Q n}
    (law : QueueAccounting queue) (nextPhase : StateM S ℝ) (input : PVDRInput ℝ ℝ n)
    (bins : List (ActiveBin n)) (unique : bins.Nodup) (work : Work ℝ Q n) (state : S)
    (nodup : (law.contents work.queue).Nodup)
    (fresh : ∀ bin ∈ bins, makeEntry input bin true ∉ law.contents work.queue) :
    (law.contents (((bins.foldlM (prepare low trigger queue nextPhase input) work).run state).1.queue)).Nodup := by
  induction bins generalizing work state with
  | nil => exact nodup
  | cons bin tail ih =>
    obtain ⟨absent, tailUnique⟩ := List.nodup_cons.mp unique
    rw [List.foldlM_cons, StateT.run_bind, id_bind]
    apply ih tailUnique
    · unfold prepare
      dsimp only
      have eqLow : (GateState.low == GateState.low) = true := rfl
      have eqTriggered : (GateState.triggered == GateState.low) = false := rfl
      have eqHigh : (GateState.high == GateState.low) = false := rfl
      have pushed := (law.push work.queue (makeEntry input bin true)).nodup_iff.mpr
        (List.nodup_cons.mpr ⟨fresh bin (by simp), nodup⟩)
      generalize updateGate low trigger (input.magnitude.get bin) (work.gates.get bin) = gate
      cases gate <;> simp [eqLow, eqTriggered, eqHigh, record,
        StateT.run_pure, StateT.run_map, id_map, id_pure, nodup, pushed]
    · intro other inTail mem
      have atStep := (prepare_at low trigger law nextPhase input work bin state).2.2.2.2
      rcases (atStep (makeEntry input other true)).mp mem with old | ⟨_, equal⟩
      · exact fresh other (List.mem_cons_of_mem _ inTail) old
      · have same : other = bin := congrArg Entry.bin equal
        exact absent (same ▸ inTail)

private theorem pop_strict_max {queue : Queue ℝ Q n} (law : QueueMaximum queue)
    (q : Q) (wanted : Entry ℝ n) (present : wanted ∈ law.contents q)
    (dominates : ∀ other ∈ law.contents q, other ≠ wanted →
      other.magnitude < wanted.magnitude) :
    ∃ rest, queue.pop q = (some wanted, rest) := by
  cases popped : queue.pop q with
  | mk entry rest =>
    cases entry with
    | none => simp [law.popNone _ _ popped] at present
    | some entry =>
      have mem : entry ∈ law.contents q :=
        (law.popSome _ _ _ popped).mem_iff.mpr (by simp)
      by_cases same : entry = wanted
      · exact ⟨rest, congrArg (fun e => (some e, rest)) same⟩
      · exact (not_lt_of_ge (law.maximum _ _ _ popped wanted present)
          (dominates entry mem same)).elim

private theorem popped_rest {queue : Queue ℝ Q n} (law : QueueAccounting queue)
    {q rest : Q} {entry other : Entry ℝ n} (pop : queue.pop q = (some entry, rest))
    (mem : other ∈ law.contents rest) : other ∈ law.contents q :=
  (law.popSome _ _ _ pop).mem_iff.mpr (List.mem_cons_of_mem _ mem)

private theorem attach_computed {queue : Queue ℝ Q n} (input : PVDRInput ℝ ℝ n)
    (work : Work ℝ Q n) (all : ∀ bin, work.computed.get bin = true)
    (parent : ActiveBin n) (neighbor : Option (ActiveBin n)) :
    attach id queue input parent neighbor work = work := by
  cases neighbor <;> simp [attach, all]

private theorem drain_computed {queue : Queue ℝ Q n} (hop : ℝ) (input : PVDRInput ℝ ℝ n)
    (fuel : ℕ) (work : Work ℝ Q n) (all : ∀ bin, work.computed.get bin = true) :
    let out := drain id hop queue input fuel work
    out.gates = work.gates ∧ out.links = work.links := by
  induction fuel generalizing work with
  | zero => exact ⟨rfl, rfl⟩
  | succ fuel ih =>
    rw [drain_succ]
    cases queue.pop work.queue with
    | mk entry rest =>
      cases entry with
      | none => exact ⟨rfl, rfl⟩
      | some entry =>
        cases flag : entry.previous <;>
          simpa [visit, flag, all, attach_computed] using
            ih { work with queue := rest } all

private theorem analyze_adjacent_of_gates {queue : Queue ℝ Q n} (law : QueueMaximum queue)
    (hop : ℝ) (thresholds : ℝ → ℝ × ℝ) (nextPhase : StateM S ℝ)
    (gates : BinTable GateState n) (input : PVDRInput ℝ ℝ n) (state : S)
    (left center right : ActiveBin n)
    (above : center.above = some right) (below : center.below = some left)
    (centerLeft : center ≠ left) (centerRight : center ≠ right) (leftRight : left ≠ right)
    (dominatesPrevious : ∀ bin, bin ≠ center →
      input.prevMagnitude.get bin < input.prevMagnitude.get center)
    (dominatesCurrent : ∀ bin, bin ≠ center →
      input.prevMagnitude.get bin < input.magnitude.get center)
    (letGate : ActiveBin n → GateState)
    (gateEq : ∀ bin, updateGate (thresholds (referenceMagnitude input)).1
      (thresholds (referenceMagnitude input)).2 (input.magnitude.get bin) (gates.get bin) = letGate bin)
    (active : letGate left ≠ .low ∧ letGate center ≠ .low ∧ letGate right ≠ .low)
    (small : ∀ bin, bin ≠ left → bin ≠ center → bin ≠ right → letGate bin = .low) :
    let result := ((analyze id hop thresholds queue nextPhase gates input).run state).1
    result.1.get center = letGate center ∧
      result.2.link.get center = ⟨center, input.omega.get center * hop, false⟩ ∧
      result.2.link.get left = ⟨center, input.phase.get left - input.phase.get center, false⟩ ∧
      result.2.link.get right = ⟨center, input.phase.get right - input.phase.get center, false⟩ ∧
      ∀ bin, bin ≠ left → bin ≠ center → bin ≠ right → (result.2.link.get bin).small = true := by
  let initial : Work ℝ Q n :=
    ⟨gates, BinTable.ofFn (fun _ => false), BinTable.ofFn (fun bin => ⟨bin, 0, true⟩), [], queue.empty⟩
  let low := (thresholds (referenceMagnitude input)).1
  let trigger := (thresholds (referenceMagnitude input)).2
  let work := (((List.finRange (n / 2 - 1)).foldlM
    (prepare low trigger queue nextPhase input) initial).run state).1
  let fuel := 2 * (n / 2 - 1)
  let out := drain id hop queue input fuel work
  have resultEq : ((analyze id hop thresholds queue nextPhase gates input).run state).1 =
      (out.gates, ⟨out.reverseOrder.reverse, out.links⟩) := by
    cases eq : thresholds (referenceMagnitude input)
    simp [analyze, out, fuel, work, low, trigger, initial, eq, id_map]
    unfold prepare
    exact ⟨rfl, rfl, rfl⟩
  simp only [resultEq]
  obtain ⟨atBin, _, inQueue⟩ := prepare_fold low trigger law.toQueueAccounting nextPhase input
    _ (List.nodup_finRange _) initial state
  have prep (bin) := atBin bin (List.mem_finRange bin)
  have gateEq' bin : updateGate low trigger (input.magnitude.get bin) (gates.get bin) = letGate bin := gateEq bin
  have acts (bin) : work.gates.get bin = letGate bin ∧
      (letGate bin = .low → work.computed.get bin = true ∧ (work.links.get bin).small = true) ∧
      (letGate bin ≠ .low → work.computed.get bin = false) := by
    simpa only [show initial.gates = gates from rfl, gateEq', show initial.computed =
      BinTable.ofFn (fun _ => false) from rfl, get_ofFn] using prep bin
  have queued (entry) : entry ∈ law.contents work.queue ↔
      ∃ bin, letGate bin ≠ .low ∧ entry = makeEntry input bin true := by
    simpa only [show initial.queue = queue.empty from rfl, law.empty, List.not_mem_nil,
      false_or, show initial.gates = gates from rfl, gateEq', List.mem_finRange, true_and]
      using inQueue entry
  let prev := makeEntry input center true
  let curr := makeEntry input center false
  obtain ⟨rest, firstPop⟩ := pop_strict_max law work.queue prev
    ((queued prev).mpr ⟨center, active.2.1, rfl⟩) (by
      intro entry mem ne
      obtain ⟨bin, _, rfl⟩ := (queued entry).mp mem
      exact dominatesPrevious bin (fun same => ne (congrArg (fun b => makeEntry input b true) same)))
  let rootLink : PhaseLink ℝ n := ⟨center, input.omega.get center * hop, false⟩
  let rooted := pushCurrent queue input (record { work with queue := rest } center rootLink) center
  have firstVisit : visit id hop queue input prev { work with queue := rest } = rooted := by
    simp [prev, rooted, visit, makeEntry, (acts center).2.2 active.2.1, rootLink]
  obtain ⟨rest₂, secondPop⟩ := pop_strict_max law rooted.queue curr
    ((law.push rest curr).mem_iff.mpr (by simp)) (by
      intro entry mem ne
      have inRest := List.mem_cons.mp ((law.push rest curr).mem_iff.mp mem)
      rcases inRest with equal | old
      · exact (ne equal).elim
      · obtain ⟨bin, _, rfl⟩ := (queued entry).mp
          (popped_rest law.toQueueAccounting firstPop old)
        have nodup := prepare_queue_nodup low trigger law.toQueueAccounting nextPhase input
          _ (List.nodup_finRange _) initial state
          (by change (law.contents queue.empty).Nodup; simp [law.empty])
          (by intro bin _; change makeEntry input bin true ∉ law.contents queue.empty; simp [law.empty])
        have without := (List.nodup_cons.mp ((law.popSome _ _ _ firstPop).nodup_iff.mp nodup)).1
        exact dominatesCurrent bin (fun same => by subst bin; exact without old))
  let rightLink : PhaseLink ℝ n := ⟨center, input.phase.get right - input.phase.get center, false⟩
  let leftLink : PhaseLink ℝ n := ⟨center, input.phase.get left - input.phase.get center, false⟩
  let attachedRight := pushCurrent queue input
    (record { rooted with queue := rest₂ } right rightLink) right
  let attached := pushCurrent queue input (record attachedRight left leftLink) left
  have secondVisit : visit id hop queue input curr { rooted with queue := rest₂ } = attached := by
    simp [curr, visit, makeEntry, above, below, attach, rooted, record, pushCurrent,
      get_set_other, get_set_same, centerRight, leftRight, Ne.symm centerLeft,
      Ne.symm centerRight, (acts left).2.2 active.1, (acts right).2.2 active.2.2, rootLink,
      attached, attachedRight, rightLink, leftLink]
  have all : ∀ bin, attached.computed.get bin = true := by
    intro bin
    by_cases l : bin = left
    · subst bin; simp [attached, pushCurrent, record, get_set_same]
    by_cases c : bin = center
    · subst bin
      simp [attached, attachedRight, rooted, pushCurrent, record, get_set_same, get_set_other,
        centerLeft, centerRight]
    by_cases r : bin = right
    · subst bin
      simp [attached, attachedRight, pushCurrent, record, get_set_same, get_set_other, Ne.symm leftRight]
    simp [attached, attachedRight, rooted, pushCurrent, record, get_set_other, l, c, r,
      (acts bin).2.1 (small bin l c r) |>.1]
  have enough : 2 ≤ fuel := by
    have valid := center.isLt
    dsimp [fuel]
    omega
  have outEq : out = drain id hop queue input (fuel - 2) attached := by
    have fuelEq : fuel = (fuel - 2) + 1 + 1 := by omega
    dsimp only [out]
    conv_lhs => rw [fuelEq]
    rw [drain_succ, firstPop]
    simp only [firstVisit]
    rw [drain_succ, secondPop]
    simp only [secondVisit]
  obtain ⟨gateDone, linksDone⟩ := drain_computed hop input (fuel - 2) attached all
  rw [outEq, gateDone, linksDone]
  refine ⟨(acts center).1, ?_, ?_, ?_, ?_⟩
  · simp [attached, attachedRight, rooted, pushCurrent, record, get_set_same, get_set_other,
      centerLeft, centerRight, rootLink]
  · simp [attached, pushCurrent, record, get_set_same, leftLink]
  · simp [attached, attachedRight, pushCurrent, record, get_set_same, get_set_other,
      Ne.symm leftRight, rightLink]
  · intro bin l c r
    simpa [attached, attachedRight, rooted, pushCurrent, record, get_set_other, l, c, r]
      using (acts bin).2.1 (small bin l c r) |>.2

private theorem referenceMagnitude_eq {input : PVDRInput ℝ ℝ n} (center : ActiveBin n)
    {peak : ℝ} (nonnegative : 0 ≤ peak) (centerPeak : input.magnitude.get center = peak)
    (bound : ∀ bin, input.magnitude.get bin ≤ peak ∧ input.prevMagnitude.get bin ≤ peak) :
    referenceMagnitude input = peak := by
  let step := fun (p : ℝ) (bin : ActiveBin n) =>
    max (max p (input.magnitude.get bin)) (input.prevMagnitude.get bin)
  have upper (bins : List (ActiveBin n)) (p : ℝ) (h : p ≤ peak) :
      bins.foldl step p ≤ peak := by
    induction bins generalizing p with
    | nil => exact h
    | cons bin rest ih =>
      exact ih _ (max_le (max_le h (bound bin).1) (bound bin).2)
  have lower (bins : List (ActiveBin n)) (p : ℝ) :
      (peak ≤ p ∨ center ∈ bins) → peak ≤ bins.foldl step p := by
    induction bins generalizing p with
    | nil => simp
    | cons bin rest ih =>
      intro h
      apply ih
      rcases h with above | includes
      · exact Or.inl (above.trans ((le_max_left _ _).trans (le_max_left _ _)))
      · rcases List.mem_cons.mp includes with rfl | tail
        · left
          rw [← centerPeak]
          exact (le_max_right _ _).trans (le_max_left _ _)
        · exact Or.inr tail
  rw [referenceMagnitude, Fin.foldl_eq_foldl_finRange]
  exact le_antisymm (upper _ _ nonnegative) (lower _ _ (Or.inr (List.mem_finRange center)))

/-- Exact PVDR analysis of a positive, bin-centred Hann tone whose two
immediate neighboring active bins are present. The hypotheses concern the
previous and current magnitude tables, leaving their phase data unrestricted. -/
theorem analyze_hann_tone {queue : Queue ℝ Q n} (law : QueueMaximum queue)
    (hop amplitude : ℝ) (amplitudePositive : 0 < amplitude) (nextPhase : StateM S ℝ)
    (gates : BinTable GateState n) (input : PVDRInput ℝ ℝ n) (state : S)
    (left center right : ActiveBin n)
    (above : center.above = some right) (below : center.below = some left)
    (current : ∀ bin, input.magnitude.get bin =
      if bin = center then amplitude / 4
      else if bin = left ∨ bin = right then amplitude / 8 else 0)
    (previous : ∀ bin, input.prevMagnitude.get bin = input.magnitude.get bin) :
    let result := ((analyze id hop exactMath.thresholds queue nextPhase gates input).run state).1
    result.1.get center = (if gates.get center = .low then .triggered else .high) ∧
      result.2.link.get center = ⟨center, input.omega.get center * hop, false⟩ ∧
      result.2.link.get left = ⟨center, input.phase.get left - input.phase.get center, false⟩ ∧
      result.2.link.get right = ⟨center, input.phase.get right - input.phase.get center, false⟩ ∧
      ∀ bin, bin ≠ left → bin ≠ center → bin ≠ right → (result.2.link.get bin).small = true := by
  have rightVal : right.val = center.val + 1 := by
    unfold ActiveBin.above at above
    split at above
    · have equal := Option.some.inj above
      exact (congrArg Fin.val equal).symm
    · contradiction
  have leftPos : 0 < center.val := by
    unfold ActiveBin.below at below
    split at below
    · assumption
    · contradiction
  have leftVal : left.val = center.val - 1 := by
    unfold ActiveBin.below at below
    split at below
    · have equal := Option.some.inj below
      exact (congrArg Fin.val equal).symm
    · contradiction
  have centerLeft : center ≠ left := fun eq => by have := congrArg Fin.val eq; omega
  have centerRight : center ≠ right := fun eq => by have := congrArg Fin.val eq; omega
  have leftRight : left ≠ right := fun eq => by have := congrArg Fin.val eq; omega
  have centerPeak : input.magnitude.get center = amplitude / 4 := by simp [current]
  have dominates (bin : ActiveBin n) (ne : bin ≠ center) :
      input.magnitude.get bin < input.magnitude.get center := by
    rw [current bin, centerPeak, if_neg ne]
    split <;> linarith
  have peak := referenceMagnitude_eq center (by linarith : 0 ≤ amplitude / 4) centerPeak (by
    intro bin
    rw [previous]
    by_cases same : bin = center
    · subst bin; simp [centerPeak]
    · exact ⟨(dominates bin same).le.trans_eq centerPeak,
        (dominates bin same).le.trans_eq centerPeak⟩)
  let lit : ActiveBin n → Prop := fun bin => bin = left ∨ bin = center ∨ bin = right
  let gate (bin : ActiveBin n) : GateState :=
    if lit bin then (if gates.get bin = .low then .triggered else .high) else .low
  have eqGate (bin : ActiveBin n) :
      updateGate (exactMath.thresholds (referenceMagnitude input)).1
        (exactMath.thresholds (referenceMagnitude input)).2
        (input.magnitude.get bin) (gates.get bin) = gate bin := by
    rw [peak]
    change updateGate (amplitude / 4 * 0.000251188643150958) (amplitude / 4 * 0.001)
      (input.magnitude.get bin) (gates.get bin) = gate bin
    have litGate (h : lit bin) :
        amplitude / 8 ≤ input.magnitude.get bin := by
      rcases h with rfl | rfl | rfl
      · simp [current, Ne.symm centerLeft]
      · rw [centerPeak]; linarith
      · simp [current, Ne.symm centerRight]
    by_cases h : lit bin
    · have positive : ¬ input.magnitude.get bin ≤ 0 := by have := litGate h; linarith
      have low : ¬ input.magnitude.get bin < amplitude / 4 * 0.000251188643150958 := by
        have := litGate h
        linarith
      have trigger : amplitude / 4 * 0.001 < input.magnitude.get bin := by
        have := litGate h
        linarith
      cases prior : gates.get bin <;> simp [gate, h, updateGate, positive, low, trigger, prior]
    · have zero : input.magnitude.get bin = 0 := by
        have noLeft : bin ≠ left := fun eq => h (Or.inl eq)
        have noCenter : bin ≠ center := fun eq => h (Or.inr (Or.inl eq))
        have noRight : bin ≠ right := fun eq => h (Or.inr (Or.inr eq))
        simp [current, noLeft, noCenter, noRight]
      simp [gate, h, updateGate, zero]
  have active : gate left ≠ .low ∧ gate center ≠ .low ∧ gate right ≠ .low := by
    have litActive (bin) (h : lit bin) : gate bin ≠ .low := by
      by_cases prior : gates.get bin = .low <;> simp [gate, h, prior]
    exact ⟨litActive left (Or.inl rfl), litActive center (Or.inr (Or.inl rfl)),
      litActive right (Or.inr (Or.inr rfl))⟩
  have small (bin) (l : bin ≠ left) (c : bin ≠ center) (r : bin ≠ right) : gate bin = .low := by
    simp [gate, lit, l, c, r]
  simpa [gate, lit] using analyze_adjacent_of_gates law hop exactMath.thresholds nextPhase gates
    input state left center right above below centerLeft centerRight leftRight
    (fun bin ne => by simpa only [previous] using dominates bin ne)
    (fun bin ne => by simpa only [previous] using dominates bin ne) gate eqGate active small

end DSP.Resynthesis.PVDR

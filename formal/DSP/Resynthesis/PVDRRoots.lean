import DSP.Resynthesis.PVDRProofs
import Mathlib.Data.Real.Basic
import Mathlib.Tactic.Ring

namespace DSP.Resynthesis.PVDR

variable {Q : Type} {n : ℕ}

/-- A computed bin's link refers directly to a computed self root. A nonself
link stores the phase difference and follows its root in the emitted order. -/
def Work.Rooted (input : PVDRInput ℝ ℝ n) (work : Work ℝ Q n) : Prop :=
  ∀ bin, work.computed.get bin = true →
    let link := work.links.get bin
    work.computed.get link.root = true ∧
      (work.links.get link.root).root = link.root ∧
      (link.root ≠ bin →
        link.delta = input.phase.get bin - input.phase.get link.root ∧
          ∃ before between after, work.reverseOrder = before ++ bin :: between ++ link.root :: after)

private theorem get_set_same {A : Type} (table : BinTable A n)
    (bin : ActiveBin n) (value : A) : (table.set bin value).get bin = value := by
  simp [BinTable.set, BinTable.get]

private theorem get_set_other {A : Type} (table : BinTable A n)
    {bin other : ActiveBin n} (value : A) (different : other ≠ bin) :
    (table.set bin value).get other = table.get other := by
  have differentIndices : bin.val ≠ other.val := fun eq => different (Fin.ext eq.symm)
  simp [BinTable.set, BinTable.get, differentIndices]

private theorem old_bin_ne_fresh {work : Work ℝ Q n} {bin other : ActiveBin n}
    (fresh : work.computed.get bin = false) (old : work.computed.get other = true) :
    other ≠ bin := fun eq => by subst other; simp [fresh] at old

private theorem set_preserves_true {table : BinTable Bool n} {bin other : ActiveBin n}
    (old : table.get other = true) : (table.set bin true).get other = true := by
  by_cases same : other = bin
  · subst other
    exact get_set_same _ _ _
  · exact (get_set_other _ _ same).trans old

private theorem record_preserves_rooted {input : PVDRInput ℝ ℝ n} {work : Work ℝ Q n}
    {bin : ActiveBin n} (link : PhaseLink ℝ n) (h : work.Rooted input)
    (recorded : work.Recorded) (fresh : work.computed.get bin = false)
    (newRoot : link.root = bin ∨
      work.computed.get link.root = true ∧
      (work.links.get link.root).root = link.root ∧
      link.delta = input.phase.get bin - input.phase.get link.root) :
    (record work bin link).Rooted input := by
  intro other computed
  simp only [record] at computed ⊢
  by_cases same : other = bin
  · subst other
    simp only [get_set_same]
    rcases newRoot with rootSame | ⟨rootComputed, rootSelf, delta⟩
    · simp [rootSame, get_set_same]
    · have different := old_bin_ne_fresh fresh rootComputed
      obtain ⟨between, after, order⟩ :=
        List.append_of_mem ((recorded.1 link.root).1 rootComputed)
      refine ⟨(get_set_other _ _ different).trans rootComputed,
        congrArg PhaseLink.root (get_set_other _ _ different) |>.trans rootSelf,
        fun _ => ⟨delta, [], between, after, ?_⟩⟩
      exact congrArg (bin :: ·) order
  · simp only [get_set_other _ _ same] at computed ⊢
    obtain ⟨rootComputed, rootSelf, delta⟩ := h other computed
    have different := old_bin_ne_fresh fresh rootComputed
    refine ⟨(get_set_other _ _ different).trans rootComputed,
      congrArg PhaseLink.root (get_set_other _ _ different) |>.trans rootSelf, ?_⟩
    intro nonself
    obtain ⟨offset, before, between, after, order⟩ := delta nonself
    exact ⟨offset, bin :: before, between, after, congrArg (bin :: ·) order⟩

private theorem attach_new_link {input : PVDRInput ℝ ℝ n} {work : Work ℝ Q n}
    (h : work.Rooted input) (parent bin : ActiveBin n)
    (parentComputed : work.computed.get parent = true) :
    let parentLink := work.links.get parent
    let offset := input.phase.get bin - input.phase.get parent
    let delta := if parentLink.root = parent then offset else offset + parentLink.delta
    work.computed.get parentLink.root = true ∧
      (work.links.get parentLink.root).root = parentLink.root ∧
      delta = input.phase.get bin - input.phase.get parentLink.root := by
  obtain ⟨rootComputed, rootSelf, rootDelta⟩ := h parent parentComputed
  refine ⟨rootComputed, rootSelf, ?_⟩
  split
  · rename_i same
    rw [same]
  · rename_i different
    rw [(rootDelta different).1]
    ring

theorem attach_preserves_rooted (queue : Queue ℝ Q n) (input : PVDRInput ℝ ℝ n)
    (parent : ActiveBin n) (neighbor : Option (ActiveBin n)) {work : Work ℝ Q n}
    (h : work.Rooted input) (recorded : work.Recorded)
    (parentComputed : work.computed.get parent = true) :
    (attach id queue input parent neighbor work).Rooted input := by
  cases neighbor with
  | none => exact h
  | some bin =>
    cases flag : work.computed.get bin with
    | true => simpa [attach, flag] using h
    | false =>
      simp only [attach, flag, Bool.false_eq_true, if_false]
      apply record_preserves_rooted _ h recorded flag
      exact Or.inr (attach_new_link h parent bin parentComputed)

private theorem attach_preserves_computed (queue : Queue ℝ Q n) (input : PVDRInput ℝ ℝ n)
    (parent : ActiveBin n) (neighbor : Option (ActiveBin n)) {work : Work ℝ Q n}
    {other : ActiveBin n} (old : work.computed.get other = true) :
    (attach id queue input parent neighbor work).computed.get other = true := by
  cases neighbor with
  | none => exact old
  | some bin =>
    cases flag : work.computed.get bin with
    | true => simpa [attach, flag] using old
    | false =>
      simp only [attach, flag, Bool.false_eq_true, if_false]
      exact set_preserves_true old

/-- The membership laws needed to prove root soundness. A successful pop
can expose only an entry already present and cannot introduce another entry. -/
structure QueueMembership (queue : Queue ℝ Q n) where
  contains : Q → Entry ℝ n → Prop
  empty : ∀ entry, ¬contains queue.empty entry
  push : ∀ q added entry, contains (queue.push q added) entry → entry = added ∨ contains q entry
  popped : ∀ q entry rest, queue.pop q = (some entry, rest) → contains q entry
  rest : ∀ q entry rest, queue.pop q = (some entry, rest) →
    ∀ other, contains rest other → contains q other

/-- Every queued current entry names a computed bin. -/
def Work.EntriesReady {queue : Queue ℝ Q n} (law : QueueMembership queue)
    (work : Work ℝ Q n) : Prop :=
  ∀ entry, law.contains work.queue entry → entry.previous = false →
    work.computed.get entry.bin = true

private theorem record_push_entries_ready {queue : Queue ℝ Q n} (law : QueueMembership queue)
    (input : PVDRInput ℝ ℝ n) (bin : ActiveBin n) (link : PhaseLink ℝ n)
    {work : Work ℝ Q n} (h : work.EntriesReady law) :
    (pushCurrent queue input (record work bin link) bin).EntriesReady law := by
  intro entry mem current
  rcases law.push _ _ _ mem with same | old
  · subst entry
    exact get_set_same _ _ _
  · exact set_preserves_true (h entry old current)

private theorem attach_entries_ready {queue : Queue ℝ Q n} (law : QueueMembership queue)
    (input : PVDRInput ℝ ℝ n) (parent : ActiveBin n) (neighbor : Option (ActiveBin n))
    {work : Work ℝ Q n} (h : work.EntriesReady law) :
    (attach id queue input parent neighbor work).EntriesReady law := by
  cases neighbor with
  | none => exact h
  | some bin =>
    cases flag : work.computed.get bin with
    | true => simpa [attach, flag] using h
    | false =>
      simp only [attach, flag, Bool.false_eq_true, if_false]
      exact record_push_entries_ready law input bin _ h

private theorem visit_rooted_ready {queue : Queue ℝ Q n} (law : QueueMembership queue)
    (input : PVDRInput ℝ ℝ n) (hop : ℝ) (entry : Entry ℝ n) {work : Work ℝ Q n}
    (rooted : work.Rooted input) (recorded : work.Recorded) (ready : work.EntriesReady law)
    (valid : entry.previous = false → work.computed.get entry.bin = true) :
    (visit id hop queue input entry work).Rooted input ∧
      (visit id hop queue input entry work).EntriesReady law := by
  cases previous : entry.previous with
  | true =>
    cases flag : work.computed.get entry.bin with
    | true => simpa [visit, previous, flag] using And.intro rooted ready
    | false =>
      simp only [visit, previous, flag, if_true, Bool.false_eq_true, if_false]
      refine ⟨?_, record_push_entries_ready law input entry.bin _ ready⟩
      exact record_preserves_rooted _ rooted recorded flag (Or.inl rfl)
  | false =>
    simp only [visit, previous, Bool.false_eq_true, if_false]
    have computed := valid previous
    refine ⟨attach_preserves_rooted queue input entry.bin entry.bin.below ?_ ?_ ?_, ?_⟩
    · exact attach_preserves_rooted queue input entry.bin entry.bin.above rooted recorded computed
    · exact attach_preserves_recorded id queue input entry.bin entry.bin.above recorded
    · exact attach_preserves_computed queue input entry.bin entry.bin.above computed
    · exact attach_entries_ready law input entry.bin entry.bin.below
        (attach_entries_ready law input entry.bin entry.bin.above ready)

/-- Draining preserves root soundness and readiness of the remaining entries. -/
theorem drain_rooted_ready {queue : Queue ℝ Q n} (law : QueueMembership queue)
    (input : PVDRInput ℝ ℝ n) (hop : ℝ) (fuel : ℕ) {work : Work ℝ Q n}
    (rooted : work.Rooted input) (recorded : work.Recorded) (ready : work.EntriesReady law) :
    (drain id hop queue input fuel work).Rooted input ∧
      (drain id hop queue input fuel work).EntriesReady law := by
  induction fuel generalizing work with
  | zero => exact ⟨rooted, ready⟩
  | succ fuel ih =>
    rw [drain_succ]
    cases popped : queue.pop work.queue with
    | mk entry rest =>
      cases entry with
      | none => exact ⟨rooted, ready⟩
      | some entry =>
        obtain ⟨nextRooted, nextReady⟩ := visit_rooted_ready law input hop entry
          (work := { work with queue := rest }) rooted recorded
          (fun other mem => ready other (law.rest _ _ _ popped other mem))
          (ready entry (law.popped _ _ _ popped))
        exact ih nextRooted
          (visit_preserves_recorded id hop queue input entry recorded) nextReady

private theorem foldlM_fresh {S : Type} (valid : Work ℝ Q n → Prop)
    (step : Work ℝ Q n → ActiveBin n → StateM S (Work ℝ Q n))
    (stepValid : ∀ work bin state, valid work → work.computed.get bin = false →
      valid ((step work bin).run state).1)
    (stepFresh : ∀ work bin other state, other ≠ bin →
      ((step work bin).run state).1.computed.get other = work.computed.get other)
    (bins : List (ActiveBin n)) (unique : bins.Nodup) (work : Work ℝ Q n)
    (state : S) (h : valid work) (fresh : ∀ bin ∈ bins, work.computed.get bin = false) :
    valid ((bins.foldlM step work).run state).1 := by
  induction bins generalizing work state with
  | nil => exact h
  | cons bin bins ih =>
    rw [List.foldlM_cons, StateT.run_bind]
    obtain ⟨absent, tailUnique⟩ := List.nodup_cons.mp unique
    refine ih tailUnique _ _ (stepValid work bin state h (fresh bin (by simp))) ?_
    intro other mem
    have different : other ≠ bin := fun eq => absent (eq ▸ mem)
    rw [stepFresh work bin other state different]
    exact fresh other (List.mem_cons_of_mem bin mem)

/-- Each emitted bin refers directly to an emitted self root. The link delta
of a nonself bin is its input phase minus its root's input phase. -/
private theorem analyze_roots_with_order {S : Type} (hop : ℝ) (thresholds : ℝ → ℝ × ℝ)
    (queue : Queue ℝ Q n) (law : QueueMembership queue)
    (nextPhase : StateM S ℝ) (gates : BinTable GateState n) (input : PVDRInput ℝ ℝ n)
    (state : S) :
    let graph := ((analyze id hop thresholds queue nextPhase gates input).run state).1.2
    ∀ bin ∈ graph.order,
      let link := graph.link.get bin
      link.root ∈ graph.order ∧
        (graph.link.get link.root).root = link.root ∧
        (link.root ≠ bin →
          link.delta = input.phase.get bin - input.phase.get link.root ∧
            ∃ before between after, graph.order = before ++ link.root :: between ++ bin :: after) := by
  unfold analyze
  cases thresholds (referenceMagnitude input) with
  | mk low trigger =>
    simp only
    rw [StateT.run_bind]
    let finalWork (work : Work ℝ Q n) := drain id hop queue input (2 * (n / 2 - 1)) work
    let valid (work : Work ℝ Q n) := work.Recorded ∧ work.Rooted input ∧ work.EntriesReady law
    suffices ∀ work, valid work →
        let final := finalWork work
        ∀ bin ∈ final.reverseOrder.reverse,
          let link := final.links.get bin
          link.root ∈ final.reverseOrder.reverse ∧
            (final.links.get link.root).root = link.root ∧
            (link.root ≠ bin →
              link.delta = input.phase.get bin - input.phase.get link.root ∧
              ∃ before between after,
                final.reverseOrder.reverse = before ++ link.root :: between ++ bin :: after) by
      apply this
      refine foldlM_fresh valid _ ?_ ?_ _ (List.nodup_finRange _) _ _ ?_ ?_
      · intro work bin state h fresh
        have ⟨recorded, rooted, ready⟩ := h
        split
        · rw [StateT.run_bind]
          exact ⟨record_preserves_recorded _ recorded fresh,
            record_preserves_rooted _ rooted recorded fresh (Or.inl rfl),
            fun entry mem current => set_preserves_true (ready entry mem current)⟩
        · refine ⟨recorded, rooted, ?_⟩
          intro entry mem current
          rcases law.push _ _ _ mem with same | already
          · subst entry
            cases current
          · exact ready entry already current
      · intro work bin other state different
        split
        · rw [StateT.run_bind]
          exact get_set_other _ _ different
        · rfl
      · refine ⟨⟨?_, List.nodup_nil⟩, ?_, ?_⟩
        · intro bin
          simp [BinTable.get, BinTable.ofFn]
        · intro bin computed
          simp [BinTable.get, BinTable.ofFn] at computed
        · intro entry mem
          exact (law.empty entry mem).elim
      · intro bin _
        simp [BinTable.get, BinTable.ofFn]
    intro work h
    dsimp only
    intro bin mem
    have recorded := drain_preserves_recorded id hop queue input (2 * (n / 2 - 1)) h.1
    have rooted := (drain_rooted_ready law input hop (2 * (n / 2 - 1)) h.2.1 h.1 h.2.2).1
    obtain ⟨rootComputed, rootSelf, delta⟩ := rooted bin ((recorded.1 bin).2 (List.mem_reverse.mp mem))
    refine ⟨List.mem_reverse.mpr ((recorded.1 _).1 rootComputed), rootSelf, ?_⟩
    intro nonself
    obtain ⟨offset, before, between, after, order⟩ := delta nonself
    refine ⟨offset, after.reverse, between.reverse, before.reverse, ?_⟩
    rw [order]
    simp [List.reverse_append, List.append_assoc, finalWork]

/-- Each emitted bin refers directly to an emitted self root. The link delta
of a nonself bin is its input phase minus its root's input phase. -/
theorem analyze_roots {S : Type} (hop : ℝ) (thresholds : ℝ → ℝ × ℝ)
    (queue : Queue ℝ Q n) (law : QueueMembership queue)
    (nextPhase : StateM S ℝ) (gates : BinTable GateState n) (input : PVDRInput ℝ ℝ n)
    (state : S) :
    let graph := ((analyze id hop thresholds queue nextPhase gates input).run state).1.2
    ∀ bin ∈ graph.order,
      let link := graph.link.get bin
      link.root ∈ graph.order ∧
        (graph.link.get link.root).root = link.root ∧
        (link.root ≠ bin → link.delta = input.phase.get bin - input.phase.get link.root) := by
  dsimp only
  intro bin mem
  obtain ⟨present, self, delta⟩ :=
    analyze_roots_with_order hop thresholds queue law nextPhase gates input state bin mem
  exact ⟨present, self, fun different => (delta different).1⟩

/-- A nonself root appears before its dependent bin in the emitted order. -/
theorem analyze_root_precedes_bin {S : Type} (hop : ℝ) (thresholds : ℝ → ℝ × ℝ)
    (queue : Queue ℝ Q n) (law : QueueMembership queue)
    (nextPhase : StateM S ℝ) (gates : BinTable GateState n) (input : PVDRInput ℝ ℝ n)
    (state : S) :
    let graph := ((analyze id hop thresholds queue nextPhase gates input).run state).1.2
    ∀ bin ∈ graph.order,
      let root := (graph.link.get bin).root
      root ≠ bin →
        ∃ before between after, graph.order = before ++ root :: between ++ bin :: after := by
  dsimp only
  intro bin mem different
  exact ((analyze_roots_with_order hop thresholds queue law nextPhase gates input state bin mem).2.2
    different).2

end DSP.Resynthesis.PVDR

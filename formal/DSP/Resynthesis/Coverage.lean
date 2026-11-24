import DSP.Resynthesis.PVDRProofs
import Mathlib.Data.Fintype.Card

namespace DSP.Resynthesis.PVDR

variable {R P Q : Type} {n : ℕ}

/-- Account for entries independently of their order in the priority queue. -/
structure QueueAccounting (queue : Queue R Q n) where
  contents : Q → List (Entry R n)
  empty : contents queue.empty = []
  push : ∀ state entry, (contents (queue.push state entry)).Perm (entry :: contents state)
  popNone : ∀ state rest, queue.pop state = (none, rest) → contents state = []
  popSome : ∀ state entry rest, queue.pop state = (some entry, rest) →
    (contents state).Perm (entry :: contents rest)

private def Covered {queue : Queue R Q n} (law : QueueAccounting queue)
    (work : Work P Q n) (bin : ActiveBin n) : Prop :=
  work.computed.get bin = true ∨
    ∃ entry ∈ law.contents work.queue, entry.bin = bin ∧ entry.previous = true

private def Balances {queue : Queue R Q n} (law : QueueAccounting queue)
    (before after : Work P Q n) : Prop :=
  (∀ bin, Covered law before bin → Covered law after bin) ∧
  (law.contents after.queue).length + before.reverseOrder.length =
    (law.contents before.queue).length + after.reverseOrder.length

private theorem balance_refl {queue : Queue R Q n} (law : QueueAccounting queue)
    (work : Work P Q n) : Balances law work work :=
  ⟨fun _ h => h, rfl⟩

private theorem balance_trans {queue : Queue R Q n} {law : QueueAccounting queue}
    {first second third : Work P Q n}
    (h : Balances law first second) (g : Balances law second third) :
    Balances law first third := by
  refine ⟨fun bin hBin => g.1 bin (h.1 bin hBin), ?_⟩
  have hLength := h.2
  have gLength := g.2
  omega

private theorem computed_after_set (table : BinTable Bool n) (bin other : ActiveBin n)
    (h : other = bin ∨ table.get other = true) : (table.set bin true).get other = true := by
  by_cases same : other = bin
  · subst other
    simp [BinTable.set, BinTable.get]
  · have different : bin.val ≠ other.val := fun eq => same (Fin.ext eq.symm)
    simpa [BinTable.set, BinTable.get, different] using h.resolve_left same

private theorem balance_record_push {queue : Queue R Q n}
    (law : QueueAccounting queue) (input : PVDRInput R P n) (work : Work P Q n)
    (bin : ActiveBin n) (link : PhaseLink P n) :
    Balances law work (pushCurrent queue input (record work bin link) bin) := by
  have pushed := law.push work.queue (makeEntry input bin false)
  constructor
  · intro other covered
    rcases covered with computed | ⟨entry, mem, same, previous⟩
    · exact Or.inl (computed_after_set work.computed bin other (Or.inr computed))
    · exact Or.inr ⟨entry, pushed.mem_iff.mpr (List.mem_cons_of_mem _ mem), same, previous⟩
  · change (law.contents (queue.push work.queue (makeEntry input bin false))).length +
      work.reverseOrder.length = (law.contents work.queue).length + (bin :: work.reverseOrder).length
    have length := pushed.length_eq
    simp only [List.length_cons] at length ⊢
    omega

private theorem balance_attach [Sub R] [Add P] {queue : Queue R Q n}
    (law : QueueAccounting queue) (widen : R → P) (input : PVDRInput R P n)
    (parent : ActiveBin n) (neighbor : Option (ActiveBin n)) (work : Work P Q n) :
    Balances law work (attach widen queue input parent neighbor work) := by
  cases neighbor with
  | none => exact balance_refl law work
  | some bin =>
    cases flag : work.computed.get bin with
    | true => simpa [attach, flag] using balance_refl law work
    | false =>
      simp only [attach, flag, Bool.false_eq_true, if_false]
      exact balance_record_push law input work bin _

private theorem balance_visit [Sub R] [Add P] [Mul P] {queue : Queue R Q n}
    (law : QueueAccounting queue) (widen : R → P) (hop : P) (input : PVDRInput R P n)
    (entry : Entry R n) (work : Work P Q n) :
    Balances law work (visit widen hop queue input entry work) := by
  cases previous : entry.previous with
  | true =>
    cases flag : work.computed.get entry.bin with
    | true => simpa [visit, previous, flag] using balance_refl law work
    | false =>
      simp only [visit, previous, flag, if_true, Bool.false_eq_true, if_false]
      exact balance_record_push law input work entry.bin _
  | false =>
    simp only [visit, previous, Bool.false_eq_true, if_false]
    exact balance_trans (balance_attach law widen input entry.bin entry.bin.above work)
      (balance_attach law widen input entry.bin entry.bin.below _)

private theorem visit_previous_computes [Sub R] [Add P] [Mul P] (widen : R → P) (hop : P)
    (queue : Queue R Q n) (input : PVDRInput R P n) (entry : Entry R n)
    (work : Work P Q n) (previous : entry.previous = true) :
    (visit widen hop queue input entry work).computed.get entry.bin = true := by
  cases flag : work.computed.get entry.bin with
  | true => simp [visit, previous, flag]
  | false =>
    simp only [visit, previous, flag, if_true, Bool.false_eq_true, if_false]
    exact computed_after_set work.computed entry.bin entry.bin (Or.inl rfl)

private theorem visit_after_pop_covered [Sub R] [Add P] [Mul P] {queue : Queue R Q n}
    (law : QueueAccounting queue) (widen : R → P) (hop : P) (input : PVDRInput R P n)
    (work : Work P Q n) (popped : Entry R n) (rest : Q)
    (pop : queue.pop work.queue = (some popped, rest)) (bin : ActiveBin n)
    (h : Covered law work bin) :
    Covered law (visit widen hop queue input popped { work with queue := rest }) bin := by
  have preserve := (balance_visit law widen hop input popped { work with queue := rest }).1 bin
  rcases h with computed | ⟨entry, mem, same, previous⟩
  · exact preserve (Or.inl computed)
  · rcases List.mem_cons.mp ((law.popSome _ _ _ pop).mem_iff.mp mem) with isPopped | remains
    · subst entry bin
      exact Or.inl (visit_previous_computes widen hop queue input popped _ previous)
    · exact preserve (Or.inr ⟨entry, remains, same, previous⟩)

/-- Each visit adds as many queue entries as recorded bins. A successful pop
therefore spends one unit of fuel while preserving the budget for all remaining bins. -/
private theorem drain_computes_all [Sub R] [Add P] [Mul P] {queue : Queue R Q n}
    (law : QueueAccounting queue) (widen : R → P) (hop : P) (input : PVDRInput R P n)
    (fuel : ℕ) (work : Work P Q n) (recorded : work.Recorded)
    (covered : ∀ bin, Covered law work bin)
    (budget : (law.contents work.queue).length + (n / 2 - 1) ≤ fuel + work.reverseOrder.length) :
    ∀ bin, (drain widen hop queue input fuel work).computed.get bin = true := by
  induction fuel generalizing work with
  | zero =>
    have length : work.reverseOrder.length ≤ n / 2 - 1 := by
      simpa using recorded.2.length_le_card
    have empty : law.contents work.queue = [] := List.length_eq_zero_iff.mp (by omega)
    intro bin
    simpa [Covered, empty, drain, foldUntil] using covered bin
  | succ fuel ih =>
    rw [drain_succ]
    cases popped : queue.pop work.queue with
    | mk entry rest =>
      cases entry with
      | none =>
        intro bin
        simpa [Covered, law.popNone _ _ popped] using covered bin
      | some entry =>
        have balance := (balance_visit law widen hop input entry { work with queue := rest }).2
        have popLength := (law.popSome _ _ _ popped).length_eq
        simp only [List.length_cons] at popLength
        refine ih _ (visit_preserves_recorded widen hop queue input entry recorded)
          (fun bin => visit_after_pop_covered law widen hop input work entry rest popped bin (covered bin)) ?_
        simp only at balance
        omega

private theorem fold_initialize {S : Type} {queue : Queue R Q n} (law : QueueAccounting queue)
    (step : Work P Q n → ActiveBin n → StateM S (Work P Q n))
    (stepFacts : ∀ work bin state, work.Recorded → work.computed.get bin = false →
      let after := ((step work bin).run state).1
      after.Recorded ∧
      (∀ other, other = bin ∨ Covered law work other → Covered law after other) ∧
      (law.contents after.queue).length ≤ (law.contents work.queue).length + 1 ∧
      (∀ other, other ≠ bin → after.computed.get other = work.computed.get other))
    (bins : List (ActiveBin n)) (unique : bins.Nodup) (work : Work P Q n) (state : S)
    (recorded : work.Recorded) (fresh : ∀ bin ∈ bins, work.computed.get bin = false) :
    let after := ((bins.foldlM step work).run state).1
    after.Recorded ∧
      (∀ other, other ∈ bins ∨ Covered law work other → Covered law after other) ∧
      (law.contents after.queue).length ≤ (law.contents work.queue).length + bins.length := by
  induction bins generalizing work state with
  | nil => exact ⟨recorded, fun _ h => h.resolve_left List.not_mem_nil, Nat.le_refl _⟩
  | cons bin bins ih =>
    dsimp only
    rw [List.foldlM_cons, StateT.run_bind]
    obtain ⟨absent, tailUnique⟩ := List.nodup_cons.mp unique
    obtain ⟨stepRecorded, stepCovered, stepLength, stepFresh⟩ :=
      stepFacts work bin state recorded (fresh bin (by simp))
    have tailFresh other (mem : other ∈ bins) :
        ((step work bin).run state).1.computed.get other = false := by
      have different : other ≠ bin := fun eq => absent (eq ▸ mem)
      rw [stepFresh other different]
      exact fresh other (List.mem_cons_of_mem bin mem)
    obtain ⟨tailRecorded, tailCovered, tailLength⟩ :=
      ih tailUnique _ _ stepRecorded tailFresh
    refine ⟨tailRecorded, ?_, ?_⟩
    · intro other h
      apply tailCovered
      rcases h with mem | prev
      · rcases List.mem_cons.mp mem with same | tailMem
        · exact Or.inr (stepCovered other (Or.inl same))
        · exact Or.inl tailMem
      · exact Or.inr (stepCovered other (Or.inr prev))
    · change (law.contents (((bins.foldlM step ((step work bin).run state).1).run
          ((step work bin).run state).2).1.queue)).length ≤ _
      simp only [List.length_cons]
      omega

/-- The fuel used by the analysis is sufficient to emit every active bin
when the queue accounts for every pushed and popped entry. -/
theorem analyze_order_complete {S : Type} [OfNat R 0] [LT R] [LE R] [DecidableLT R]
    [DecidableLE R] [Max R] [Sub R] [OfNat P 0] [Add P] [Mul P]
    (widen : R → P) (hop : P) (thresholds : R → R × R) (queue : Queue R Q n)
    (law : QueueAccounting queue) (nextPhase : StateM S R) (gates : BinTable GateState n)
    (input : PVDRInput R P n) (state : S) :
    ∀ bin, bin ∈ ((analyze widen hop thresholds queue nextPhase gates input).run state).1.2.order := by
  unfold analyze
  cases thresholds (referenceMagnitude input) with
  | mk low trigger =>
    rw [StateT.run_bind]
    intro bin
    let step (work : Work P Q n) (bin : ActiveBin n) : StateM S (Work P Q n) := do
      let gate := updateGate low trigger (input.magnitude.get bin) (work.gates.get bin)
      let work := { work with gates := work.gates.set bin gate }
      if gate == .low then
        let delta ← nextPhase
        pure (record work bin ⟨bin, widen delta, true⟩)
      else
        pure { work with queue := queue.push work.queue (makeEntry input bin true) }
    let initial : Work P Q n :=
      ⟨gates, BinTable.ofFn (fun _ => false), BinTable.ofFn (fun bin => ⟨bin, 0, true⟩),
        [], queue.empty⟩
    let work := (((List.finRange (n / 2 - 1)).foldlM step initial).run state).1
    change bin ∈ (drain widen hop queue input (2 * (n / 2 - 1)) work).reverseOrder.reverse
    have facts := fold_initialize law step (by
      intro work bin state recorded fresh
      simp only [step]
      split
      · rw [StateT.run_bind]
        refine ⟨record_preserves_recorded _ recorded fresh, ?_, Nat.le_succ _, ?_⟩
        · intro other h
          rcases h with eq | computed | ⟨entry, mem, same, previous⟩
          · exact Or.inl (computed_after_set work.computed bin other (Or.inl eq))
          · exact Or.inl (computed_after_set work.computed bin other (Or.inr computed))
          · exact Or.inr ⟨entry, mem, same, previous⟩
        · intro other different
          have indices : bin.val ≠ other.val := fun eq => different (Fin.ext eq.symm)
          change (work.computed.set bin true).get other = work.computed.get other
          simp [BinTable.get, BinTable.set, indices]
      · have pushed := law.push work.queue (makeEntry input bin true)
        refine ⟨recorded, ?_, ?_, fun _ _ => rfl⟩
        · intro other h
          rcases h with eq | computed | ⟨entry, mem, same, previous⟩
          · subst other
            exact Or.inr ⟨_, pushed.mem_iff.mpr List.mem_cons_self, rfl, rfl⟩
          · exact Or.inl computed
          · exact Or.inr ⟨entry, pushed.mem_iff.mpr (List.mem_cons_of_mem _ mem), same, previous⟩
        · exact pushed.length_eq.le) (List.finRange (n / 2 - 1)) (List.nodup_finRange _)
      initial state
      ⟨fun bin => by simp [initial, BinTable.get, BinTable.ofFn], List.nodup_nil⟩
      (fun bin _ => by simp [initial, BinTable.get, BinTable.ofFn])
    have recorded : work.Recorded := facts.1
    have covered : ∀ bin, Covered law work bin := fun bin =>
      facts.2.1 bin (Or.inl (List.mem_finRange bin))
    have length : (law.contents work.queue).length ≤ n / 2 - 1 := by
      simpa [initial, law.empty] using facts.2.2
    have budget : (law.contents work.queue).length + (n / 2 - 1) ≤
        2 * (n / 2 - 1) + work.reverseOrder.length := by omega
    have finalComputed := drain_computes_all law widen hop input _ work recorded covered budget bin
    have finalRecorded := drain_preserves_recorded widen hop queue input (2 * (n / 2 - 1)) recorded
    exact List.mem_reverse.mpr ((finalRecorded.1 bin).1 finalComputed)

/-- The emitted order is a permutation of the active bins. -/
theorem analyze_order_perm_finRange {S : Type} [OfNat R 0] [LT R] [LE R] [DecidableLT R]
    [DecidableLE R] [Max R] [Sub R] [OfNat P 0] [Add P] [Mul P]
    (widen : R → P) (hop : P) (thresholds : R → R × R) (queue : Queue R Q n)
    (law : QueueAccounting queue) (nextPhase : StateM S R) (gates : BinTable GateState n)
    (input : PVDRInput R P n) (state : S) :
    (((analyze widen hop thresholds queue nextPhase gates input).run state).1.2.order).Perm
      (List.finRange (n / 2 - 1)) := by
  apply (List.perm_ext_iff_of_nodup
    (analyze_order_nodup widen hop thresholds queue nextPhase gates input state)
    (List.nodup_finRange _)).mpr
  intro bin
  exact ⟨fun _ => List.mem_finRange bin,
    fun _ => analyze_order_complete widen hop thresholds queue law nextPhase gates input state bin⟩

end DSP.Resynthesis.PVDR

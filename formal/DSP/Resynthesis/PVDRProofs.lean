import DSP.Resynthesis.PVDR

namespace DSP.Resynthesis.PVDR

variable {R P Q : Type} {n : ℕ}

/-- The computed flags agree with the recorded bins, which occur at most once. -/
def Work.Recorded (work : Work P Q n) : Prop :=
  (∀ bin, work.computed.get bin = true ↔ bin ∈ work.reverseOrder) ∧
    work.reverseOrder.Nodup

theorem record_preserves_recorded {work : Work P Q n} {bin : ActiveBin n}
    (link : PhaseLink P n) (h : work.Recorded) (fresh : work.computed.get bin = false) :
    (record work bin link).Recorded := by
  constructor
  · intro other
    by_cases same : other = bin
    · subst other
      simp [record, BinTable.get, BinTable.set]
    · have differentIndices : bin.val ≠ other.val := fun eq => same (Fin.ext eq.symm)
      simpa [record, BinTable.get, BinTable.set, same, Ne.symm same, differentIndices,
        Ne.symm differentIndices] using h.1 other
  · apply List.nodup_cons.mpr
    exact ⟨fun mem => by simpa [fresh] using (h.1 bin).2 mem, h.2⟩

theorem pushCurrent_preserves_recorded (queue : Queue R Q n) (input : PVDRInput R P n)
    (bin : ActiveBin n) {work : Work P Q n} (h : work.Recorded) :
    (pushCurrent queue input work bin).Recorded := h

theorem attach_preserves_recorded [Sub R] [Add P] (widen : R → P)
    (queue : Queue R Q n) (input : PVDRInput R P n) (parent : ActiveBin n)
    (neighbor : Option (ActiveBin n)) {work : Work P Q n} (h : work.Recorded) :
    (attach widen queue input parent neighbor work).Recorded := by
  cases neighbor with
  | none => exact h
  | some bin =>
    cases flag : work.computed.get bin with
    | true => simpa [attach, flag] using h
    | false =>
      simp only [attach, flag, Bool.false_eq_true, if_false]
      exact pushCurrent_preserves_recorded queue input _ (record_preserves_recorded _ h flag)

theorem visit_preserves_recorded [Sub R] [Add P] [Mul P] (widen : R → P)
    (hop : P) (queue : Queue R Q n) (input : PVDRInput R P n)
    (entry : Entry R n) {work : Work P Q n} (h : work.Recorded) :
    (visit widen hop queue input entry work).Recorded := by
  cases previous : entry.previous with
  | true =>
    cases flag : work.computed.get entry.bin with
    | true => simpa [visit, previous, flag] using h
    | false =>
      simp only [visit, previous, flag, if_true, Bool.false_eq_true, if_false]
      exact pushCurrent_preserves_recorded queue input _ (record_preserves_recorded _ h flag)
  | false =>
    simp only [visit, previous, Bool.false_eq_true, if_false]
    exact attach_preserves_recorded widen queue input entry.bin entry.bin.below
      (attach_preserves_recorded widen queue input entry.bin entry.bin.above h)

theorem drain_preserves_recorded [Sub R] [Add P] [Mul P] (widen : R → P)
    (hop : P) (queue : Queue R Q n) (input : PVDRInput R P n)
    (fuel : ℕ) {work : Work P Q n} (h : work.Recorded) :
    (drain widen hop queue input fuel work).Recorded := by
  induction fuel generalizing work with
  | zero => exact h
  | succ fuel ih =>
    rw [drain_succ]
    cases popped : queue.pop work.queue with
    | mk entry rest =>
      cases entry with
      | none => exact h
      | some entry =>
        exact ih (visit_preserves_recorded widen hop queue input entry h)

private theorem foldlM_recorded {S : Type}
    (step : Work P Q n → ActiveBin n → StateM S (Work P Q n))
    (stepRecorded : ∀ work bin state, work.Recorded → work.computed.get bin = false →
      ((step work bin).run state).1.Recorded)
    (stepUnchanged : ∀ work bin other state, other ≠ bin →
      ((step work bin).run state).1.computed.get other = work.computed.get other)
    (bins : List (ActiveBin n)) (unique : bins.Nodup) (work : Work P Q n)
    (state : S) (h : work.Recorded) (fresh : ∀ bin ∈ bins, work.computed.get bin = false) :
    ((bins.foldlM step work).run state).1.Recorded := by
  induction bins generalizing work state with
  | nil => exact h
  | cons bin bins ih =>
    rw [List.foldlM_cons, StateT.run_bind]
    obtain ⟨absent, tailUnique⟩ := List.nodup_cons.mp unique
    refine ih tailUnique _ _ (stepRecorded work bin state h (fresh bin (by simp))) ?_
    intro other mem
    have different : other ≠ bin := fun eq => absent (eq ▸ mem)
    rw [stepUnchanged work bin other state different]
    exact fresh other (List.mem_cons_of_mem bin mem)

/-- Every bin occurs at most once in the order returned by the analysis. -/
theorem analyze_order_nodup {S : Type} [OfNat R 0] [LT R] [LE R] [DecidableLT R]
    [DecidableLE R] [Max R] [Sub R] [OfNat P 0] [Add P] [Mul P]
    (widen : R → P) (hop : P) (thresholds : R → R × R) (queue : Queue R Q n)
    (nextPhase : StateM S R) (gates : BinTable GateState n) (input : PVDRInput R P n)
    (state : S) :
    (((analyze widen hop thresholds queue nextPhase gates input).run state).1.2.order).Nodup := by
  unfold analyze
  cases thresholds (referenceMagnitude input) with
  | mk low trigger =>
    rw [StateT.run_bind]
    change (drain widen hop queue input _ _).reverseOrder.reverse.Nodup
    rw [List.nodup_reverse]
    apply (drain_preserves_recorded widen hop queue input _ ?_).2
    refine foldlM_recorded _ ?_ ?_ _ (List.nodup_finRange _) _ _ ?_ ?_
    · intro work bin state h fresh
      simp only
      split
      · rw [StateT.run_bind]
        exact record_preserves_recorded _ h fresh
      · exact h
    · intro work bin other state different
      simp only
      split
      · rw [StateT.run_bind]
        have differentIndices : bin.val ≠ other.val :=
          fun eq => different (Fin.ext eq.symm)
        change (record _ bin _).computed.get other = work.computed.get other
        simp [record, BinTable.get, BinTable.set, differentIndices]
      · rfl
    · constructor
      · intro bin
        simp [BinTable.get, BinTable.ofFn]
      · exact List.nodup_nil
    · intro bin _
      simp [BinTable.get, BinTable.ofFn]

end DSP.Resynthesis.PVDR

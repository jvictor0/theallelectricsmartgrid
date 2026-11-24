import PhaseTime.InverseRuntimeReachability

namespace PhaseTime.InverseRuntime

noncomputable section

attribute [local instance] exactArithmetic
attribute [local simp] get_set_same get_set_other

private def stateAt1 : State ℝ ℝ 1 :=
  write (write {} 1 ⟨0, 0⟩) 1 ⟨1, 1⟩

private def stateAt3 : State ℝ ℝ 1 :=
  write (write stateAt1 1 ⟨2, 2⟩) 1 ⟨3, 3⟩

private theorem le_simp (a b : ℝ) : exactArithmetic.le a b = decide (a ≤ b) := rfl
private theorem lt_simp (a b : ℝ) : exactArithmetic.lt a b = decide (a < b) := rfl
private theorem floor_simp (a : ℝ) : exactArithmetic.floor a = ⌊a⌋ := rfl
private theorem ceil_simp (a : ℝ) : exactArithmetic.ceil a = ⌈a⌉ := rfl
private theorem idx_simp (a : Int) : exactArithmetic.ofIndex a = (a : ℝ) := rfl

private theorem toNat_two : (2 : Int).toNat = 2 := rfl
private theorem wrap_one_two : wrap 1 2 = wrap 1 0 := by decide
private theorem wrap_one_three : wrap 1 3 = wrap 1 1 := by decide
private theorem captured_ring_selects_wrong_cell :
    selectInterval (stateAt3.payload.cells.get (wrap 1 0)) stateAt3.payload.seed
      stateAt3.payload.frontier 0 (1/2) = some ⟨⟨0, 2⟩, 1, some 3⟩ := by
  have scatter_two (s : Published ℝ ℝ 1) (left right : Point ℝ) (i : Int) :
      scatter s left right i 2 = commitBatch s left right i 1 := by
    rw [scatter]
    norm_num [batchCapacity]
    rw [scatter]
    simp
  norm_num [stateAt3, stateAt1, write, beginBranch, retained,
    scatter_two, commitBatch, commitTail, batchCapacity, assignCell, finishCell, edgeAt, interpolate, interpolateUsing,
    selectInterval, retireSeed, toNat_two, wrap_one_two, wrap_one_three, wrap,
    le_simp, lt_simp, floor_simp, ceil_simp, idx_simp]
  change (do
    let _ ← (some (2 : ℝ))
    some (⟨⟨0, 2⟩, 1, some 3⟩ : Interval ℝ)) = _
  rfl

private theorem captured_ring_rejects_stale_read :
    read stateAt3.payload (1/2) 1 = none := by
  have index : Arithmetic.floor (1/2 : ℝ) = 0 := by norm_num [floor_simp]
  simp only [read, index]
  have frontier : stateAt3.payload.frontier = some ⟨3, 3⟩ := by
    norm_num [stateAt3, stateAt1, write, beginBranch, floor_simp, idx_simp, lt_simp]
  have selected := captured_ring_selects_wrong_cell
  rw [frontier] at selected ⊢
  simp only [readCell]
  have notFrontier : ((1/2 : ℝ) == 3) = false := by
    change decide ((1/2 : ℝ) = 3) = false
    norm_num
  rw [notFrontier]
  simp only [Bool.false_eq_true, ↓reduceIte, selected, Option.bind_some]
  norm_num [readInterval, readIntervalWith, entryInRange, lt_simp]

private theorem sampled_step {s : State ℝ ℝ 1} {last : ℝ}
    (history : SampledWrites id 1 1 s)
    (latest : s.previous = some ⟨last, last⟩) (next : ℝ) (ordered : last < next) :
    SampledWrites id 1 1 (write s 1 ⟨next, next⟩) := by
  apply SampledWrites.observe history _ (by simp)
  intro previous member
  rw [latest, Option.mem_some_iff] at member
  subst previous
  exact ⟨ordered, by simp⟩

/-- A query admitted by an older view can select an aliased cell in a newer
payload after ring reuse. The captured cutoff rejects this particular read. -/
theorem captured_view_can_select_uncertified :
    ∃ old current : State ℝ ℝ 1, SampledWrites id 1 1 old ∧ SampledWrites id 1 1 current ∧
      old.previous = some ⟨1, 1⟩ ∧ RetainedQuery old 1 (1/2) ∧
      current = write (write old 1 ⟨2, 2⟩) 1 ⟨3, 3⟩ ∧
      read current.payload (1/2) 1 = none ∧
      ∃ interval, selectInterval (current.payload.cells.get (wrap 1 0)) current.payload.seed
          current.payload.frontier 0 (1/2) = some interval ∧
        ¬ FixedCell.Certified id 0 1 3 interval.piece := by
  have initial : SampledWrites id 1 1 (write {} 1 ⟨0, 0⟩) :=
    SampledWrites.observe .empty _ (by simp) (by simp)
  have oldSampled : SampledWrites id 1 1 stateAt1 :=
    sampled_step initial (write_previous _ _ _) 1 (by norm_num)
  have currentSampled : SampledWrites id 1 1 stateAt3 :=
    sampled_step (sampled_step oldSampled (write_previous _ _ _) 2 (by norm_num))
      (write_previous _ _ _) 3 (by norm_num)
  have oldLatest : stateAt1.previous = some ⟨1, 1⟩ := write_previous _ _ _
  refine ⟨stateAt1, stateAt3, oldSampled, currentSampled, oldLatest, ?_, rfl,
    captured_ring_rejects_stale_read, _, captured_ring_selects_wrong_cell, ?_⟩
  · refine ⟨1, ⟨1, 1⟩, ?_, oldLatest, ?_⟩
    · norm_num [stateAt1, write, beginBranch, floor_simp, idx_simp, lt_simp]
    · norm_num
  · intro certificate
    have exactLeft := certificate.leftExact
    norm_num [Interval.piece, Point.exact] at exactLeft

end
end PhaseTime.InverseRuntime

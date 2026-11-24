import PhaseTime.InverseRuntimeProofs
import PhaseTime.InverseRuntimeStorage

namespace PhaseTime.InverseRuntime

noncomputable section

attribute [local instance] exactArithmetic

/-- The forward trajectory is affine between consecutive samples in grid coordinates. -/
def SampleSegment (F : ℝ → ℝ) (resolution : ℝ) (left right : Point ℝ) : Prop :=
  left.time < right.time ∧
    ∀ alpha : ℝ, 0 ≤ alpha → alpha ≤ 1 →
      F (left.time + alpha * (right.time - left.time)) =
        left.phase * resolution + alpha * (right.phase * resolution - left.phase * resolution)

/-- States reached by completed writes of time-ordered samples of the trajectory. -/
inductive SampledWrites (F : ℝ → ℝ) (resolution : ℝ) (n : Nat) : State ℝ ℝ n → Prop where
  | empty : SampledWrites F resolution n {}
  | observe {s : State ℝ ℝ n} (history : SampledWrites F resolution n s)
      (observation : Point ℝ) (onTrajectory : F observation.time = observation.phase * resolution)
      (segment : ∀ previous ∈ s.previous, SampleSegment F resolution previous observation) :
      SampledWrites F resolution n (write s resolution observation)

/-- In grid coordinates, a current view excludes cells outside its retention window
and coordinates above the newest observed phase. -/
def RetainedQuery {n : Nat} (s : State ℝ ℝ n) (resolution query : ℝ) : Prop :=
  ∃ (highest : Int) (previous : Point ℝ), s.highest = some highest ∧
    s.previous = some previous ∧ highest - (n : Int) ≤ ⌊query⌋ ∧
    ⌊query⌋ ≤ highest ∧ query ≤ previous.phase * resolution

/-- Certification of whatever interval the runtime selects at a retained query. -/
def RetainedCellsCertified {n : Nat} (F : ℝ → ℝ) (resolution : ℝ)
    (s : State ℝ ℝ n) : Prop :=
  ∀ previous ∈ s.previous, ∀ query, RetainedQuery s resolution query →
    ∀ interval, selectInterval (s.payload.cells.get (wrap n ⌊query⌋)) s.payload.seed
      s.payload.frontier ⌊query⌋ query = some interval →
      FixedCell.Certified F (⌊query⌋ : Int) ((⌊query⌋ : Int) + (1 : ℝ))
        previous.time interval.piece

/-- The reachable-read contract to establish for the exact-arithmetic writer.
The cutoff applies to the returned approximation. The exact preimage belongs
to the history through the newest observation. -/
def ReachableReadSoundness : Prop :=
  ∀ {n : Nat} {F : ℝ → ℝ} {resolution : ℝ} {s : State ℝ ℝ n},
    0 < resolution → Continuous F → SampledWrites F resolution n s →
    ∀ {previous : Point ℝ} {query cutoff time : ℝ},
      s.previous = some previous → RetainedQuery s resolution query →
      read s.payload query cutoff = some time →
      time ≤ previous.time ∧ time ≤ cutoff ∧ |F time - query| ≤ 1 ∧
        ∃ exactTime ≤ previous.time, F exactTime = query

private theorem beginBranch_previous {n : Nat} (s : State ℝ ℝ n) (edge : Point ℝ) :
    (beginBranch s edge).previous = s.previous := by
  unfold beginBranch
  simp only [Id.run, pure]
  repeat first | rfl | split

theorem write_previous {n : Nat} (s : State ℝ ℝ n) (resolution : ℝ)
    (observation : Point ℝ) :
    (write s resolution observation).previous = some observation := by
  unfold write
  split
  · rfl
  · split
    · simp only
      split
      · apply beginBranch_previous
      · rfl
    · split <;> rfl

theorem SampledWrites.highest_covers_previous {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    {s : State ℝ ℝ n} (history : SampledWrites F resolution n s)
    (resolutionPositive : 0 < resolution) {previous : Point ℝ} {highest : Int}
    (latest : previous ∈ s.previous) (upper : highest ∈ s.highest) :
    ⌊previous.phase * resolution⌋ ≤ highest := by
  induction history generalizing previous highest with
  | empty => simp at upper
  | @observe s history observation _ _ ih =>
    rw [write_previous, Option.mem_some_iff] at latest
    subst previous
    have exactLT (a b : ℝ) : Arithmetic.lt a b = decide (a < b) := rfl
    have exactFloor (a : ℝ) : Arithmetic.floor a = ⌊a⌋ := rfl
    cases prior : s.previous with
    | none =>
      have noHigh : s.highest = none := by
        cases history with
        | empty => rfl
        | observe => simp [write_previous] at prior
      simp [write, prior, noHigh] at upper
    | some previous =>
      by_cases forward : previous.phase < observation.phase
      · simp [write, prior, exactLT, exactFloor, forward] at upper
        subst highest
        exact le_max_left _ _
      · have sameHigh : (write s resolution observation).highest = s.highest := by
          simp [write, prior, exactLT, forward]
          split <;> rfl
        rw [sameHigh] at upper
        exact (Int.floor_le_floor (mul_le_mul_of_nonneg_right (le_of_not_gt forward)
          resolutionPositive.le)).trans (ih (by simp [prior]) upper)

theorem SampledWrites.empty_inverse_without_highest {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    {s : State ℝ ℝ n} (history : SampledWrites F resolution n s) (absent : s.highest = none) :
    s.payload = {} ∧ s.hasForward = false := by
  induction history with
  | empty => exact ⟨rfl, rfl⟩
  | @observe s _ observation _ _ ih =>
    have exactLT (a b : ℝ) : Arithmetic.lt a b = decide (a < b) := rfl
    cases prior : s.previous with
    | none =>
      simp [write, prior] at absent ⊢
      exact ih absent
    | some previous =>
      by_cases forward : previous.phase < observation.phase
      · simp [write, prior, exactLT, forward] at absent
      · have sameHigh : (write s resolution observation).highest = s.highest := by
          simp [write, prior, exactLT, forward]
          split <;> rfl
        rw [sameHigh] at absent
        have sameHasForward : (write s resolution observation).hasForward = s.hasForward := by
          simp [write, prior, exactLT, forward]
          split <;> rfl
        rw [nonforward_keeps_payload s resolution previous observation prior (le_of_not_gt forward),
          sameHasForward]
        exact ih absent

theorem retainedCellsCertified_empty (n : Nat) (F : ℝ → ℝ) (resolution : ℝ) :
    RetainedCellsCertified F resolution ({} : State ℝ ℝ n) := by
  simp [RetainedCellsCertified]

theorem retainedCellsCertified_nonforward {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    {s : State ℝ ℝ n} (certified : RetainedCellsCertified F resolution s)
    (resolutionPositive : 0 < resolution) {previous observation : Point ℝ}
    (prior : s.previous = some previous) (timeOrder : previous.time ≤ observation.time)
    (nonforward : observation.phase ≤ previous.phase) :
    RetainedCellsCertified F resolution (write s resolution observation) := by
  have payload := nonforward_keeps_payload s resolution previous observation prior nonforward
  have exactLT (a b : ℝ) : Arithmetic.lt a b = decide (a < b) := rfl
  have highest : (write s resolution observation).highest = s.highest := by
    simp [write, prior, exactLT, not_lt.mpr nonforward]
    split <;> rfl
  intro newest latest query retained interval selected
  rw [write_previous, Option.mem_some_iff] at latest
  subst newest
  obtain ⟨high, newest, highEq, newestEq, lower, upper, cursor⟩ := retained
  rw [write_previous, Option.some.injEq] at newestEq
  subst newest
  rw [highest] at highEq
  have oldRetained : RetainedQuery s resolution query :=
    ⟨high, previous, highEq, prior, lower, upper, cursor.trans
      (mul_le_mul_of_nonneg_right nonforward resolutionPositive.le)⟩
  rw [payload] at selected
  have piece := certified previous (by simp [prior]) query oldRetained interval selected
  exact { piece with causal := piece.causal.trans timeOrder }

private theorem write_frontier {n : Nat} (s : State ℝ ℝ n) (resolution : ℝ)
    (observation : Point ℝ) :
    (write s resolution observation).payload.frontier =
      if s.previous.any (fun previous => Arithmetic.lt previous.phase observation.phase) then
        some { observation with phase := observation.phase * resolution }
      else s.payload.frontier := by
  unfold write
  cases s.previous with
  | none => simp
  | some previous =>
    simp only [Option.any]
    split
    · rfl
    · split <;> rfl

theorem SampledWrites.frontier_exact {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    {s : State ℝ ℝ n} (history : SampledWrites F resolution n s) :
    ∀ edge ∈ s.payload.frontier, F edge.time = edge.phase ∧
      ∃ previous ∈ s.previous, edge.time ≤ previous.time := by
  induction history with
  | empty => simp
  | observe _ observation onTrajectory segment ih =>
    intro edge live
    rw [write_previous, write_frontier] at *
    split at live
    · have atObservation := Option.mem_some_iff.mp live
      subst edge
      exact ⟨onTrajectory, observation, by simp, le_rfl⟩
    · obtain ⟨phase, previous, member, past⟩ := ih edge live
      exact ⟨phase, observation, by simp, past.trans (segment previous member).1.le⟩

theorem SampledWrites.previous_exact {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    {s : State ℝ ℝ n} (history : SampledWrites F resolution n s)
    {previous : Point ℝ} (latest : previous ∈ s.previous) :
    F previous.time = previous.phase * resolution := by
  cases history with
  | empty => simp at latest
  | observe _ observation onTrajectory _ =>
    rw [write_previous, Option.mem_some_iff] at latest
    simpa [latest] using onTrajectory

theorem SampleSegment.phase_at {F : ℝ → ℝ} {resolution : ℝ} {left right : Point ℝ}
    (segment : SampleSegment F resolution left right) {time : ℝ}
    (within : time ∈ Set.Icc left.time right.time) :
    F time = left.phase * resolution + ((time - left.time) / (right.time - left.time)) *
      (right.phase * resolution - left.phase * resolution) := by
  have width := sub_pos.mpr segment.1
  have cancel : left.time + ((time - left.time) / (right.time - left.time)) *
      (right.time - left.time) = time := by
    rw [div_mul_cancel₀ _ width.ne']
    exact add_sub_cancel _ _
  have lower := div_nonneg (sub_nonneg.mpr within.1) width.le
  have upper := (div_le_one width).2 (sub_le_sub_right within.2 _)
  simpa only [cancel] using segment.2 _ lower upper

theorem SampleSegment.monotoneOn {F : ℝ → ℝ} {resolution : ℝ} {left right : Point ℝ}
    (segment : SampleSegment F resolution left right) (resolutionNonnegative : 0 ≤ resolution)
    (phaseOrder : left.phase ≤ right.phase) :
    MonotoneOn F (Set.Icc left.time right.time) := by
  intro x hx y hy hxy
  rw [segment.phase_at hx, segment.phase_at hy]
  apply add_le_add_right
  exact mul_le_mul_of_nonneg_right
    (div_le_div_of_nonneg_right (sub_le_sub_right hxy _) (sub_nonneg.mpr segment.1.le))
    (sub_nonneg.mpr (mul_le_mul_of_nonneg_right phaseOrder resolutionNonnegative))

theorem SampleSegment.interpolate_exact {F : ℝ → ℝ} {resolution : ℝ}
    {left right : Point ℝ} (segment : SampleSegment F resolution left right)
    (resolutionPositive : 0 < resolution) (phaseOrder : left.phase < right.phase)
    {phase : ℝ} (lower : left.phase * resolution ≤ phase) (upper : phase ≤ right.phase * resolution) :
    let time := interpolate { left with phase := left.phase * resolution }
      { right with phase := right.phase * resolution } phase
    F time = phase ∧ left.time ≤ time ∧ time ≤ right.time := by
  let alpha := (phase - left.phase * resolution) / (right.phase * resolution - left.phase * resolution)
  have width := sub_pos.mpr (mul_lt_mul_of_pos_right phaseOrder resolutionPositive)
  have duration := sub_nonneg.mpr segment.1.le
  have lowerAlpha := div_nonneg (sub_nonneg.mpr lower) width.le
  have upperAlpha := (div_le_one width).2 (sub_le_sub_right upper _)
  change F (left.time + alpha * (right.time - left.time)) = phase ∧
    left.time ≤ left.time + alpha * (right.time - left.time) ∧
    left.time + alpha * (right.time - left.time) ≤ right.time
  refine ⟨?_, le_add_of_nonneg_right (mul_nonneg lowerAlpha duration), ?_⟩
  · rw [segment.2 _ lowerAlpha upperAlpha]
    rw [div_mul_cancel₀ _ width.ne']
    exact add_sub_cancel _ _
  · linarith [mul_le_mul_of_nonneg_right upperAlpha duration]

theorem SampleSegment.grid_interval_certified {F : ℝ → ℝ} {resolution : ℝ}
    {left right : Point ℝ} (segment : SampleSegment F resolution left right)
    (continuous : Continuous F) (resolutionPositive : 0 < resolution)
    (phaseOrder : left.phase < right.phase) {lo hi phase endPhase : ℝ}
    (lower : left.phase * resolution ≤ phase) (ordered : phase ≤ endPhase)
    (upper : endPhase ≤ right.phase * resolution) (lowerCell : lo ≤ phase) (upperCell : endPhase ≤ hi) :
    let leftGrid := { left with phase := left.phase * resolution }
    let rightGrid := { right with phase := right.phase * resolution }
    FixedCell.Certified F lo hi right.time
      ⟨⟨phase, interpolate leftGrid rightGrid phase⟩,
        ⟨endPhase, interpolate leftGrid rightGrid endPhase⟩⟩ := by
  obtain ⟨leftExact, leftStart, _⟩ :=
    segment.interpolate_exact resolutionPositive phaseOrder lower (ordered.trans upper)
  obtain ⟨rightExact, _, rightEnd⟩ :=
    segment.interpolate_exact resolutionPositive phaseOrder (lower.trans ordered) upper
  let leftGrid := { left with phase := left.phase * resolution }
  let rightGrid := { right with phase := right.phase * resolution }
  let start := interpolate leftGrid rightGrid phase
  let stop := interpolate leftGrid rightGrid endPhase
  have timeOrder : start ≤ stop := by
    change left.time + ((phase - leftGrid.phase) / (rightGrid.phase - leftGrid.phase)) *
      (right.time - left.time) ≤ left.time +
        ((endPhase - leftGrid.phase) / (rightGrid.phase - leftGrid.phase)) * (right.time - left.time)
    exact add_le_add_right (mul_le_mul_of_nonneg_right (div_le_div_of_nonneg_right
      (sub_le_sub_right ordered _) (sub_nonneg.mpr (mul_le_mul_of_nonneg_right
        phaseOrder.le resolutionPositive.le))) (sub_nonneg.mpr segment.1.le)) _
  refine ⟨ordered, timeOrder, rightEnd, lowerCell, upperCell, leftExact, rightExact,
    continuous.continuousOn, ?_⟩
  intro t atLeast atMost
  have mono := segment.monotoneOn resolutionPositive.le phaseOrder.le
  have startIn : start ∈ Set.Icc left.time right.time := ⟨leftStart, timeOrder.trans rightEnd⟩
  have stopIn : stop ∈ Set.Icc left.time right.time := ⟨leftStart.trans timeOrder, rightEnd⟩
  have tIn : t ∈ Set.Icc left.time right.time := ⟨leftStart.trans atLeast, atMost.trans rightEnd⟩
  exact ⟨lowerCell.trans (leftExact ▸ mono startIn tIn atLeast),
    (rightExact ▸ mono tIn stopIn atMost).trans upperCell⟩

theorem SampleSegment.antitoneOn {F : ℝ → ℝ} {resolution : ℝ} {left right : Point ℝ}
    (segment : SampleSegment F resolution left right) (resolutionNonnegative : 0 ≤ resolution)
    (phaseOrder : right.phase ≤ left.phase) :
    AntitoneOn F (Set.Icc left.time right.time) := by
  intro x hx y hy hxy
  rw [segment.phase_at hx, segment.phase_at hy]
  apply add_le_add_right
  exact mul_le_mul_of_nonpos_right
    (div_le_div_of_nonneg_right (sub_le_sub_right hxy _) (sub_nonneg.mpr segment.1.le))
    (sub_nonpos.mpr (mul_le_mul_of_nonneg_right phaseOrder resolutionNonnegative))

private theorem antitoneOn_Icc_glue (F : ℝ → ℝ) (a b c : ℝ)
    (ab : a ≤ b) (bc : b ≤ c)
    (before : AntitoneOn F (Set.Icc a b)) (after : AntitoneOn F (Set.Icc b c)) :
    AntitoneOn F (Set.Icc a c) := by
  intro x hx y hy hxy
  by_cases hyb : y ≤ b
  · exact before ⟨hx.1, hxy.trans hyb⟩ ⟨hy.1, hyb⟩ hxy
  by_cases hbx : b ≤ x
  · exact after ⟨hbx, hx.2⟩ ⟨hbx.trans hxy, hy.2⟩ hxy
  have hxb := (lt_of_not_ge hbx).le
  have hby := (lt_of_not_ge hyb).le
  exact (after ⟨le_rfl, bc⟩ ⟨hby, hy.2⟩ hby).trans
    (before ⟨hx.1, hxb⟩ ⟨ab, le_rfl⟩ hxb)

theorem SampledWrites.frontier_to_previous_antitone {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    {s : State ℝ ℝ n} (history : SampledWrites F resolution n s) (resolutionPositive : 0 < resolution) :
    ∀ previous ∈ s.previous, ∀ edge ∈ s.payload.frontier,
      AntitoneOn F (Set.Icc edge.time previous.time) := by
  induction history with
  | empty => simp
  | @observe s history observation _ segment ih =>
    intro ending latest edge live
    rw [write_previous, Option.mem_some_iff] at latest
    subst ending
    rw [write_frontier] at live
    cases prior : s.previous with
    | none =>
      simp only [prior, Option.any, Bool.false_eq_true, if_false] at live
      obtain ⟨_, previous, member, _⟩ := history.frontier_exact edge live
      simp [prior] at member
    | some previous =>
      have member : previous ∈ s.previous := by simp [prior]
      simp only [prior, Option.any] at live
      change edge ∈ (if decide (previous.phase < observation.phase) = true then
        some { observation with phase := observation.phase * resolution }
        else s.payload.frontier) at live
      by_cases forward : previous.phase < observation.phase
      · simp only [forward, decide_true, if_true, Option.mem_some_iff] at live
        subst edge
        intro x hx y hy _
        obtain rfl : x = observation.time := le_antisymm hx.2 hx.1
        obtain rfl : y = observation.time := le_antisymm hy.2 hy.1
        exact le_rfl
      · simp only [forward, decide_false, Bool.false_eq_true, if_false] at live
        obtain ⟨_, newest, newestMember, past⟩ := history.frontier_exact edge live
        rw [prior, Option.mem_some_iff] at newestMember
        subst newest
        have portion := segment previous member
        exact antitoneOn_Icc_glue F edge.time previous.time observation.time past portion.1.le
          (ih previous member edge live)
          (portion.antitoneOn resolutionPositive.le (le_of_not_gt forward))

theorem SampledWrites.previous_phase_le_frontier {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    {s : State ℝ ℝ n} (history : SampledWrites F resolution n s)
    (resolutionPositive : 0 < resolution) {previous edge : Point ℝ}
    (latest : previous ∈ s.previous) (live : edge ∈ s.payload.frontier) :
    previous.phase * resolution ≤ edge.phase := by
  obtain ⟨phase, other, member, past⟩ := history.frontier_exact edge live
  have same : other = previous := Option.mem_unique member latest
  subst other
  rw [← phase, ← history.previous_exact latest]
  exact history.frontier_to_previous_antitone resolutionPositive previous latest edge live
    ⟨le_rfl, past⟩ ⟨past, le_rfl⟩ past

/-- An old frontier and the next forward chord's origin delimit a descending bridge.
The lower bound at the origin and upper bound at the frontier suffice. -/
theorem SampledWrites.frontier_bridge_confined {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    {s : State ℝ ℝ n} (history : SampledWrites F resolution n s)
    (resolutionPositive : 0 < resolution) {previous frontier : Point ℝ}
    (latest : previous ∈ s.previous) (live : frontier ∈ s.payload.frontier)
    {lo hi : ℝ} (lower : lo ≤ F previous.time) (upper : F frontier.time ≤ hi) :
    frontier.time ≤ previous.time ∧
      AnalysisStability.Confined F frontier.time previous.time lo hi := by
  obtain ⟨_, observation, member, ordered⟩ := history.frontier_exact frontier live
  have same : observation = previous := Option.mem_unique member latest
  subst observation
  refine ⟨ordered, ?_⟩
  intro t after before
  have bridge := history.frontier_to_previous_antitone resolutionPositive previous latest frontier live
  exact ⟨lower.trans (bridge ⟨after, before⟩ ⟨ordered, le_rfl⟩ before),
    (bridge ⟨le_rfl, ordered⟩ ⟨after, before⟩ after).trans upper⟩

/-- Extend an active traversal through the descending bridge and the fresh forward chord. -/
theorem SampledWrites.restart_same_visit_certified {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    {s : State ℝ ℝ n} (history : SampledWrites F resolution n s)
    (resolutionPositive : 0 < resolution) (continuous : Continuous F)
    {previous frontier : Point ℝ} (latest : previous ∈ s.previous)
    (live : frontier ∈ s.payload.frontier) {lo hi now : ℝ}
    {old fresh : InverseGrid.Piece} (active : FixedCell.Certified F lo hi now old)
    (chord : FixedCell.Certified F lo hi now fresh)
    (oldStop : old.right.time = frontier.time)
    (freshStart : fresh.left.time = previous.time) :
    FixedCell.Certified F lo hi now (FixedCell.restart old fresh true) := by
  have lower : lo ≤ F previous.time := by rw [← freshStart, chord.leftExact]; exact chord.leftBound
  have upper : F frontier.time ≤ hi := by rw [← oldStop, active.rightExact]; exact active.rightBound
  obtain ⟨ordered, bridge⟩ := history.frontier_bridge_confined resolutionPositive latest live lower upper
  exact FixedCell.restart_certified F lo hi now old fresh active chord
    (by simpa only [oldStop, freshStart] using ordered) continuous.continuousOn
    (by simpa only [oldStop, freshStart] using bridge)

theorem read_frontier_sound {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    {s : State ℝ ℝ n} (history : SampledWrites F resolution n s)
    {previous edge : Point ℝ} (latest : s.previous = some previous)
    (frontier : s.payload.frontier = some edge) {cutoff time : ℝ}
    (found : read s.payload edge.phase cutoff = some time) :
    time ≤ previous.time ∧ time ≤ cutoff ∧ F time = edge.phase := by
  have live := history.frontier_exact edge (by simp [frontier])
  obtain ⟨phase, observation, member, past⟩ := live
  rw [latest, Option.mem_some_iff] at member
  subst observation
  have exactLE (a b : ℝ) : Arithmetic.le a b = decide (a ≤ b) := rfl
  by_cases within : edge.time ≤ cutoff
  · simp [read, readCell, frontier, timeWithinCutoff, exactLE, within] at found
    subst time
    exact ⟨past, within, phase⟩
  · simp [read, readCell, frontier, timeWithinCutoff, exactLE, within] at found

theorem read_sound_of_selected_piece {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    {s : State ℝ ℝ n} (history : SampledWrites F resolution n s)
    {previous : Point ℝ} (latest : s.previous = some previous)
    {query cutoff time : ℝ}
    (found : read s.payload query cutoff = some time)
    (certify : ∀ interval,
      selectInterval (s.payload.cells.get (wrap n ⌊query⌋)) s.payload.seed
        s.payload.frontier ⌊query⌋ query = some interval →
      readInterval interval.left interval.rightPhase interval.rightTime query cutoff = some time →
      FixedCell.Certified F (⌊query⌋ : Int) ((⌊query⌋ : Int) + (1 : ℝ))
        previous.time interval.piece) :
    time ≤ previous.time ∧ time ≤ cutoff ∧ |F time - query| ≤ 1 ∧
      ∃ exactTime ≤ previous.time, F exactTime = query := by
  have exactFloor (a : ℝ) : Arithmetic.floor a = ⌊a⌋ := rfl
  have segment (h : (selectInterval (s.payload.cells.get (wrap n ⌊query⌋)) s.payload.seed
      s.payload.frontier ⌊query⌋ query).bind (fun interval =>
        readInterval interval.left interval.rightPhase interval.rightTime query cutoff) = some time) :
      time ≤ previous.time ∧ time ≤ cutoff ∧ |F time - query| ≤ 1 ∧
        ∃ exactTime ≤ previous.time, F exactTime = query := by
    cases selected : selectInterval (s.payload.cells.get (wrap n ⌊query⌋)) s.payload.seed
        s.payload.frontier ⌊query⌋ query with
    | none => simp [selected] at h
    | some interval =>
      have lookup : readInterval interval.left interval.rightPhase interval.rightTime
          query cutoff = some time := by simpa only [selected, Option.bind] using h
      have sound := read_interval_sound F (⌊query⌋ : Int) ((⌊query⌋ : Int) + (1 : ℝ))
        previous.time query cutoff time interval (certify interval selected lookup) lookup
      simpa using sound
  cases frontier : s.payload.frontier with
  | none =>
    apply segment
    simpa [read, readCell, exactFloor, frontier] using found
  | some edge =>
    by_cases here : query = edge.phase
    · subst query
      obtain ⟨past, bounded, exactPhase⟩ := read_frontier_sound history latest frontier found
      exact ⟨past, bounded, by simp [exactPhase], time, past, exactPhase⟩
    · apply segment
      simpa [read, readCell, exactFloor, frontier, here] using found

private theorem interpolate_right_endpoint (left right : Point ℝ)
    (positive : left.phase < right.phase) :
    interpolate left right right.phase = right.time := by
  change left.time + (right.phase - left.phase) / (right.phase - left.phase) *
    (right.time - left.time) = right.time
  rw [div_self (sub_ne_zero.mpr (ne_of_gt positive)), one_mul, add_sub_cancel]

/-- A retained cell entered through a grid edge on this write is certified by
the current affine sample segment. Its previous contents can be arbitrary. -/
theorem write_crossed_cell_certified {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    (s : State ℝ ℝ n) {previous observation : Point ℝ}
    (prior : s.previous = some previous)
    (segment : SampleSegment F resolution previous observation)
    (continuous : Continuous F) (resolutionPositive : 0 < resolution)
    (forward : previous.phase < observation.phase) {query : ℝ}
    (retained : RetainedQuery (write s resolution observation) resolution query)
    (crossed : ⌈previous.phase * resolution⌉ ≤ ⌊query⌋)
    {interval : Interval ℝ}
    (selected : let payload := (write s resolution observation).payload
      selectInterval (payload.cells.get (wrap n ⌊query⌋)) payload.seed
        payload.frontier ⌊query⌋ query = some interval) :
    FixedCell.Certified F (⌊query⌋ : Int) ((⌊query⌋ : Int) + (1 : ℝ))
      observation.time interval.piece := by
  let left := { previous with phase := previous.phase * resolution }
  let right := { observation with phase := observation.phase * resolution }
  let opened := if !s.hasForward || s.descending then
      beginBranch { s with previous := some observation } left
    else { s with previous := some observation }
  let first : Int := ⌈left.phase⌉
  let last : Int := ⌊right.phase⌋
  let index : Int := ⌊query⌋
  let highest := max last (s.highest.getD last)
  let count := (last - first + 1).toNat
  let scattered := scatter opened.payload left right first count
  have exactLT (a b : ℝ) : Arithmetic.lt a b = decide (a < b) := rfl
  have exactLE (a b : ℝ) : Arithmetic.le a b = decide (a ≤ b) := rfl
  have exactFloor (a : ℝ) : Arithmetic.floor a = ⌊a⌋ := rfl
  have exactCeil (a : ℝ) : Arithmetic.ceil a = ⌈a⌉ := rfl
  have exactIndex (i : Int) : (Arithmetic.ofIndex i : ℝ) = i := rfl
  have payload : (write s resolution observation).payload = { scattered with frontier := some right } := by
    simp [write, prior, exactLT, forward, exactFloor, exactCeil, opened, scattered, first, last,
      count, left, right]
  have high : (write s resolution observation).highest = some highest := by
    simp [write, prior, exactLT, forward, exactFloor, last, right, highest]
  obtain ⟨retainedHigh, newest, actualHigh, newestEq, retainedLow, _, queryBound⟩ := retained
  rw [high, Option.some.injEq] at actualHigh
  subst retainedHigh
  rw [write_previous, Option.some.injEq] at newestEq
  subst newest
  have lastBound : index ≤ last := Int.floor_le_floor queryBound
  have countEnds : first + (count : Int) - 1 = last := by
    have cross : first ≤ index := crossed
    dsimp only [count]
    omega
  have strict : ∀ j, first < j → Arithmetic.lt left.phase (edgeAt left right j).phase = true := by
    intro j after
    change Arithmetic.lt left.phase (Arithmetic.ofIndex j) = true
    rw [exactLT, exactIndex, decide_eq_true_eq]
    exact (Int.le_ceil left.phase).trans_lt (Int.cast_lt.mpr after)
  have tail := scatter_fresh_cells count opened.payload left right highest first
    (by rw [countEnds]; exact le_max_left _ _) strict
  have fresh := tail index retainedLow crossed (by rwa [countEnds])
  rw [countEnds] at fresh
  simp only [payload] at selected
  change selectInterval (scattered.cells.get (wrap n index)) scattered.seed
    (some right) index query = some interval at selected
  rw [fresh.1] at selected
  have seedRetired (start : ℝ) (seed : scattered.seed = some start) : ⌊start⌋ ≠ index := by
    intro same
    have retired := fresh.2
    change scattered.seed.any (fun p => wrap n (Arithmetic.floor p) == wrap n index) = false at retired
    rw [seed] at retired
    simp [exactFloor, same] at retired
  have edgeLower : left.phase ≤ (index : ℝ) :=
    (Int.ceil_le.mp crossed)
  by_cases complete : index < last
  · have endBound : (index : ℝ) + 1 ≤ right.phase := by
      exact_mod_cast (Int.le_floor.mp (show index + 1 ≤ last by omega))
    have resolved : interval =
        ⟨⟨index, (edgeAt left right index).time⟩, (index : ℝ) + 1,
          some (edgeAt left right (index + 1)).time⟩ := by
      cases seed : scattered.seed with
      | none => simpa [seed, selectInterval, freshCell, complete, exactIndex] using selected.symm
      | some start =>
        simpa [seed, selectInterval, freshCell, complete, exactFloor, exactIndex,
          seedRetired start seed] using selected.symm
    rw [resolved]
    simpa [Interval.piece, Point.exact, edgeAt, exactIndex, left, right] using
      segment.grid_interval_certified continuous resolutionPositive forward
        edgeLower (show (index : ℝ) ≤ (index : ℝ) + 1 by linarith) endBound le_rfl le_rfl
  · have final : index = last := le_antisymm lastBound (le_of_not_gt complete)
    have lowerRight : (index : ℝ) ≤ right.phase := by
      rw [final]
      exact Int.floor_le _
    have upperRight : right.phase < (index : ℝ) + 1 := by
      rw [final]
      exact Int.lt_floor_add_one _
    have resolved : interval = ⟨⟨index, (edgeAt left right index).time⟩,
        right.phase, some observation.time⟩ := by
      cases seed : scattered.seed with
      | none =>
        simpa [seed, selectInterval, freshCell, complete, exactIndex, frontierCloses,
          exactLE, exactLT, lowerRight, upperRight] using selected.symm
      | some start =>
        simpa [seed, selectInterval, freshCell, complete, exactFloor, exactIndex,
          seedRetired start seed, frontierCloses, exactLE, exactLT, lowerRight, upperRight]
          using selected.symm
    have edgeEnd := interpolate_right_endpoint left right
      (mul_lt_mul_of_pos_right forward resolutionPositive)
    rw [resolved]
    simpa [Interval.piece, Point.exact, edgeAt, exactIndex, edgeEnd, left, right] using
      segment.grid_interval_certified continuous resolutionPositive forward
        edgeLower lowerRight le_rfl le_rfl upperRight.le

theorem SampledWrites.read_sound_crossed {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    {s : State ℝ ℝ n} (history : SampledWrites F resolution n s)
    {previous observation : Point ℝ} (prior : s.previous = some previous)
    (onTrajectory : F observation.time = observation.phase * resolution)
    (segment : SampleSegment F resolution previous observation)
    (continuous : Continuous F) (resolutionPositive : 0 < resolution)
    (forward : previous.phase < observation.phase) {query cutoff time : ℝ}
    (retained : RetainedQuery (write s resolution observation) resolution query)
    (crossed : ⌈previous.phase * resolution⌉ ≤ ⌊query⌋)
    (found : read (write s resolution observation).payload query cutoff = some time) :
    time ≤ observation.time ∧ time ≤ cutoff ∧ |F time - query| ≤ 1 ∧
      ∃ exactTime ≤ observation.time, F exactTime = query := by
  have extended : SampledWrites F resolution n (write s resolution observation) :=
    history.observe observation onTrajectory (by simpa [prior] using segment)
  refine read_sound_of_selected_piece extended (write_previous s resolution observation)
    found (fun interval selected _ => ?_)
  exact write_crossed_cell_certified s prior segment continuous resolutionPositive forward
    retained crossed selected

private theorem selectInterval_above_cell (cell : Cell ℝ ℝ) (seed : Option ℝ)
    (frontier : Option (Point ℝ)) (index : Int) (query : ℝ)
    (above : ∀ edge ∈ frontier, (index : ℝ) + 1 ≤ edge.phase) :
    selectInterval cell seed frontier index query = selectInterval cell seed none index query := by
  cases frontier with
  | none => rfl
  | some edge =>
    have exactLT (a b : ℝ) : Arithmetic.lt a b = decide (a < b) := rfl
    have exactIndex (i : Int) : (Arithmetic.ofIndex i : ℝ) = i := rfl
    have notClosing (phase : ℝ) :
        frontierCloses phase (Arithmetic.ofIndex (index + 1)) edge.phase = false := by
      have past := above edge (by simp)
      have bound : ¬ edge.phase < (Arithmetic.ofIndex (index + 1) : ℝ) := by
        simpa [exactIndex] using not_lt.mpr past
      simp [frontierCloses, exactLT, bound]
    have decode (base delta : ℝ) : Arithmetic.decode base delta = some (base + delta) := rfl
    cases base : cell.base with
    | none => simp [selectInterval, base]
    | some entry =>
      cases replacement : cell.replacement with
      | none =>
        cases finish : cell.fallbackEnd <;> simp [selectInterval, base, replacement, finish, notClosing]
      | some branch =>
        by_cases useReplacement : Arithmetic.le branch.cut query = true
        · cases finish : branch.stop <;>
            simp [selectInterval, base, replacement, useReplacement, finish, decode, notClosing]
        · cases finish : cell.fallbackEnd <;>
            simp [selectInterval, base, replacement, useReplacement, finish, notClosing]

private theorem selectInterval_seedAtSlot {n : Nat} (s : Published ℝ ℝ n)
    (frontier : Option (Point ℝ)) (index : Int) (query : ℝ) :
    selectInterval (s.cells.get (wrap n index)) s.seed frontier index query =
      selectInterval (s.cells.get (wrap n index)) (seedAtSlot s (wrap n index)) frontier index query := by
  simp only [seedAtSlot]
  cases seed : s.seed with
  | none => rfl
  | some start =>
    dsimp only [Option.filter]
    split
    · rfl
    · rename_i different
      have notThisCell : Arithmetic.floor start ≠ index := by
        intro same
        simp [same] at different
      simp [selectInterval, notThisCell]

/-- A forward write can only preserve or clear a retained cell older than its
first predecessor. The same holds for the predecessor when the write starts
on a grid edge. Both frontiers are beyond such a cell. -/
theorem retainedCellsCertified_older {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    {s : State ℝ ℝ n} (history : SampledWrites F resolution n s)
    (certified : RetainedCellsCertified F resolution s)
    {previous observation : Point ℝ} (prior : s.previous = some previous)
    (timeOrder : previous.time ≤ observation.time) (resolutionPositive : 0 < resolution)
    (forward : previous.phase < observation.phase) {query : ℝ}
    (retained : RetainedQuery (write s resolution observation) resolution query)
    (older : ⌊query⌋ < ⌈previous.phase * resolution⌉ - 1 ∨
      ⌊query⌋ = ⌈previous.phase * resolution⌉ - 1 ∧
        previous.phase * resolution = (⌈previous.phase * resolution⌉ : Int))
    {interval : Interval ℝ}
    (selected : let payload := (write s resolution observation).payload
      selectInterval (payload.cells.get (wrap n ⌊query⌋)) payload.seed
        payload.frontier ⌊query⌋ query = some interval) :
    FixedCell.Certified F (⌊query⌋ : Int) ((⌊query⌋ : Int) + (1 : ℝ))
      observation.time interval.piece := by
  let left := { previous with phase := previous.phase * resolution }
  let right := { observation with phase := observation.phase * resolution }
  let observed := { s with previous := some observation }
  let opened := if !s.hasForward || s.descending then beginBranch observed left else observed
  let first : Int := ⌈left.phase⌉
  let last : Int := ⌊right.phase⌋
  let index : Int := ⌊query⌋
  let highest := max last (s.highest.getD last)
  let count := (last - first + 1).toNat
  let scattered := scatter opened.payload left right first count
  let output := { scattered with frontier := some right }
  change index < first - 1 ∨ index = first - 1 ∧ left.phase = (first : ℝ) at older
  have exactLT (a b : ℝ) : Arithmetic.lt a b = decide (a < b) := rfl
  have exactFloor (a : ℝ) : Arithmetic.floor a = ⌊a⌋ := rfl
  have exactCeil (a : ℝ) : Arithmetic.ceil a = ⌈a⌉ := rfl
  have payload : (write s resolution observation).payload = output := by
    simp [write, prior, exactLT, forward, exactFloor, exactCeil, output, opened, observed,
      scattered, first, last, count, left, right]
  have high : (write s resolution observation).highest = some highest := by
    simp [write, prior, exactLT, forward, exactFloor, last, right, highest]
  obtain ⟨retainedHigh, _, upperEq, _, retainedLow, _, _⟩ := retained
  rw [high, Option.some.injEq] at upperEq
  subst retainedHigh
  have gridOrder : left.phase ≤ right.phase := (mul_lt_mul_of_pos_right forward resolutionPositive).le
  have firstLower : ((first - 1 : Int) : ℝ) ≤ left.phase := by
    have ceilBound := Int.ceil_lt_add_one left.phase
    push_cast
    linarith
  have beforeLeft : (index : ℝ) + 1 ≤ left.phase := by
    rcases older with earlier | ⟨predecessor, aligned⟩
    · have below : index + 1 ≤ first - 1 := by omega
      exact (by exact_mod_cast below : (index : ℝ) + 1 ≤ ((first - 1 : Int) : ℝ)).trans firstLower
    · rw [predecessor, aligned]
      push_cast
      linarith
  have countEnds : first + (count : Int) - 1 = last := by
    have bound : first - 1 ≤ last := Int.le_floor.mpr (firstLower.trans gridOrder)
    dsimp only [count]
    omega
  have scatterUnchanged :
      scattered.cells.get (wrap n index) = opened.payload.cells.get (wrap n index) ∧
        seedAtSlot scattered (wrap n index) = seedAtSlot opened.payload (wrap n index) := by
    rcases older with earlier | ⟨predecessor, aligned⟩
    · exact scatter_preserves_older count opened.payload left right highest first index
        (by rw [countEnds]; exact le_max_left _ _) retainedLow earlier
    · have nonempty : 0 < count := by
        have bound : first ≤ last := Int.le_floor.mpr (aligned ▸ gridOrder)
        dsimp only [count]
        omega
      rw [predecessor]
      have finish := scatter_first_predecessor count opened.payload left right highest first
        (by rw [countEnds]; exact le_max_left _ _)
        (by simpa only [← predecessor] using retainedLow) nonempty
      have noFinish : Arithmetic.lt left.phase (edgeAt left right first).phase = false := by
        have edgePhase : (edgeAt left right first).phase = (first : ℝ) := rfl
        simp [edgePhase, aligned, exactLT]
      simpa only [noFinish, Bool.false_eq_true, if_false] using finish
  obtain ⟨scatterCell, scatterSeed⟩ := scatterUnchanged
  have openCell :
      (opened.payload.cells.get (wrap n index) = s.payload.cells.get (wrap n index) ∧
        seedAtSlot opened.payload (wrap n index) = seedAtSlot s.payload (wrap n index)) ∨
      opened.payload.cells.get (wrap n index) = {} := by
    have floorHigher : index < ⌊left.phase⌋ := by
      have bound : index + 1 ≤ ⌊left.phase⌋ := Int.le_floor.mpr (by exact_mod_cast beforeLeft)
      omega
    have aliases : wrap n index ≠ wrap n (Arithmetic.floor left.phase) := by
      rw [exactFloor]
      have floorBound : ⌊left.phase⌋ ≤ highest :=
        (Int.floor_le_floor gridOrder).trans (le_max_left _ _)
      intro equal
      have same := wrap_injective_on_retained_window n highest index ⌊left.phase⌋
        retainedLow (floorHigher.le.trans floorBound) (by omega) floorBound equal
      omega
    unfold opened
    split
    · exact (beginBranch_other_slot observed left (wrap n index) aliases).imp_right And.left
    · exact Or.inl ⟨rfl, rfl⟩
  rw [payload] at selected
  change selectInterval (scattered.cells.get (wrap n index)) output.seed output.frontier index query =
    some interval at selected
  rw [scatterCell] at selected
  rcases openCell with ⟨oldCell, oldSeed⟩ | cleared
  · rw [oldCell] at selected
    cases oldHighest : s.highest with
    | none =>
      have empty := (history.empty_inverse_without_highest oldHighest).1
      simp [empty, selectInterval, Vector.get] at selected
    | some oldHigh =>
      have oldHighestLe : oldHigh ≤ highest := by simp [highest, oldHighest]
      have prevMember : previous ∈ s.previous := by simp [prior]
      have highestCovers := history.highest_covers_previous resolutionPositive prevMember
        (show oldHigh ∈ s.highest by simp [oldHighest])
      have oldRetained : RetainedQuery s resolution query := by
        have queryBelow : query ≤ left.phase := (Int.lt_floor_add_one query).le.trans beforeLeft
        have indexBelow : index ≤ ⌊left.phase⌋ :=
          Int.le_floor.mpr ((le_add_of_nonneg_right zero_le_one).trans beforeLeft)
        exact ⟨oldHigh, previous, oldHighest, prior, by omega, indexBelow.trans highestCovers,
          queryBelow⟩
      have oldAbove edge (live : edge ∈ s.payload.frontier) : (index : ℝ) + 1 ≤ edge.phase :=
        beforeLeft.trans (history.previous_phase_le_frontier resolutionPositive prevMember live)
      have newAbove edge (live : edge ∈ output.frontier) : (index : ℝ) + 1 ≤ edge.phase := by
        change edge ∈ some right at live
        obtain rfl := Option.mem_some_iff.mp live
        exact beforeLeft.trans gridOrder
      have oldSelected :
          selectInterval (s.payload.cells.get (wrap n index)) s.payload.seed s.payload.frontier
            index query = some interval := by
        rw [selectInterval_above_cell _ _ _ _ _ oldAbove,
          selectInterval_seedAtSlot s.payload none index query]
        rw [selectInterval_above_cell _ _ _ _ _ newAbove] at selected
        rw [← oldCell, ← scatterCell, selectInterval_seedAtSlot output none index query] at selected
        have seedEq : seedAtSlot output (wrap n index) = seedAtSlot s.payload (wrap n index) :=
          scatterSeed.trans oldSeed
        rwa [seedEq, scatterCell, oldCell] at selected
      have piece := certified previous prevMember query oldRetained interval oldSelected
      exact { piece with causal := piece.causal.trans timeOrder }
  · simp [cleared, selectInterval] at selected

private theorem select_scattered_seed_predecessor_certified {n : Nat} {F : ℝ → ℝ}
    {resolution : ℝ} {previous observation : Point ℝ}
    (segment : SampleSegment F resolution previous observation)
    (continuous : Continuous F) (resolutionPositive : 0 < resolution)
    (forward : previous.phase < observation.phase) (payload : Published ℝ ℝ n)
    (highest : Int) {interval : Interval ℝ} (query : ℝ)
    (fractional : (⌊previous.phase * resolution⌋ : ℤ) < previous.phase * resolution)
    (horizon : ⌊observation.phase * resolution⌋ ≤ highest)
    (retained : highest - (n : ℤ) ≤ ⌊previous.phase * resolution⌋) :
    let left := { previous with phase := previous.phase * resolution }
    let right := { observation with phase := observation.phase * resolution }
    let index := ⌊left.phase⌋
    let first := ⌈left.phase⌉
    let count := (⌊right.phase⌋ - first + 1).toNat
    let out := scatter (seedCell payload left) left right first count
    selectInterval (out.cells.get (wrap n index)) out.seed (some right) index query = some interval →
      FixedCell.Certified F (index : ℝ) ((index : ℝ) + 1) observation.time interval.piece := by
  intro left right index first count out selected
  let seeded := seedCell payload left
  have exactFloor (a : ℝ) : Arithmetic.floor a = ⌊a⌋ := rfl
  have exactLT (a b : ℝ) : Arithmetic.lt a b = decide (a < b) := rfl
  have exactLE (a b : ℝ) : Arithmetic.le a b = decide (a ≤ b) := rfl
  have exactIndex (i : Int) : (Arithmetic.ofIndex i : ℝ) = i := rfl
  have firstEq : first = index + 1 := by
    apply Int.ceil_eq_iff.mpr
    push_cast
    constructor
    · simpa only [add_sub_cancel_right] using fractional
    · exact (Int.lt_floor_add_one left.phase).le
  have lower : (index : ℝ) ≤ left.phase := Int.floor_le _
  have leftUpper : left.phase < (first : ℝ) := by
    rw [firstEq, Int.cast_add, Int.cast_one]
    exact Int.lt_floor_add_one _
  have phaseOrder : left.phase < right.phase := mul_lt_mul_of_pos_right forward resolutionPositive
  have base : seeded.cells.get (wrap n index) = ⟨some left.time, none, none⟩ := by
    simp [seeded, seedCell, assignCell, exactFloor, index, get_set_same]
  have seed : seedAtSlot seeded (wrap n index) = some left.phase := by
    simp [seeded, seedAtSlot, seedCell, exactFloor, index, Option.filter]
  have startEq : interpolate left right left.phase = left.time := by
    change left.time + ((left.phase - left.phase) / (right.phase - left.phase)) *
      (right.time - left.time) = left.time
    simp
  rw [selectInterval_seedAtSlot] at selected
  by_cases crossing : first ≤ ⌊right.phase⌋
  · have countEnd : first + (count : ℤ) - 1 = ⌊right.phase⌋ := by
      dsimp only [count]
      omega
    have nonempty : 0 < count := by
      dsimp only [count]
      omega
    change highest - (n : ℤ) ≤ index at retained
    have hlo : highest - (n : ℤ) ≤ first - 1 := by omega
    have hmax : first + (count : ℤ) - 1 ≤ highest := by rwa [countEnd]
    have effects := scatter_first_predecessor count seeded left right highest first hmax hlo nonempty
    have predecessor : first - 1 = index := by omega
    have finish : (finishCell seeded index (edgeAt left right first).time).cells.get (wrap n index) =
        ⟨some left.time, some (edgeAt left right first).time, none⟩ := by
      simp [finishCell, get_set_same, base]
    simp only [edgeAt, exactIndex, exactLT, leftUpper, decide_true, if_true, predecessor] at effects
    change out.cells.get (wrap n index) =
      (finishCell seeded index (edgeAt left right first).time).cells.get (wrap n index) ∧
      seedAtSlot out (wrap n index) = seedAtSlot seeded (wrap n index) at effects
    rw [effects.1, finish, effects.2, seed] at selected
    have selectedEq : interval =
        ⟨⟨left.phase, left.time⟩, (first : ℝ), some (interpolate left right first)⟩ := by
      simpa [selectInterval, exactFloor, exactIndex, index, ← firstEq, edgeAt] using selected.symm
    have rightBound : (first : ℝ) ≤ right.phase := Int.le_floor.mp crossing
    have firstBound : (first : ℝ) ≤ (index : ℝ) + 1 := by rw [firstEq, Int.cast_add, Int.cast_one]
    have cert := segment.grid_interval_certified continuous resolutionPositive forward
      le_rfl leftUpper.le rightBound lower firstBound
    change FixedCell.Certified F (index : ℝ) ((index : ℝ) + 1) observation.time
      ⟨⟨left.phase, interpolate left right left.phase⟩,
        ⟨(first : ℝ), interpolate left right first⟩⟩ at cert
    rw [selectedEq]
    simpa only [Interval.piece, Point.exact, Option.elim, startEq] using cert
  · have empty : count = 0 := by
      dsimp only [count]
      omega
    have unchanged : out = seeded := by
      change scatter seeded left right first count = seeded
      rw [empty, scatter]
      simp
    have rightBound : right.phase < (index : ℝ) + 1 := by
      have below := Int.floor_lt.mp (lt_of_not_ge crossing)
      simpa [firstEq] using below
    have frontierLower : left.phase ≤ right.phase := phaseOrder.le
    rw [unchanged, base, seed] at selected
    have selectedEq : interval = ⟨⟨left.phase, left.time⟩, right.phase, some right.time⟩ := by
      simpa [selectInterval, exactFloor, exactIndex, index, frontierCloses, exactLE, exactLT,
        frontierLower, rightBound] using selected.symm
    have cert := segment.grid_interval_certified continuous resolutionPositive forward
      le_rfl phaseOrder.le le_rfl lower rightBound.le
    have rightEq : interpolate left right right.phase = right.time := by
      change left.time + ((right.phase - left.phase) / (right.phase - left.phase)) *
        (right.time - left.time) = right.time
      rw [div_self (sub_ne_zero.mpr (ne_of_gt phaseOrder)), one_mul]
      exact add_sub_cancel _ _
    change FixedCell.Certified F (index : ℝ) ((index : ℝ) + 1) observation.time
      ⟨⟨left.phase, interpolate left right left.phase⟩,
        ⟨right.phase, interpolate left right right.phase⟩⟩ at cert
    rw [selectedEq]
    simpa only [Interval.piece, Point.exact, Option.elim, startEq, rightEq] using cert

private theorem write_first_fractional_cell_certified {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    {s : State ℝ ℝ n} (history : SampledWrites F resolution n s) (noHigh : s.highest = none)
    {previous observation : Point ℝ} (prior : s.previous = some previous)
    (segment : SampleSegment F resolution previous observation) (continuous : Continuous F)
    (resolutionPositive : 0 < resolution) (forward : previous.phase < observation.phase)
    (fractional : previous.phase * resolution < (⌈previous.phase * resolution⌉ : Int))
    {query : ℝ} (retained : RetainedQuery (write s resolution observation) resolution query)
    (predecessor : ⌊query⌋ = ⌈previous.phase * resolution⌉ - 1) {interval : Interval ℝ}
    (selected : let payload := (write s resolution observation).payload
      selectInterval (payload.cells.get (wrap n ⌊query⌋)) payload.seed
        payload.frontier ⌊query⌋ query = some interval) :
    FixedCell.Certified F (⌊query⌋ : Int) ((⌊query⌋ : Int) + (1 : ℝ))
      observation.time interval.piece := by
  let left := { previous with phase := previous.phase * resolution }
  let right := { observation with phase := observation.phase * resolution }
  let observed := { s with previous := some observation, hasForward := false }
  let first : Int := ⌈left.phase⌉
  let last : Int := ⌊right.phase⌋
  let index : Int := ⌊left.phase⌋
  let count := (last - first + 1).toNat
  let input := { s.payload with frontier := some left }
  let scattered := scatter (seedCell input left) left right first count
  have exactFloor (a : ℝ) : Arithmetic.floor a = ⌊a⌋ := rfl
  have exactCeil (a : ℝ) : Arithmetic.ceil a = ⌈a⌉ := rfl
  have exactIndex (i : Int) : (Arithmetic.ofIndex i : ℝ) = i := rfl
  have exactLT (a b : ℝ) : Arithmetic.lt a b = decide (a < b) := rfl
  have fractional' : (index : ℝ) < left.phase := by
    have lower : (index : ℝ) ≤ left.phase := Int.floor_le _
    apply lt_of_le_of_ne lower
    intro atGrid
    have ceiling : first = index := Int.ceil_eq_iff.mpr ⟨by linarith, atGrid.symm.le⟩
    change left.phase < (first : ℝ) at fractional
    linarith [show (first : ℝ) = (index : ℝ) by exact_mod_cast ceiling]
  have firstEq : first = index + 1 := by
    apply Int.ceil_eq_iff.mpr
    push_cast
    exact ⟨by simpa only [add_sub_cancel_right] using fractional', (Int.lt_floor_add_one left.phase).le⟩
  have queryIndex : ⌊query⌋ = index := by
    change ⌊query⌋ = first - 1 at predecessor
    omega
  have seed :
      (beginBranch observed left).payload = seedCell input left := by
    have notGrid : left.phase ≠ (index : ℝ) := ne_of_gt fractional'
    simp [beginBranch, observed, exactFloor, exactIndex, show ⌊left.phase⌋ = index from rfl,
      notGrid, noHigh, input, seedCell, assignCell]
    cases s.payload.seed <;> rfl
  have forwardFlag := (history.empty_inverse_without_highest noHigh).2
  have payload : (write s resolution observation).payload = { scattered with frontier := some right } := by
    simp only [write, prior, exactLT, forward, decide_true, if_true, forwardFlag, Bool.not_false,
      Bool.true_or, exactFloor, exactCeil]
    change { scatter (beginBranch observed left).payload left right first count with frontier := some right } =
      { scattered with frontier := some right }
    rw [seed]
  have highest : (write s resolution observation).highest = some last := by
    simp [write, prior, exactLT, forward, exactFloor, noHigh, last, right]
  obtain ⟨high, _, highEq, _, lower, _, _⟩ := retained
  rw [highest, Option.some.injEq] at highEq
  subst high
  rw [queryIndex] at selected ⊢
  rw [payload] at selected
  exact select_scattered_seed_predecessor_certified segment continuous resolutionPositive forward
    input last query fractional' le_rfl (by simpa only [queryIndex] using lower) selected

/-- The forward write's first predecessor, including continued and reopened
traversals, is the remaining certification obligation. -/
def FirstPredecessorCertified : Prop :=
  ∀ {n : Nat} {F : ℝ → ℝ} {resolution : ℝ} {s : State ℝ ℝ n},
    Continuous F → 0 < resolution → SampledWrites F resolution n s →
    RetainedCellsCertified F resolution s → s.highest.isSome →
    ∀ {previous observation : Point ℝ},
      s.previous = some previous → SampleSegment F resolution previous observation →
      previous.phase < observation.phase →
      previous.phase * resolution < (⌈previous.phase * resolution⌉ : Int) →
      ∀ {query : ℝ}, RetainedQuery (write s resolution observation) resolution query →
        ⌊query⌋ = ⌈previous.phase * resolution⌉ - 1 →
        ∀ {interval : Interval ℝ},
          (let payload := (write s resolution observation).payload
           selectInterval (payload.cells.get (wrap n ⌊query⌋)) payload.seed
             payload.frontier ⌊query⌋ query = some interval) →
          FixedCell.Certified F (⌊query⌋ : Int) ((⌊query⌋ : Int) + (1 : ℝ))
            observation.time interval.piece

theorem SampledWrites.retained_cells_certified_of_predecessor
    (predecessor : FirstPredecessorCertified)
    {n : Nat} {F : ℝ → ℝ} {resolution : ℝ} {s : State ℝ ℝ n}
    (continuous : Continuous F) (resolutionPositive : 0 < resolution)
    (history : SampledWrites F resolution n s) :
    RetainedCellsCertified F resolution s := by
  induction history with
  | empty => exact retainedCellsCertified_empty n F resolution
  | @observe s history observation _ allSegments ih =>
    cases prior : s.previous with
    | none =>
      have noHigh : s.highest = none := by
        cases history with
        | empty => rfl
        | observe => simp [write_previous] at prior
      intro _ _ _ retained
      obtain ⟨highest, _, bound, _, _, _, _⟩ := retained
      simp [write, prior, noHigh] at bound
    | some previous =>
      have segment := allSegments previous (by simp [prior])
      by_cases forward : previous.phase < observation.phase
      · intro newest latest query retained interval selected
        rw [write_previous, Option.mem_some_iff] at latest
        subst newest
        by_cases crossed : ⌈previous.phase * resolution⌉ ≤ ⌊query⌋
        · exact write_crossed_cell_certified s prior segment continuous resolutionPositive forward
            retained crossed selected
        by_cases older : ⌊query⌋ < ⌈previous.phase * resolution⌉ - 1
        · exact retainedCellsCertified_older history ih prior segment.1.le resolutionPositive
            forward retained (Or.inl older) selected
        · have firstPredecessor : ⌊query⌋ = ⌈previous.phase * resolution⌉ - 1 := by omega
          rcases (Int.le_ceil (previous.phase * resolution)).lt_or_eq with fractional | aligned
          · cases highest : s.highest with
            | none =>
              exact write_first_fractional_cell_certified history highest prior segment
                continuous resolutionPositive forward fractional retained firstPredecessor selected
            | some _ =>
              exact predecessor continuous resolutionPositive history ih
                (by simp [highest]) prior segment forward fractional retained firstPredecessor selected
          · exact retainedCellsCertified_older history ih prior segment.1.le resolutionPositive
              forward retained (Or.inr ⟨firstPredecessor, aligned⟩) selected
      · exact retainedCellsCertified_nonforward ih resolutionPositive prior segment.1.le
          (le_of_not_gt forward)

theorem reachable_read_soundness_of_predecessor (predecessor : FirstPredecessorCertified) :
    ReachableReadSoundness := by
  intro n F resolution s resolutionPositive continuous history previous query cutoff time latest retained found
  have certified := history.retained_cells_certified_of_predecessor predecessor continuous resolutionPositive
  refine read_sound_of_selected_piece history latest found (fun interval selected _ => ?_)
  exact certified previous (by simp [latest]) query retained interval selected

end

end PhaseTime.InverseRuntime

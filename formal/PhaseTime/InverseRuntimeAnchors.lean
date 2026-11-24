import PhaseTime.InverseRuntimeReachability

namespace PhaseTime.InverseRuntime

noncomputable section
attribute [local instance] exactArithmetic

private theorem anchors_mono {R : Type} [arithmetic : Arithmetic R] {n : Nat}
    {At Later : R → R → Prop} {s : Published R arithmetic.Delta n} {owners : SlotOwners n}
    (h : TracksAnchors At s owners) (mono : ∀ t p, At t p → Later t p) :
    TracksAnchors Later s owners := by
  intro slot
  have hslot := h slot
  cases howner : owners slot <;> cases hbase : (s.cells.get slot).base <;>
    simp only [howner, hbase] at hslot ⊢
  · cases hseed : seedAtSlot s slot <;> simp only [hseed] at hslot ⊢
    · exact ⟨hslot.1, mono _ _ hslot.2⟩
    · exact ⟨hslot.1, hslot.2.1, mono _ _ hslot.2.2⟩

/-- Base anchors lie on the trajectory by the latest sample, and retained slots carry their logical indices. -/
def StateAnchors {n : Nat} (F : ℝ → ℝ) (resolution : ℝ) (s : State ℝ ℝ n) : Prop :=
  ∃ owners : SlotOwners n,
    TracksAnchors (fun t p => F t = p ∧ ∀ previous ∈ s.previous, t ≤ previous.time) s.payload owners ∧
    (s.highest = none → ∀ slot, owners slot = none) ∧
    ∀ highest ∈ s.highest, ∀ previous ∈ s.previous,
      RetainedOwners owners highest ⌊previous.phase * resolution⌋

theorem SampledWrites.stateAnchors {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    {s : State ℝ ℝ n} (h : SampledWrites F resolution n s) (positive : 0 < resolution) :
    StateAnchors F resolution s := by
  induction h with
  | empty => exact ⟨fun _ => none, anchors_empty _, fun _ _ => rfl, by simp⟩
  | @observe s history observation _ segment ih =>
    obtain ⟨owners, anchors, emptyTags, retained⟩ := ih
    have nextPrevious := write_previous s resolution observation
    have exactLT (a b : ℝ) : Arithmetic.lt a b = decide (a < b) := rfl
    cases hp : s.previous with
    | none =>
      have empty : s = {} := by
        cases history with
        | empty => rfl
        | observe _ _ _ _ => simp [write_previous] at hp
      subst s
      exact ⟨fun _ => none, anchors_empty _, fun _ _ => rfl, by simp [write]⟩
    | some previous =>
      let At : ℝ → ℝ → Prop := fun t p => F t = p ∧ t ≤ observation.time
      have member : previous ∈ s.previous := by simp [hp]
      have seg := segment previous member
      have later : TracksAnchors At s.payload owners := anchors_mono anchors
        (fun t p h => ⟨h.1, (h.2 previous member).trans seg.1.le⟩)
      by_cases forward : previous.phase < observation.phase
      · let left := {previous with phase := previous.phase * resolution}
        let right := {observation with phase := observation.phase * resolution}
        let first : Int := ⌈left.phase⌉
        let last : Int := ⌊right.phase⌋
        let highest := max last (s.highest.getD last)
        have rightFloor : ⌊left.phase⌋ ≤ last :=
          Int.floor_mono (mul_le_mul_of_nonneg_right forward.le positive.le)
        have hmax : ⌊left.phase⌋ ≤ highest := le_trans rightFloor (le_max_left _ _)
        have hold : RetainedOwners owners highest ⌊left.phase⌋ := by
          intro i hlo hhi tag htag
          cases h : s.highest with
          | none => rw [emptyTags h] at htag; cases htag
          | some old =>
            have ho : old ≤ highest := by simp [highest, h]
            exact retained old (by simp [h]) previous member i (by omega) hhi tag htag
        let observed := {s with previous := some observation}
        let opened := if !s.hasForward || s.descending then beginBranch observed left else observed
        have openedAnchors :
            ∃ tags, TracksAnchors At opened.payload tags ∧ RetainedOwners tags highest ⌊left.phase⌋ := by
          dsimp only [opened]
          split
          · exact beginBranch_retained_anchors At observed owners left highest hmax later
              ⟨history.previous_exact member, seg.1.le⟩ hold
          · exact ⟨owners, later, hold⟩
        obtain ⟨tags, current, currentOwners⟩ := openedAnchors
        have hfirst : first - 1 ≤ ⌊left.phase⌋ := by
          have hc := Int.ceil_le_floor_add_one left.phase
          change first ≤ _ at hc
          omega
        have hremaining : first + (↑((last - first + 1).toNat) : Int) - 1 = last := by
          omega
        have hedges : ∀ i, first ≤ i → i ≤ first + (↑((last - first + 1).toNat) : Int) - 1 →
            At (edgeAt left right i).time (Arithmetic.ofIndex i) := by
          intro i lo hi
          rw [hremaining] at hi
          have exactPhase := seg.interpolate_exact positive forward (Int.ceil_le.mp lo) (Int.le_floor.mp hi)
          exact ⟨exactPhase.1, exactPhase.2.2⟩
        obtain ⟨updated, updateAnchors, updateOwners⟩ :=
          scatter_retained_anchors At (last - first + 1).toNat opened.payload tags left right first highest
            (by rw [hremaining]; exact le_max_left _ _) current
            (fun i hlo hhi => currentOwners i hlo (hhi.trans hfirst)) hedges
        rw [hremaining] at updateOwners
        refine ⟨updated, ?_, ?_, ?_⟩
        · intro slot
          have ht := updateAnchors slot
          have hwrite : (write s resolution observation).payload =
              { scatter opened.payload left right first (last - first + 1).toNat with frontier := some right } := by
            simp only [write, hp, exactLT, forward, decide_true, if_true]
            rfl
          rw [hwrite]
          simpa [nextPrevious, At, seedAtSlot] using ht
        · simp [write, hp, exactLT, forward]
        · rw [nextPrevious]
          simp only [write, hp, exactLT, forward, decide_true, if_true, Option.mem_some_iff]
          intro H hH p hP
          subst H
          subst p
          exact updateOwners
      · have hdown : observation.phase ≤ previous.phase := le_of_not_gt forward
        have bound : ⌊observation.phase * resolution⌋ ≤ ⌊previous.phase * resolution⌋ :=
          Int.floor_mono (mul_le_mul_of_nonneg_right hdown positive.le)
        have payload : (write s resolution observation).payload = s.payload := by
          simp only [write, hp, exactLT, forward, decide_false, Bool.false_eq_true, if_false]
          split <;> rfl
        have highest : (write s resolution observation).highest = s.highest := by
          simp only [write, hp, exactLT, forward, decide_false, Bool.false_eq_true, if_false]
          split <;> rfl
        refine ⟨owners, ?_, highest ▸ emptyTags, ?_⟩
        · rw [payload]
          simpa [nextPrevious, At] using later
        · intro H hH p hP i hlo hhi
          rw [nextPrevious, Option.mem_some_iff] at hP
          subst p
          exact retained H (highest ▸ hH) previous member i hlo (hhi.trans bound)
/-- Every active replacement starts at a past sample of its cut phase. -/
theorem SampledWrites.cuts {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    {s : State ℝ ℝ n} (h : SampledWrites F resolution n s) :
    TracksCuts (fun t p => F t = p ∧ ∀ previous ∈ s.previous, t ≤ previous.time) s.payload := by
  induction h with
  | empty => exact cuts_empty _
  | @observe s history observation _ segment ih =>
    have newest := write_previous s resolution observation
    have exactLT (a b : ℝ) : Arithmetic.lt a b = decide (a < b) := rfl
    cases hp : s.previous with
    | none =>
      have empty : s = {} := by
        cases history with
        | empty => rfl
        | observe _ _ _ _ => simp [write_previous] at hp
      subst s
      exact cuts_empty _
    | some previous =>
      let At : ℝ → ℝ → Prop := fun t p => F t = p ∧ t ≤ observation.time
      have member : previous ∈ s.previous := by simp [hp]
      have seg := segment previous member
      have later : TracksCuts At s.payload := by
        intro slot base hb branch hc
        obtain ⟨t, decode, trajectory, past⟩ := ih slot base hb branch hc
        exact ⟨t, decode, trajectory, (past previous member).trans seg.1.le⟩
      by_cases forward : previous.phase < observation.phase
      · let left := {previous with phase := previous.phase * resolution}
        let right := {observation with phase := observation.phase * resolution}
        let first : Int := ⌈left.phase⌉
        let last : Int := ⌊right.phase⌋
        let observed := {s with previous := some observation}
        let opened := if !s.hasForward || s.descending then beginBranch observed left else observed
        have openedCuts : TracksCuts At opened.payload := by
          dsimp only [opened]
          split
          · apply cuts_beginBranch At observed left later ⟨history.previous_exact member, seg.1.le⟩
            intro base t
            change some (base + (t - base)) = some t
            congr 1
            ring
          · exact later
        have result := cuts_scatter At (last - first + 1).toNat opened.payload openedCuts left right first
        have hwrite : (write s resolution observation).payload =
            { scatter opened.payload left right first (last - first + 1).toNat with frontier := some right } := by
          simp only [write, hp, exactLT, forward, decide_true, if_true]
          rfl
        rw [hwrite]
        simpa [TracksCuts, At, newest] using result
      · have payload : (write s resolution observation).payload = s.payload := by
          simp only [write, hp, exactLT, forward, decide_false, Bool.false_eq_true, if_false]
          split <;> rfl
        rw [payload]
        simpa [TracksCuts, At, newest] using later

/-- The fallback entry phase used by lookup has an exact past anchor at its stored base. -/
theorem SampledWrites.fallback_anchor {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    {s : State ℝ ℝ n} (history : SampledWrites F resolution n s) (positive : 0 < resolution)
    {previous : Point ℝ} (latest : s.previous = some previous) {query base : ℝ}
    (retained : RetainedQuery s resolution query)
    (hbase : (s.payload.cells.get (wrap n ⌊query⌋)).base = some base) :
    F base = (match s.payload.seed with
      | some phase => if ⌊phase⌋ = ⌊query⌋ then phase else (⌊query⌋ : ℝ)
      | none => (⌊query⌋ : ℝ)) ∧ base ≤ previous.time := by
  obtain ⟨highest, newest, hh, hn, hlo, _, hp⟩ := retained
  rw [latest] at hn
  injection hn with hn
  subst newest
  obtain ⟨owners, anchors, _, rights⟩ := history.stateAnchors positive
  let i : Int := ⌊query⌋
  have htag := anchors (wrap n i)
  change (s.payload.cells.get (wrap n i)).base = some base at hbase
  rw [hbase] at htag
  cases ho : owners (wrap n i) with
  | none =>
    rw [ho] at htag
    exact False.elim htag
  | some tag =>
    rw [ho] at htag
    have htagEq := rights highest (by simp [hh]) previous (by simp [latest])
      i hlo (Int.floor_mono hp) tag (by simp [ho])
    subst tag
    cases hs : s.payload.seed with
    | none =>
      have absence : seedAtSlot s.payload (wrap n i) = none := by simp [seedAtSlot, hs]
      rw [absence] at htag
      exact ⟨htag.2.1, htag.2.2 previous (by simp [latest])⟩
    | some phase =>
      by_cases hs' : wrap n (⌊phase⌋ : Int) = wrap n i
      · have active : seedAtSlot s.payload (wrap n i) = some phase := by
          simp [seedAtSlot, hs, Option.filter, show Arithmetic.floor phase = ⌊phase⌋ from rfl, hs']
        rw [active] at htag
        have hf : ⌊phase⌋ = ⌊query⌋ := htag.2.1
        exact ⟨by simpa [hf] using htag.2.2.1, htag.2.2.2 previous (by simp [latest])⟩
      · have absent : seedAtSlot s.payload (wrap n i) = none := by
          simp [seedAtSlot, hs, Option.filter, show Arithmetic.floor phase = ⌊phase⌋ from rfl, hs']
        rw [absent] at htag
        have hf : ⌊phase⌋ ≠ ⌊query⌋ := fun heq => hs' (congrArg (wrap n) heq)
        exact ⟨by simpa [hf, show Arithmetic.ofIndex i = (⌊query⌋ : ℝ) from rfl] using htag.2.1, htag.2.2 previous (by simp [latest])⟩

private theorem readInterval_above_entry (left : Point ℝ) (rightPhase : ℝ)
    (rightTime : Option ℝ) {query cutoff time : ℝ}
    (found : readInterval left rightPhase rightTime query cutoff = some time) :
    left.phase ≤ query := by
  apply le_of_not_gt
  intro below
  have exactLT (a b : ℝ) : Arithmetic.lt a b = decide (a < b) := rfl
  simp [readInterval, readIntervalWith, entryInRange, exactLT, below] at found

theorem SampledWrites.selected_left_exact {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    {s : State ℝ ℝ n} (history : SampledWrites F resolution n s)
    (positive : 0 < resolution) {previous : Point ℝ} (latest : s.previous = some previous)
    {query : ℝ} (retained : RetainedQuery s resolution query) {interval : Interval ℝ}
    (selected : selectInterval (s.payload.cells.get (wrap n ⌊query⌋)) s.payload.seed
      s.payload.frontier ⌊query⌋ query = some interval) :
    F interval.left.time = interval.left.phase ∧ interval.left.time ≤ previous.time := by
  let index : Int := ⌊query⌋
  let slot := wrap n index
  let cell := s.payload.cells.get slot
  change selectInterval cell s.payload.seed s.payload.frontier index query = some interval at selected
  have member : previous ∈ s.previous := by simp [latest]
  have exactFloor (a : ℝ) : Arithmetic.floor a = ⌊a⌋ := rfl
  have exactIndex (i : Int) : (Arithmetic.ofIndex i : ℝ) = i := rfl
  cases baseEq : cell.base with
  | none => simp [selectInterval, baseEq] at selected
  | some base =>
    have fallback := history.fallback_anchor positive latest retained baseEq
    cases branchEq : cell.replacement with
    | none =>
      simp [selectInterval, baseEq, branchEq] at selected
      subst interval
      cases seed : s.payload.seed <;> simpa [seed, exactFloor, exactIndex, index] using fallback
    | some branch =>
      by_cases replacement : Arithmetic.le branch.cut query = true
      · obtain ⟨time, decoded, exactPhase, past⟩ :=
          history.cuts slot base baseEq branch branchEq
        simp [selectInterval, baseEq, branchEq, replacement, decoded] at selected
        subst interval
        exact ⟨exactPhase, past previous member⟩
      · simp [selectInterval, baseEq, branchEq, replacement] at selected
        subst interval
        cases seed : s.payload.seed <;> simpa [seed, exactFloor, exactIndex, index] using fallback

/-- Each successful retained read has an exact historical preimage, even when
the timestamp returned by interpolation is approximate. -/
theorem SampledWrites.read_exact_preimage {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    {s : State ℝ ℝ n} (history : SampledWrites F resolution n s)
    (positive : 0 < resolution) (continuous : Continuous F)
    {previous : Point ℝ} (latest : s.previous = some previous)
    {query cutoff time : ℝ} (retained : RetainedQuery s resolution query)
    (found : read s.payload query cutoff = some time) :
    ∃ exactTime ≤ previous.time, F exactTime = query := by
  have entry (h : (selectInterval (s.payload.cells.get (wrap n ⌊query⌋)) s.payload.seed
      s.payload.frontier ⌊query⌋ query).bind (fun interval =>
        readInterval interval.left interval.rightPhase interval.rightTime query cutoff) = some time) :
      ∃ exactTime ≤ previous.time, F exactTime = query := by
    cases selected : selectInterval (s.payload.cells.get (wrap n ⌊query⌋)) s.payload.seed
        s.payload.frontier ⌊query⌋ query with
    | none => simp [selected] at h
    | some interval =>
      have lookup : readInterval interval.left interval.rightPhase interval.rightTime query cutoff =
          some time := by simpa only [selected, Option.bind] using h
      obtain ⟨leftExact, leftPast⟩ := history.selected_left_exact positive latest retained selected
      obtain ⟨_, newest, _, actual, _, _, cursor⟩ := retained
      rw [latest, Option.some.injEq] at actual
      subst newest
      have bounds : query ∈ Set.Icc (F interval.left.time) (F previous.time) := by
        rw [leftExact, history.previous_exact (show previous ∈ s.previous by simp [latest])]
        exact ⟨readInterval_above_entry _ _ _ lookup, cursor⟩
      obtain ⟨exactTime, within, exactPhase⟩ :=
        intermediate_value_Icc leftPast continuous.continuousOn bounds
      exact ⟨exactTime, within.2, exactPhase⟩
  have exactFloor (a : ℝ) : Arithmetic.floor a = ⌊a⌋ := rfl
  cases frontier : s.payload.frontier with
  | none => exact entry (by simpa [read, readCell, exactFloor, frontier] using found)
  | some edge =>
    by_cases here : query = edge.phase
    · subst query
      obtain ⟨past, _, exactPhase⟩ := read_frontier_sound history latest frontier found
      exact ⟨time, past, exactPhase⟩
    · exact entry (by simpa [read, readCell, exactFloor, frontier, here] using found)


end
end PhaseTime.InverseRuntime

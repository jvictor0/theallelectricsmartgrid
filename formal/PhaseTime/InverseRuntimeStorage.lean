import PhaseTime.InverseRuntime

namespace PhaseTime.InverseRuntime

theorem wrap_injective_on_retained_window (n : Nat) (highest i j : Int)
    (hi₀ : highest - (n : Int) ≤ i) (hi₁ : i ≤ highest)
    (hj₀ : highest - (n : Int) ≤ j) (hj₁ : j ≤ highest)
    (hslot : wrap n i = wrap n j) : i = j := by
  have hmod : i % ((n : Int) + 1) = j % ((n : Int) + 1) := by
    have hv := congrArg Fin.val hslot
    change (i % ((n : Int) + 1)).toNat = (j % ((n : Int) + 1)).toNat at hv
    have hii := Int.emod_nonneg i (by omega : (n : Int) + 1 ≠ 0)
    have hjj := Int.emod_nonneg j (by omega : (n : Int) + 1 ≠ 0)
    omega
  have hd : (i - j) % ((n : Int) + 1) = 0 := by
    rw [Int.sub_emod, hmod, Int.sub_self, Int.zero_emod]
  rcases Int.le_total j i with h | h
  · have hd' : (i - j) % ((n : Int) + 1) = i - j :=
      Int.emod_eq_of_lt (by omega) (by omega)
    omega
  · have hd : (j - i) % ((n : Int) + 1) = 0 := by
      rw [Int.sub_emod, hmod, Int.sub_self, Int.zero_emod]
    have hd' : (j - i) % ((n : Int) + 1) = j - i :=
      Int.emod_eq_of_lt (by omega) (by omega)
    omega

@[local simp] private theorem get_set_same {A : Type} {n : Nat}
    (cells : Vector A n) (slot : Fin n) (cell : A) : (cells.set slot.val cell).get slot = cell :=
  Vector.getElem_set_self slot.isLt

@[local simp] private theorem get_set_other {A : Type} {n : Nat}
    (cells : Vector A n) (written queried : Fin n) (cell : A) (different : written ≠ queried) :
    (cells.set written.val cell).get queried = cells.get queried :=
  Vector.getElem_set_ne written.isLt queried.isLt (fun h => different (Fin.ext h))

variable {R : Type} [arithmetic : Arithmetic R] {n : Nat}

private theorem assignCell_slot_self (s : Published R arithmetic.Delta n)
    (index : Int) (time : R) (stop : Option R) :
    (assignCell s index time stop).cells.get (wrap n index) = ⟨some time, stop, none⟩ := by
  simp [assignCell]

private theorem assignCell_slot_other (s : Published R arithmetic.Delta n)
    (index : Int) (time : R) (stop : Option R) (slot : Fin (n + 1))
    (h : slot ≠ wrap n index) :
    (assignCell s index time stop).cells.get slot = s.cells.get slot := by
  simp [assignCell, Ne.symm h]

private theorem seedCell_slot_self (s : Published R arithmetic.Delta n) (edge : Point R) :
    (seedCell s edge).cells.get (wrap n (arithmetic.floor edge.phase)) =
      ⟨some edge.time, none, none⟩ := by
  unfold seedCell
  exact assignCell_slot_self _ _ _ _

private theorem seedCell_slot_other (s : Published R arithmetic.Delta n) (edge : Point R)
    (slot : Fin (n + 1)) (h : slot ≠ wrap n (arithmetic.floor edge.phase)) :
    (seedCell s edge).cells.get slot =
      if s.seed.any (fun phase => wrap n (arithmetic.floor phase) == slot) then {}
      else s.cells.get slot := by
  unfold seedCell
  rw [assignCell_slot_other _ _ _ _ _ h]
  cases s.seed with
  | none => rfl
  | some p =>
    by_cases hs : wrap n (arithmetic.floor p) = slot
    · simp [hs]
    · simp [hs]

private theorem finishCell_base (s : Published R arithmetic.Delta n)
    (index : Int) (time : R) (slot : Fin (n + 1)) :
    ((finishCell s index time).cells.get slot).base = (s.cells.get slot).base := by
  unfold finishCell
  by_cases h : slot = wrap n index
  · subst slot
    simp only [get_set_same]
    split <;> rfl
  · simp [Ne.symm h]

/-- A proof-only map from physical slots to the logical indices that supplied their bases. -/
abbrev SlotOwners (n : Nat) := Fin (n + 1) → Option Int

private def ownerAssign (owners : SlotOwners n) (index : Int) : SlotOwners n :=
  fun slot => if slot = wrap n index then some index else owners slot

def OwnersMatchBases (s : Published R arithmetic.Delta n) (owners : SlotOwners n) : Prop :=
  ∀ slot, (owners slot).isSome = (s.cells.get slot).base.isSome

private theorem ownersMatchBases_assignCell (s : Published R arithmetic.Delta n)
    (owners : SlotOwners n) (matchBases : OwnersMatchBases s owners)
    (index : Int) (time : R) (stop : Option R) :
    OwnersMatchBases (assignCell s index time stop) (ownerAssign owners index) := by
  intro slot
  by_cases h : slot = wrap n index
  · subst slot
    simp [ownerAssign, assignCell_slot_self]
  · simp [ownerAssign, h, assignCell_slot_other _ _ _ _ _ h, matchBases slot]

private def ownerSeed (owners : SlotOwners n) (seed : Option R) (index : Int) : SlotOwners n :=
  ownerAssign (fun slot =>
    if seed.any (fun p => wrap n (arithmetic.floor p) == slot) then none else owners slot) index

private theorem ownersMatchBases_seedCell (s : Published R arithmetic.Delta n)
    (owners : SlotOwners n) (matchBases : OwnersMatchBases s owners) (edge : Point R) :
    OwnersMatchBases (seedCell s edge) (ownerSeed owners s.seed (arithmetic.floor edge.phase)) := by
  intro slot
  by_cases h : slot = wrap n (arithmetic.floor edge.phase)
  · subst slot
    simp [ownerSeed, ownerAssign, seedCell_slot_self]
  · simp only [ownerSeed, ownerAssign, if_neg h, seedCell_slot_other s edge slot h]
    split
    · rfl
    · exact matchBases slot

private theorem ownersMatchBases_finishCell (s : Published R arithmetic.Delta n)
    (owners : SlotOwners n) (matchBases : OwnersMatchBases s owners)
    (index : Int) (time : R) :
    OwnersMatchBases (finishCell s index time) owners := by
  intro slot
  rw [finishCell_base]
  exact matchBases slot

/-- Every occupied slot reachable by a retained lookup has its expected logical owner. -/
def RetainedOwners (owners : SlotOwners n) (highest frontier : Int) : Prop :=
  ∀ i, highest - (n : Int) ≤ i → i ≤ frontier →
    ∀ owner ∈ owners (wrap n i), owner = i

private theorem retainedOwners_ownerAssign (owners : SlotOwners n) (highest index : Int)
    (hmax : index ≤ highest)
    (hold : RetainedOwners owners highest (index - 1)) :
    RetainedOwners (ownerAssign owners index) highest index := by
  intro i hlo hhi owner howner
  by_cases hs : wrap n i = wrap n index
  · have heq := wrap_injective_on_retained_window n highest i index hlo (by omega)
      (by omega) hmax hs
    exact (by simpa [ownerAssign, hs, heq] using howner : index = owner).symm.trans heq.symm
  · have hlt : i ≤ index - 1 := by
      have hne : i ≠ index := fun heq => hs (congrArg (wrap n) heq)
      omega
    exact hold i hlo hlt owner (by simpa [ownerAssign, hs] using howner)

private def tailOwners (owners : SlotOwners n) (index : Int) : Nat → SlotOwners n
  | 0 => ownerAssign owners index
  | count + 1 => tailOwners (ownerAssign owners index) (index + 1) count

private theorem ownersMatchBases_commitTail (owners : SlotOwners n) (count : Nat)
    (s : Published R arithmetic.Delta n) (matchBases : OwnersMatchBases s owners)
    (left right edge : Point R) (index : Int) :
    OwnersMatchBases (commitTail s left right edge index count) (tailOwners owners index count) := by
  induction count generalizing owners s edge index with
  | zero => exact ownersMatchBases_assignCell s owners matchBases index edge.time none
  | succ count ih =>
    exact ih _ _ (ownersMatchBases_assignCell s owners matchBases _ _ _) _ _

private theorem retainedOwners_tailOwners (owners : SlotOwners n) (count : Nat) (highest index : Int)
    (hmax : index + (count : Int) ≤ highest)
    (hold : RetainedOwners owners highest (index - 1)) :
    RetainedOwners (tailOwners owners index count) highest (index + (count : Int)) := by
  induction count generalizing owners index with
  | zero => simpa [tailOwners] using retainedOwners_ownerAssign owners highest index (by omega) hold
  | succ count ih =>
    rw [tailOwners]
    have hnext : index + 1 + (count : Int) = index + (↑(count + 1) : Int) := by omega
    rw [← hnext]
    apply ih _ _ (by omega)
    rw [Int.add_sub_cancel]
    exact retainedOwners_ownerAssign owners highest index (by omega) hold

private theorem ownersMatchBases_commitBatch (s : Published R arithmetic.Delta n)
    (owners : SlotOwners n) (matchBases : OwnersMatchBases s owners)
    (left right : Point R) (first : Int) (count : Nat) :
    OwnersMatchBases (commitBatch s left right first count) (tailOwners owners first count) := by
  unfold commitBatch
  dsimp only
  split
  · exact ownersMatchBases_commitTail owners count _
      (ownersMatchBases_finishCell s owners matchBases _ _) _ _ _ _
  · exact ownersMatchBases_commitTail owners count s matchBases _ _ _ _

theorem scatter_retained_owners (remaining : Nat) (s : Published R arithmetic.Delta n)
    (owners : SlotOwners n) (left right : Point R) (first highest : Int)
    (hmax : first + (remaining : Int) - 1 ≤ highest)
    (matchBases : OwnersMatchBases s owners)
    (hold : RetainedOwners owners highest (first - 1)) :
    ∃ updated, OwnersMatchBases (scatter s left right first remaining) updated ∧
      RetainedOwners updated highest (first + (remaining : Int) - 1) := by
  rw [scatter]
  split
  · rename_i positive
    let count := min batchCapacity remaining
    have hcount : count ≤ remaining := Nat.min_le_right _ _
    have hpositive : 0 < count := by
      dsimp [count, batchCapacity]
      omega
    have hend : first + ((count - 1 : Nat) : Int) = first + (count : Int) - 1 := by omega
    have hfinish : first + (count : Int) + ((remaining - count : Nat) : Int) - 1 =
        first + (remaining : Int) - 1 := by omega
    have hprefix := retainedOwners_tailOwners owners (count - 1) highest first (by omega) hold
    rw [hend] at hprefix
    rw [← hfinish]
    exact scatter_retained_owners (remaining - count) (commitBatch s left right first (count - 1))
      (tailOwners owners first (count - 1)) left right (first + (count : Int)) highest
      (by omega) (ownersMatchBases_commitBatch s owners matchBases left right first (count - 1)) hprefix
  · rename_i empty
    have hzero : remaining = 0 := by omega
    subst remaining
    exact ⟨owners, matchBases, by simpa using hold⟩
termination_by remaining
decreasing_by omega

private theorem retainedOwners_ownerSeed (s : Published R arithmetic.Delta n) (owners : SlotOwners n)
    (highest index : Int) (hmax : index ≤ highest)
    (hold : RetainedOwners owners highest index) :
    RetainedOwners (ownerSeed owners s.seed index) highest index := by
  intro i hlo hhi owner howner
  by_cases hs : wrap n i = wrap n index
  · have heq := wrap_injective_on_retained_window n highest i index hlo (by omega)
      (by omega) hmax hs
    exact (by simpa [ownerSeed, ownerAssign, hs, heq] using howner : index = owner).symm.trans heq.symm
  · simp only [ownerSeed, ownerAssign, if_neg hs] at howner
    split at howner
    · nomatch howner
    · exact hold i hlo hhi owner howner

def freshCell (left right : Point R) (i : Int) (completed : Bool) : Cell R arithmetic.Delta :=
  ⟨some (edgeAt left right i).time,
    if completed then some (edgeAt left right (i + 1)).time else none, none⟩

/-- All retained cells in a newly crossed prefix have affine endpoints, with the final cell open. -/
def FreshThrough (s : Published R arithmetic.Delta n) (left right : Point R)
    (highest start last : Int) : Prop :=
  ∀ i, highest - (n : Int) ≤ i → start ≤ i → i ≤ last →
    s.cells.get (wrap n i) = freshCell left right i (decide (i < last)) ∧
      s.seed.any (fun p => wrap n (arithmetic.floor p) == wrap n i) = false

private def FreshBefore (s : Published R arithmetic.Delta n) (left right : Point R)
    (highest start next : Int) : Prop :=
  ∀ i, highest - (n : Int) ≤ i → start ≤ i → i < next →
    s.cells.get (wrap n i) = freshCell left right i true ∧
      s.seed.any (fun p => wrap n (arithmetic.floor p) == wrap n i) = false

private theorem noSeedAfterAssign (seed : Option R) (written queried : Fin (n + 1))
    (h : queried = written ∨
      seed.any (fun p => wrap n (arithmetic.floor p) == queried) = false) :
    (retireSeed seed written).any (fun p => wrap n (arithmetic.floor p) == queried) = false := by
  cases seed with
  | none => rfl
  | some p =>
    by_cases heq : wrap n (arithmetic.floor p) = written
    · simp [retireSeed, heq]
    · simp [retireSeed, heq]
      rcases h with h | h
      · exact fun he => heq (he.trans h)
      · simpa using h

private theorem freshBefore_assignComplete (s : Published R arithmetic.Delta n) (left right : Point R)
    (highest start index : Int) (hmax : index ≤ highest)
    (hold : FreshBefore s left right highest start index) :
    FreshBefore (assignCell s index (edgeAt left right index).time
      (some (edgeAt left right (index+1)).time)) left right highest start (index+1) := by
  intro i hlo hstart hi
  by_cases hi' : i = index
  · subst i
    exact ⟨assignCell_slot_self _ _ _ _, noSeedAfterAssign _ _ _ (Or.inl rfl)⟩
  · have hi'' : i < index := by omega
    have hslot : wrap n i ≠ wrap n index := fun same => hi'
      (wrap_injective_on_retained_window n highest i index hlo (by omega)
        (by omega) hmax same)
    obtain ⟨hcell, hseed⟩ := hold i hlo hstart hi''
    exact ⟨(assignCell_slot_other _ _ _ _ _ hslot).trans hcell,
      noSeedAfterAssign _ _ _ (Or.inr hseed)⟩

private theorem freshThrough_assignOpen (s : Published R arithmetic.Delta n) (left right : Point R)
    (highest start index : Int) (hmax : index ≤ highest)
    (hold : FreshBefore s left right highest start index) :
    FreshThrough (assignCell s index (edgeAt left right index).time none)
      left right highest start index := by
  intro i hlo hstart hi
  by_cases hi' : i = index
  · subst i
    exact ⟨by simpa [freshCell] using assignCell_slot_self s index _ none,
      noSeedAfterAssign _ _ _ (Or.inl rfl)⟩
  · have hi'' : i < index := by omega
    have hslot : wrap n i ≠ wrap n index := fun same => hi'
      (wrap_injective_on_retained_window n highest i index hlo (by omega)
        (by omega) hmax same)
    obtain ⟨hcell, hseed⟩ := hold i hlo hstart hi''
    refine ⟨?_, noSeedAfterAssign _ _ _ (Or.inr hseed)⟩
    simpa [hi''] using (assignCell_slot_other _ _ _ _ _ hslot).trans hcell

private theorem freshThrough_commitTail (s : Published R arithmetic.Delta n) (left right : Point R)
    (highest start index : Int) (count : Nat) (hmax : index + (count : Int) ≤ highest)
    (hold : FreshBefore s left right highest start index) :
    FreshThrough (commitTail s left right (edgeAt left right index) index count)
      left right highest start (index + (count : Int)) := by
  induction count generalizing s index with
  | zero => simpa [commitTail, FreshThrough] using
      freshThrough_assignOpen s left right highest start index (by omega) hold
  | succ count ih =>
    have hnext : index + 1 + (count : Int) = index + (↑(count + 1) : Int) := by omega
    rw [commitTail, ← hnext]
    exact ih _ _ (by omega) (freshBefore_assignComplete s left right highest start index (by omega) hold)

private theorem freshBefore_finishCell (s : Published R arithmetic.Delta n) (left right : Point R)
    (highest start next : Int) (hmax : next - 1 ≤ highest)
    (hold : FreshThrough s left right highest start (next - 1)) :
    FreshBefore (finishCell s (next-1) (edgeAt left right next).time)
      left right highest start next := by
  intro i hlo hstart hi
  obtain ⟨hcell, hseed⟩ := hold i hlo hstart (by omega)
  refine ⟨?_, hseed⟩
  by_cases heq : i = next - 1
  · subst i
    simp at hcell
    unfold finishCell
    rw [get_set_same, hcell]
    have hs : next - 1 + 1 = next := by omega
    simp [freshCell, hs]
  · have hslot : wrap n (next - 1) ≠ wrap n i := fun same => heq
      (wrap_injective_on_retained_window n highest i (next - 1) hlo (by omega)
        (by omega) hmax same.symm)
    have hlt : i < next - 1 := by omega
    simpa [finishCell, hslot, hlt] using hcell

private theorem freshThrough_commitBatch (s : Published R arithmetic.Delta n) (left right : Point R)
    (highest start next : Int) (count : Nat)
    (hmax : next + (count : Int) ≤ highest)
    (hstrict : start < next → arithmetic.lt left.phase (edgeAt left right next).phase = true)
    (hold : FreshThrough s left right highest start (next-1)) :
    FreshThrough (commitBatch s left right next count)
      left right highest start (next + (count : Int)) := by
  unfold commitBatch
  dsimp only
  split
  · exact freshThrough_commitTail _ left right highest start next count hmax
      (freshBefore_finishCell s left right highest start next (by omega) hold)
  · rename_i hnon
    exact freshThrough_commitTail s left right highest start next count hmax
      (by intro i _ hi hn; exact False.elim (hnon (hstrict (by omega))))

private theorem freshThrough_scatter (remaining : Nat) (s : Published R arithmetic.Delta n)
    (left right : Point R) (highest start next : Int)
    (hmax : next + (remaining : Int) - 1 ≤ highest)
    (hstrict : ∀ j, start < j → arithmetic.lt left.phase (edgeAt left right j).phase = true)
    (hold : FreshThrough s left right highest start (next - 1)) :
    FreshThrough (scatter s left right next remaining)
      left right highest start (next + (remaining : Int) - 1) := by
  rw [scatter]
  split
  · rename_i positive
    let count := min batchCapacity remaining
    have hcount : count ≤ remaining := Nat.min_le_right _ _
    have hpositive : 0 < count := by
      dsimp [count, batchCapacity]
      omega
    have hend : next + ((count - 1 : Nat) : Int) = next + (count : Int) - 1 := by omega
    have hfinish : next + (count : Int) + ((remaining - count : Nat) : Int) - 1 =
        next + (remaining : Int) - 1 := by omega
    have hprefix := freshThrough_commitBatch s left right highest start next (count - 1)
      (by omega) (hstrict next) hold
    rw [hend] at hprefix
    rw [← hfinish]
    exact freshThrough_scatter (remaining - count) (commitBatch s left right next (count - 1))
      left right highest start (next + (count : Int)) (by omega) hstrict hprefix
  · have hzero : remaining = 0 := by omega
    simpa [hzero] using hold
termination_by remaining
decreasing_by omega

theorem scatter_fresh_cells (remaining : Nat) (s : Published R arithmetic.Delta n)
    (left right : Point R) (highest first : Int)
    (hmax : first + (remaining : Int) - 1 ≤ highest)
    (hstrict : ∀ j, first < j → arithmetic.lt left.phase (edgeAt left right j).phase = true) :
    FreshThrough (scatter s left right first remaining)
      left right highest first (first + (remaining : Int) - 1) :=
  freshThrough_scatter remaining s left right highest first first hmax hstrict
    (by intro i _ hlo hhi; omega)

theorem beginBranch_retained_owners (s : State R arithmetic.Delta n)
    (owners : SlotOwners n) (edge : Point R) (highest : Int)
    (hmax : arithmetic.floor edge.phase ≤ highest)
    (matchBases : OwnersMatchBases s.payload owners)
    (hold : RetainedOwners owners highest (arithmetic.floor edge.phase)) :
    ∃ updated, OwnersMatchBases (beginBranch s edge).payload updated ∧
      RetainedOwners updated highest (arithmetic.floor edge.phase) := by
  unfold beginBranch
  dsimp [Id.run, Pure.pure]
  split
  · exact ⟨owners, matchBases, hold⟩
  · split
    · exact ⟨_, ownersMatchBases_seedCell s.payload owners matchBases edge,
        retainedOwners_ownerSeed s.payload owners highest _ hmax hold⟩
    · split
      · refine ⟨owners, ?_, hold⟩
        intro slot
        have hh := matchBases slot
        split <;> (try split) <;> (try split) <;> (try dsimp only)
        all_goals
          by_cases hs : wrap n (arithmetic.floor edge.phase) = slot
          · subst slot
            rw [get_set_same]
            exact matchBases _
          · rw [get_set_other _ _ _ _ hs]
            exact hh
      · exact ⟨owners, matchBases, hold⟩

/-- Only a seed in the queried physical slot can affect that slot's lookup. -/
def seedAtSlot (s : Published R arithmetic.Delta n) (slot : Fin (n + 1)) : Option R :=
  s.seed.filter (fun p => wrap n (arithmetic.floor p) == slot)

private theorem seedAtSlot_assignOther (s : Published R arithmetic.Delta n)
    (written : Int) (time : R) (stop : Option R) (slot : Fin (n + 1))
    (different : slot ≠ wrap n written) :
    seedAtSlot (assignCell s written time stop) slot = seedAtSlot s slot := by
  cases hseed : s.seed with
  | none => simp [seedAtSlot, assignCell, retireSeed, hseed]
  | some phase =>
    by_cases hs : wrap n (arithmetic.floor phase) = slot
    · simp [seedAtSlot, assignCell, retireSeed, hseed, Option.filter, hs, different]
    · by_cases hw : wrap n (arithmetic.floor phase) = wrap n written <;>
        simp [seedAtSlot, assignCell, retireSeed, hseed, Option.filter, hs, hw] <;> exact Ne.symm different

private theorem commitTail_preserves_older (s : Published R arithmetic.Delta n)
    (left right edge : Point R) (highest index i : Int) (count : Nat)
    (hmax : index + (count : Int) ≤ highest) (hlo : highest - (n : Int) ≤ i) (hi : i < index) :
    let out := commitTail s left right edge index count
    out.cells.get (wrap n i) = s.cells.get (wrap n i) ∧
      seedAtSlot out (wrap n i) = seedAtSlot s (wrap n i) := by
  induction count generalizing s edge index with
  | zero =>
    have hn : wrap n i ≠ wrap n index := fun same =>
      (Int.ne_of_lt hi) (wrap_injective_on_retained_window n highest i index hlo
        (by omega) (by omega) (by omega) same)
    exact ⟨assignCell_slot_other _ _ _ _ _ hn, seedAtSlot_assignOther s index edge.time none _ hn⟩
  | succ count ih =>
    have hn : wrap n i ≠ wrap n index := fun same =>
      (Int.ne_of_lt hi) (wrap_injective_on_retained_window n highest i index hlo
        (by omega) (by omega) (by omega) same)
    rw [commitTail]
    obtain ⟨hc, hs⟩ := ih (assignCell s index edge.time (some (edgeAt left right (index+1)).time))
      (edgeAt left right (index+1)) (index+1) (by omega) (by omega)
    exact ⟨hc.trans (assignCell_slot_other _ _ _ _ _ hn),
      hs.trans (seedAtSlot_assignOther s index edge.time (some (edgeAt left right (index+1)).time) _ hn)⟩

private theorem commitBatch_preserves_older (s : Published R arithmetic.Delta n)
    (left right : Point R) (highest index i : Int) (count : Nat)
    (hmax : index + (count : Int) ≤ highest) (hlo : highest - (n : Int) ≤ i)
    (hi : i < index - 1) :
    let out := commitBatch s left right index count
    out.cells.get (wrap n i) = s.cells.get (wrap n i) ∧
      seedAtSlot out (wrap n i) = seedAtSlot s (wrap n i) := by
  have hn : wrap n (index - 1) ≠ wrap n i := fun same =>
    (Int.ne_of_lt hi) (wrap_injective_on_retained_window n highest i (index - 1) hlo
      (by omega) (by omega) (by omega) same.symm)
  unfold commitBatch
  dsimp only
  split
  · obtain ⟨hc, hs⟩ := commitTail_preserves_older _ left right _ highest index i count hmax hlo (by omega)
    exact ⟨hc.trans (by simp [finishCell, hn]), hs⟩
  · exact commitTail_preserves_older s left right _ highest index i count hmax hlo (by omega)

/-- Scatter cannot mutate a retained cell strictly before its first predecessor. -/
theorem scatter_preserves_older (remaining : Nat) (s : Published R arithmetic.Delta n)
    (left right : Point R) (highest first i : Int)
    (hmax : first + (remaining : Int) - 1 ≤ highest)
    (hlo : highest - (n : Int) ≤ i) (hi : i < first - 1) :
    let out := scatter s left right first remaining
    out.cells.get (wrap n i) = s.cells.get (wrap n i) ∧
      seedAtSlot out (wrap n i) = seedAtSlot s (wrap n i) := by
  dsimp only
  rw [scatter]
  split
  · rename_i positive
    let count := min batchCapacity remaining
    have hcount : count ≤ remaining := Nat.min_le_right _ _
    have hpositive : 0 < count := by
      dsimp [count, batchCapacity]
      omega
    have hfinish : first + (count : Int) + ((remaining - count : Nat) : Int) - 1 =
        first + (remaining : Int) - 1 := by omega
    obtain ⟨hc, hs⟩ := commitBatch_preserves_older s left right highest first i (count - 1)
      (by omega) hlo hi
    obtain ⟨hc', hs'⟩ := scatter_preserves_older (remaining - count)
      (commitBatch s left right first (count - 1)) left right highest
      (first + (count : Int)) i (hfinish ▸ hmax) hlo (by omega)
    exact ⟨hc'.trans hc, hs'.trans hs⟩
  · exact ⟨rfl, rfl⟩
termination_by remaining
decreasing_by omega

/-- The initial predecessor sees precisely the conditional finish used by the first batch. -/
theorem scatter_first_predecessor (remaining : Nat) (s : Published R arithmetic.Delta n)
    (left right : Point R) (highest first : Int)
    (hmax : first + (remaining : Int) - 1 ≤ highest)
    (hlo : highest - (n : Int) ≤ first - 1) (nonempty : 0 < remaining) :
    let edge := edgeAt left right first
    let ready := if arithmetic.lt left.phase edge.phase then finishCell s (first - 1) edge.time else s
    let out := scatter s left right first remaining
    out.cells.get (wrap n (first - 1)) = ready.cells.get (wrap n (first - 1)) ∧
      seedAtSlot out (wrap n (first - 1)) = seedAtSlot s (wrap n (first - 1)) := by
  dsimp only
  rw [scatter, dif_pos nonempty]
  dsimp only
  let count := min batchCapacity remaining
  have hc : count ≤ remaining := Nat.min_le_right _ _
  have hp : 0 < count := by
    dsimp [count, batchCapacity]
    omega
  obtain ⟨cell, seed⟩ := scatter_preserves_older (remaining - count)
    (commitBatch s left right first (count - 1)) left right highest
    (first + (count : Int)) (first - 1) (by omega) hlo (by omega)
  have hbatch :
      let edge := edgeAt left right first
      let ready := if arithmetic.lt left.phase edge.phase then finishCell s (first - 1) edge.time else s
      let batch := commitBatch s left right first (count - 1)
      batch.cells.get (wrap n (first - 1)) = ready.cells.get (wrap n (first - 1)) ∧
        seedAtSlot batch (wrap n (first - 1)) = seedAtSlot s (wrap n (first - 1)) := by
    unfold commitBatch
    dsimp only
    split
    · exact commitTail_preserves_older _ left right _ highest first (first-1) (count - 1)
        (by omega) hlo (by omega)
    · exact commitTail_preserves_older s left right _ highest first (first-1) (count - 1)
        (by omega) hlo (by omega)
  exact ⟨cell.trans hbatch.1, seed.trans hbatch.2⟩

/-- Branch creation affects other slots only when it clears the previous seed cell. -/
theorem beginBranch_other_slot (s : State R arithmetic.Delta n) (edge : Point R)
    (slot : Fin (n + 1)) (different : slot ≠ wrap n (arithmetic.floor edge.phase)) :
    let out := (beginBranch s edge).payload
    (out.cells.get slot = s.payload.cells.get slot ∧
      seedAtSlot out slot = seedAtSlot s.payload slot) ∨
      (out.cells.get slot = {} ∧ seedAtSlot out slot = none) := by
  unfold beginBranch
  dsimp [Id.run, Pure.pure]
  split
  · exact Or.inl ⟨rfl, rfl⟩
  · split
    · have hnew : seedAtSlot (seedCell s.payload edge) slot = none := by
        simp [seedAtSlot, seedCell, Option.filter, Ne.symm different]
      have hc := seedCell_slot_other s.payload edge slot different
      change
        (((seedCell s.payload edge).cells.get slot = s.payload.cells.get slot ∧
          seedAtSlot (seedCell s.payload edge) slot = seedAtSlot s.payload slot) ∨
          ((seedCell s.payload edge).cells.get slot = {} ∧ seedAtSlot (seedCell s.payload edge) slot = none))
      rw [hnew, hc]
      cases hseed : s.payload.seed with
      | none => exact Or.inl ⟨rfl, by simp [seedAtSlot, hseed]⟩
      | some p =>
        by_cases h : wrap n (arithmetic.floor p) = slot
        · exact Or.inr ⟨by simp [h], rfl⟩
        · exact Or.inl ⟨by simp [h], by simp [seedAtSlot, Option.filter, hseed, h]⟩
    · split
      · split <;> (try split) <;> (try split) <;> (try dsimp only) <;>
          exact Or.inl ⟨get_set_other _ _ _ _ (Ne.symm different), rfl⟩
      · exact Or.inl ⟨rfl, rfl⟩

/-- Each occupied base comes from its tagged grid edge or its live fractional seed. -/
def TracksAnchors (At : R → R → Prop) (s : Published R arithmetic.Delta n)
    (owners : SlotOwners n) : Prop :=
  ∀ slot, match owners slot, (s.cells.get slot).base with
  | none, none => True
  | some i, some t =>
    wrap n i = slot ∧
      match seedAtSlot s slot with
      | none => At t (arithmetic.ofIndex i)
      | some p => arithmetic.floor p = i ∧ At t p
  | _, _ => False

theorem anchors_empty (At : R → R → Prop) :
    TracksAnchors At ({} : Published R arithmetic.Delta n) (fun _ => none) := by
  intro slot
  simp [Vector.get]

theorem anchors_match_bases (At : R → R → Prop) (s : Published R arithmetic.Delta n) (owners : SlotOwners n)
    (h : TracksAnchors At s owners) : OwnersMatchBases s owners := by
  intro slot
  have hs := h slot
  split at hs <;> simp_all

private theorem seedAtSlot_assignSelf (s : Published R arithmetic.Delta n)
    (i : Int) (time : R) (stop : Option R) :
    seedAtSlot (assignCell s i time stop) (wrap n i) = none := by
  cases hseed : s.seed with
  | none => simp [seedAtSlot, assignCell, retireSeed, hseed]
  | some phase =>
    by_cases hs : wrap n (arithmetic.floor phase) = wrap n i <;>
    simp [seedAtSlot, assignCell, retireSeed, hseed, Option.filter, hs]

private theorem anchors_assignCell (At : R → R → Prop) (s : Published R arithmetic.Delta n)
    (owners : SlotOwners n) (h : TracksAnchors At s owners)
    (i : Int) (base : R) (stop : Option R) (hbase : At base (arithmetic.ofIndex i)) :
    TracksAnchors At (assignCell s i base stop) (ownerAssign owners i) := by
  intro slot
  by_cases hs : slot = wrap n i
  · subst slot
    rw [assignCell_slot_self, seedAtSlot_assignSelf]
    simpa [ownerAssign] using hbase
  · rw [assignCell_slot_other _ _ _ _ _ hs, seedAtSlot_assignOther _ _ _ _ _ hs]
    simpa only [ownerAssign, if_neg hs] using h slot

private theorem anchors_seedCell (At : R → R → Prop) (s : Published R arithmetic.Delta n)
    (owners : SlotOwners n) (h : TracksAnchors At s owners)
    (edge : Point R) (hedge : At edge.time edge.phase) :
    TracksAnchors At (seedCell s edge) (ownerSeed owners s.seed (arithmetic.floor edge.phase)) := by
  intro slot
  by_cases hs : slot = wrap n (arithmetic.floor edge.phase)
  · subst slot
    rw [seedCell_slot_self]
    simpa [ownerSeed, ownerAssign, seedAtSlot, seedCell, Option.filter] using hedge
  · have hnone : seedAtSlot (seedCell s edge) slot = none := by
      simp [seedAtSlot, seedCell, Option.filter, Ne.symm hs]
    rw [seedCell_slot_other s edge slot hs, hnone]
    simp only [ownerSeed, ownerAssign, if_neg hs]
    by_cases hm : s.seed.any (fun p => wrap n (arithmetic.floor p) == slot) = true
    · simp [hm]
    · simp only [if_neg hm]
      have hnone' : seedAtSlot s slot = none := by
        cases he : s.seed with
        | none => simp [seedAtSlot, he]
        | some p => simpa [seedAtSlot, he, Option.filter] using hm
      have hslot := h slot
      rw [hnone'] at hslot
      exact hslot

private theorem anchors_finishCell (At : R → R → Prop) (s : Published R arithmetic.Delta n)
    (owners : SlotOwners n) (h : TracksAnchors At s owners)
    (i : Int) (time : R) : TracksAnchors At (finishCell s i time) owners := by
  intro slot
  rw [finishCell_base]
  exact h slot

theorem beginBranch_retained_anchors (At : R → R → Prop) (s : State R arithmetic.Delta n)
    (owners : SlotOwners n) (edge : Point R) (highest : Int)
    (hmax : arithmetic.floor edge.phase ≤ highest)
    (anchors : TracksAnchors At s.payload owners) (hedge : At edge.time edge.phase)
    (hold : RetainedOwners owners highest (arithmetic.floor edge.phase)) :
    ∃ updated, TracksAnchors At (beginBranch s edge).payload updated ∧
      RetainedOwners updated highest (arithmetic.floor edge.phase) := by
  unfold beginBranch
  dsimp [Id.run, Pure.pure]
  split
  · exact ⟨owners, anchors, hold⟩
  · split
    · exact ⟨_, anchors_seedCell At s.payload owners anchors edge hedge,
        retainedOwners_ownerSeed s.payload owners highest _ hmax hold⟩
    · split
      · refine ⟨owners, ?_, hold⟩
        have payload_ite (p : Prop) [Decidable p] (a b : State R arithmetic.Delta n)
            (heq : a.payload = b.payload) : (if p then a else b).payload = b.payload := by
          split
          · exact heq
          · rfl
        simp only [Id]
        rw [payload_ite] <;> try rfl
        intro slot
        have hh := anchors slot
        by_cases hs : wrap n (arithmetic.floor edge.phase) = slot
        · subst slot
          rw [get_set_same]
          exact anchors _
        · rw [get_set_other _ _ _ _ hs]
          exact hh
      · exact ⟨owners, anchors, hold⟩

private theorem anchors_commitTail (At : R → R → Prop) (owners : SlotOwners n) (count : Nat)
    (s : Published R arithmetic.Delta n) (anchors : TracksAnchors At s owners)
    (left right : Point R) (index : Int)
    (hedges : ∀ i, index ≤ i → i ≤ index + (count : Int) →
      At (edgeAt left right i).time (arithmetic.ofIndex i)) :
    TracksAnchors At (commitTail s left right (edgeAt left right index) index count)
      (tailOwners owners index count) := by
  induction count generalizing owners s index with
  | zero => exact anchors_assignCell At s owners anchors index _ none (hedges index (by omega) (by omega))
  | succ count ih =>
    exact ih _ _ (anchors_assignCell At s owners anchors index _ _ (hedges index (by omega) (by omega)))
      _ (fun i hl hh => hedges i (by omega) (by omega))

private theorem anchors_commitBatch (At : R → R → Prop) (s : Published R arithmetic.Delta n)
    (owners : SlotOwners n) (anchors : TracksAnchors At s owners)
    (left right : Point R) (first : Int) (count : Nat)
    (hedges : ∀ i, first ≤ i → i ≤ first + (count : Int) →
      At (edgeAt left right i).time (arithmetic.ofIndex i)) :
    TracksAnchors At (commitBatch s left right first count) (tailOwners owners first count) := by
  unfold commitBatch
  dsimp only
  split
  · exact anchors_commitTail At owners count _ (anchors_finishCell At s owners anchors _ _) _ _ _ hedges
  · exact anchors_commitTail At owners count s anchors _ _ _ hedges

theorem scatter_retained_anchors (At : R → R → Prop) (remaining : Nat) (s : Published R arithmetic.Delta n)
    (owners : SlotOwners n) (left right : Point R) (first highest : Int)
    (hmax : first + (remaining : Int) - 1 ≤ highest)
    (anchors : TracksAnchors At s owners)
    (hold : RetainedOwners owners highest (first - 1))
    (hedges : ∀ i, first ≤ i → i ≤ first + (remaining : Int) - 1 →
      At (edgeAt left right i).time (arithmetic.ofIndex i)) :
    ∃ updated, TracksAnchors At (scatter s left right first remaining) updated ∧
      RetainedOwners updated highest (first + (remaining : Int) - 1) := by
  rw [scatter]
  split
  · rename_i positive
    let count := min batchCapacity remaining
    have hcount : count ≤ remaining := Nat.min_le_right _ _
    have hpositive : 0 < count := by
      dsimp [count, batchCapacity]
      omega
    have hend : first + ((count - 1 : Nat) : Int) = first + (count : Int) - 1 := by omega
    have hfinish : first + (count : Int) + ((remaining - count : Nat) : Int) - 1 =
        first + (remaining : Int) - 1 := by omega
    have hprefix := retainedOwners_tailOwners owners (count - 1) highest first (by omega) hold
    rw [hend] at hprefix
    rw [← hfinish]
    exact scatter_retained_anchors At (remaining - count) (commitBatch s left right first (count - 1))
      (tailOwners owners first (count - 1)) left right (first + (count : Int)) highest
      (by omega) (anchors_commitBatch At s owners anchors left right first (count - 1)
        (fun i hl hh => hedges i hl (by omega))) hprefix (fun i hl hh => hedges i (by omega) (by omega))
  · have hzero : remaining = 0 := by omega
    subst remaining
    exact ⟨owners, anchors, by simpa using hold⟩
termination_by remaining
decreasing_by omega

/-- Every active replacement cut decodes to a timestamp on that phase. -/
def TracksCuts (At : R → R → Prop) (s : Published R arithmetic.Delta n) : Prop :=
  ∀ slot base, (s.cells.get slot).base = some base →
    ∀ branch, (s.cells.get slot).replacement = some branch →
      ∃ t, arithmetic.decode base branch.start = some t ∧ At t branch.cut

theorem cuts_empty (At : R → R → Prop) :
    TracksCuts At ({} : Published R arithmetic.Delta n) := by
  intro slot base hb
  simp [Vector.get] at hb

private theorem cuts_assignCell (At : R → R → Prop) (s : Published R arithmetic.Delta n)
    (h : TracksCuts At s) (i : Int) (t : R) (stop : Option R) :
    TracksCuts At (assignCell s i t stop) := by
  intro slot base hb branch hc
  by_cases hs : slot = wrap n i
  · subst slot
    rw [assignCell_slot_self] at hc
    cases hc
  · rw [assignCell_slot_other _ _ _ _ _ hs] at hb hc
    exact h slot base hb branch hc

private theorem cuts_seedCell (At : R → R → Prop) (s : Published R arithmetic.Delta n)
    (h : TracksCuts At s) (edge : Point R) : TracksCuts At (seedCell s edge) := by
  intro slot base hb branch hc
  by_cases hs : slot = wrap n (arithmetic.floor edge.phase)
  · subst slot
    rw [seedCell_slot_self] at hc
    cases hc
  · rw [seedCell_slot_other _ _ _ hs] at hb hc
    split at hb
    · cases hb
    · rename_i hs'
      rw [if_neg hs'] at hc
      exact h slot base hb branch hc

private theorem cuts_finishCell (At : R → R → Prop) (s : Published R arithmetic.Delta n)
    (h : TracksCuts At s) (i : Int) (t : R) : TracksCuts At (finishCell s i t) := by
  intro slot base hb branch hc
  rw [finishCell_base] at hb
  unfold finishCell at hc
  by_cases hs : slot = wrap n i
  · subst slot
    rw [get_set_same] at hc
    split at hc
    · exact h _ base hb branch hc
    · rename_i _ old _ hrep
      injection hc with same
      subst branch
      exact h _ base hb old hrep
    · exact h _ base hb branch hc
  · rw [get_set_other _ _ _ _ (Ne.symm hs)] at hc
    exact h _ base hb branch hc

theorem cuts_beginBranch (At : R → R → Prop) (s : State R arithmetic.Delta n)
    (edge : Point R) (h : TracksCuts At s.payload) (hedge : At edge.time edge.phase)
    (roundtrip : ∀ base t, arithmetic.decode base (arithmetic.encode base t) = some t) :
    TracksCuts At (beginBranch s edge).payload := by
  unfold beginBranch
  dsimp [Id.run, Pure.pure]
  split
  · exact h
  · split
    · exact cuts_seedCell At s.payload h edge
    · split
      · rename_i base stop hpair
        have payload_ite (p : Prop) [Decidable p] (a b : State R arithmetic.Delta n)
            (heq : a.payload = b.payload) : (if p then a else b).payload = b.payload := by
          split
          · exact heq
          · rfl
        simp only [Id]
        rw [payload_ite] <;> try rfl
        intro slot b hb branch hc
        by_cases hs : wrap n (arithmetic.floor edge.phase) = slot
        · subst slot
          rw [get_set_same] at hb hc
          have hb' : (s.payload.cells.get (wrap n (arithmetic.floor edge.phase))).base = some base :=
            congrArg Prod.fst hpair
          change (s.payload.cells.get (wrap n (arithmetic.floor edge.phase))).base = some b at hb
          rw [hb'] at hb
          injection hb with baseEq
          subst b
          injection hc with hc
          split at hc
          · subst branch
            exact ⟨_, roundtrip _ _, hedge⟩
          · rename_i old hrep
            split at hc
            · subst branch
              exact h _ base hb' old hrep
            · subst branch
              exact ⟨_, roundtrip _ _, hedge⟩
        · rw [get_set_other _ _ _ _ hs] at hb hc
          exact h slot b hb branch hc
      · exact h

private theorem cuts_commitTail (At : R → R → Prop) (s : Published R arithmetic.Delta n)
    (h : TracksCuts At s) (left right edge : Point R) (index : Int) (count : Nat) :
    TracksCuts At (commitTail s left right edge index count) := by
  induction count generalizing s edge index with
  | zero => exact cuts_assignCell At s h index edge.time none
  | succ count ih => exact ih _ (cuts_assignCell At s h _ _ _) _ _

private theorem cuts_commitBatch (At : R → R → Prop) (s : Published R arithmetic.Delta n)
    (h : TracksCuts At s) (left right : Point R) (first : Int) (count : Nat) :
    TracksCuts At (commitBatch s left right first count) := by
  unfold commitBatch
  dsimp only
  split
  · exact cuts_commitTail At _ (cuts_finishCell At s h _ _) _ _ _ _ _
  · exact cuts_commitTail At s h _ _ _ _ _

theorem cuts_scatter (At : R → R → Prop) (remaining : Nat) (s : Published R arithmetic.Delta n)
    (h : TracksCuts At s) (left right : Point R) (first : Int) :
    TracksCuts At (scatter s left right first remaining) := by
  rw [scatter]
  split
  · exact cuts_scatter At _ _ (cuts_commitBatch At s h _ _ _ _) _ _ _
  · exact h
termination_by remaining
decreasing_by
  simp only [batchCapacity] at *
  omega

end PhaseTime.InverseRuntime

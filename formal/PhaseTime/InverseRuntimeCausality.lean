import PhaseTime.InverseRuntimeReachability

namespace PhaseTime.InverseRuntime

noncomputable section
open Classical

attribute [local instance] exactArithmetic

@[local simp] theorem causal_lt (a b : ℝ) : Arithmetic.lt a b = decide (a < b) := rfl
@[local simp] theorem causal_le (a b : ℝ) : Arithmetic.le a b = decide (a ≤ b) := rfl
@[local simp] theorem causal_encode (base time : ℝ) : Arithmetic.encode base time = time - base := rfl
@[local simp] theorem causal_decode (base delta : ℝ) :
    Arithmetic.decode base delta = some (base + delta) := rfl
@[local simp] theorem causal_floor (a : ℝ) : Arithmetic.floor a = ⌊a⌋ := rfl
@[local simp] theorem causal_ceil (a : ℝ) : Arithmetic.ceil a = ⌈a⌉ := rfl
@[local simp] theorem causal_index (i : Int) : (Arithmetic.ofIndex i : ℝ) = i := rfl

/-- Each stored timestamp is bounded after decoding replacement deltas from the base. -/
def TimesBounded (now : ℝ) (c : Cell ℝ ℝ) : Prop :=
  (∀ t ∈ c.base, t ≤ now) ∧
  (∀ t ∈ c.fallbackEnd, t ≤ now) ∧
  ∀ base ∈ c.base, ∀ branch ∈ c.replacement,
    base + branch.start ≤ now ∧ ∀ stop ∈ branch.stop, base + stop ≤ now

def StorageBounded {n : Nat} (now : ℝ) (s : Published ℝ ℝ n) : Prop :=
  ∀ slot, TimesBounded now (s.cells.get slot)

private theorem TimesBounded.mono {now later : ℝ} {c : Cell ℝ ℝ} (h : TimesBounded now c)
    (hle : now ≤ later) : TimesBounded later c :=
  ⟨fun _ hm => (h.1 _ hm).trans hle, fun _ hm => (h.2.1 _ hm).trans hle,
    fun base hb branch hr => ⟨(h.2.2 base hb branch hr).1.trans hle,
      fun stop hs => ((h.2.2 base hb branch hr).2 stop hs).trans hle⟩⟩

@[local simp] theorem bounded_get_set_same {A : Type} {n : Nat}
    (cells : Vector A n) (slot : Fin n) (cell : A) : (cells.set slot.val cell).get slot = cell :=
  Vector.getElem_set_self slot.isLt

@[local simp] theorem bounded_get_set_other {A : Type} {n : Nat}
    (cells : Vector A n) (written queried : Fin n) (cell : A) (different : written ≠ queried) :
    (cells.set written.val cell).get queried = cells.get queried :=
  Vector.getElem_set_ne written.isLt queried.isLt (fun h => different (Fin.ext h))

private theorem bounded_set {n : Nat} {now : ℝ} {s : Published ℝ ℝ n}
    (h : StorageBounded now s) (slot : Fin (n + 1)) (cell : Cell ℝ ℝ)
    (hc : TimesBounded now cell) :
    StorageBounded now { s with cells := s.cells.set slot.val cell } := by
  intro i
  by_cases he : slot = i
  · subst i
    simpa using hc
  · simpa [he] using h i

private theorem assignCell_bounded {n : Nat} {now : ℝ} {s : Published ℝ ℝ n}
    (h : StorageBounded now s) (index : Int) (time : ℝ) (stop : Option ℝ)
    (ht : time ≤ now) (hs : ∀ t ∈ stop, t ≤ now) :
    StorageBounded now (assignCell s index time stop) := by
  exact bounded_set h _ _ (by simpa [TimesBounded, ht] using hs)

private theorem seedCell_bounded {n : Nat} {now : ℝ} {s : Published ℝ ℝ n}
    (h : StorageBounded now s) (edge : Point ℝ) (ht : edge.time ≤ now) :
    StorageBounded now (seedCell s edge) := by
  unfold seedCell
  apply assignCell_bounded _ _ _ _ ht (by simp)
  split
  · exact bounded_set h _ _ (by simp [TimesBounded])
  · exact h

private theorem finishCell_bounded {n : Nat} {now : ℝ} {s : Published ℝ ℝ n}
    (h : StorageBounded now s) (index : Int) (time : ℝ) (ht : time ≤ now) :
    StorageBounded now (finishCell s index time) := by
  unfold finishCell
  apply bounded_set h
  have hc := h (wrap n index)
  generalize s.cells.get (wrap n index) = cell at hc ⊢
  rcases cell with ⟨base, fallback, branch⟩
  cases base <;> cases fallback <;> cases branch <;>
    simp_all [TimesBounded, add_sub_cancel]

private theorem beginBranch_bounded {n : Nat} {now : ℝ} {s : State ℝ ℝ n}
    (h : StorageBounded now s.payload) (edge : Point ℝ) (ht : edge.time ≤ now) :
    StorageBounded now (beginBranch s edge).payload := by
  unfold beginBranch
  simp only [Id.run, pure]
  split
  · exact h
  · split
    · exact seedCell_bounded h edge ht
    · split
      next _ base oldTime pair =>
        have hb := congrArg Prod.fst pair
        simp only at hb
        have hc := h (wrap n (Arithmetic.floor edge.phase))
        have updated (branch : Replacement ℝ ℝ)
            (hs : base + branch.start ≤ now) (hstop : ∀ t ∈ branch.stop, base + t ≤ now) :
            TimesBounded now
              { (s.payload.cells.get (wrap n (Arithmetic.floor edge.phase))) with
                replacement := some branch } := by
          refine ⟨hc.1, hc.2.1, ?_⟩
          simp only [hb, Option.mem_some_iff]
          intro _ rfl _ rfl
          exact ⟨hs, hstop⟩
        have fresh : TimesBounded now
            { (s.payload.cells.get (wrap n (Arithmetic.floor edge.phase))) with
              replacement := some ⟨edge.phase, Arithmetic.encode base edge.time, none⟩ } :=
          updated _ (by simpa) (by simp)
        split
        · split <;> exact bounded_set h _ _ fresh
        next _ old hold =>
          have kept : TimesBounded now
              { (s.payload.cells.get (wrap n (Arithmetic.floor edge.phase))) with
                replacement := some { old with stop := none } } :=
            updated _ (hc.2.2 base hb old hold).1 (by simp)
          repeat first | exact bounded_set h _ _ kept | exact bounded_set h _ _ fresh | split
      · exact h

private theorem interpolate_bounded (left right : Point ℝ) (q now : ℝ)
    (hl : left.time ≤ now) (hr : right.time ≤ now)
    (phase : left.phase < right.phase) (lower : left.phase ≤ q) (upper : q ≤ right.phase) :
    interpolate left right q ≤ now := by
  unfold interpolate
  change left.time + (q - left.phase) / (right.phase - left.phase) * (right.time - left.time) ≤ now
  let a := (q - left.phase) / (right.phase - left.phase)
  have width := sub_pos.mpr phase
  have a0 : 0 ≤ a := div_nonneg (sub_nonneg.mpr lower) width.le
  have a1 : a ≤ 1 := (div_le_iff₀ width).2 (by linarith)
  change left.time + a * (right.time - left.time) ≤ now
  nlinarith [mul_nonneg a0 (sub_nonneg.mpr hr),
    mul_nonneg (sub_nonneg.mpr a1) (sub_nonneg.mpr hl)]

private theorem edgeAt_bounded (left right : Point ℝ) (index : Int) (now : ℝ)
    (hl : left.time ≤ now) (hr : right.time ≤ now)
    (phase : left.phase < right.phase) (first : ⌈left.phase⌉ ≤ index)
    (last : index ≤ ⌊right.phase⌋) : (edgeAt left right index).time ≤ now := by
  change interpolate left right (index : ℝ) ≤ now
  exact interpolate_bounded left right _ now hl hr phase
    (Int.ceil_le.mp first) (Int.le_floor.mp last)

private theorem commitTail_bounded {n : Nat} {now : ℝ} {s : Published ℝ ℝ n}
    (h : StorageBounded now s) (left right edge : Point ℝ) (index : Int) (count : Nat)
    (ht : edge.time ≤ now)
    (remaining : ∀ i : Int, index < i → i ≤ index + (count : Int) →
      (edgeAt left right i).time ≤ now) :
    StorageBounded now (commitTail s left right edge index count) := by
  induction count generalizing s edge index with
  | zero =>
    exact assignCell_bounded h index edge.time none ht (by simp)
  | succ count ih =>
    have next : (edgeAt left right (index + 1)).time ≤ now :=
      remaining _ (by omega) (by omega)
    have complete := assignCell_bounded h index edge.time
      (some (edgeAt left right (index + 1)).time) ht (by simpa using next)
    exact ih complete _ _ next (fun i hlow hhigh => remaining i (by omega) (by omega))

private theorem commitBatch_bounded {n : Nat} {now : ℝ} {s : Published ℝ ℝ n}
    (h : StorageBounded now s) (left right : Point ℝ) (index : Int) (count : Nat)
    (remaining : ∀ i : Int, index ≤ i → i ≤ index + (count : Int) →
      (edgeAt left right i).time ≤ now) :
    StorageBounded now (commitBatch s left right index count) := by
  have first := remaining index (by omega) (by omega)
  unfold commitBatch
  dsimp only
  split
  · exact commitTail_bounded (finishCell_bounded h _ _ first) _ _ _ _ _ first
      (fun i hlow => remaining i hlow.le)
  · exact commitTail_bounded h _ _ _ _ _ first (fun i hlow => remaining i hlow.le)

private theorem scatter_bounded {n : Nat} {now : ℝ} {s : Published ℝ ℝ n}
    (h : StorageBounded now s) (left right : Point ℝ) (index : Int) (count : Nat)
    (remaining : ∀ i : Int, index ≤ i → i < index + (count : Int) →
      (edgeAt left right i).time ≤ now) :
    StorageBounded now (scatter s left right index count) := by
  rw [scatter]
  split
  next positive =>
    dsimp only
    apply scatter_bounded
    · apply commitBatch_bounded h
      intro i hlow hhigh
      apply remaining i hlow
      simp only [batchCapacity] at *
      omega
    · intro i hlow hhigh
      change index + (↑(min batchCapacity count) : Int) ≤ i at hlow
      change i < index + (↑(min batchCapacity count) : Int) +
        (↑(count - min batchCapacity count) : Int) at hhigh
      have hmin : min batchCapacity count ≤ count := Nat.min_le_right _ _
      exact remaining i (by omega) (by omega)
  · exact h
termination_by count
decreasing_by simp only [batchCapacity] at *; omega

theorem write_storage_bounded {n : Nat} (s : State ℝ ℝ n) (resolution : ℝ)
    (observation : Point ℝ) (positive : 0 < resolution)
    (bounded : StorageBounded observation.time s.payload)
    (chronological : ∀ previous ∈ s.previous, previous.time ≤ observation.time) :
    StorageBounded observation.time (write s resolution observation).payload := by
  cases hp : s.previous with
  | none => simpa [write, hp] using bounded
  | some previous =>
    have timeOrder := chronological previous hp
    simp only [write, hp, causal_lt, decide_eq_true_eq]
    split
    next forward =>
      dsimp only
      apply scatter_bounded
      · split
        · apply beginBranch_bounded
          · exact bounded
          · exact timeOrder
        · exact bounded
      · intro i hlow hhigh
        refine edgeAt_bounded _ _ i _ ?_ ?_ ?_ hlow ?_
        · exact timeOrder
        · exact le_rfl
        · exact mul_lt_mul_of_pos_right forward positive
        · dsimp only at hhigh hlow ⊢
          simp only [causal_ceil, causal_floor] at hhigh hlow
          omega
    · split <;> exact bounded

private theorem storage_bounded_empty (n : Nat) (now : ℝ) :
    StorageBounded now ({} : Published ℝ ℝ n) := by
  intro slot
  simp [TimesBounded, Vector.get]

private theorem readInterval_bounded (left : Point ℝ) (rightPhase : ℝ)
    (rightTime : Option ℝ) (q cutoff now time : ℝ)
    (hl : left.time ≤ now) (hr : ∀ t ∈ rightTime, t ≤ now)
    (found : readInterval left rightPhase rightTime q cutoff = some time) :
    time ≤ now ∧ time ≤ cutoff := by
  by_cases early : q < left.phase
  · simp [readInterval, readIntervalWith, entryInRange, early] at found
  by_cases late : cutoff < left.time
  · simp [readInterval, readIntervalWith, entryInRange, late] at found
  by_cases entry : q = left.phase
  · simp [readInterval, readIntervalWith, entryInRange, late, entry] at found
    subst time
    exact ⟨hl, not_lt.mp late⟩
  cases rightTime with
  | none => simp [readInterval, readIntervalWith, entryInRange, entry] at found
  | some rightTime =>
    by_cases beyond : rightPhase < q
    · simp [readInterval, readIntervalWith, interiorInRange, entry, beyond] at found
    by_cases ordered : left.phase < rightPhase
    · let right : Point ℝ := ⟨rightPhase, rightTime⟩
      have hb : interpolate left right q ≤ now :=
        interpolate_bounded left right q now hl (hr rightTime (by simp)) ordered
          (not_lt.mp early) (not_lt.mp beyond)
      simp [readInterval, readIntervalWith, entryInRange, interiorInRange, early, late,
        entry, ordered, beyond] at found
      obtain ⟨within, rfl⟩ := found
      exact ⟨hb, within⟩
    · simp [readInterval, readIntervalWith, interiorInRange, entry, ordered] at found

private def selectedRight (low high : ℝ) (stop : Option ℝ)
    (frontier : Option (Point ℝ)) : ℝ × Option ℝ :=
  match stop, frontier with
  | none, some p =>
    if frontierCloses low high p.phase then (p.phase, some p.time) else (high, none)
  | _, _ => (high, stop)

private theorem selectedRight_bounded (low high : ℝ) (stop : Option ℝ)
    (frontier : Option (Point ℝ)) (now : ℝ) (hs : ∀ t ∈ stop, t ≤ now)
    (live : ∀ p ∈ frontier, p.time ≤ now) :
    ∀ t ∈ (selectedRight low high stop frontier).2, t ≤ now := by
  cases stop with
  | some stop => cases frontier <;> exact hs
  | none =>
    cases frontier with
    | none => exact hs
    | some edge =>
      by_cases h : frontierCloses low high edge.phase
      · simpa [selectedRight, h] using live edge rfl
      · simp [selectedRight, h]

private theorem selectInterval_bounded (c : Cell ℝ ℝ) (seed : Option ℝ)
    (frontier : Option (Point ℝ)) (index : Int) (q now : ℝ) (interval : Interval ℝ)
    (bounded : TimesBounded now c) (live : ∀ p ∈ frontier, p.time ≤ now)
    (found : selectInterval c seed frontier index q = some interval) :
    interval.left.time ≤ now ∧ ∀ t ∈ interval.rightTime, t ≤ now := by
  have build (low high time : ℝ) (stop : Option ℝ) (hl : time ≤ now)
      (hs : ∀ t ∈ stop, t ≤ now)
      (eq : (⟨⟨low, time⟩, (selectedRight low high stop frontier).1,
        (selectedRight low high stop frontier).2⟩ : Interval ℝ) = interval) :
      interval.left.time ≤ now ∧ ∀ t ∈ interval.rightTime, t ≤ now := by
    subst interval
    exact ⟨hl, selectedRight_bounded _ _ _ _ _ hs live⟩
  let low : ℝ := match seed with
    | some phase => if ⌊phase⌋ = index then phase else index
    | none => index
  let high : ℝ := (index : ℝ) + 1
  cases hb : c.base with
  | none => simp [selectInterval, hb] at found
  | some base =>
    have baseBounded := bounded.1 base hb
    cases hc : c.replacement with
    | none =>
      simp [selectInterval, hb, hc] at found
      apply build low high base c.fallbackEnd baseBounded bounded.2.1
      cases hst : c.fallbackEnd <;> cases frontier <;> cases seed <;>
        simpa [selectedRight, low, high, hst] using found
    | some branch =>
      have branchBounded := bounded.2.2 base hb branch hc
      by_cases active : branch.cut ≤ q
      · simp [selectInterval, hb, hc, active] at found
        apply build branch.cut high (base + branch.start)
          (branch.stop.bind (Arithmetic.decode base)) branchBounded.1
        · intro stop hstop
          cases hd : branch.stop with
          | none => simp [hd] at hstop
          | some delta =>
            have he : base + delta = stop := by simpa [hd] using hstop
            exact he ▸ branchBounded.2 delta hd
        · cases hst : branch.stop <;> cases frontier <;>
            simpa [selectedRight, high, hst] using found
      · simp [selectInterval, hb, hc, active] at found
        apply build low high base c.fallbackEnd baseBounded bounded.2.1
        cases hst : c.fallbackEnd <;> cases frontier <;> cases seed <;>
        simpa [selectedRight, low, high, hst] using found

theorem read_bounded {n : Nat} (s : Published ℝ ℝ n) (q cutoff now time : ℝ)
    (bounded : StorageBounded now s) (live : ∀ p ∈ s.frontier, p.time ≤ now)
    (found : read s q cutoff = some time) :
    time ≤ now ∧ time ≤ cutoff := by
  let index := Arithmetic.floor q
  let cell := s.cells.get (wrap n index)
  have segment (lookup : (selectInterval cell s.seed s.frontier index q).bind
      (fun i => readInterval i.left i.rightPhase i.rightTime q cutoff) = some time) :
      time ≤ now ∧ time ≤ cutoff := by
    cases hs : selectInterval cell s.seed s.frontier index q with
    | none => simp [hs] at lookup
    | some interval =>
      obtain ⟨hl, hr⟩ := selectInterval_bounded cell s.seed s.frontier index q now interval
        (bounded _) live hs
      exact readInterval_bounded _ _ _ q cutoff now time hl hr (by simpa [hs] using lookup)
  change readCell cell s.seed s.frontier index q cutoff = some time at found
  cases hf : s.frontier with
  | none => exact segment (by simpa [readCell, hf] using found)
  | some edge =>
    by_cases atEdge : q = edge.phase
    · simp [readCell, hf, atEdge, timeWithinCutoff] at found
      obtain ⟨within, rfl⟩ := found
      exact ⟨live edge hf, within⟩
    · exact segment (by simpa [readCell, hf, atEdge] using found)

theorem SampledWrites.storage_bounded {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    {s : State ℝ ℝ n} (history : SampledWrites F resolution n s)
    (positive : 0 < resolution) (now : ℝ)
    (latest : ∀ p ∈ s.previous, p.time ≤ now) : StorageBounded now s.payload := by
  induction history generalizing now with
  | empty => exact storage_bounded_empty n now
  | observe _ observation _ segment ih =>
    have chronological (p : Point ℝ) (hp : p ∈ _) : p.time ≤ observation.time :=
      (segment p hp).1.le
    have obsBound := latest observation (write_previous _ _ _)
    have bounded := write_storage_bounded _ _ _ positive
      (ih observation.time chronological) chronological
    exact fun slot => (bounded slot).mono obsBound

/-- A successful lookup is causal and respects its cutoff, even without a retained-query premise. -/
theorem SampledWrites.read_causal {n : Nat} {F : ℝ → ℝ} {resolution : ℝ}
    {s : State ℝ ℝ n} (history : SampledWrites F resolution n s)
    (positive : 0 < resolution) {previous : Point ℝ} (latest : s.previous = some previous)
    {query cutoff time : ℝ} (found : read s.payload query cutoff = some time) :
    time ≤ previous.time ∧ time ≤ cutoff := by
  have latestBound (p : Point ℝ) (hp : p ∈ s.previous) : p.time ≤ previous.time := by
    rw [latest, Option.mem_some_iff] at hp
    subst p
    exact le_rfl
  exact read_bounded _ _ _ _ _ (history.storage_bounded positive _ latestBound)
    (fun p hp => by
      obtain ⟨_, obs, member, past⟩ := history.frontier_exact p hp
      exact past.trans (latestBound obs member)) found

end

end PhaseTime.InverseRuntime

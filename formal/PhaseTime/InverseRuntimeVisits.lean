import PhaseTime.InverseRuntimeReachability

namespace PhaseTime.InverseRuntime

noncomputable section
attribute [local instance] exactArithmetic

private theorem real_lt (a b : ℝ) : Arithmetic.lt a b = decide (a < b) := rfl
private theorem real_le (a b : ℝ) : Arithmetic.le a b = decide (a ≤ b) := rfl
private theorem real_floor (a : ℝ) : Arithmetic.floor a = ⌊a⌋ := rfl
private theorem real_ceil (a : ℝ) : Arithmetic.ceil a = ⌈a⌉ := rfl
private theorem real_index (i : Int) : (Arithmetic.ofIndex i : ℝ) = i := rfl
private theorem real_decode (a b : ℝ) : Arithmetic.decode a b = some (a + b) := rfl

def entryPhase (i : Int) (seed : Option ℝ) : ℝ :=
  match seed with
  | some phase => if ⌊phase⌋ = i then phase else i
  | none => i

def cellSpan (F : ℝ → ℝ) (i : Int) (now : ℝ) (start stop : Point ℝ) : Prop :=
  FixedCell.Certified F i ((i : ℝ) + 1) now ⟨start.exact, stop.exact⟩

/-- The last forward sample bounds the unfinished traversal. A replacement
preserves the certificate for the completed fallback. -/
inductive OpenVisit (F : ℝ → ℝ) (i : Int) (seed : Option ℝ)
    (edge : Point ℝ) : Cell ℝ ℝ → Prop
  | first (base : ℝ)
      (span : cellSpan F i edge.time ⟨entryPhase i seed, base⟩ edge) :
      OpenVisit F i seed edge ⟨some base, none, none⟩
  | replacement (base stop cut delta : ℝ)
      (fallback : cellSpan F i edge.time ⟨entryPhase i seed, base⟩ ⟨(i : ℝ) + 1, stop⟩)
      (span : cellSpan F i edge.time ⟨cut, base + delta⟩ edge) :
      OpenVisit F i seed edge ⟨some base, some stop, some ⟨cut, delta, none⟩⟩

/-- A completed traversal ends at the upper grid edge. A replacement stop
at the current frontier is also the start of the possible descending bridge. -/
inductive ClosedVisit (F : ℝ → ℝ) (i : Int) (seed : Option ℝ)
    (edge : Point ℝ) : Cell ℝ ℝ → Prop
  | empty (cell : Cell ℝ ℝ) (absent : cell.base = none) : ClosedVisit F i seed edge cell
  | first (base stop : ℝ)
      (span : cellSpan F i edge.time ⟨entryPhase i seed, base⟩ ⟨(i : ℝ) + 1, stop⟩) :
      ClosedVisit F i seed edge ⟨some base, some stop, none⟩
  | replacement (base stop cut delta finish : ℝ)
      (fallback : cellSpan F i edge.time ⟨entryPhase i seed, base⟩ ⟨(i : ℝ) + 1, stop⟩)
      (span : cellSpan F i edge.time ⟨cut, base + delta⟩ ⟨(i : ℝ) + 1, base + finish⟩)
      (touch : edge.phase = (i : ℝ) + 1 → base + finish = edge.time) :
      ClosedVisit F i seed edge ⟨some base, some stop, some ⟨cut, delta, some finish⟩⟩

def visitAt {n : Nat} (F : ℝ → ℝ) (p : Published ℝ ℝ n) (edge : Point ℝ)
    (i : Int) : Prop :=
  let seed := seedAtSlot p (wrap n i)
  let cell := p.cells.get (wrap n i)
  if i = ⌊edge.phase⌋ then OpenVisit F i seed edge cell else ClosedVisit F i seed edge cell

/-- Certify retained slots through the last forward sample. A descending
write leaves this payload intact and can lower the read cursor. -/
def Visits {n : Nat} (F : ℝ → ℝ) (s : State ℝ ℝ n) : Prop :=
  ∀ high ∈ s.highest, ∃ edge ∈ s.payload.frontier,
    ∀ i, high - (n : Int) ≤ i → i ≤ ⌊edge.phase⌋ → visitAt F s.payload edge i

/-- A forward write in grid coordinates, normalized for the storage lemmas. -/
structure Rise {n : Nat} (F : ℝ → ℝ) (res : ℝ) (s : State ℝ ℝ n)
    (previous next : Point ℝ) : Prop where
  prior : s.previous = some previous
  segment : SampleSegment F res previous next
  continuous : Continuous F
  positive : 0 < res
  forward : previous.phase < next.phase

namespace Rise
variable {n : Nat} {F : ℝ → ℝ} {res : ℝ} {s : State ℝ ℝ n} {previous next : Point ℝ}

def left (previous : Point ℝ) (res : ℝ) : Point ℝ := { previous with phase := previous.phase * res }
def right (next : Point ℝ) (res : ℝ) : Point ℝ := { next with phase := next.phase * res }
def opened (s : State ℝ ℝ n) (previous next : Point ℝ) (res : ℝ) : Published ℝ ℝ n :=
  let observed := { s with previous := some next }
  (if !s.hasForward || s.descending then beginBranch observed (left previous res) else observed).payload
def first (previous : Point ℝ) (res : ℝ) : Int := ⌈(left previous res).phase⌉
def last (next : Point ℝ) (res : ℝ) : Int := ⌊(right next res).phase⌋
def high (s : State ℝ ℝ n) (next : Point ℝ) (res : ℝ) : Int :=
  max (last next res) (s.highest.getD (last next res))
def count (previous next : Point ℝ) (res : ℝ) : Nat :=
  (last next res - first previous res + 1).toNat
def out (s : State ℝ ℝ n) (previous next : Point ℝ) (res : ℝ) : Published ℝ ℝ n :=
  { scatter (opened s previous next res) (left previous res) (right next res) (first previous res)
      (count previous next res) with frontier := some (right next res) }

theorem payload (h : Rise F res s previous next) :
    (write s res next).payload = out s previous next res := by
  simp only [write, h.prior, real_lt, h.forward, decide_true, if_true]
  rfl

theorem highest (h : Rise F res s previous next) :
    (write s res next).highest = some (high s next res) := by
  simp only [write, h.prior, real_lt, h.forward, decide_true, if_true]
  rfl

theorem gridOrder (h : Rise F res s previous next) :
    (left previous res).phase < (right next res).phase :=
  mul_lt_mul_of_pos_right h.forward h.positive

theorem countEnd (h : Rise F res s previous next) :
    first previous res + (count previous next res : Int) - 1 = last next res := by
  have hf : first previous res ≤ ⌊(left previous res).phase⌋ + 1 := Int.ceil_le_floor_add_one _
  have hf' : ⌊(left previous res).phase⌋ ≤ last next res := Int.floor_le_floor h.gridOrder.le
  unfold count
  omega

theorem lastLeHigh : last next res ≤ high s next res := le_max_left _ _

theorem edgeStrict :
    ∀ j, first previous res < j →
      Arithmetic.lt (left previous res).phase (edgeAt (left previous res) (right next res) j).phase =
        true := by
  intro j greater
  have lt := (Int.le_ceil (left previous res).phase).trans_lt (Int.cast_lt.mpr greater)
  simpa [edgeAt, real_index, real_lt] using lt

end Rise

theorem cellSpan.later {F : ℝ → ℝ} {i : Int} {now later : ℝ} {a b : Point ℝ}
    (span : cellSpan F i now a b) (order : now ≤ later) : cellSpan F i later a b :=
  { span with causal := span.causal.trans order }

theorem cellSpan.point {F : ℝ → ℝ} {i : Int} {edge : Point ℝ}
    (exactPhase : F edge.time = edge.phase) (lo : (i : ℝ) ≤ edge.phase)
    (hi : edge.phase ≤ (i : ℝ) + 1) (continuous : Continuous F) :
    cellSpan F i edge.time edge edge :=
  ⟨le_rfl, le_rfl, le_rfl, lo, hi, exactPhase, exactPhase, continuous.continuousOn,
    fun t lower upper => by
      obtain (rfl : t = edge.time) := le_antisymm upper lower
      rw [exactPhase]
      exact ⟨lo, hi⟩⟩

theorem cellSpan.extend {F : ℝ → ℝ} {i : Int} {start middle stop : Point ℝ}
    (old : cellSpan F i middle.time start middle) (continuous : Continuous F)
    (timeOrder : middle.time ≤ stop.time) (phaseOrder : start.phase ≤ stop.phase)
    (stopExact : F stop.time = stop.phase) (stopBound : stop.phase ≤ (i : ℝ) + 1)
    (bridge : AnalysisStability.Confined F middle.time stop.time i ((i : ℝ) + 1)) :
    cellSpan F i stop.time start stop :=
  ⟨phaseOrder, old.timeOrder.trans timeOrder, le_rfl, old.leftBound, stopBound, old.leftExact,
    stopExact, continuous.continuousOn,
    FixedCell.confined_join F _ _ _ _ _ old.confined bridge⟩

theorem OpenVisit.extend {F : ℝ → ℝ} {i : Int} {seed : Option ℝ}
    {middle stop : Point ℝ} {cell : Cell ℝ ℝ}
    (old : OpenVisit F i seed middle cell) (continuous : Continuous F)
    (timeOrder : middle.time ≤ stop.time)
    (entryBefore : ∀ base ∈ cell.base,
      (cell.replacement.map Replacement.cut).getD (entryPhase i seed) ≤ stop.phase)
    (stopExact : F stop.time = stop.phase) (stopBound : stop.phase ≤ (i : ℝ) + 1)
    (bridge : AnalysisStability.Confined F middle.time stop.time i ((i : ℝ) + 1)) :
    OpenVisit F i seed stop cell := by
  cases old with
  | first base span =>
    exact .first base (span.extend continuous timeOrder (entryBefore base (by simp)) stopExact stopBound bridge)
  | replacement base close cut delta fallback span =>
    exact .replacement base close cut delta (fallback.later timeOrder)
      (span.extend continuous timeOrder (entryBefore base (by simp)) stopExact stopBound bridge)

theorem OpenVisit.advance {F : ℝ → ℝ} {i : Int} {seed : Option ℝ}
    {middle stop : Point ℝ} {cell : Cell ℝ ℝ}
    (old : OpenVisit F i seed middle cell) (continuous : Continuous F)
    (next : cellSpan F i stop.time middle stop) : OpenVisit F i seed stop cell := by
  apply old.extend continuous next.timeOrder _ next.rightExact next.rightBound next.confined
  intro base _
  cases old with
  | first _ span => exact span.phaseOrder.trans next.phaseOrder
  | replacement _ _ _ _ _ span => exact span.phaseOrder.trans next.phaseOrder

theorem ClosedVisit.later_above {F : ℝ → ℝ} {i : Int} {seed : Option ℝ}
    {edge next : Point ℝ} {cell : Cell ℝ ℝ} (cert : ClosedVisit F i seed edge cell)
    (later : edge.time ≤ next.time) (above : (i : ℝ) + 1 < next.phase) :
    ClosedVisit F i seed next cell := by
  cases cert with
  | empty cell absent => exact .empty cell absent
  | first base stop span => exact .first base stop (span.later later)
  | replacement base stop cut delta finish fallback span _ =>
    exact .replacement base stop cut delta finish (fallback.later later) (span.later later)
      (fun eq => by rw [eq] at above; exact (lt_irrefl _ above).elim)

def finishVisit (cell : Cell ℝ ℝ) (time : ℝ) : Cell ℝ ℝ :=
  match cell.base, cell.fallbackEnd, cell.replacement with
  | _, none, _ => { cell with fallbackEnd := some time }
  | base, some _, some branch =>
    { cell with replacement := some { branch with stop := base.map (fun t => time - t) } }
  | _, _, _ => cell

theorem OpenVisit.finish {F : ℝ → ℝ} {i : Int} {seed : Option ℝ}
    {atEdge next : Point ℝ} {cell : Cell ℝ ℝ} (visit : OpenVisit F i seed atEdge cell)
    (edgePhase : atEdge.phase = (i : ℝ) + 1) (later : atEdge.time ≤ next.time)
    (touch : next.phase = (i : ℝ) + 1 → next.time = atEdge.time) :
    ClosedVisit F i seed next (finishVisit cell atEdge.time) := by
  cases visit with
  | first base span =>
    exact .first base atEdge.time (by simpa [cellSpan, Point.exact, edgePhase] using span.later later)
  | replacement base stop cut delta fallback span =>
    have finishTime : base + (atEdge.time - base) = atEdge.time := by ring
    exact .replacement base stop cut delta (atEdge.time - base) (fallback.later later)
      (by simpa [cellSpan, Point.exact, edgePhase, finishTime] using span.later later)
      (fun eq => finishTime.trans (touch eq).symm)

theorem slots_finish {n : Nat} (p : Published ℝ ℝ n) (i : Int) (time : ℝ) :
    (finishCell p i time).cells.get (wrap n i) = finishVisit (p.cells.get (wrap n i)) time := by
  unfold finishCell finishVisit
  rw [get_set_same]
  generalize p.cells.get (wrap n i) = cell
  rcases cell with ⟨base, stop, branch⟩
  cases stop <;> cases branch <;> rfl

private theorem localSeedIndex {n : Nat} (p : Published ℝ ℝ n) (i : Int) :
    entryPhase i (seedAtSlot p (wrap n i)) = entryPhase i p.seed := by
  cases seed : p.seed with
  | none => simp [seedAtSlot, entryPhase, seed]
  | some start =>
    by_cases same : ⌊start⌋ = i
    · simp [seedAtSlot, entryPhase, seed, real_floor, Option.filter, same]
    · by_cases coincides : wrap n ⌊start⌋ = wrap n i <;>
        simp [seedAtSlot, entryPhase, seed, real_floor, Option.filter, same, coincides]

namespace Rise
variable {n : Nat} {F : ℝ → ℝ} {res : ℝ} {s : State ℝ ℝ n} {previous next : Point ℝ}

def startIndex (previous : Point ℝ) (res : ℝ) : Int := ⌊(left previous res).phase⌋

def startInBounds (s : State ℝ ℝ n) (previous next : Point ℝ) (res : ℝ) : Prop :=
  high s next res - (n : Int) ≤ startIndex previous res

def startsInside (previous : Point ℝ) (res : ℝ) : Prop :=
  (startIndex previous res : ℝ) < (left previous res).phase

def startOpen (F : ℝ → ℝ) (s : State ℝ ℝ n) (previous next : Point ℝ) (res : ℝ) : Prop :=
  let index := startIndex previous res
  let p := opened s previous next res
  OpenVisit F index (seedAtSlot p (wrap n index)) (left previous res) (p.cells.get (wrap n index))

def startCrosses (previous next : Point ℝ) (res : ℝ) : Prop :=
  startIndex previous res < last next res

def shouldSeed (s : State ℝ ℝ n) (previous : Point ℝ) (res : ℝ) : Bool :=
  let i := startIndex previous res
  !retained s.highest n i || (s.payload.cells.get (wrap n i)).base.isNone ||
    s.payload.seed.any (fun phase => arithmeticFloor phase == i && arithmeticLess (left previous res).phase phase)
where
  arithmeticFloor := @Arithmetic.floor ℝ exactArithmetic
  arithmeticLess := @Arithmetic.lt ℝ exactArithmetic

theorem startPartition (h : Rise F res s previous next) {i : Int} :
    first previous res ≤ i ∨
    ((i : ℝ) + 1 ≤ (left previous res).phase ∧ i < last next res) ∨
    (i = startIndex previous res ∧ startsInside previous res) := by
  by_cases crossed : first previous res ≤ i
  · exact Or.inl crossed
  have beyond : (i : ℝ) < (left previous res).phase := by
    by_contra! bound
    exact crossed (Int.ceil_le.mpr bound)
  by_cases older : (i : ℝ) + 1 ≤ (left previous res).phase
  · refine Or.inr (Or.inl ⟨older, ?_⟩)
    have bound : i + 1 ≤ last next res := Int.le_floor.mpr (by
      push_cast
      exact older.trans h.gridOrder.le)
    omega
  · have eq : i = startIndex previous res := (Int.floor_eq_iff.mpr ⟨beyond.le, lt_of_not_ge older⟩).symm
    exact Or.inr (Or.inr ⟨eq, by rwa [startsInside, ← eq]⟩)

end Rise

private def CheckOlder : Prop :=
  ∀ {n : Nat} {F : ℝ → ℝ} {res : ℝ} {s : State ℝ ℝ n} {previous next : Point ℝ},
    SampledWrites F res n s → Visits F s →
    Rise F res s previous next →
    ∀ i, Rise.high s next res - (n : Int) ≤ i →
      (i : ℝ) + 1 ≤ (Rise.left previous res).phase →
      visitAt F (Rise.out s previous next res) (Rise.right next res) i

private def CheckCrossed : Prop :=
  ∀ {n : Nat} {F : ℝ → ℝ} {res : ℝ} {s : State ℝ ℝ n} {previous next : Point ℝ},
    Rise F res s previous next →
    ∀ i, Rise.high s next res - (n : Int) ≤ i →
      Rise.first previous res ≤ i → i ≤ Rise.last next res →
      visitAt F (Rise.out s previous next res) (Rise.right next res) i

private def CheckStayInside : Prop :=
  ∀ {n : Nat} {F : ℝ → ℝ} {res : ℝ} {s : State ℝ ℝ n} {previous next : Point ℝ},
    Rise F res s previous next → Rise.startsInside previous res →
    ¬ Rise.startCrosses previous next res → Rise.startOpen F s previous next res →
    visitAt F (Rise.out s previous next res) (Rise.right next res) (Rise.startIndex previous res)

private def CheckFinishStart : Prop :=
  ∀ {n : Nat} {F : ℝ → ℝ} {res : ℝ} {s : State ℝ ℝ n} {previous next : Point ℝ},
    Rise F res s previous next → Rise.startInBounds s previous next res → Rise.startsInside previous res →
    Rise.startCrosses previous next res → Rise.startOpen F s previous next res →
    visitAt F (Rise.out s previous next res) (Rise.right next res) (Rise.startIndex previous res)

private def CheckSeedStart : Prop :=
  ∀ {n : Nat} {F : ℝ → ℝ} {res : ℝ} {s : State ℝ ℝ n} {previous next : Point ℝ},
    SampledWrites F res n s → Rise F res s previous next → Rise.startsInside previous res →
    (!s.hasForward || s.descending) = true → Rise.shouldSeed s previous res = true →
    Rise.startOpen F s previous next res

private def CheckReuseStart : Prop :=
  ∀ {n : Nat} {F : ℝ → ℝ} {res : ℝ} {s : State ℝ ℝ n} {previous next : Point ℝ},
    SampledWrites F res n s → Visits F s → Rise F res s previous next → Rise.startsInside previous res →
    (!s.hasForward || s.descending) = true → Rise.shouldSeed s previous res = false →
    Rise.startOpen F s previous next res

private def CheckContinueStart : Prop :=
  ∀ {n : Nat} {F : ℝ → ℝ} {res : ℝ} {s : State ℝ ℝ n} {previous next : Point ℝ},
    SampledWrites F res n s → Visits F s → Rise F res s previous next →
    Rise.startInBounds s previous next res →
    (!s.hasForward || s.descending) = false → Rise.startOpen F s previous next res

private theorem visits_fractional_of_cases (seeded : CheckSeedStart) (reuse : CheckReuseStart)
    (carry : CheckContinueStart) (stay : CheckStayInside) (finish : CheckFinishStart)
    {n : Nat} {F : ℝ → ℝ} {res : ℝ} {s : State ℝ ℝ n} {previous next : Point ℝ}
    (history : SampledWrites F res n s) (old : Visits F s) (rise : Rise F res s previous next)
    (bounded : Rise.startInBounds s previous next res) (fractional : Rise.startsInside previous res) :
    visitAt F (Rise.out s previous next res) (Rise.right next res) (Rise.startIndex previous res) := by
  have opened : Rise.startOpen F s previous next res := by
    cases branch : (!s.hasForward || s.descending) with
    | false => exact carry history old rise bounded branch
    | true =>
      cases reset : Rise.shouldSeed s previous res with
      | false => exact reuse history old rise fractional branch reset
      | true => exact seeded history rise fractional branch reset
  by_cases crosses : Rise.startCrosses previous next res
  · exact finish rise bounded fractional crosses opened
  · exact stay rise fractional crosses opened

private theorem visits_forward_of_cases (older : CheckOlder) (crossed : CheckCrossed)
    (seeded : CheckSeedStart) (reuse : CheckReuseStart) (carry : CheckContinueStart)
    (stay : CheckStayInside) (finish : CheckFinishStart)
    {n : Nat} {F : ℝ → ℝ} {res : ℝ} {s : State ℝ ℝ n} {previous next : Point ℝ}
    (history : SampledWrites F res n s) (old : Visits F s) (rise : Rise F res s previous next) :
    Visits F (write s res next) := by
  intro high member
  rw [rise.highest, Option.mem_some_iff] at member
  subst high
  rw [rise.payload]
  refine ⟨Rise.right next res, by simp [Rise.out], ?_⟩
  intro i retained bound
  rcases rise.startPartition (i := i) with start | ⟨earlier, _⟩ | ⟨same, fractional⟩
  · exact crossed rise i retained start bound
  · exact older history old rise i retained earlier
  · subst i
    exact visits_fractional_of_cases seeded reuse carry stay finish history old rise retained fractional

private theorem SampledWrites.visits_of_cases (older : CheckOlder) (crossed : CheckCrossed)
    (seeded : CheckSeedStart) (reuse : CheckReuseStart) (carry : CheckContinueStart)
    (stay : CheckStayInside) (finish : CheckFinishStart)
    {n : Nat} {F : ℝ → ℝ} {res : ℝ} {s : State ℝ ℝ n}
    (history : SampledWrites F res n s) (continuous : Continuous F) (positive : 0 < res) :
    Visits F s := by
  induction history with
  | empty => simp [Visits]
  | @observe s history next _ segments ih =>
    cases prior : s.previous with
    | none => simpa [Visits, write, prior, visitAt] using ih
    | some previous =>
      by_cases rise : previous.phase < next.phase
      · have segment := segments previous (by simp [prior])
        exact visits_forward_of_cases older crossed seeded reuse carry stay finish history ih
          ⟨prior, segment, continuous, positive, rise⟩
      · have highest : (write s res next).highest = s.highest := by
          simp [write, prior, real_lt, rise]
          split <;> rfl
        simpa only [Visits, highest, nonforward_keeps_payload s res previous next prior (le_of_not_gt rise)]
          using ih

private theorem select_with_seedAtSlot {n : Nat} (p : Published ℝ ℝ n) (i : Int)
    (edge : Option (Point ℝ)) (q : ℝ) :
    selectInterval (p.cells.get (wrap n i)) p.seed edge i q =
      selectInterval (p.cells.get (wrap n i)) (seedAtSlot p (wrap n i)) edge i q := by
  simp only [seedAtSlot]
  cases seed : p.seed with
  | none => rfl
  | some start =>
    dsimp only [Option.filter]
    split
    · rfl
    · rename_i different
      have notIndex : Arithmetic.floor start ≠ i := by
        intro same
        simp [same] at different
      simp [selectInterval, notIndex]

private theorem OpenVisit.selected {F : ℝ → ℝ} {i : Int} {seed : Option ℝ} {edge : Point ℝ}
    {cell : Cell ℝ ℝ} (visit : OpenVisit F i seed edge cell) (inside : edge.phase < (i : ℝ) + 1)
    {query : ℝ} {interval : Interval ℝ}
    (selected : selectInterval cell seed (some edge) i query = some interval) :
    FixedCell.Certified F i ((i : ℝ) + 1) edge.time interval.piece := by
  cases visit with
  | first base span =>
    have order : entryPhase i seed ≤ edge.phase := span.phaseOrder
    have selectedEq : interval = ⟨⟨entryPhase i seed, base⟩, edge.phase, some edge.time⟩ := by
      cases seed <;> simp [entryPhase] at order ⊢ <;>
        simpa [selectInterval, real_floor, real_index, frontierCloses, real_le, real_lt, inside,
          order] using selected.symm
    rw [selectedEq]
    exact span
  | replacement base stop cut delta fallback span =>
    by_cases useBranch : cut ≤ query
    · have order : cut ≤ edge.phase := span.phaseOrder
      have selectedEq : interval = ⟨⟨cut, base + delta⟩, edge.phase, some edge.time⟩ := by
        simpa [selectInterval, real_index, frontierCloses, real_le, real_lt, inside, real_decode,
          useBranch, order] using selected.symm
      rw [selectedEq]
      exact span
    · have selectedEq : interval = ⟨⟨entryPhase i seed, base⟩, (i : ℝ) + 1, some stop⟩ := by
        cases seed <;> simpa [selectInterval, real_floor, real_index, real_le, useBranch, entryPhase] using selected.symm
      rw [selectedEq]
      exact fallback

private theorem ClosedVisit.selected {F : ℝ → ℝ} {i : Int} {seed : Option ℝ} {edge : Point ℝ}
    {cell : Cell ℝ ℝ} (visit : ClosedVisit F i seed edge cell) {query : ℝ} {interval : Interval ℝ}
    (selected : selectInterval cell seed (some edge) i query = some interval) :
    FixedCell.Certified F i ((i : ℝ) + 1) edge.time interval.piece := by
  cases visit with
  | empty cell absent => simp [selectInterval, absent] at selected
  | first base stop span =>
    have selectedEq : interval = ⟨⟨entryPhase i seed, base⟩, (i : ℝ) + 1, some stop⟩ := by
      cases seed <;> simpa [selectInterval, real_floor, real_index, entryPhase] using selected.symm
    rw [selectedEq]
    exact span
  | replacement base stop cut delta finish fallback span _ =>
    by_cases useBranch : cut ≤ query
    · have selectedEq : interval = ⟨⟨cut, base + delta⟩, (i : ℝ) + 1, some (base + finish)⟩ := by
        simpa [selectInterval, real_index, real_le, real_decode, useBranch] using selected.symm
      rw [selectedEq]
      exact span
    · have selectedEq : interval = ⟨⟨entryPhase i seed, base⟩, (i : ℝ) + 1, some stop⟩ := by
        cases seed <;> simpa [selectInterval, real_floor, real_index, real_le, useBranch, entryPhase] using selected.symm
      rw [selectedEq]
      exact fallback

theorem Visits.retained_cells_certified
    {n : Nat} {F : ℝ → ℝ} {res : ℝ} {s : State ℝ ℝ n}
    (visits : Visits F s) (history : SampledWrites F res n s) (positive : 0 < res) :
    RetainedCellsCertified F res s := by
  intro previous previousMember query retained interval selected
  obtain ⟨high, newest, highEq, newestEq, lower, _, upper⟩ := retained
  rw [newestEq, Option.mem_some_iff] at previousMember
  subst newest
  obtain ⟨edge, live, cert⟩ := visits high (by simp [highEq])
  have older := history.previous_phase_le_frontier positive (show previous ∈ s.previous by simp [newestEq]) live
  have cert := cert ⌊query⌋ lower (Int.floor_le_floor (upper.trans older))
  have live : s.payload.frontier = some edge := by
    cases here : s.payload.frontier with
    | none => simp [here] at live
    | some other => simpa [here] using live
  rw [live, select_with_seedAtSlot] at selected
  unfold visitAt at cert
  split at cert
  · rename_i atFrontier
    have inside : edge.phase < (⌊query⌋ : Int) + (1 : ℝ) := by
      rw [atFrontier]
      exact Int.lt_floor_add_one _
    have span := cert.selected inside selected
    have facts := history.frontier_exact edge (by simp [live])
    obtain ⟨_, prev, member, order⟩ := facts
    rw [newestEq, Option.mem_some_iff] at member
    subst prev
    exact { span with causal := span.causal.trans order }
  · have span := cert.selected selected
    obtain ⟨_, prev, member, order⟩ := history.frontier_exact edge (by simp [live])
    rw [newestEq, Option.mem_some_iff] at member
    subst prev
    exact { span with causal := span.causal.trans order }

def reopenVisit (i : Int) (old next : Point ℝ) (cell : Cell ℝ ℝ) : Cell ℝ ℝ :=
  match cell.base, cell.fallbackEnd with
  | some base, some _ =>
    let fresh : Replacement ℝ ℝ := ⟨next.phase, next.time - base, none⟩
    let replacement := match cell.replacement with
      | some branch =>
        if keepCut old.phase ((i : ℝ) + 1) next.phase branch.cut then { branch with stop := none }
        else fresh
      | none => fresh
    { cell with replacement := some replacement }
  | _, _ => cell

def ReopenContext (F : ℝ → ℝ) (i : Int) (seed : Option ℝ) (old next : Point ℝ) : Prop :=
  Continuous F ∧ old.time ≤ next.time ∧ F next.time = next.phase ∧
    (i : ℝ) ≤ next.phase ∧ next.phase ≤ (i : ℝ) + 1 ∧ entryPhase i seed ≤ next.phase ∧
    ((old.phase ≤ (i : ℝ) + 1) → AnalysisStability.Confined F old.time next.time i ((i : ℝ) + 1))

private def CheckReopenOpen : Prop :=
  ∀ {F : ℝ → ℝ} {i : Int} {seed : Option ℝ} {old next : Point ℝ} {cell : Cell ℝ ℝ},
    OpenVisit F i seed old cell → old.phase ≤ (i : ℝ) + 1 →
    ReopenContext F i seed old next → OpenVisit F i seed next (reopenVisit i old next cell)

private def CheckReopenClosed : Prop :=
  ∀ {F : ℝ → ℝ} {i : Int} {seed : Option ℝ} {old next : Point ℝ} {cell : Cell ℝ ℝ},
    ClosedVisit F i seed old cell → (i : ℝ) + 1 ≤ old.phase → cell.base ≠ none →
    ReopenContext F i seed old next → OpenVisit F i seed next (reopenVisit i old next cell)

private def CheckReuseSlot : Prop :=
  ∀ {n : Nat} {s : State ℝ ℝ n} {res : ℝ} {previous next old : Point ℝ},
    Rise.startsInside previous res → (!s.hasForward || s.descending) = true →
    Rise.shouldSeed s previous res = false → s.payload.frontier = some old →
    let i := Rise.startIndex previous res
    let slot := wrap n i
    let p := Rise.opened s previous next res
    p.cells.get slot = reopenVisit i old (Rise.left previous res) (s.payload.cells.get slot) ∧
      seedAtSlot p slot = seedAtSlot s.payload slot

private theorem no_reset_facts {n : Nat} {s : State ℝ ℝ n} {res : ℝ} {previous : Point ℝ}
    (noReset : Rise.shouldSeed s previous res = false) :
    let i := Rise.startIndex previous res
    retained s.highest n i = true ∧ (s.payload.cells.get (wrap n i)).base ≠ none ∧
      entryPhase i (seedAtSlot s.payload (wrap n i)) ≤ (Rise.left previous res).phase := by
  let i := Rise.startIndex previous res
  let start := Rise.left previous res
  change ((!retained s.highest n i || (s.payload.cells.get (wrap n i)).base.isNone) ||
    s.payload.seed.any (fun p => (⌊p⌋ == i) && decide (start.phase < p))) = false at noReset
  simp only [Bool.or_eq_false_iff, and_assoc] at noReset
  change retained s.highest n i = true ∧ (s.payload.cells.get (wrap n i)).base ≠ none ∧
    entryPhase i (seedAtSlot s.payload (wrap n i)) ≤ start.phase
  refine ⟨by simpa using noReset.1, ?_, ?_⟩
  · intro empty
    have marked := noReset.2.1
    simp [empty] at marked
  · rw [localSeedIndex]
    have lo : (i : ℝ) ≤ start.phase := Int.floor_le _
    cases oldSeed : s.payload.seed with
    | none => exact lo
    | some seed =>
      by_cases same : ⌊seed⌋ = i
      · have bound : seed ≤ start.phase := by
          have st := noReset.2.2
          simpa [oldSeed, same] using st
        simpa [entryPhase, oldSeed, same] using bound
      · simpa [entryPhase, oldSeed, same] using lo

private theorem reuse_of_cases (slotEffects : CheckReuseSlot) (openCase : CheckReopenOpen)
    (closedCase : CheckReopenClosed) : CheckReuseStart := by
  intro n F res s previous next history visits rise fractional branch noReset
  obtain ⟨oldRetained, occupied, entryBefore⟩ := no_reset_facts noReset
  let i := Rise.startIndex previous res
  let start := Rise.left previous res
  let p := Rise.opened s previous next res
  let slot := wrap n i
  obtain ⟨high, highEq, lower, _⟩ : ∃ high, s.highest = some high ∧ high - (n : Int) ≤ i ∧ i ≤ high := by
    cases highest : s.highest with
    | none => simp [retained, highest] at oldRetained
    | some high =>
      exact ⟨high, rfl, by simpa [retained, highest] using oldRetained⟩
  have member : previous ∈ s.previous := by simp [rise.prior]
  obtain ⟨edge, live, valid⟩ := visits high (by simp [highEq])
  have before := history.previous_phase_le_frontier rise.positive member live
  have valid := valid i lower (Int.floor_le_floor before)
  have liveEq : s.payload.frontier = some edge := by
    cases here : s.payload.frontier with
    | none => simp [here] at live
    | some other => simpa [here] using live
  obtain ⟨cellEq, seedEq⟩ := slotEffects (next := next) fractional branch noReset liveEq
  change OpenVisit F i (seedAtSlot p slot) start (p.cells.get slot)
  rw [cellEq, seedEq]
  obtain ⟨oldExact, latest, latestMember, timeOrder⟩ := history.frontier_exact edge live
  have eq : latest = previous := Option.mem_unique latestMember member
  subst latest
  have context : ReopenContext F i (seedAtSlot s.payload slot) edge start := by
    have exactPhase : F start.time = start.phase := history.previous_exact member
    refine ⟨rise.continuous, timeOrder, exactPhase, Int.floor_le _, (Int.lt_floor_add_one _).le, entryBefore, ?_⟩
    intro upper
    exact (history.frontier_bridge_confined rise.positive member live
      (by change (i : ℝ) ≤ F start.time; rw [exactPhase]; exact Int.floor_le _) (by rwa [oldExact])).2
  unfold visitAt at valid
  split at valid
  · rename_i active
    apply openCase valid _ context
    rw [active]
    exact (Int.lt_floor_add_one _).le
  · rename_i inactive
    have past : i < ⌊edge.phase⌋ := lt_of_le_of_ne (Int.floor_le_floor before) inactive
    apply closedCase valid _ occupied context
    have ip : i + 1 ≤ ⌊edge.phase⌋ := by omega
    simpa only [Int.cast_add, Int.cast_one] using (Int.le_floor.mp ip)

namespace Rise
variable {n : Nat} {F : ℝ → ℝ} {res : ℝ} {s : State ℝ ℝ n} {previous next : Point ℝ}

theorem first_after_fractional (fractional : startsInside previous res) :
    first previous res = startIndex previous res + 1 := by
  apply Int.ceil_eq_iff.mpr
  push_cast
  exact ⟨by simpa only [startsInside, add_sub_cancel_right] using fractional, (Int.lt_floor_add_one _).le⟩

theorem startChord (h : Rise F res s previous next) (phase : ℝ)
    (afterStart : (left previous res).phase ≤ phase)
    (beforeEnd : phase ≤ (right next res).phase)
    (withinCell : phase ≤ (startIndex previous res : ℝ) + 1) :
    let a := left previous res
    let b := right next res
    let stop : Point ℝ := ⟨phase, interpolate a b phase⟩
    cellSpan F (startIndex previous res) stop.time a stop := by
  intro a b stop
  have atStart : interpolate a b a.phase = a.time := by
    change a.time + ((a.phase - a.phase) / (b.phase - a.phase)) * (b.time - a.time) = a.time
    simp
  have bounds : (startIndex previous res : ℝ) ≤ a.phase := Int.floor_le _
  have chord := h.segment.grid_interval_certified h.continuous h.positive h.forward
    (phase := a.phase) (endPhase := phase) le_rfl afterStart beforeEnd bounds withinCell
  change FixedCell.Certified F (startIndex previous res) ((startIndex previous res : ℝ) + 1)
    next.time ⟨⟨a.phase, interpolate a b a.phase⟩, stop.exact⟩ at chord
  rw [atStart] at chord
  exact { chord with causal := le_rfl }

theorem interpolate_end (h : Rise F res s previous next) :
    interpolate (left previous res) (right next res) (right next res).phase = next.time := by
  let a := left previous res
  let b := right next res
  change a.time + ((b.phase - a.phase) / (b.phase - a.phase)) * (b.time - a.time) = b.time
  rw [div_self (sub_ne_zero.mpr h.gridOrder.ne'), one_mul, add_sub_cancel]

theorem startChordToRight (h : Rise F res s previous next)
    (within : (right next res).phase ≤ (startIndex previous res : ℝ) + 1) :
    cellSpan F (startIndex previous res) next.time (left previous res) (right next res) := by
  have chord := h.startChord (right next res).phase h.gridOrder.le le_rfl within
  change cellSpan F (startIndex previous res)
    (interpolate (left previous res) (right next res) (right next res).phase)
    (left previous res)
    ⟨(right next res).phase, interpolate (left previous res) (right next res) (right next res).phase⟩ at chord
  rw [h.interpolate_end] at chord
  exact chord

end Rise

private theorem stay_inside : CheckStayInside := by
  intro n F res s previous next rise fractional noCross visit
  let i := Rise.startIndex previous res
  have floorEq : Rise.last next res = i :=
    le_antisymm (le_of_not_gt noCross) (Int.floor_le_floor rise.gridOrder.le)
  have firstEq := Rise.first_after_fractional fractional
  have noEdges : Rise.count previous next res = 0 := by
    unfold Rise.count
    rw [floorEq, firstEq]
    change (i - (i + 1) + 1).toNat = 0
    omega
  have phaseBound : (Rise.right next res).phase < (i : ℝ) + 1 := by
    rw [← floorEq]
    exact Int.lt_floor_add_one _
  have extended := OpenVisit.advance (stop := Rise.right next res) visit rise.continuous (rise.startChordToRight phaseBound.le)
  have same : Rise.startIndex previous res = ⌊(Rise.right next res).phase⌋ := floorEq.symm
  simp only [visitAt, same, if_true]
  simpa [Rise.out, seedAtSlot, noEdges, scatter, ← same] using extended

theorem SampledWrites.frontier_matches_if_not_descending {n : Nat} {F : ℝ → ℝ}
    {res : ℝ} {s : State ℝ ℝ n} (history : SampledWrites F res n s)
    (notDescending : s.descending = false) {previous edge : Point ℝ}
    (latest : previous ∈ s.previous) (live : edge ∈ s.payload.frontier) :
    previous.phase * res = edge.phase := by
  induction history generalizing previous edge with
  | empty => simp at live
  | @observe s history next _ _ ih =>
    rw [write_previous, Option.mem_some_iff] at latest
    subst previous
    cases prior : s.previous with
    | none =>
      have noPayload : s.payload = {} := by
        cases history with
        | empty => rfl
        | observe => simp [write_previous] at prior
      simp [write, prior, noPayload] at live
    | some previous =>
      by_cases increasing : previous.phase < next.phase
      · have newest : (write s res next).payload.frontier = some (Rise.right next res) := by
          simp only [write, prior, real_lt, increasing, decide_true, if_true]
          rfl
        rw [newest, Option.mem_some_iff] at live
        subst edge
        rfl
      · have payload := nonforward_keeps_payload s res previous next prior (le_of_not_gt increasing)
        rw [payload] at live
        by_cases decreasing : next.phase < previous.phase
        · simp [write, prior, real_lt, increasing, decreasing] at notDescending
        · have beforeNotDescending : s.descending = false := by
            simpa [write, prior, real_lt, increasing, decreasing] using notDescending
          have same := le_antisymm (le_of_not_gt increasing) (le_of_not_gt decreasing)
          rw [same]
          exact ih beforeNotDescending (by simp [prior]) live

theorem OpenVisit.entry_le {F : ℝ → ℝ} {i : Int} {seed : Option ℝ} {edge : Point ℝ}
    {cell : Cell ℝ ℝ} (visit : OpenVisit F i seed edge cell) :
    (cell.replacement.map Replacement.cut).getD (entryPhase i seed) ≤ edge.phase := by
  cases visit with
  | first _ span => exact span.phaseOrder
  | replacement _ _ _ _ _ span => exact span.phaseOrder

private theorem continue_start : CheckContinueStart := by
  intro n F res s previous next history visits rise bounded branch
  have flags : s.hasForward = true ∧ s.descending = false := by
    cases has : s.hasForward <;> cases desc : s.descending <;> simp [has, desc] at branch ⊢
  have hp : previous ∈ s.previous := by simp [rise.prior]
  cases highEq : s.highest with
  | none =>
    have empty := (history.empty_inverse_without_highest highEq).2
    simp [flags.1] at empty
  | some high =>
    let i := Rise.startIndex previous res
    let start := Rise.left previous res
    obtain ⟨edge, live, valid⟩ := visits high (by simp [highEq])
    have samePhase : start.phase = edge.phase :=
      history.frontier_matches_if_not_descending flags.2 hp live
    have sameIndex : i = ⌊edge.phase⌋ := by
      change ⌊start.phase⌋ = ⌊edge.phase⌋
      rw [samePhase]
    have oldHighLe : high ≤ Rise.high s next res := by simp [Rise.high, highEq]
    change Rise.high s next res - (n : Int) ≤ i at bounded
    have active := valid i (by omega) sameIndex.le
    have oldOpen : OpenVisit F i (seedAtSlot s.payload (wrap n i)) edge
        (s.payload.cells.get (wrap n i)) := by simpa [visitAt, sameIndex] using active
    have exactPhase : F start.time = start.phase := history.previous_exact hp
    have lo : (i : ℝ) ≤ start.phase := Int.floor_le _
    have hi : start.phase ≤ (i : ℝ) + 1 := (Int.lt_floor_add_one _).le
    obtain ⟨order, bridge⟩ := history.frontier_bridge_confined (lo := (i : ℝ)) (hi := (i : ℝ) + 1) rise.positive hp live
      (by change (i : ℝ) ≤ F start.time; rwa [exactPhase])
      (by rw [(history.frontier_exact edge live).1, ← samePhase]; exact hi)
    have entryBefore : ∀ base ∈ (s.payload.cells.get (wrap n i)).base,
        ((s.payload.cells.get (wrap n i)).replacement.map Replacement.cut).getD
          (entryPhase i (seedAtSlot s.payload (wrap n i))) ≤ start.phase := by
      intro _ _
      exact oldOpen.entry_le.trans_eq samePhase.symm
    have extended : OpenVisit F i (seedAtSlot s.payload (wrap n i)) start
        (s.payload.cells.get (wrap n i)) :=
      oldOpen.extend rise.continuous order entryBefore exactPhase hi bridge
    simpa [Rise.startOpen, Rise.opened, branch] using extended

private theorem seed_start : CheckSeedStart := by
  intro n F res s previous next history rise fractional opens reset
  let i := Rise.startIndex previous res
  let start := Rise.left previous res
  let p := seedCell s.payload start
  let slot := wrap n i
  have notGrid : start.phase ≠ (i : ℝ) := ne_of_gt fractional
  change ((!retained s.highest n i || (s.payload.cells.get slot).base.isNone) ||
    s.payload.seed.any (fun p => (⌊p⌋ == i) && decide (start.phase < p))) = true at reset
  have opened : Rise.opened s previous next res = { p with frontier := some start } := by
    simp only [Rise.opened, opens, if_true]
    change (beginBranch { s with previous := some next } start).payload =
      { p with frontier := some start }
    have offGrid : (start.phase == (Arithmetic.ofIndex (Arithmetic.floor start.phase) : ℝ)) = false := by
      simpa [real_index, real_floor, show ⌊start.phase⌋ = i from rfl] using notGrid
    unfold beginBranch
    simp only [Id.run, pure, offGrid, Bool.false_eq_true, if_false]
    rw [show (!retained s.highest n (Arithmetic.floor start.phase) ||
      (s.payload.cells.get (wrap n (Arithmetic.floor start.phase))).base.isNone ||
      s.payload.seed.any (fun phase => (Arithmetic.floor phase == Arithmetic.floor start.phase) &&
        Arithmetic.lt start.phase phase)) = true from reset]
    simp only [if_true, p]
  have cell : p.cells.get slot = ⟨some start.time, none, none⟩ := by
    change (seedCell s.payload start).cells.get (wrap n (Arithmetic.floor start.phase)) = _
    unfold seedCell assignCell
    rw [get_set_same]
  have seed : seedAtSlot p slot = some start.phase := by
    change seedAtSlot (seedCell s.payload start) (wrap n (Arithmetic.floor start.phase)) = _
    simp [seedCell, seedAtSlot, Option.filter]
  have exactPhase : F start.time = start.phase :=
    history.previous_exact (show previous ∈ s.previous by simp [rise.prior])
  have span := cellSpan.point (i := i) exactPhase (Int.floor_le _) (Int.lt_floor_add_one _).le rise.continuous
  change OpenVisit F i (seedAtSlot (Rise.opened s previous next res) slot) start
    ((Rise.opened s previous next res).cells.get slot)
  rw [opened]
  change OpenVisit F i (seedAtSlot p slot) start (p.cells.get slot)
  rw [cell, seed]
  exact .first start.time (by simpa [entryPhase, show ⌊start.phase⌋ = i from rfl] using span)

private def CheckOlderSlot : Prop :=
  ∀ {n : Nat} {F : ℝ → ℝ} {res : ℝ} {s : State ℝ ℝ n} {previous next : Point ℝ},
    Rise F res s previous next →
    ∀ i, Rise.high s next res - (n : Int) ≤ i → (i : ℝ) + 1 ≤ (Rise.left previous res).phase →
      let p := Rise.out s previous next res
      let slot := wrap n i
      (p.cells.get slot = s.payload.cells.get slot ∧ seedAtSlot p slot = seedAtSlot s.payload slot) ∨
        (p.cells.get slot).base = none

private theorem older_of_slot (effects : CheckOlderSlot) : CheckOlder := by
  intro n F res s previous next history visits rise i retained older
  let edge := Rise.right next res
  let p := Rise.out s previous next res
  let slot := wrap n i
  have above : (i : ℝ) + 1 < edge.phase := older.trans_lt rise.gridOrder
  have belowIndex : i < ⌊edge.phase⌋ := by
    have le : i + 1 ≤ ⌊edge.phase⌋ :=
      Int.le_floor.mpr (by simpa only [Int.cast_add, Int.cast_one] using above.le)
    omega
  change (if i = ⌊edge.phase⌋ then OpenVisit F i (seedAtSlot p slot) edge (p.cells.get slot)
    else ClosedVisit F i (seedAtSlot p slot) edge (p.cells.get slot))
  rw [if_neg belowIndex.ne]
  rcases effects rise i retained older with ⟨cell, seed⟩ | empty
  · rw [cell, seed]
    cases oldHighest : s.highest with
    | none =>
      exact .empty _ (by simp [(history.empty_inverse_without_highest oldHighest).1, Vector.get])
    | some high =>
      obtain ⟨old, live, valid⟩ := visits high (by simp [oldHighest])
      have latest : previous ∈ s.previous := by simp [rise.prior]
      have bound := history.previous_phase_le_frontier rise.positive latest live
      have olderThanOld : i < ⌊old.phase⌋ := by
        have le : i + 1 ≤ ⌊old.phase⌋ := Int.le_floor.mpr
          (by simpa only [Int.cast_add, Int.cast_one] using older.trans bound)
        omega
      have highOrder : high ≤ Rise.high s next res := by simp [Rise.high, oldHighest]
      have cert := valid i (by omega) olderThanOld.le
      have cert : ClosedVisit F i (seedAtSlot s.payload slot) old (s.payload.cells.get slot) := by
        simpa [visitAt, olderThanOld.ne, slot] using cert
      obtain ⟨_, obs, member, timeOrder⟩ := history.frontier_exact old live
      have same : obs = previous := Option.mem_unique member latest
      subst obs
      exact cert.later_above (timeOrder.trans rise.segment.1.le) above
  · exact .empty _ empty



private theorem older_slot : CheckOlderSlot := by
  intro n F res s previous next rise i retained older
  let left := Rise.left previous res
  let right := Rise.right next res
  let first := Rise.first previous res
  let last := Rise.last next res
  let highest := Rise.high s next res
  let count := Rise.count previous next res
  let opened := Rise.opened s previous next res
  let scattered := scatter opened left right first count
  let slot := wrap n i
  change highest - (n : Int) ≤ i at retained
  change (i : ℝ) + 1 ≤ left.phase at older
  have highBound : last ≤ highest := le_max_left _ _
  have gridOrder : left.phase ≤ right.phase := rise.gridOrder.le
  have firstLower : ((first - 1 : Int) : ℝ) ≤ left.phase := by
    have ceilBound : (first : ℝ) < left.phase + 1 := Int.ceil_lt_add_one _
    push_cast
    linarith
  have countEnds : first + (count : Int) - 1 = last := by
    have bound : first - 1 ≤ last := Int.le_floor.mpr (firstLower.trans gridOrder)
    dsimp only [count, Rise.count]
    change first + ((last - first + 1).toNat : Int) - 1 = last
    omega
  have leftUpper : left.phase ≤ (first : ℝ) := Int.le_ceil _
  have indexBelow : i + 1 ≤ first := by
    exact_mod_cast older.trans leftUpper
  have scatterUnchanged :
      scattered.cells.get slot = opened.cells.get slot ∧
        seedAtSlot scattered slot = seedAtSlot opened slot := by
    by_cases earlier : i < first - 1
    · exact scatter_preserves_older count opened left right highest first i
        (by rw [countEnds]; exact highBound) retained earlier
    · have predecessor : i = first - 1 := by omega
      have aligned : left.phase = (first : ℝ) := by
        have indexEq : i + 1 = first := by omega
        have castEq : (i : ℝ) + 1 = (first : ℝ) := by exact_mod_cast indexEq
        exact leftUpper.antisymm (castEq ▸ older)
      have nonempty : 0 < count := by
        have bound : first ≤ last := Int.le_floor.mpr (aligned ▸ gridOrder)
        dsimp only [count, Rise.count]
        change 0 < (last - first + 1).toNat
        omega
      change scattered.cells.get (wrap n i) = opened.cells.get (wrap n i) ∧
        seedAtSlot scattered (wrap n i) = seedAtSlot opened (wrap n i)
      rw [predecessor]
      have finish := scatter_first_predecessor count opened left right highest first
        (by rw [countEnds]; exact highBound)
        (by simpa only [← predecessor] using retained) nonempty
      have noFinish : Arithmetic.lt left.phase (edgeAt left right first).phase = false := by
        have edgePhase : (edgeAt left right first).phase = (first : ℝ) := rfl
        change decide (left.phase < (edgeAt left right first).phase) = false
        simp [edgePhase, aligned]
      simpa only [scattered, noFinish, Bool.false_eq_true, if_false] using finish
  have openCell :
      (opened.cells.get slot = s.payload.cells.get slot ∧
        seedAtSlot opened slot = seedAtSlot s.payload slot) ∨ opened.cells.get slot = {} := by
    have floorHigher : i < ⌊left.phase⌋ := by
      have bound : i + 1 ≤ ⌊left.phase⌋ := Int.le_floor.mpr (by exact_mod_cast older)
      omega
    have aliases : slot ≠ wrap n (Arithmetic.floor left.phase) := by
      change wrap n i ≠ wrap n ⌊left.phase⌋
      have floorBound : ⌊left.phase⌋ ≤ highest :=
        (Int.floor_le_floor gridOrder).trans highBound
      intro equal
      have same := wrap_injective_on_retained_window n highest i ⌊left.phase⌋
        retained (floorHigher.le.trans floorBound) (by omega) floorBound equal
      omega
    dsimp only [opened, Rise.opened]
    split
    · exact (beginBranch_other_slot { s with previous := some next } left slot aliases).imp_right And.left
    · exact Or.inl ⟨rfl, rfl⟩
  change (scattered.cells.get slot = s.payload.cells.get slot ∧
    seedAtSlot scattered slot = seedAtSlot s.payload slot) ∨
    (scattered.cells.get slot).base = none
  rw [scatterUnchanged.1, scatterUnchanged.2]
  exact openCell.imp_right (fun cleared => by rw [cleared])

private theorem crossed_cells : CheckCrossed := by
  intro n F res s previous next rise i retained crossed beforeLast
  let a := Rise.left previous res
  let b := Rise.right next res
  let p := Rise.opened s previous next res
  let first := Rise.first previous res
  let last := Rise.last next res
  let count := Rise.count previous next res
  let scattered := scatter p a b first count
  have ending : first + (count : Int) - 1 = last := rise.countEnd
  have fresh := scatter_fresh_cells count p a b (Rise.high s next res) first
    (by rw [ending]; exact Rise.lastLeHigh) Rise.edgeStrict
  rw [ending] at fresh
  obtain ⟨cell, retired⟩ := fresh i retained crossed beforeLast
  have noSeed : seedAtSlot scattered (wrap n i) = none := by
    unfold seedAtSlot
    cases seedEq : scattered.seed with
    | none => rfl
    | some phase =>
      change scattered.seed.any _ = false at retired
      rw [seedEq] at retired
      change (wrap n (Arithmetic.floor phase) == wrap n i) = false at retired
      simpa using retired
  change (if i = last then OpenVisit F i (seedAtSlot scattered (wrap n i)) b
    (scattered.cells.get (wrap n i)) else ClosedVisit F i (seedAtSlot scattered (wrap n i)) b
    (scattered.cells.get (wrap n i)))
  rw [cell, noSeed]
  have lower : a.phase ≤ (i : ℝ) := Int.ceil_le.mp crossed
  by_cases complete : i < last
  · simp only [if_neg complete.ne, freshCell, decide_eq_true_eq, if_pos complete]
    have upper : (i : ℝ) + 1 ≤ b.phase := by
      exact_mod_cast (Int.le_floor.mp (show i + 1 ≤ last by omega))
    apply ClosedVisit.first
    simpa [cellSpan, entryPhase, Point.exact, edgeAt, real_index, a, b, Rise.left, Rise.right] using
      rise.segment.grid_interval_certified rise.continuous rise.positive rise.forward
        lower (show (i : ℝ) ≤ (i : ℝ) + 1 by linarith) upper le_rfl le_rfl
  · have final : i = last := le_antisymm beforeLast (le_of_not_gt complete)
    simp only [if_pos final, freshCell, decide_eq_true_eq, if_neg complete]
    have lowerRight : (i : ℝ) ≤ b.phase := by rw [final]; exact Int.floor_le _
    have upperRight : b.phase ≤ (i : ℝ) + 1 := by rw [final]; exact (Int.lt_floor_add_one _).le
    apply OpenVisit.first
    have span := rise.segment.grid_interval_certified rise.continuous rise.positive rise.forward
      lower lowerRight le_rfl le_rfl upperRight
    change cellSpan F i next.time ⟨i, interpolate a b i⟩
      ⟨b.phase, interpolate a b b.phase⟩ at span
    rw [show interpolate a b b.phase = next.time from rise.interpolate_end] at span
    simpa [entryPhase, edgeAt, real_index, b, Rise.right] using span

private theorem finish_start : CheckFinishStart := by
  intro n F res s previous next rise retained fractional crossing visit
  let i := Rise.startIndex previous res
  let a := Rise.left previous res
  let b := Rise.right next res
  let p := Rise.opened s previous next res
  let first := Rise.first previous res
  let last := Rise.last next res
  let count := Rise.count previous next res
  let edge := edgeAt a b first
  let scattered := scatter p a b first count
  have firstEq : first = i + 1 := Rise.first_after_fractional fractional
  have crossed : i < last := crossing
  have firstBound : first ≤ last := by omega
  have predEq : first - 1 = i := by omega
  have ending : first + (count : Int) - 1 = last := rise.countEnd
  have nonempty : 0 < count := by omega
  have lo : Rise.high s next res - (n : Int) ≤ first - 1 := by
    rw [predEq]
    exact retained
  have edgePhase : edge.phase = (i : ℝ) + 1 := by simp [edge, edgeAt, real_index, firstEq]
  have edgeAfter : a.phase < edge.phase := by
    rw [edgePhase]
    exact Int.lt_floor_add_one _
  have edgeBefore : edge.phase ≤ b.phase := by
    change Arithmetic.ofIndex first ≤ b.phase
    rw [real_index]
    exact Int.le_floor.mp firstBound
  have positiveEdge : Arithmetic.lt a.phase edge.phase = true := by simpa [real_lt] using edgeAfter
  have effects := scatter_first_predecessor count p a b (Rise.high s next res) first
    (by rw [ending]; exact Rise.lastLeHigh) lo nonempty
  change scattered.cells.get (wrap n (first - 1)) =
    (if Arithmetic.lt a.phase edge.phase then finishCell p (first - 1) edge.time else p).cells.get
      (wrap n (first - 1)) ∧
    seedAtSlot scattered (wrap n (first - 1)) = seedAtSlot p (wrap n (first - 1)) at effects
  rw [positiveEdge, if_pos rfl, predEq, slots_finish] at effects
  have chord := rise.startChord edge.phase edgeAfter.le edgeBefore edgePhase.le
  change cellSpan F i edge.time a edge at chord
  have extended := OpenVisit.advance visit rise.continuous chord
  have edgeTime : edge.time ≤ b.time :=
    (rise.segment.interpolate_exact rise.positive rise.forward edgeAfter.le edgeBefore).2.2
  have touch (equal : b.phase = (i : ℝ) + 1) : b.time = edge.time := by
    change next.time = interpolate a b edge.phase
    rw [edgePhase, ← equal]
    exact rise.interpolate_end.symm
  change (if i = last then OpenVisit F i (seedAtSlot scattered (wrap n i)) b
    (scattered.cells.get (wrap n i)) else ClosedVisit F i (seedAtSlot scattered (wrap n i)) b
    (scattered.cells.get (wrap n i)))
  rw [if_neg crossed.ne, effects.1, effects.2]
  exact extended.finish edgePhase edgeTime touch

private theorem keepCut_exact (frontier upper newCut oldCut : ℝ) :
    keepCut frontier upper newCut oldCut = true ↔ frontier ≤ upper ∧ oldCut ≤ newCut := by
  change (decide (frontier ≤ upper) && !decide (newCut < oldCut)) = true ↔ _
  simp [not_lt]

private theorem reopen_fresh {F : ℝ → ℝ} {i : Int} {seed : Option ℝ}
    {next : Point ℝ} {base stop : ℝ}
    (fallback : cellSpan F i next.time ⟨entryPhase i seed, base⟩ ⟨(i : ℝ) + 1, stop⟩)
    (exactPhase : F next.time = next.phase) (lo : (i : ℝ) ≤ next.phase)
    (hi : next.phase ≤ (i : ℝ) + 1) (continuous : Continuous F) :
    OpenVisit F i seed next ⟨some base, some stop, some ⟨next.phase, next.time - base, none⟩⟩ := by
  refine .replacement base stop next.phase (next.time - base) fallback ?_
  have timeEq : base + (next.time - base) = next.time := by ring
  rw [timeEq]
  cases next
  exact cellSpan.point exactPhase lo hi continuous

private theorem reopen_open : CheckReopenOpen := by
  intro F i seed old next cell visit oldBound context
  obtain ⟨continuous, order, exactPhase, lo, hi, entry, bridge⟩ := context
  cases visit with
  | first base span =>
    exact .first base (span.extend continuous order entry exactPhase hi (bridge oldBound))
  | replacement base stop cut delta fallback span =>
    by_cases keep : keepCut old.phase ((i : ℝ) + 1) next.phase cut = true
    · simp only [reopenVisit, keep, if_true]
      exact .replacement base stop cut delta (fallback.later order)
        (span.extend continuous order ((keepCut_exact _ _ _ _).mp keep).2 exactPhase hi (bridge oldBound))
    · simpa [reopenVisit, keep] using
        reopen_fresh (fallback.later order) exactPhase lo hi continuous

private theorem reopen_closed : CheckReopenClosed := by
  intro F i seed old next cell visit above present context
  obtain ⟨continuous, order, exactPhase, lo, hi, _, bridge⟩ := context
  cases visit with
  | empty cell absent => exact False.elim (present absent)
  | first base stop span =>
    exact reopen_fresh (span.later order) exactPhase lo hi continuous
  | replacement base stop cut delta finish fallback span touch =>
    by_cases keep : keepCut old.phase ((i : ℝ) + 1) next.phase cut = true
    · simp only [reopenVisit, keep, if_true]
      obtain ⟨oldBound, cutBound⟩ := (keepCut_exact _ _ _ _).mp keep
      have atBoundary := le_antisymm oldBound above
      have atTime := touch atBoundary
      have stopEq : (⟨(i : ℝ) + 1, base + finish⟩ : Point ℝ) = old := by
        rw [← atBoundary, atTime]
      rw [stopEq] at span
      exact .replacement base stop cut delta (fallback.later order)
        (span.extend continuous order cutBound exactPhase hi (bridge oldBound))
    · simpa [reopenVisit, keep] using
        reopen_fresh (fallback.later order) exactPhase lo hi continuous

private theorem reuse_slot : CheckReuseSlot := by
  intro n s res previous next old fractional opens noReset frontierEq
  let start := Rise.left previous res
  let i := Rise.startIndex previous res
  let slot := wrap n i
  let cell := s.payload.cells.get slot
  let observed := { s with previous := some next }
  have notGrid : start.phase ≠ (i : ℝ) := ne_of_gt fractional
  have offGrid : (start.phase == (Arithmetic.ofIndex (Arithmetic.floor start.phase) : ℝ)) = false := by
    exact beq_eq_false_iff_ne.mpr notGrid
  have noReset' :
      (!retained s.highest n (Arithmetic.floor start.phase) ||
        (s.payload.cells.get (wrap n (Arithmetic.floor start.phase))).base.isNone ||
        s.payload.seed.any (fun phase => (Arithmetic.floor phase == Arithmetic.floor start.phase) &&
          Arithmetic.lt start.phase phase)) = false := noReset
  simp only [Rise.opened, opens, if_true]
  change ((beginBranch observed start).payload.cells.get slot = reopenVisit i old start cell ∧
    seedAtSlot (beginBranch observed start).payload slot = seedAtSlot s.payload slot)
  unfold beginBranch
  simp only [Id.run, pure, offGrid, Bool.false_eq_true, if_false]
  rw [noReset']
  simp only [Bool.false_eq_true, if_false]
  have indexEq : Arithmetic.floor start.phase = i := rfl
  have idx : (Arithmetic.ofIndex (i + 1) : ℝ) = (i : ℝ) + 1 := by
    change (↑(i + 1 : Int) : ℝ) = _
    push_cast
    rfl
  simp only [indexEq, show observed.payload = s.payload from rfl, frontierEq]
  dsimp only [slot, cell]
  generalize stored : s.payload.cells.get (wrap n i) = rawCell
  have enc (base : ℝ) : Arithmetic.encode base start.time = start.time - base := rfl
  cases hb : rawCell.base with
  | none => simpa [hb, reopenVisit, seedAtSlot] using stored
  | some base =>
    cases hs : rawCell.fallbackEnd with
    | none => simpa [hb, hs, reopenVisit, seedAtSlot] using stored
    | some stop =>
      cases branchEq : rawCell.replacement <;>
        simp [hb, hs, branchEq, reopenVisit, Option.any, idx, enc, get_set_same, seedAtSlot]
      split <;> split <;> simp [get_set_same]

theorem SampledWrites.visits {n : Nat} {F : ℝ → ℝ} {res : ℝ} {s : State ℝ ℝ n}
    (history : SampledWrites F res n s) (continuous : Continuous F) (positive : 0 < res) :
    Visits F s :=
  history.visits_of_cases (older_of_slot older_slot) crossed_cells seed_start
    (reuse_of_cases reuse_slot reopen_open reopen_closed) continue_start stay_inside finish_start
    continuous positive

theorem SampledWrites.retained_cells_certified {n : Nat} {F : ℝ → ℝ} {res : ℝ}
    {s : State ℝ ℝ n} (history : SampledWrites F res n s)
    (continuous : Continuous F) (positive : 0 < res) : RetainedCellsCertified F res s :=
  (history.visits continuous positive).retained_cells_certified history positive

theorem first_predecessor_certified : FirstPredecessorCertified := by
  intro n F res s continuous positive history _ _ previous next prior segment _ _ query retained _ interval selected
  have onTrajectory : F next.time = next.phase * res := by
    simpa using segment.2 1 (by norm_num) le_rfl
  have nextHistory : SampledWrites F res n (write s res next) :=
    history.observe next onTrajectory (by simpa [prior] using segment)
  exact nextHistory.retained_cells_certified continuous positive next (by simp [write_previous])
    query retained interval selected

theorem reachable_read_soundness : ReachableReadSoundness :=
  reachable_read_soundness_of_predecessor first_predecessor_certified

end
end PhaseTime.InverseRuntime

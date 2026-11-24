import DSP.Resynthesis.FrameStorage

namespace DSP.Resynthesis.FrameStorage

open SpectrumStorage (Lane Bin forLanes)

variable {R P : Type}

private theorem foldlM_range {S : Type} (n : Nat) (step : Fin n → StateM S PUnit) :
    Fin.foldlM n (fun _ i => step i) (⟨⟩ : PUnit) =
      (List.finRange n).foldlM (fun _ i => step i) ⟨⟩ := by
  exact Fin.foldlM_eq_foldlM_finRange ..

private theorem foldFocus {S A V : Type} [DecidableEq A]
    (items : List A) (distinct : items.Nodup)
    (step : A → StateM S PUnit) (inv : S → Prop) (observe : S → V)
    (target : Option A) (update : V → V)
    (step_ok : ∀ s, inv s → ∀ i,
      inv ((step i).run s).2 ∧
        observe ((step i).run s).2 = if some i = target then update (observe s) else observe s)
    (state : S) (valid : inv state) :
    let final := ((items.foldlM (fun _ i => step i) (⟨⟩ : PUnit)).run state).2
    inv final ∧
      observe final = if (∃ i ∈ items, some i = target) then update (observe state) else observe state := by
  classical
  induction items generalizing state with
  | nil => simpa [StateT.run, StateT.pure, Pure.pure] using valid
  | cons i items ih =>
    obtain ⟨notMem, distinct⟩ := List.nodup_cons.mp distinct
    obtain ⟨nextValid, nextObserve⟩ := step_ok state valid i
    have result := ih distinct _ nextValid
    rw [List.foldlM_cons, StateT.run_bind]
    let final := ((items.foldlM (fun _ a => step a) (⟨⟩ : PUnit)).run ((step i).run state).2).2
    change inv final ∧ observe final = _
    simp only [List.mem_cons, exists_eq_or_imp]
    refine ⟨result.1, ?_⟩
    rw [result.2, nextObserve]
    by_cases hit : some i = target
    · have absent : ¬∃ j ∈ items, some j = target := by
        rintro ⟨j, member, equal⟩
        exact notMem (Option.some.inj (equal.trans hit.symm) ▸ member)
      simp [hit, absent]
    · simp [hit]

private theorem forOneBin (bin : Bin)
    (step : Bin → StateM (Scene R P) PUnit)
    {V : Type} (inv : Scene R P → Prop) (observe : Scene R P → V) (update : V → V)
    (step_ok : ∀ s, inv s → ∀ b, inv ((step b).run s).2 ∧
      observe ((step b).run s).2 = if b = bin then update (observe s) else observe s)
    (state : Scene R P) (valid : inv state) :
    let final := ((forBins step).run state).2
    inv final ∧ observe final = update (observe state) := by
  rw [forBins, foldlM_range]
  simpa using foldFocus (List.finRange 2047) (List.nodup_finRange _) step inv observe
    (some bin) update (by simpa using step_ok) state valid

private theorem forOneLane (count : Fin 5) (lane : Lane)
    (step : Lane → StateM (Scene R P) PUnit)
    {V : Type} (inv : Scene R P → Prop) (observe : Scene R P → V) (update : V → V)
    (step_ok : ∀ s, inv s → ∀ l, inv ((step l).run s).2 ∧
      observe ((step l).run s).2 = if l = lane then update (observe s) else observe s)
    (state : Scene R P) (valid : inv state) :
    let final := ((forLanes count step).run state).2
    inv final ∧ observe final = if lane.val < count.val then update (observe state) else observe state := by
  let asLane := fun (i : Fin count.val) => (⟨i.val, by have := i.isLt; have := count.isLt; omega⟩ : Lane)
  let target := if h : lane.val < count.val then some (⟨lane.val, h⟩ : Fin count.val) else none
  have result := foldFocus (List.finRange count.val) (List.nodup_finRange _)
    (fun i => step (asLane i)) inv observe target update (by
      intro s hs i
      have hit : some i = target ↔ asLane i = lane := by
        by_cases h : lane.val < count.val
        · simp [target, asLane, h, Fin.ext_iff]
        · have neq : i.val ≠ lane.val := fun eq => h (eq ▸ i.isLt)
          simp [target, asLane, h, Fin.ext_iff, neq]
      simpa only [hit] using step_ok s hs (asLane i)) state valid
  unfold forLanes
  rw [foldlM_range]
  by_cases h : lane.val < count.val <;> simpa [target, h] using result

private theorem forSelectedBin (count : Fin 5) (lane : Lane) (bin : Bin)
    (step : Lane → Bin → StateM (Scene R P) PUnit)
    {V : Type} (inv : Scene R P → Prop) (observe : Scene R P → V) (update : V → V)
    (step_ok : ∀ s, inv s → ∀ l b,
      inv ((step l b).run s).2 ∧
        observe ((step l b).run s).2 = if l = lane ∧ b = bin then update (observe s) else observe s)
    (state : Scene R P) (valid : inv state) :
    let final := ((forLanes count fun l => forBins (step l)).run state).2
    inv final ∧ observe final = if lane.val < count.val then update (observe state) else observe state := by
  apply forOneLane count lane _ inv observe update _ state valid
  intro s hs l
  by_cases hit : l = lane
  · subst l
    simp only [if_true]
    exact forOneBin bin (step lane) inv observe update (by
      intro t ht b
      simpa using step_ok t ht lane b) s hs
  · simp only [hit, if_false]
    have result := forOneBin bin (step l) inv observe id (by
      intro t ht b
      simpa [hit] using step_ok t ht l b) s hs
    exact result

private theorem get_set {A : Type} {n : Nat} (table : BinTable A n)
    (i j : ActiveBin n) (value : A) :
    (table.set i value).get j = if i = j then value else table.get j := by
  by_cases h : i = j
  · subst j
    simp [BinTable.set, BinTable.get]
  · have hval : i.val ≠ j.val := fun equal => h (Fin.ext equal)
    simp [BinTable.set, BinTable.get, h, hval]

/-- Fields read by the downstream graph, synthesis and render stages. -/
def downstreamFields (s : Scene R P) :=
  (s.spectrum.math, s.spectrum.controls, s.spectrum.gates, s.spectrum.graph,
    s.spectrum.phases, s.spectrum.output, s.spectrum.drive, s.forward, s.inverse,
    s.window, s.draw, s.rng, s.frame)

/-- Priming measures the loaded DFT in all active lanes and preserves the analysis inputs
that are read by later stages. -/
theorem primeCoefficients_run (state : Scene R P) (count : Fin 5) (lane : Lane) (bin : Bin) :
    let final := ((primeCoefficients count).run state).2
    final.spectrum.math = state.spectrum.math ∧ final.dft = state.dft ∧
    final.forward = state.forward ∧ final.spectrum.magnitudes = state.spectrum.magnitudes ∧
    final.slew = state.slew ∧ downstreamFields final = downstreamFields state ∧
    ((final.spectrum.input lane).prevMagnitude.get bin, (final.prevPhase lane).get bin) =
      if lane.val < count.val then
        (state.spectrum.math.magnitude ((state.dft lane).coeff bin.coefficient),
          state.spectrum.math.phase ((state.dft lane).coeff bin.coefficient))
      else ((state.spectrum.input lane).prevMagnitude.get bin, (state.prevPhase lane).get bin) := by
  let inv := fun (s : Scene R P) =>
    s.spectrum.math = state.spectrum.math ∧ s.dft = state.dft ∧
      s.forward = state.forward ∧
      s.spectrum.magnitudes = state.spectrum.magnitudes ∧ s.slew = state.slew ∧
      downstreamFields s = downstreamFields state
  let observe := fun (s : Scene R P) =>
    ((s.spectrum.input lane).prevMagnitude.get bin, (s.prevPhase lane).get bin)
  let update := fun (_ : R × R) =>
    (state.spectrum.math.magnitude ((state.dft lane).coeff bin.coefficient),
      state.spectrum.math.phase ((state.dft lane).coeff bin.coefficient))
  have result := forSelectedBin count lane bin (fun l b => do
    let value ← measurement l b
    setPrevious l b value) inv observe update (by
      intro s hs l b
      unfold measurement setPrevious
      refine ⟨hs, ?_⟩
      obtain ⟨hMath, hDft, _⟩ := hs
      simp only [observe, update, StateT.run, Bind.bind, StateT.bind]
      by_cases hLane : l = lane
      · subst l
        simp only [Function.update_self, true_and, get_set, hMath, hDft]
        by_cases hBin : b = bin <;> simp [hBin]
      · simp [Function.update_of_ne (Ne.symm hLane), hLane]) state
    ⟨rfl, rfl, rfl, rfl, rfl, rfl⟩
  exact ⟨result.1.1, result.1.2.1, result.1.2.2.1, result.1.2.2.2.1,
    result.1.2.2.2.2.1, result.1.2.2.2.2.2, result.2⟩

theorem prepareSlew_run (state : Scene R P) (count : Fin 5) (lane : Lane) :
    let final := ((forLanes count prepareSlew).run state).2
    {final with slew := state.slew} = state ∧
    final.slew lane = if lane.val < count.val then (state.spectrum.controls lane).slewUp else state.slew lane := by
  let inv := fun (s : Scene R P) => {s with slew := state.slew} = state
  apply forOneLane count lane prepareSlew inv (fun s => s.slew lane)
    (fun _ => (state.spectrum.controls lane).slewUp)
  · intro s hs l
    change {s with slew := state.slew} = state at hs
    have hControls := congrArg (fun t : Scene R P => t.spectrum.controls) hs
    change s.spectrum.controls = state.spectrum.controls at hControls
    refine ⟨hs, ?_⟩
    change Function.update s.slew l (s.spectrum.controls l).slewUp lane = _
    by_cases hit : l = lane
    · subst l
      simp [hControls]
    · simp [hit, Function.update_of_ne (Ne.symm hit)]
  · rfl

private theorem table_ext {A : Type} {n : Nat} {x y : BinTable A n}
    (h : ∀ bin : ActiveBin n, x.get bin = y.get bin) : x = y := by
  cases x
  cases y
  congr
  ext i h₁
  exact h ⟨i, h₁⟩

private theorem slew_get {n : Nat} [OfScientific R] [OfNat R 1] [LE R] [DecidableLE R]
    [Sub R] [Add R] [Mul R] [IfLess R] (math : GrainMath R P) (cutoff : R)
    (prior target : BinTable R n) (bin : ActiveBin n) :
    (slewMagnitudes math cutoff prior target).get bin =
      slewMagnitude (prior.get bin) (target.get bin)
        (if (0.5 : R) ≤ cutoff then 1 else math.slewCoefficient cutoff) := by
  let alpha := if (0.5 : R) ≤ cutoff then 1 else math.slewCoefficient cutoff
  let f := fun i : ActiveBin n => slewMagnitude (prior.get i) (target.get i) alpha
  change (Vector.ofFn f)[bin.val]'bin.isLt = f bin
  exact Vector.getElem_ofFn bin.isLt

private theorem preparedAlpha_run [OfScientific R] [OfNat R 1] [LE R] [DecidableLE R]
    (lane : Lane) (state : Scene R P)
    (step : R → StateM (Scene R P) PUnit) :
    let cutoff := state.slew lane
    let alpha : R := if (0.5 : R) ≤ cutoff then 1 else state.spectrum.math.slewCoefficient cutoff
    ((do
      let cutoff ← slewCutoff lane
      let a ← if (0.5 : R) ≤ cutoff then pure 1 else slewCoefficient cutoff
      step a) : StateM (Scene R P) PUnit).run state = (step alpha).run state := by
  dsimp only
  unfold slewCutoff slewCoefficient
  by_cases h : (0.5 : R) ≤ state.slew lane
  · simp only [StateT.run, Bind.bind, StateT.bind, h, if_true]
    rfl
  · simp only [StateT.run, Bind.bind, StateT.bind, h, if_false]

section Analysis

variable [OfScientific R] [OfNat R 1] [LE R] [DecidableLE R] [Div R]
    [Sub R] [Add R] [Mul R] [IfLess R]
    [Add P] [Sub P] [Mul P] [Div P] [OfNat P 1] [OfScientific P] [IfLess P]

private def analyzeBin (widen : R → P) (natSample : Nat → R) (natPhase : Nat → P)
    (floor : P → P) (lane : Lane) (alpha : R) (bin : Bin) : StateM (Scene R P) PUnit := do
  let value ← measurement lane bin
  let previous ← previousPhase lane bin
  setCurrent lane bin value (frequency widen natSample natPhase floor bin value.phase previous)
  let old ← magnitude lane bin
  setMagnitude lane bin (slewMagnitude old value.magnitude alpha)

private def analyzeLane (widen : R → P) (natSample : Nat → R) (natPhase : Nat → P)
    (floor : P → P) (lane : Lane) : StateM (Scene R P) PUnit := do
  let cutoff ← slewCutoff lane
  let a ← if (0.5 : R) ≤ cutoff then pure 1 else slewCoefficient cutoff
  forBins (analyzeBin widen natSample natPhase floor lane a)

private theorem analyzeLane_run (widen : R → P) (natSample : Nat → R) (natPhase : Nat → P)
    (floor : P → P) (lane : Lane) (state : Scene R P) :
    let cutoff := state.slew lane
    let alpha : R := if (0.5 : R) ≤ cutoff then 1 else state.spectrum.math.slewCoefficient cutoff
    (analyzeLane widen natSample natPhase floor lane).run state =
      (forBins (analyzeBin widen natSample natPhase floor lane alpha)).run state :=
  preparedAlpha_run lane state fun alpha => forBins (analyzeBin widen natSample natPhase floor lane alpha)

/-- Measurement uses the loaded DFT, previous phase, and the prepared cutoff. Each active
bin updates its own analysis and slew values once. -/
theorem analyzeCoefficients_run (state : Scene R P) (count : Fin 5) (lane : Lane) (bin : Bin)
    (widen : R → P) (natSample : Nat → R) (natPhase : Nat → P) (floor : P → P) :
    let final := ((analyzeCoefficients widen natSample natPhase floor count).run state).2
    let input := final.spectrum.input lane
    let old := state.spectrum.input lane
    let value := (state.dft lane).coeff bin.coefficient
    let mag := state.spectrum.math.magnitude value
    let phase := state.spectrum.math.phase value
    let cutoff := state.slew lane
    let alpha : R := if (0.5 : R) ≤ cutoff then 1 else state.spectrum.math.slewCoefficient cutoff
    downstreamFields final = downstreamFields state ∧ input.prevMagnitude = old.prevMagnitude ∧
    (input.magnitude.get bin, input.phase.get bin, input.omega.get bin,
      (final.spectrum.magnitudes lane).get bin) =
      if lane.val < count.val then
        (mag, phase, frequency widen natSample natPhase floor bin phase ((state.prevPhase lane).get bin),
          slewMagnitude ((state.spectrum.magnitudes lane).get bin) mag alpha)
      else (old.magnitude.get bin, old.phase.get bin, old.omega.get bin,
        (state.spectrum.magnitudes lane).get bin) := by
  let inv := fun (s : Scene R P) =>
    s.spectrum.math = state.spectrum.math ∧ s.dft = state.dft ∧ s.slew = state.slew ∧
      s.prevPhase = state.prevPhase ∧
      (s.spectrum.input lane).prevMagnitude = (state.spectrum.input lane).prevMagnitude ∧
      downstreamFields s = downstreamFields state
  let observe := fun (s : Scene R P) =>
    let input := s.spectrum.input lane
    (input.magnitude.get bin, input.phase.get bin, input.omega.get bin, (s.spectrum.magnitudes lane).get bin)
  let alpha := fun (l : Lane) =>
    if (0.5 : R) ≤ state.slew l then (1 : R) else state.spectrum.math.slewCoefficient (state.slew l)
  let update := fun (old : R × R × P × R) =>
    let value := (state.dft lane).coeff bin.coefficient
    let mag := state.spectrum.math.magnitude value
    let phase := state.spectrum.math.phase value
    (mag, phase, frequency widen natSample natPhase floor bin phase ((state.prevPhase lane).get bin),
      slewMagnitude old.2.2.2 mag (alpha lane))
  have bin_ok (s : Scene R P) (hs : inv s) (l : Lane) (b : Bin) :
      let next := ((analyzeBin widen natSample natPhase floor l (alpha l) b).run s).2
      inv next ∧
        observe next = if l = lane ∧ b = bin then update (observe s) else observe s := by
    obtain ⟨hMath, hDft, hSlew, hPrev, hMag, hDown⟩ := hs
    unfold analyzeBin measurement previousPhase setCurrent magnitude setMagnitude
    simp only [StateT.run, Bind.bind, StateT.bind]
    by_cases hLane : l = lane
    · subst l
      simp only [inv, observe, update, Function.update_self, get_set, true_and]
      refine ⟨⟨hMath, hDft, hSlew, hPrev, hMag, hDown⟩, ?_⟩
      by_cases hBin : b = bin
      · subst b
        simp [hMath, hDft, hPrev]
      · simp [hBin]
    · refine ⟨⟨hMath, hDft, hSlew, hPrev, ?_, hDown⟩, ?_⟩
      · simpa only [Function.update_of_ne (Ne.symm hLane)] using hMag
      · simp only [observe, Function.update_of_ne (Ne.symm hLane), hLane, false_and, if_false]
  have result := forOneLane count lane (analyzeLane widen natSample natPhase floor) inv observe update (by
    intro s hs l
    have hMath := hs.1
    have hSlew := hs.2.2.1
    rw [analyzeLane_run]
    have hAlpha : (if (0.5 : R) ≤ s.slew l then (1 : R)
      else s.spectrum.math.slewCoefficient (s.slew l)) = alpha l := by
      rw [hSlew, hMath]
    rw [hAlpha]
    by_cases hit : l = lane
    · subst l
      simp only [if_true]
      exact forOneBin bin _ inv observe update (by
        intro t ht b
        simpa using bin_ok t ht lane b) s hs
    · simp only [hit, if_false]
      exact forOneBin bin _ inv observe id (by
        intro t ht b
        simpa [hit] using bin_ok t ht l b) s hs) state ⟨rfl, rfl, rfl, rfl, rfl, rfl⟩
  unfold analyzeCoefficients
  exact ⟨result.1.2.2.2.2.2, result.1.2.2.2.2.1, result.2⟩

/-- Analysis projects onto the Grain model for every active lane. The transform is supplied
by the scene and the analysis calls the same numerical operations in both models. -/
theorem analyzeFrame_run (state : Scene R P) (count : Fin 5) (lane : Lane)
    (active : lane.val < count.val) :
    let math := state.spectrum.math
    let previous := state.forward true lane
    let current := state.forward false lane
    let input := analysisInput math previous current
    let final := ((analyzeFrame math.widen math.natSample math.natPhase math.phaseFloor count).run state).2
    downstreamFields final = downstreamFields state ∧
    final.spectrum.input lane = input ∧
    final.spectrum.magnitudes lane =
      slewMagnitudes math (state.spectrum.controls lane).slewUp
        (state.spectrum.magnitudes lane) input.magnitude := by
  let prepared := ((forLanes count prepareSlew).run state).2
  let previous := {prepared with dft := prepared.forward true}
  let primed := ((primeCoefficients count).run previous).2
  let current := {primed with dft := primed.forward false}
  let math := state.spectrum.math
  let input := analysisInput math (state.forward true lane) (state.forward false lane)
  let final := ((analyzeCoefficients math.widen math.natSample math.natPhase math.phaseFloor count).run current).2
  change downstreamFields final = downstreamFields state ∧
    final.spectrum.input lane = input ∧ final.spectrum.magnitudes lane = _
  obtain ⟨hPre, hCutoff⟩ := prepareSlew_run state count lane
  have previousDown := congrArg (fun s : Scene R P => downstreamFields s) hPre
  have previousMath := congrArg (fun s : Scene R P => s.spectrum.math) hPre
  change previous.spectrum.math = math at previousMath
  have previousForward := congrArg (fun s : Scene R P => s.forward) hPre
  have previousMag := congrArg (fun s : Scene R P => s.spectrum.magnitudes) hPre
  obtain ⟨primedMath, _, primedForward, primedMag, primedCutoff, primedDown, _⟩ :=
    primeCoefficients_run previous count lane (0 : Bin)
  have measuredDown (bin : Bin) := analyzeCoefficients_run current count lane bin
    math.widen math.natSample math.natPhase math.phaseFloor
  have currentMath : current.spectrum.math = math := primedMath.trans previousMath
  have currentDft : current.dft = state.forward false := congrArg (fun f => f false)
    (primedForward.trans previousForward)
  have currentMag : current.spectrum.magnitudes = state.spectrum.magnitudes :=
    primedMag.trans previousMag
  have currentCutoff : current.slew lane = (state.spectrum.controls lane).slewUp :=
    (congrFun primedCutoff lane).trans (by simpa only [if_pos active] using hCutoff)
  have expected (bin : Bin) :
      let measured := final.spectrum.input lane
      let value := (state.forward false lane).coeff bin.coefficient
      measured.prevMagnitude.get bin =
          math.magnitude ((state.forward true lane).coeff bin.coefficient) ∧
      (measured.magnitude.get bin, measured.phase.get bin, measured.omega.get bin,
          (final.spectrum.magnitudes lane).get bin) =
        (math.magnitude value, math.phase value,
          frequency math.widen math.natSample math.natPhase math.phaseFloor bin (math.phase value)
            (math.phase ((state.forward true lane).coeff bin.coefficient)),
          slewMagnitude ((state.spectrum.magnitudes lane).get bin) (math.magnitude value)
            (if (0.5 : R) ≤ (state.spectrum.controls lane).slewUp then
              1 else math.slewCoefficient (state.spectrum.controls lane).slewUp)) := by
    obtain ⟨_, hPreserve, hMeasure⟩ := measuredDown bin
    have hPrime := (primeCoefficients_run previous count lane bin).2.2.2.2.2.2
    simp only [if_pos active] at hPrime hMeasure
    have hPrevPhase := congrArg Prod.snd hPrime
    have hPrevMag := congrArg Prod.fst hPrime
    change (current.prevPhase lane).get bin = _ at hPrevPhase
    change (current.spectrum.input lane).prevMagnitude.get bin = _ at hPrevMag
    have prevDft : previous.dft = state.forward true :=
      congrArg (fun f => f true) previousForward
    change (final.spectrum.input lane).prevMagnitude.get bin = _ ∧ _
    rw [hPreserve]
    refine ⟨?_, ?_⟩
    · simpa only [previousMath, prevDft] using hPrevMag
    · simpa only [currentMath, currentDft, currentMag, currentCutoff, hPrevPhase,
        previousMath, prevDft] using hMeasure
  refine ⟨(measuredDown 0).1.trans (primedDown.trans previousDown), ?_, ?_⟩
  · have prev := table_ext (x := (final.spectrum.input lane).prevMagnitude)
      (y := input.prevMagnitude) (fun bin => by
        simpa [input, analysisInput, BinTable.get, BinTable.ofFn] using (expected bin).1)
    have mag := table_ext (x := (final.spectrum.input lane).magnitude)
      (y := input.magnitude) (fun bin => by
        simpa [input, analysisInput, BinTable.get, BinTable.ofFn] using congrArg Prod.fst (expected bin).2)
    have ph := table_ext (x := (final.spectrum.input lane).phase)
      (y := input.phase) (fun bin => by
        simpa [input, analysisInput, BinTable.get, BinTable.ofFn] using
          congrArg (fun tuple => tuple.2.1) (expected bin).2)
    have freq := table_ext (x := (final.spectrum.input lane).omega)
      (y := input.omega) (fun bin => by
        simpa [input, analysisInput, frequency, BinTable.get, BinTable.ofFn] using
          congrArg (fun tuple => tuple.2.2.1) (expected bin).2)
    generalize hX : final.spectrum.input lane = x at prev mag ph freq ⊢
    generalize hY : input = y at prev mag ph freq ⊢
    cases x
    cases y
    cases prev
    cases mag
    cases ph
    cases freq
    rfl
  · apply table_ext
    intro bin
    rw [slew_get]
    have currentMagnitude : input.magnitude.get bin =
        math.magnitude ((state.forward false lane).coeff bin.coefficient) := by
      let f := fun i : Bin => math.magnitude ((state.forward false lane).coeff i.coefficient)
      change (Vector.ofFn f)[bin.val]'bin.isLt = f bin
      exact Vector.getElem_ofFn bin.isLt
    rw [currentMagnitude]
    exact congrArg (fun tuple => tuple.2.2.2) (expected bin).2

end Analysis

end DSP.Resynthesis.FrameStorage

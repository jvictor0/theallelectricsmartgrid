import DSP.Resynthesis.SpectrumStorage

namespace DSP.Resynthesis.SpectrumStorage

variable {R P : Type}

def replaceOutput (state : Scene R P) (lane : Lane)
    (output : BinTable (ComplexValue R) 4096) : Scene R P :=
  { state with output := Function.update state.output lane output }

@[simp] theorem replaceOutput_here (state : Scene R P) (lane : Lane)
    (output : BinTable (ComplexValue R) 4096) :
    (replaceOutput state lane output).output lane = output :=
  Function.update_self ..

theorem replaceOutput_twice (state : Scene R P) (lane : Lane)
    (first second : BinTable (ComplexValue R) 4096) :
    replaceOutput (replaceOutput state lane first) lane second =
      replaceOutput state lane second := by
  simp [replaceOutput, Function.update_idem]

theorem replaceOutput_initial (state : Scene R P) (lane : Lane) :
    replaceOutput state lane (state.output lane) = state := by
  cases state
  simp [replaceOutput, Function.update_eq_self]

theorem scatter_run [Add R] (lane : Lane) (bin : Bin) (radius : R) (angle : P)
    (state : Scene R P) :
    (scatter lane bin radius angle).run state =
      (⟨⟩, replaceOutput state lane
        (addCoefficient (state.output lane) bin (state.math.polar radius angle))) :=
  rfl

theorem foldlM_finRange {m : Type → Type} [Monad m] [LawfulMonad m] {A : Type}
    (k : Nat) (f : A → Fin k → m A) (init : A) :
    Fin.foldlM k f init = (List.finRange k).foldlM f init := by
  induction k generalizing init with
  | zero => simp [Fin.foldlM_zero]
  | succ k ih =>
    rw [Fin.foldlM_succ, List.finRange_succ, List.foldlM_cons]
    simp only [List.foldlM_map]
    congr 1
    funext acc
    exact ih _ acc

theorem foldOutput_run {A : Type} (bins : List A) (lane : Lane)
    (state : Scene R P) (output : BinTable (ComplexValue R) 4096)
    (step : A → StateM (Scene R P) PUnit)
    (update : BinTable (ComplexValue R) 4096 → A → BinTable (ComplexValue R) 4096)
    (h : ∀ output bin, (step bin).run (replaceOutput state lane output) =
      (⟨⟩, replaceOutput state lane (update output bin))) :
    (bins.foldlM (fun _ bin => step bin) (⟨⟩ : PUnit)).run
      (replaceOutput state lane output) =
    (⟨⟩, replaceOutput state lane (bins.foldl update output)) := by
  induction bins generalizing output with
  | nil => rfl
  | cons bin bins ih =>
    simp only [List.foldlM_cons, List.foldl_cons, StateT.run_bind, h]
    exact ih _

theorem accumulateDry_run [Add R] [Mul R] (lane : Lane) (osc : Oscillator)
    (gain : R) (state : Scene R P) :
    (accumulateDry lane osc gain).run state =
      (⟨⟩, replaceOutput state lane
        (DSP.Resynthesis.accumulateDry state.math gain (state.magnitudes lane)
          (state.phases lane osc) (state.output lane))) := by
  rw [accumulateDry, foldlM_finRange]
  let update := fun output (bin : Bin) =>
    addCoefficient output bin
      (state.math.polar (gain * (state.magnitudes lane).get bin) ((state.phases lane osc).get bin))
  have result := foldOutput_run (List.finRange 2047) lane state (state.output lane) (fun bin => do
      let magnitude ← synthesisMagnitude lane bin
      let angle ← phase lane osc bin
      scatter lane bin (gain * magnitude) angle) update (by
    intro output bin
    change (scatter lane bin (gain * (state.magnitudes lane).get bin)
      ((state.phases lane osc).get bin)).run (replaceOutput state lane output) = _
    rw [scatter_run, replaceOutput_here, replaceOutput_twice]
    rfl)
  rw [replaceOutput_initial] at result
  unfold DSP.Resynthesis.accumulateDry
  exact result

section Shift

variable [Mul P] [Div P] [Add P] [Sub P] [Neg P] [OfScientific P] [LT P] [DecidableLT P]

def shiftedStep [Add R] [Mul R] [Div R] (widen : R → P) (natSample : Nat → R) (natPhase : Nat → P)
    (lane : Lane) (osc : Oscillator) (gain : R) (scale : P) (visit : Visit P) :
    StateM (Scene R P) PUnit := do
  if visit.link.small then pure ⟨⟩
  else
    let root := visit.link.root
    let frequency ← omega lane root
    let rootOmega := frequency * scale
    let rounded ← round (rootOmega * natPhase 4096)
    let rootTarget := clampShiftedBin 4096 rounded
    let phase ← phase lane osc root
    let rootPhase := phase * scale
    let target := if Nat.beq root.val visit.bin.val then rootTarget
      else rootTarget - Int.ofNat root.coefficient.val + Int.ofNat visit.bin.coefficient.val
    let angle := if Nat.beq root.val visit.bin.val then rootPhase
      else followerPhase rootPhase visit.link.delta
    match acceptedBinWith (n := 4096) widen natSample natPhase rootOmega target with
    | none => pure ⟨⟩
    | some bin =>
      let magnitude ← synthesisMagnitude lane visit.bin
      scatter lane bin (gain * magnitude) angle

def shiftedUpdate [Add R] [Mul R] [Div R] (state : Scene R P) (lane : Lane)
    (osc : Oscillator) (gain : R) (scale : P) (spectrum : BinTable (ComplexValue R) 4096)
    (bin : Bin) : BinTable (ComplexValue R) 4096 :=
  let link := (state.graph lane).link.get bin
  if link.small then spectrum
  else
    let rootOmega := (state.input lane).omega.get link.root * scale
    let rootTarget := shiftedBin (n := 4096) state.math rootOmega
    let rootPhase := (state.phases lane osc).get link.root * scale
    let (target, angle) := if link.root = bin then (rootTarget, rootPhase)
      else (rootTarget - Int.ofNat link.root.coefficient.val + Int.ofNat bin.coefficient.val,
        followerPhase rootPhase link.delta)
    match acceptedBin state.math rootOmega target with
    | none => spectrum
    | some binOut => addCoefficient spectrum binOut
      (state.math.polar (gain * (state.magnitudes lane).get bin) angle)

theorem shiftedStep_run [Add R] [Mul R] [Div R] (state : Scene R P)
    (lane : Lane) (osc : Oscillator) (gain : R) (scale : P)
    (output : BinTable (ComplexValue R) 4096) (bin : Bin) :
    (shiftedStep state.math.widen state.math.natSample state.math.natPhase lane osc gain scale
      ⟨bin, (state.graph lane).link.get bin⟩).run (replaceOutput state lane output) =
    (⟨⟩, replaceOutput state lane (shiftedUpdate state lane osc gain scale output bin)) := by
  have hBin : ∀ (a b : Bin), (Nat.beq a.val b.val = true) ↔ a = b :=
    fun a b => by rw [Nat.beq_eq]; exact Fin.val_inj
  unfold shiftedStep shiftedUpdate
  dsimp only
  by_cases hSmall : ((state.graph lane).link.get bin).small = true
  · simp only [hSmall, if_true]
    rfl
  · simp only [hSmall, acceptedBin, shiftedBin, hBin]
    simp only [Bool.false_eq_true, if_false]
    unfold omega round phase
    simp only [StateT.run, Bind.bind, StateT.bind, replaceOutput]
    by_cases hRoot : ((state.graph lane).link.get bin).root = bin <;>
      simp only [hRoot, if_true, if_false]
    all_goals
      split <;> rename_i h <;> rw [h]
      · rfl
      · simp [scatter, coefficient, polar, setCoefficient, synthesisMagnitude,
          Bind.bind, StateT.bind, addCoefficient, Function.update_idem]

end Shift

theorem forVisits_run (state : Scene R P) (lane : Lane)
    (step : Visit P → StateM (Scene R P) PUnit)
    (update : BinTable (ComplexValue R) 4096 → Bin → BinTable (ComplexValue R) 4096)
    (bounded : (state.graph lane).order.length ≤ 2047)
    (h : ∀ output bin, (step ⟨bin, (state.graph lane).link.get bin⟩).run
      (replaceOutput state lane output) = (⟨⟩, replaceOutput state lane (update output bin))) :
    (forVisits lane step).run state =
      (⟨⟩, replaceOutput state lane ((state.graph lane).order.foldl update (state.output lane))) := by
  let order := (state.graph lane).order
  let count := min order.length 2047
  have countEq : count = order.length := min_eq_left bounded
  let binAt := fun (i : Fin count) => order[i.val]?.getD 0
  let indexStep := fun (i : Fin count) => do
    let visit ← visitAt (R := R) lane ⟨i.val, by have := i.isLt; omega⟩
    step visit
  have orderEq : (List.finRange count).map binAt = order := by
    apply List.ext_getElem
    · simp [countEq]
    · intro i hi hj
      simp [binAt, List.getElem?_eq_getElem hj]
  change (Fin.foldlM count (fun _ i => indexStep i) (⟨⟩ : PUnit)).run state = _
  rw [foldlM_finRange]
  have result := foldOutput_run (List.finRange count) lane state (state.output lane)
    indexStep (fun output i => update output (binAt i)) (fun output i => h output (binAt i))
  rw [replaceOutput_initial] at result
  change _ = (PUnit.unit, replaceOutput state lane (order.foldl update (state.output lane)))
  rw [← orderEq, List.foldl_map]
  exact result

theorem accumulateShifted_run [Add R] [Mul R] [Div R]
    [Mul P] [Div P] [Add P] [Sub P] [Neg P] [OfScientific P] [LT P] [DecidableLT P]
    (state : Scene R P)
    (widen : R → P) (natSample : Nat → R) (natPhase : Nat → P) (lane : Lane) (osc : Oscillator)
    (gain : R) (shift : PitchRatio) (bounded : (state.graph lane).order.length ≤ 2047)
    (hWiden : widen = state.math.widen) (hSample : natSample = state.math.natSample)
    (hPhase : natPhase = state.math.natPhase) :
    (accumulateShifted widen natSample natPhase lane osc gain shift).run state =
      (⟨⟩, replaceOutput state lane (DSP.Resynthesis.accumulateShifted
        state.math gain shift (state.input lane).omega (state.magnitudes lane) (state.phases lane osc)
        (state.graph lane) (state.output lane))) := by
  subst widen natSample natPhase
  let scale := state.math.intPhase shift.numerator / state.math.intPhase shift.denominator
  change (forVisits lane (shiftedStep state.math.widen state.math.natSample state.math.natPhase lane osc gain scale)).run state = _
  rw [forVisits_run state lane _ (shiftedUpdate state lane osc gain scale) bounded
    (fun output bin => shiftedStep_run state lane osc gain scale output bin)]
  unfold DSP.Resynthesis.accumulateShifted shiftedUpdate
  dsimp only [scale]
  congr 3
  funext spectrum bin
  by_cases h : ((state.graph lane).link.get bin).root = bin <;>
    simp only [h, if_true, if_false]
  all_goals
    by_cases hSmall : ((state.graph lane).link.get bin).small = true <;>
      simp only [hSmall, if_true, if_false, Bool.false_eq_true]
  all_goals
    split <;> rename_i hCase <;> rw [hCase]


end DSP.Resynthesis.SpectrumStorage

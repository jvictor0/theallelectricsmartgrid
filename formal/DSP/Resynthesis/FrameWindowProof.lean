import DSP.Resynthesis.FrameStorage

namespace DSP.Resynthesis.FrameStorage

open SpectrumStorage (Lane forLanes)

variable {R P : Type}

private theorem foldPrefix {S V : Type} {n : Nat}
    (step : Fin n → StateM S PUnit) (target : Option (Fin n))
    (inv : S → Prop) (observe : S → V) (changeValue : V → V)
    (step_ok : ∀ s, inv s → ∀ i,
      inv ((step i).run s).2 ∧
        observe ((step i).run s).2 =
          if some i = target then changeValue (observe s) else observe s)
    (s : S) (valid : inv s) :
    let final := ((Fin.foldlM n (fun _ i => step i) ⟨⟩).run s).2
    inv final ∧ observe final = if target.isSome then changeValue (observe s) else observe s := by
  have loop (items : List (Fin n)) (distinct : items.Nodup) (s : S) (valid : inv s) :
      let final := ((items.foldlM (fun _ i => step i) ⟨⟩).run s).2
      inv final ∧ observe final =
        if (∃ i ∈ items, some i = target) then changeValue (observe s) else observe s := by
    induction items generalizing s with
    | nil => simpa [StateT.run, StateT.pure, Pure.pure] using valid
    | cons i rest ih =>
      obtain ⟨absent, nodup⟩ := List.nodup_cons.mp distinct
      obtain ⟨nextValid, nextObserve⟩ := step_ok s valid i
      obtain ⟨finalValid, finalObserve⟩ := ih nodup _ nextValid
      rw [List.foldlM_cons, StateT.run_bind]
      let final := ((rest.foldlM (fun _ a => step a) (⟨⟩ : PUnit)).run ((step i).run s).2).2
      change inv final ∧ observe final = _
      exact ⟨finalValid, by
        rw [finalObserve, nextObserve]
        simp only [List.mem_cons, exists_eq_or_imp]
        by_cases hit : some i = target
        · have miss : ¬∃ j ∈ rest, some j = target := by
            rintro ⟨j, mem, eq⟩
            exact absent (Option.some.inj (eq.trans hit.symm) ▸ mem)
          simp [hit, miss]
        · simp [hit]⟩
  rw [Fin.foldlM_eq_foldlM_finRange]
  cases target <;> simpa using loop (List.finRange n) (List.nodup_finRange _) s valid

private theorem windowPoint [Mul R] (state : Scene R P) (count : Fin 5)
    (frameSize : Fin 16385) (lane : Lane) (sample : FrameIndex) :
    let final := ((applyWindow count frameSize : StateM (Scene R P) PUnit).run state).2
    {final with frame := state.frame} = state ∧
      final.frame lane sample =
        if lane.val < count.val ∧ sample.val < frameSize.val then
          state.frame lane sample * state.window sample
        else state.frame lane sample := by
  let inv := fun (s : Scene R P) => {s with frame := state.frame} = state
  let observe := fun (s : Scene R P) => s.frame lane sample
  let scale := fun (v : R) => v * state.window sample
  let sampleAt (i : Fin frameSize.val) : FrameIndex :=
    ⟨i.val, by have := i.isLt; have := frameSize.isLt; omega⟩
  let laneAt (i : Fin count.val) : Lane :=
    ⟨i.val, by have := i.isLt; have := count.isLt; omega⟩
  let sampleTarget := if h : sample.val < frameSize.val then some (⟨sample.val, h⟩ : Fin frameSize.val)
    else none
  let laneTarget := if h : lane.val < count.val then some (⟨lane.val, h⟩ : Fin count.val) else none
  have sampleHit (i : Fin frameSize.val) : some i = sampleTarget ↔ sampleAt i = sample := by
    by_cases h : sample.val < frameSize.val
    · simp [sampleTarget, sampleAt, h, Fin.ext_iff]
    · have neq : i.val ≠ sample.val := fun eq => h (eq ▸ i.isLt)
      simp [sampleTarget, sampleAt, h, Fin.ext_iff, neq]
  have laneHit (i : Fin count.val) : some i = laneTarget ↔ laneAt i = lane := by
    by_cases h : lane.val < count.val
    · simp [laneTarget, laneAt, h, Fin.ext_iff]
    · have neq : i.val ≠ lane.val := fun eq => h (eq ▸ i.isLt)
      simp [laneTarget, laneAt, h, Fin.ext_iff, neq]
  let modifyLane (p : FrameIndex) (weight : R) (l : Lane) : StateM (Scene R P) PUnit := do
    let value ← getSample l p
    putSample l p (value * weight)
  let modifySample (i : Fin frameSize.val) : StateM (Scene R P) PUnit := do
    let weight ← window (sampleAt i)
    forLanes count (modifyLane (sampleAt i) weight)
  let transform := if lane.val < count.val then scale else id
  have sample_ok (s : Scene R P) (valid : inv s) (i : Fin frameSize.val) :
      inv ((modifySample i).run s).2 ∧
        observe ((modifySample i).run s).2 =
          if some i = sampleTarget then transform (observe s) else observe s := by
    have unchanged : {s with frame := state.frame} = state := valid
    have sameWindow : s.window = state.window := by
      have h := congrArg Scene.window unchanged
      exact h
    let action := if sampleAt i = sample then scale else id
    have inner := foldPrefix (fun l => modifyLane (sampleAt i) (s.window (sampleAt i)) (laneAt l))
      laneTarget inv observe action (by
        intro t ht l
        simp only [laneHit]
        simp only [modifyLane, getSample, putSample, StateT.run, Bind.bind, StateT.bind]
        refine ⟨ht, ?_⟩
        by_cases hLane : laneAt l = lane
        · by_cases hSample : sampleAt i = sample
          · simp [observe, action, scale, hLane, hSample, sameWindow]
          · simp [observe, action, hLane, hSample, Function.update_of_ne (Ne.symm hSample)]
        · simp [observe, hLane, Function.update_of_ne (Ne.symm hLane)]) s valid
    let final := ((forLanes count (modifyLane (sampleAt i) (s.window (sampleAt i)))).run s).2
    change inv final ∧ observe final = _
    dsimp only [final]
    unfold forLanes
    simp only [sampleHit]
    by_cases hLane : lane.val < count.val <;>
      by_cases hSample : sampleAt i = sample <;>
      simpa [laneTarget, action, transform, hLane, hSample] using inner
  have result := foldPrefix modifySample sampleTarget inv observe transform sample_ok state rfl
  change inv ((Fin.foldlM frameSize.val (fun _ i => modifySample i) ⟨⟩).run state).2 ∧ _
  by_cases hLane : lane.val < count.val <;>
    by_cases hSample : sample.val < frameSize.val <;>
    simpa [applyWindow, modifySample, modifyLane, sampleAt, sampleTarget, transform, inv,
      observe, scale, hLane, hSample] using result

/-- Applying the window multiplies each selected sample exactly once and preserves
all frame values outside the selected ranges and all other scene fields. -/
theorem applyWindow_run [Mul R] (state : Scene R P) (count : Fin 5) (frameSize : Fin 16385) :
    ((applyWindow count frameSize : StateM (Scene R P) PUnit).run state).2 =
      {state with frame := fun lane sample =>
        if lane.val < count.val ∧ sample.val < frameSize.val then
          state.frame lane sample * state.window sample
        else state.frame lane sample} := by
  let final := ((applyWindow count frameSize : StateM (Scene R P) PUnit).run state).2
  let newFrame := fun (lane : Lane) (sample : FrameIndex) =>
    if lane.val < count.val ∧ sample.val < frameSize.val then
      state.frame lane sample * state.window sample
    else state.frame lane sample
  have unchanged : {final with frame := state.frame} = state := (windowPoint state count frameSize 0 0).1
  have frame : final.frame = newFrame := by
    funext lane sample
    exact (windowPoint state count frameSize lane sample).2
  have updated := congrArg (fun s : Scene R P => {s with frame := newFrame}) unchanged
  change final = {state with frame := newFrame}
  rw [← updated, ← frame]

end DSP.Resynthesis.FrameStorage

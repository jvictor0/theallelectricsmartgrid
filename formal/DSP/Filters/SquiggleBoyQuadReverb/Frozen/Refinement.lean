import DSP.Filters.SquiggleBoyQuadReverb.Frozen.Model
import DSP.Filters.SquiggleBoyQuadReverb.Frozen.RuntimeTaps
import DSP.Processor.Run

namespace DSP.SquiggleBoyQuadReverb.Frozen

open WanderingSchroederAllPass
open scoped BigOperators

noncomputable section

/-- Each stage retains its own frozen clock readout. The same generated
runtime action executes with an identity clock tick for this interpretation. -/
abbrev FrozenState := Runtime.State (Lanes ℝ) (Lanes ℝ)
abbrev Diffusion (order : List (Fin 8)) := Runtime.Diffusion (Lanes ℝ) (Lanes ℝ) order

private theorem intro_capacity_positive (i : Fin 8) : 0 < squiggleBoyReverbIntroAPFCapacity i := by
  revert i
  decide

def diffusionHistory : (order : List (Fin 8)) → Diffusion order → IntroHistory order ℝ
  | [], _ => fun j => j.elim0
  | i :: order, s =>
    introCons (F := fun i => Tape (squiggleBoyReverbIntroAPFCapacity i) ℝ)
      (i := i) (order := order) s.1.history.history (diffusionHistory order s.2)

def diffusionTaps : (order : List (Fin 8)) → Diffusion order → IntroTaps order ℝ
  | [], _ => fun j => j.elim0
  | i :: order, s =>
    introCons (F := fun i => Tap (squiggleBoyReverbIntroAPFCapacity i) ℝ)
      (i := i) (order := order) (apfTap (intro_capacity_positive i) (Runtime.introTap i) s.1.clock)
      (diffusionTaps order s.2)

theorem diffusion_frozen (order : List (Fin 8)) (s : Diffusion order) (x : Lanes ℝ) :
    let result : Lanes ℝ × Diffusion order := (Runtime.diffuse get order x).run s
    diffusionTaps order result.2 = diffusionTaps order s ∧
      (diffusionHistory order result.2, result.1) =
        intro allPassGain order (diffusionTaps order s) (diffusionHistory order s, x) := by
  induction order generalizing x with
  | nil =>
    exact ⟨funext fun i => i.elim0, Prod.ext (funext fun i => i.elim0) rfl⟩
  | cons i order ih =>
    let head : Lanes ℝ × Storage (squiggleBoyReverbIntroAPFCapacity i) (Lanes ℝ) (Lanes ℝ) :=
      (WanderingSchroederAllPass.step (Runtime.introTap i) get x).run s.1
    let tail : Lanes ℝ × Diffusion order := (Runtime.diffuse get order head.1).run s.2
    let plan := apfTap (intro_capacity_positive i) (Runtime.introTap i) s.1.clock
    let a := allPass plan allPassGain (s.1.history.history, x)
    let b (u : Lanes ℝ) :=
      intro allPassGain order (diffusionTaps order s.2) (diffusionHistory order s.2, u)
    have hhead : head.2.clock = s.1.clock ∧ (head.2.history.history, head.1) = a :=
      apfStep_frozen (intro_capacity_positive i) (Runtime.introTap i) s.1 x
    have htail : diffusionTaps order tail.2 = diffusionTaps order s.2 ∧
        (diffusionHistory order tail.2, tail.1) = b head.1 := ih s.2 head.1
    change diffusionTaps (i :: order) (head.2, tail.2) = diffusionTaps (i :: order) s ∧
      (diffusionHistory (i :: order) (head.2, tail.2), tail.1) = _
    constructor
    · simp only [diffusionTaps, hhead.1, htail.1]
    · rw [intro_cons]
      let join (h : Tape (squiggleBoyReverbIntroAPFCapacity i) ℝ)
          (ys : IntroHistory order ℝ × Lanes ℝ) :=
        (introCons (F := fun j => Tape (squiggleBoyReverbIntroAPFCapacity j) ℝ)
          (i := i) (order := order) h ys.1, ys.2)
      change join head.2.history.history (diffusionHistory order tail.2, tail.1) =
        join a.1 (b a.2)
      rw [← hhead.2, htail.2]

def dampingMemory (d : Runtime.Damping (Lanes ℝ)) : Lanes ℝ × Lanes ℝ :=
  (d.filter.highPass.lowPass.s, d.filter.lowPass.s)

def prepared (s : FrozenState) : Spec ℝ where
  intro := diffusionTaps _ s.diffusion
  main := mainTap s.settings.delaySamples
  first := apfTap (by decide) (Runtime.feedbackTap 0) s.first.clock
  second := apfTap (by decide) (Runtime.feedbackTap 1) s.second.clock
  feedback := s.settings.feedback
  satGain := s.settings.saturatorGain
  highAlpha := s.damping.filter.highPass.lowPass.G
  lowAlpha := s.damping.filter.lowPass.G

def histories (s : FrozenState) : History ℝ :=
  (diffusionHistory _ s.diffusion, s.delay.history, s.first.history.history,
    dampingMemory s.damping, s.second.history.history, s.wet)

def processor : Processor FrozenState (Lanes ℝ) (Lanes ℝ) := fun s x =>
  let (y, next) := (Runtime.process get ⟨x⟩).run s
  (next, y)

private theorem dampingStep_frozen (state : Runtime.Damping (Lanes ℝ)) (x : Lanes ℝ) :
    let (y, next) := (Runtime.dampingStep x).run state
    next.filter.highPass.lowPass.G = state.filter.highPass.lowPass.G ∧
      next.filter.lowPass.G = state.filter.lowPass.G ∧
      (dampingMemory next, y) =
        damping state.filter.highPass.lowPass.G state.filter.lowPass.G (dampingMemory state, x) := by
  simp only [Runtime.dampingStep, StateT.run, modifyGet, TPTBaseWidth.Runtime.process,
    Processor.compact, TPTBaseWidth.Runtime.model, TPTBaseWidth.Runtime.observe,
    TPTBaseWidth.Runtime.restore, TPTBaseWidth.Runtime.save, TPTBaseWidth.step,
    OnePoleTPTHighPass.Runtime.restore, OnePoleTPTHighPass.Runtime.save, OnePoleTPTHighPass.step,
    OnePoleTPTLowPass.Runtime.restore, OnePoleTPTLowPass.Runtime.save, OnePoleTPTLowPass.step,
    dampingMemory, fma_eq, damping, lowPass]
  refine ⟨rfl, rfl, ?_⟩
  apply Prod.ext
  · apply Prod.ext <;> funext i <;> simp [laneGain] <;> ring
  · funext i
    simp [laneGain]
    ring

private theorem mix_broadcast (x : Lanes ℝ) :
    (DelayBuffer.Gather.gather (squiggleBoyReverbMix (DelayBuffer.BroadcastLane.lane x)) : Lanes ℝ) =
      mix x := by
  funext i
  fin_cases i <;> rfl

section
variable {R Clock : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfNat R 3]
  [OfNat R 4] [OfNat R 5] [OfScientific R] [Add R] [Sub R] [Neg R] [Mul R]
  [FMA R] [IfLess R] [DelayMath R] [DelayBuffer.SampleIndex R] [DelayBuffer.Gather R]
  [Div R] [Tanh R] [DelayBuffer.SampleAges R] [DelayBuffer.BroadcastLane R]

set_option maxHeartbeats 300000 in
private theorem process_run (s : Runtime.State R Clock) (tick : StateM Clock R) (x : R) :
    (Runtime.process tick ⟨x⟩).run s =
      let intro := (Runtime.diffuse tick squiggleBoyReverbIntroOrder x).run s.diffusion
      let delay := s.delay.push (sumFeedback intro.1 s.wet s.settings.feedback)
      let read := (readMainDelay ⟨s.settings.delaySamples⟩).run delay
      let mixed := DelayBuffer.Gather.gather (squiggleBoyReverbMix (DelayBuffer.BroadcastLane.lane read.1))
      let first := (WanderingSchroederAllPass.step (Runtime.feedbackTap 0) tick mixed).run s.first
      let damping := (Runtime.dampingStep (saturate s.settings.saturatorGain first.1)).run s.damping
      let second := (WanderingSchroederAllPass.step (Runtime.feedbackTap 1) tick damping.1).run s.second
      (second.1, {s with
        diffusion := intro.2
        delay := read.2
        first := first.2
        damping := damping.2
        second := second.2
        wet := second.1}) := by
  simp only [Runtime.process, run_bind, run_focus, run_pure, run_get, run_set, run_pushFrame]

end

private def frozenStep (s : FrozenState) (x : Lanes ℝ) : FrozenState × Lanes ℝ :=
  let diffused : Lanes ℝ × Diffusion squiggleBoyReverbIntroOrder :=
    (Runtime.diffuse get squiggleBoyReverbIntroOrder x).run s.diffusion
  let delay := s.delay.push (sumFeedback diffused.1 s.wet s.settings.feedback)
  let mixed := mix (tapRead (mainTap s.settings.delaySamples) delay.history)
  let first := (WanderingSchroederAllPass.step (Runtime.feedbackTap 0) get mixed).run s.first
  let damped := (Runtime.dampingStep (saturate s.settings.saturatorGain first.1)).run s.damping
  let second := (WanderingSchroederAllPass.step (Runtime.feedbackTap 1) get damped.1).run s.second
  ({s with
    diffusion := diffused.2
    delay := delay
    first := first.2
    damping := damped.2
    second := second.2
    wet := second.1}, second.1)

set_option maxHeartbeats 130000 in
private theorem processor_eq_frozenStep (s : FrozenState) (x : Lanes ℝ) :
    processor s x = frozenStep s x := by
  unfold processor frozenStep
  rw [process_run]
  dsimp only
  rw [readMainDelay_frozen]
  dsimp only
  rw [mix_broadcast]


set_option maxRecDepth 2200 in
set_option maxHeartbeats 230000 in
private theorem update_of_blocks (p : Spec ℝ) (h : History ℝ) (x : Lanes ℝ)
    (introHistory : IntroHistory squiggleBoyReverbIntroOrder ℝ) (diffused : Lanes ℝ)
    (main : Tape squiggleBoyReverbDelayCapacity ℝ)
    (firstHistory : Tape squiggleBoyReverbFirstCapacity ℝ) (first : Lanes ℝ)
    (dampingHistory : Lanes ℝ × Lanes ℝ) (damped : Lanes ℝ)
    (secondHistory : Tape squiggleBoyReverbSecondCapacity ℝ) (wet : Lanes ℝ)
    (intro_eq : (introHistory, diffused) = intro allPassGain squiggleBoyReverbIntroOrder p.intro (h.1, x))
    (main_eq : main = push (h.2.1, diffused + laneGain p.feedback h.wet))
    (first_eq : (firstHistory, first) = allPass p.first allPassGain (h.2.2.1, mix (tapRead p.main main)))
    (damped_eq : (dampingHistory, damped) =
      damping p.highAlpha p.lowAlpha (h.2.2.2.1, saturate p.satGain first))
    (second_eq : (secondHistory, wet) = allPass p.second allPassGain (h.2.2.2.2.1, damped)) :
    (introHistory, main, firstHistory, dampingHistory, secondHistory, wet) = update p (h, x) := by
  rw [update, beforeSAT_apply, ← intro_eq]
  dsimp only
  rw [← main_eq, ← first_eq, afterSAT_apply]
  dsimp only
  rw [← damped_eq, ← second_eq]


set_option maxRecDepth 3000 in
set_option maxHeartbeats 1200000 in
theorem processor_frozen (s : FrozenState) (x : Lanes ℝ) :
    prepared (processor s x).1 = prepared s ∧
      histories (processor s x).1 = update (prepared s) (histories s, x) ∧
      (processor s x).2 = (processor s x).1.wet := by
  rw [processor_eq_frozenStep]
  let diffusion : Lanes ℝ × Diffusion squiggleBoyReverbIntroOrder :=
    (Runtime.diffuse (R := Lanes ℝ) (Clock := Lanes ℝ) get squiggleBoyReverbIntroOrder x).run s.diffusion
  let delay := s.delay.push (sumFeedback diffusion.1 s.wet s.settings.feedback)
  let input := mix (tapRead (mainTap s.settings.delaySamples) delay.history)
  let first : Lanes ℝ × Storage squiggleBoyReverbFirstCapacity (Lanes ℝ) (Lanes ℝ) :=
    (WanderingSchroederAllPass.step (Runtime.feedbackTap 0) get input).run s.first
  let filtered : Lanes ℝ × Runtime.Damping (Lanes ℝ) :=
    (Runtime.dampingStep (saturate s.settings.saturatorGain first.1)).run s.damping
  let second : Lanes ℝ × Storage squiggleBoyReverbSecondCapacity (Lanes ℝ) (Lanes ℝ) :=
    (WanderingSchroederAllPass.step (Runtime.feedbackTap 1) get filtered.1).run s.second
  let next : FrozenState := {s with
    diffusion := diffusion.2
    delay := delay
    first := first.2
    damping := filtered.2
    second := second.2
    wet := second.1}
  change prepared next = prepared s ∧ histories next = update (prepared s) (histories s, x) ∧
    second.1 = next.wet
  have hi : diffusionTaps squiggleBoyReverbIntroOrder diffusion.2 = diffusionTaps squiggleBoyReverbIntroOrder s.diffusion ∧
      (diffusionHistory squiggleBoyReverbIntroOrder diffusion.2, diffusion.1) =
        intro allPassGain squiggleBoyReverbIntroOrder (prepared s).intro ((histories s).1, x) :=
    diffusion_frozen squiggleBoyReverbIntroOrder s.diffusion x
  have hfirst : first.2.clock = s.first.clock ∧
      (first.2.history.history, first.1) =
        allPass (prepared s).first allPassGain
          ((histories s).2.2.1, mix (tapRead (prepared s).main delay.history)) :=
    apfStep_frozen (by decide) (Runtime.feedbackTap 0) s.first input
  have hsecond : second.2.clock = s.second.clock ∧
      (second.2.history.history, second.1) =
        allPass (prepared s).second allPassGain ((histories s).2.2.2.2.1, filtered.1) :=
    apfStep_frozen (by decide) (Runtime.feedbackTap 1) s.second filtered.1
  have hd : filtered.2.filter.highPass.lowPass.G = s.damping.filter.highPass.lowPass.G ∧
      filtered.2.filter.lowPass.G = s.damping.filter.lowPass.G ∧
      (dampingMemory filtered.2, filtered.1) =
        damping (prepared s).highAlpha (prepared s).lowAlpha
          ((histories s).2.2.2.1, saturate (prepared s).satGain first.1) :=
    dampingStep_frozen s.damping (saturate s.settings.saturatorGain first.1)
  refine ⟨?_, ?_, rfl⟩
  · simp only [prepared, next, hi.1, hfirst.1, hsecond.1, hd.1, hd.2.1]
  · have hwrite : delay.history =
        push ((histories s).2.1, diffusion.1 + laneGain (prepared s).feedback (histories s).wet) := by
      change _ = push (s.delay.history, diffusion.1 + laneGain s.settings.feedback s.wet)
      rw [push_eq_shift, ← DelayBuffer.history_push]
      have hframe : diffusion.1 + laneGain s.settings.feedback s.wet =
          sumFeedback diffusion.1 s.wet s.settings.feedback := by
        funext i
        change diffusion.1 i + s.settings.feedback i * s.wet i =
          diffusion.1 i + s.wet i * s.settings.feedback i
        rw [mul_comm (s.settings.feedback i)]
      rw [hframe]
    conv_lhs => simp only [histories, next]
    refine update_of_blocks (prepared s) (histories s) x
      (diffusionHistory squiggleBoyReverbIntroOrder diffusion.2) diffusion.1
      delay.history first.2.history.history first.1 (dampingMemory filtered.2) filtered.1
      second.2.history.history second.1 ?_ ?_ ?_ ?_ ?_
    · exact hi.2
    · exact hwrite
    · exact hfirst.2
    · exact hd.2.2
    · exact hsecond.2

end
end DSP.SquiggleBoyQuadReverb.Frozen

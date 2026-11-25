import DSP.Filters.SchroederAllPass.Storage
import DSP.Filters.Runtime

namespace DSP.SquiggleBoyQuadReverb.Runtime

open WanderingSchroederAllPass WanderingRandomLFO

abbrev Diffusion (R Clock : Type) (order : List (Fin 8)) :=
  SizedFields (fun capacity => Storage capacity R Clock) (order.map squiggleBoyReverbIntroAPFCapacity)

structure Settings (R : Type) where
  delaySamples : R
  feedback : R
  saturatorGain : R

structure Spec (R : Type) where
  delaySamples : R
  feedback : R
  dampingBase : R
  dampingWidth : R
  saturatorGain : R

structure Damping (R : Type) where
  filter : TPTBaseWidth.Runtime.State R
  snapshot : TPTBaseWidth.Snapshot R

structure State (R Clock : Type) where
  diffusion : Diffusion R Clock squiggleBoyReverbIntroOrder
  delay : DelayBuffer squiggleBoyReverbDelayCapacity R
  settings : Settings R
  first : Storage squiggleBoyReverbFirstCapacity R Clock
  damping : Damping R
  second : Storage squiggleBoyReverbSecondCapacity R Clock
  wet : R

private def introTap (stage : Fin 8) : TapSpec :=
  let tap := UInt64.ofNat (squiggleBoyReverbIntroAPFsDelayTime stage)
  ⟨UInt64.ofNat squiggleBoyReverbWanderSpec.depth, tap, tap, tap, tap⟩

private def feedbackTap (stage : Fin 2) : TapSpec :=
  let tap := fun ch => UInt64.ofNat (squiggleBoyReverbLoopAPFsDelayTime stage ch)
  ⟨UInt64.ofNat squiggleBoyReverbWanderSpec.depth, tap 0, tap 1, tap 2, tap 3⟩

private def diffusionInit {R Clock : Type} [OfNat R 0]
    (start : Float → UInt64 → StateM Clock PUnit) :
    (order : List (Fin 8)) → StateM (Diffusion R Clock order) PUnit
  | [] => pure ⟨⟩
  | stage :: rest => do
    focus Prod.fst (fun s child => (child, s.2))
      (initializeStage (start squiggleBoyReverbWanderSpec.lambdaSamples (squiggleBoyReverbIntroSeed stage)))
    focus Prod.snd (fun s child => (s.1, child)) (diffusionInit start rest)

private def dampingTune {R : Type} [OfNat R 0] [OfNat R 1] [OfScientific R] [Add R]
    [Mul R] [Div R] [Trig R] [IfLess R] (spec : Spec R) : StateM (Damping R) PUnit :=
  modify fun state =>
    let (high, low) := dampingCutoffs spec.dampingBase spec.dampingWidth
    let (filter, snapshot) := TPTBaseWidth.Runtime.prepareCoefficients ⟨⟨high⟩, ⟨low⟩⟩ state.filter
    { filter, snapshot }

def tune {R Clock : Type} [OfNat R 0] [OfNat R 1] [OfScientific R] [Add R] [Mul R]
    [Div R] [Trig R] [IfLess R] (spec : Spec R) : StateM (State R Clock) PUnit := do
  focus State.settings (fun s settings => { s with settings })
    (set (⟨spec.delaySamples, spec.feedback, spec.saturatorGain⟩ : Settings R))
  focus State.damping (fun s damping => { s with damping }) (dampingTune spec)

private def init {R Clock : Type} [OfNat R 0] [OfNat R 1] [OfScientific R] [Add R] [Mul R]
    [Div R] [Trig R] [IfLess R] (start : Float → UInt64 → StateM Clock PUnit) (spec : Spec R) :
    StateM (State R Clock) PUnit := do
  let startFeedback := fun i => start squiggleBoyReverbWanderSpec.lambdaSamples (squiggleBoyReverbLoopSeed i)
  focus State.diffusion (fun s diffusion => { s with diffusion }) (diffusionInit start squiggleBoyReverbIntroOrder)
  focus State.delay (fun s delay => { s with delay }) DelayBuffer.clear
  focus State.first (fun s first => { s with first }) (initializeStage (startFeedback 0))
  focus State.second (fun s second => { s with second }) (initializeStage (startFeedback 1))
  focus State.wet (fun s wet => { s with wet }) (set (0 : R))
  focus State.damping (fun s damping => { s with damping })
    (set (⟨⟨⟨⟨0, 0⟩⟩, ⟨0, 0⟩⟩, ⟨⟨0⟩, ⟨0⟩⟩⟩ : Damping R))
  tune spec

def initializeFramed {R : Type} [OfNat R 0] [OfNat R 1] [OfScientific R] [Add R] [Sub R] [Mul R]
    [Div R] [Trig R] [IfLess R] [FMA R] [WanderingRandomLFO.SampleValue R] (spec : Spec R) :
    StateM (State R (QuadClock (FramedStream R))) PUnit :=
  init (fun lambda seed => QuadClock.initializeLanes fun lane =>
    FramedStream.initialize lambda (squiggleBoyReverbLaneSeed seed lane)) spec

def initializeSample {R : Type} [OfNat R 0] [OfNat R 1] [OfScientific R] [Add R] [Mul R]
    [Div R] [Trig R] [IfLess R] [FMA R] [WanderingRandomLFO.SampleValue R] (spec : Spec R) :
    StateM (State R (QuadClock (Stream R))) PUnit :=
  init (fun lambda seed => QuadClock.initializeLanes fun lane =>
    Stream.initialize lambda (squiggleBoyReverbLaneSeed seed lane)) spec

def tuneFramed {R : Type} [OfNat R 0] [OfNat R 1] [OfScientific R] [Add R] [Mul R]
    [Div R] [Trig R] [IfLess R] (spec : Spec R) :
    StateM (State R (QuadClock (FramedStream R))) PUnit :=
  tune spec

section Processing
variable {R Clock : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfNat R 3]
  [OfNat R 4] [OfNat R 5] [OfScientific R] [Add R] [Sub R] [Neg R] [Mul R]
  [FMA R] [IfLess R] [DelayMath R] [DelayBuffer.SampleIndex R] [DelayBuffer.SampleAges R]
  [DelayBuffer.Gather R]

private def diffuse (tick : StateM Clock R) :
    (order : List (Fin 8)) → R → StateM (Diffusion R Clock order) R
  | [], input => pure input
  | stage :: rest, input => do
    let sample ← focus Prod.fst (fun s child => (child, s.2))
      (WanderingSchroederAllPass.step (introTap stage) tick input)
    focus Prod.snd (fun s child => (s.1, child)) (diffuse tick rest sample)

private def dampingStep (input : R) : StateM (Damping R) R :=
  modifyGet fun state =>
    let (filter, output) := TPTBaseWidth.Runtime.process state.filter ⟨input⟩
    (output.x, { state with filter })

variable [Div R] [Tanh R] [DelayBuffer.BroadcastLane R]

def process (tick : StateM Clock R) (input : Sample R) : StateM (State R Clock) R := do
  let diffused ← focus State.diffusion (fun s diffusion => { s with diffusion })
    (diffuse tick squiggleBoyReverbIntroOrder input.x)
  let previous ← focus State.wet (fun s wet => { s with wet }) get
  let feedback ← focus State.settings (fun s settings => { s with settings })
    (focus Settings.feedback (fun s feedback => { s with feedback }) get)
  focus State.delay (fun s delay => { s with delay })
    (DelayBuffer.pushFrame (sumFeedback diffused previous feedback))
  let samples ← focus State.settings (fun s settings => { s with settings })
    (focus Settings.delaySamples (fun s delaySamples => { s with delaySamples }) get)
  let delayed ← focus State.delay (fun s delay => { s with delay }) (readMainDelay ⟨samples⟩)
  let mixed := DelayBuffer.Gather.gather (squiggleBoyReverbMix (DelayBuffer.BroadcastLane.lane delayed))
  let first ← focus State.first (fun s first => { s with first })
    (WanderingSchroederAllPass.step (feedbackTap 0) tick mixed)
  let gain ← focus State.settings (fun s settings => { s with settings })
    (focus Settings.saturatorGain (fun s saturatorGain => { s with saturatorGain }) get)
  let damped ← focus State.damping (fun s damping => { s with damping })
    (dampingStep (saturate gain first))
  let wet ← focus State.second (fun s second => { s with second })
    (WanderingSchroederAllPass.step (feedbackTap 1) tick damped)
  focus State.wet (fun s wet => { s with wet }) (set wet)
  pure wet

variable [WanderingRandomLFO.SampleValue R]

def processFramed (input : Sample R) : StateM (State R (QuadClock (FramedStream R))) R :=
  process (QuadClock.nextValue FramedStream.nextValue) input

def processSample (input : Sample R) : StateM (State R (QuadClock (Stream R))) R :=
  process (QuadClock.nextValue Stream.nextValue) input

end Processing
end DSP.SquiggleBoyQuadReverb.Runtime

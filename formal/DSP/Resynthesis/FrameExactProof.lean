import DSP.Resynthesis.FrameAnalysisProof
import DSP.Resynthesis.FrameGraphProof
import DSP.Resynthesis.FrameWindowProof
import DSP.Resynthesis.FrameModelMonoProof
import DSP.Resynthesis.SpectrumStageProof
import DSP.Resynthesis.Real

namespace DSP.Resynthesis.FrameStorage

open SpectrumStorage (Lane forLanes)

noncomputable section

/-- The reference max heap over the 4096-point frame's active bins. -/
abbrev exactQueue :=
  PVDR.binaryQueue (n := 4096) (fun (a b : ℝ) => decide (a < b))

/-- The gate, magnitude, and phase memories retained by the one-channel model. -/
def exactChannel (state : Scene ℝ ℝ) : ChannelState ℝ ℝ 4096 :=
  ⟨state.spectrum.gates 0, state.spectrum.magnitudes 0, state.spectrum.phases 0⟩

private def renderOne (s : Scene ℝ ℝ) : Scene ℝ ℝ :=
  ((applyWindow 1 16384).run (ifft.run ((synthesize 1).run s).2).2).2

private def output (spectrum : SpectrumStorage.Scene ℝ ℝ) (lane : Lane) : DFT ℝ 4096 :=
  ⟨fun k => if h : 0 < k.val then
      (spectrum.output lane).get ⟨k.val - 1, by have := k.isLt; omega⟩
    else ⟨0, 0⟩⟩

private theorem finishOne_run (state : Scene ℝ ℝ)
    (bound : (state.spectrum.graph 0).order.length ≤ 2047)
    (flattened : ∀ bin ∈ (state.spectrum.graph 0).order,
      let root := ((state.spectrum.graph 0).link.get bin).root
      ((state.spectrum.graph 0).link.get root).root = root)
    (inverse : ∀ spectra, state.inverse spectra 0 =
      fun sample => (DSP.Resynthesis.ifft (m := 16384) (spectra 0)).sample sample.val) :
    let math := state.spectrum.math
    let input := state.spectrum.input 0
    let control := state.spectrum.controls 0
    let mag := state.spectrum.magnitudes 0
    let gates := state.spectrum.gates 0
    let graph := state.spectrum.graph 0
    let phases := fun osc => fixupPhases math (oscillatorDetune control osc)
      input gates graph (state.spectrum.phases 0 osc)
    let dft := synthesizeSpectrum math control input.omega mag phases graph
    let actual := renderOne state
    exactChannel actual = ⟨gates, mag, phases⟩ ∧ actual.rng = state.rng ∧
      ∀ sample : FrameIndex, actual.frame 0 sample =
        (DSP.Resynthesis.ifft (m := 16384) dft).sample sample.val * state.window sample := by
  have hs := SpectrumStorage.synthesize_one_lane_eq state.spectrum bound flattened
  dsimp only at hs ⊢
  unfold renderOne
  rw [applyWindow_run]
  simp only [ifft, synthesize, StateT.run]
  simp only [StateT.run] at hs
  generalize hRun : (SpectrumStorage.synthesize state.spectrum.math.widen
    state.spectrum.math.natSample state.spectrum.math.natPhase state.spectrum.math.sqrt 1
    state.spectrum).2 = spectrum at hs ⊢
  obtain ⟨gates, magnitudes, phases, coefficients⟩ := hs
  change output spectrum 0 = _ at coefficients
  refine ⟨?_, trivial, ?_⟩
  · simp only [exactChannel]
    rw [gates, magnitudes, phases]
  · intro sample
    have inside : (0 : Lane).val < (1 : Fin 5).val ∧ sample.val < (16384 : Fin 16385).val :=
      ⟨by decide, sample.isLt⟩
    rw [if_pos inside]
    change state.inverse (output spectrum) 0 sample * state.window sample = _
    rw [inverse, coefficients]

private theorem oneLane {S : Type} (step : Lane → StateM S PUnit) :
    forLanes 1 step = step 0 := by
  unfold forLanes
  change Fin.foldlM (0 + 1) _ _ = _
  simp [Fin.foldlM_succ]

/-- The generated process stage interpreted over the reals for one channel. -/
def processOne : StateM (Scene ℝ ℝ) PUnit :=
  run exactMath.widen exactMath.natSample exactMath.natPhase exactMath.phaseFloor
    ⟨.process, 1, 16384, 0, 0⟩

private def analyzeOne : StateM (Scene ℝ ℝ) PUnit :=
  analyzeFrame exactMath.widen exactMath.natSample exactMath.natPhase exactMath.phaseFloor 1

private theorem run_grouped {A : Type} (a b c d e : StateM A PUnit) (s : A) :
    ((do a; b; c; d; e : StateM A PUnit).run s).2 =
      (e.run (d.run (c.run (b.run (a.run s).2).2).2).2).2 := rfl

private theorem processOne_run (state : Scene ℝ ℝ) :
    let analyzed := (analyzeOne.run state).2
    let tracked := ((analyzeGraph 0).run analyzed).2
    (processOne.run state).2 = renderOne tracked := by
  have grouped : processOne = (do
      analyzeOne
      analyzeGraph 0
      synthesize 1
      ifft
      applyWindow 1 16384) := by
    unfold processOne run analyzeOne
    dsimp only
    rw [oneLane (analyzeGraph (R := ℝ) (P := ℝ))]
  unfold renderOne
  rw [grouped]
  exact run_grouped analyzeOne (analyzeGraph 0) (synthesize 1) ifft (applyWindow 1 16384) state

private abbrev SpectralResult := (ChannelState ℝ ℝ 4096 × DFT ℝ 4096) × Nat
private abbrev GraphResult := (BinTable GateState 4096 × PVDRGraph ℝ 4096) × Nat
private abbrev MonoObservation := ChannelState ℝ ℝ 4096 × Nat × (FrameIndex → ℝ)

private def synthesisFromGraph (state : Scene ℝ ℝ) (input : PVDRInput ℝ ℝ 4096)
    (magnitude : BinTable ℝ 4096) (result : GraphResult) : SpectralResult :=
  let phases := fun osc => fixupPhases exactMath (oscillatorDetune (state.spectrum.controls 0) osc)
    input result.1.1 result.1.2 ((exactChannel state).phase osc)
  ((⟨result.1.1, magnitude, phases⟩, synthesizeSpectrum exactMath (state.spectrum.controls 0)
    input.omega magnitude phases result.1.2), result.2)

private theorem finishOne_with_graph (state analyzed : Scene ℝ ℝ)
    (input : PVDRInput ℝ ℝ 4096) (magnitude : BinTable ℝ 4096)
    (result : (BinTable GateState 4096 × PVDRGraph ℝ 4096) × Nat)
    (math : state.spectrum.math = exactMath)
    (fields : downstreamFields analyzed = downstreamFields state)
    (hInput : analyzed.spectrum.input 0 = input)
    (hMagnitude : analyzed.spectrum.magnitudes 0 = magnitude)
    (bound : result.1.2.order.length ≤ 2047)
    (flattened : ∀ bin ∈ result.1.2.order,
      let root := (result.1.2.link.get bin).root
      (result.1.2.link.get root).root = root)
    (inverse : ∀ spectra, state.inverse spectra 0 =
      fun sample => (DSP.Resynthesis.ifft (m := 16384) (spectra 0)).sample sample.val) :
    let tracked := installGraphResult analyzed 0 result
    let actual := renderOne tracked
    let expected := synthesisFromGraph state input magnitude result
    exactChannel actual = expected.1.1 ∧ actual.rng = expected.2 ∧
      ∀ sample : FrameIndex, actual.frame 0 sample =
        (DSP.Resynthesis.ifft (m := 16384) expected.1.2).sample sample.val * state.window sample := by
  simp only [downstreamFields, Prod.mk.injEq] at fields
  obtain ⟨hMath, hControls, _, _, hPhases, _, _, _, hInverse, hWindow, _, _, _⟩ := fields
  have inverseTracked : ∀ spectra, (installGraphResult analyzed 0 result).inverse spectra 0 =
      fun sample => (DSP.Resynthesis.ifft (m := 16384) (spectra 0)).sample sample.val := by
    change ∀ spectra, analyzed.inverse spectra 0 = _
    rw [hInverse]
    exact inverse
  have hBound : ((installGraphResult analyzed 0 result).spectrum.graph 0).order.length ≤ 2047 := by
    simpa only [installGraphResult, Function.update_self] using bound
  have hFlattened : ∀ bin ∈ ((installGraphResult analyzed 0 result).spectrum.graph 0).order,
      let root := (((installGraphResult analyzed 0 result).spectrum.graph 0).link.get bin).root
      (((installGraphResult analyzed 0 result).spectrum.graph 0).link.get root).root = root := by
    simpa only [installGraphResult, Function.update_self] using flattened
  have finished := finishOne_run (installGraphResult analyzed 0 result) hBound hFlattened inverseTracked
  simpa only [installGraphResult, Function.update_self, hMath, math, hControls, hInput, hMagnitude,
    hPhases, hWindow, synthesisFromGraph, exactChannel] using finished

private def modelAnalysis (s : Scene ℝ ℝ) : PVDRInput ℝ ℝ 4096 :=
  analysisInput exactMath (s.forward true 0) (s.forward false 0)

private def modelMagnitude (s : Scene ℝ ℝ) : BinTable ℝ 4096 :=
  slewMagnitudes exactMath (s.spectrum.controls 0).slewUp (exactChannel s).magnitude
    (modelAnalysis s).magnitude

private def modelGraph (s : Scene ℝ ℝ) : GraphResult :=
  exactGraphResult exactQueue s.draw (exactChannel s).gates (modelAnalysis s) s.rng

private abbrev modelSynthesis (s : Scene ℝ ℝ) (g : GraphResult) : SpectralResult :=
  synthesisFromGraph s (modelAnalysis s) (modelMagnitude s) g

/-- The mathematical spectral step for the active lane. -/
def processOneSpectrum (s : Scene ℝ ℝ) : SpectralResult :=
  (processSpectrum exactMath exactQueue s.draw (exactChannel s) (s.spectrum.controls 0)
    (s.forward true 0) (s.forward false 0)).run s.rng

private def observeFrame (s : Scene ℝ ℝ) : MonoObservation :=
  (exactChannel s, s.rng, s.frame 0)

private def observeSpectrum (s : Scene ℝ ℝ) (r : SpectralResult) : MonoObservation :=
  (r.1.1, r.2, fun sample =>
    (DSP.Resynthesis.ifft (m := 16384) r.1.2).sample sample.val * s.window sample)

private theorem observe_eq (before after : Scene ℝ ℝ) (r : SpectralResult)
    (h : exactChannel after = r.1.1 ∧ after.rng = r.2 ∧
      ∀ sample : FrameIndex, after.frame 0 sample =
        (DSP.Resynthesis.ifft (m := 16384) r.1.2).sample sample.val * before.window sample) :
    observeFrame after = observeSpectrum before r :=
  Prod.ext h.1 (Prod.ext h.2.1 (funext h.2.2))

private theorem synthesize_model (s : Scene ℝ ℝ) :
    processOneSpectrum s = modelSynthesis s (modelGraph s) := by
  unfold processOneSpectrum
  rw [processSpectrum_run]
  have widen : exactMath.widen = @id ℝ := rfl
  have hop : (4096 : Nat) / 4 = 1024 := by decide
  unfold modelSynthesis synthesisFromGraph modelGraph modelMagnitude modelAnalysis exactGraphResult
  simp only [widen, hop]

private theorem frameAnalysis (state : Scene ℝ ℝ) (math : state.spectrum.math = exactMath) :
    let analyzed := (analyzeOne.run state).2
    downstreamFields analyzed = downstreamFields state ∧
      analyzed.spectrum.input 0 = modelAnalysis state ∧
      analyzed.spectrum.magnitudes 0 = modelMagnitude state := by
  have analysis := analyzeFrame_run state 1 0 (by decide)
  rw [math] at analysis
  simpa only [analyzeOne, modelAnalysis, modelMagnitude, exactChannel] using analysis

private theorem frameGraph (state analyzed : Scene ℝ ℝ)
    (math : state.spectrum.math = exactMath)
    (fields : downstreamFields analyzed = downstreamFields state)
    (input : analyzed.spectrum.input 0 = modelAnalysis state) :
    ((analyzeGraph 0).run analyzed).2 = installGraphResult analyzed 0 (modelGraph state) := by
  simp only [downstreamFields, Prod.mk.injEq] at fields
  obtain ⟨hMath, _, hGates, _, _, _, _, _, _, _, hDraw, hRng, _⟩ := fields
  have step := analyzeGraph_run_exact analyzed 0 (hMath.trans math)
  have transfer : exactGraphResult exactQueue analyzed.draw (analyzed.spectrum.gates 0)
      (analyzed.spectrum.input 0) analyzed.rng = modelGraph state := by
    unfold modelGraph exactChannel
    rw [hDraw, hRng, hGates, input]
  exact (congrArg (fun x : PUnit × Scene ℝ ℝ => x.2) step).trans
    (congrArg (installGraphResult analyzed 0) transfer)

private theorem frameRender (state analyzed : Scene ℝ ℝ)
    (graph : GraphResult)
    (math : state.spectrum.math = exactMath)
    (fields : downstreamFields analyzed = downstreamFields state)
    (hInput : analyzed.spectrum.input 0 = modelAnalysis state)
    (hMagnitude : analyzed.spectrum.magnitudes 0 = modelMagnitude state)
    (bound : graph.1.2.order.length ≤ 2047)
    (flat : ∀ bin ∈ graph.1.2.order,
      let root := (graph.1.2.link.get bin).root
      root ∈ graph.1.2.order ∧ (graph.1.2.link.get root).root = root)
    (inverse : ∀ spectra, state.inverse spectra 0 =
      fun sample => (DSP.Resynthesis.ifft (m := 16384) (spectra 0)).sample sample.val) :
    observeFrame (renderOne (installGraphResult analyzed 0 graph)) = observeSpectrum state (modelSynthesis state graph) := by
  have flatOnly (bin : ActiveBin 4096) (inOrder : bin ∈ graph.1.2.order) :
      let root := (graph.1.2.link.get bin).root
      (graph.1.2.link.get root).root = root := (flat bin inOrder).2
  exact observe_eq state (renderOne (installGraphResult analyzed 0 graph)) (modelSynthesis state graph)
    (finishOne_with_graph state analyzed _ _ graph math fields hInput hMagnitude bound flatOnly inverse)

private theorem frameGraphFacts (state : Scene ℝ ℝ) (heap : PVDR.QueueMembership exactQueue) :
    let graph := (modelGraph state).1.2
    graph.order.length ≤ 2047 ∧ ∀ bin ∈ graph.order,
      let root := (graph.link.get bin).root
      root ∈ graph.order ∧ (graph.link.get root).root = root := by
  unfold modelGraph
  exact exactGraph_bounds_and_roots exactQueue heap state.draw (exactChannel state).gates
    (modelAnalysis state) state.rng

private theorem pipelineObserve
    (state analyzed : Scene ℝ ℝ) (graph : GraphResult)
    (actual : MonoObservation)
    (hProcess : actual = observeFrame (renderOne ((analyzeGraph 0).run analyzed).2))
    (hGraph : ((analyzeGraph 0).run analyzed).2 = installGraphResult analyzed 0 graph)
    (hRender : observeFrame (renderOne (installGraphResult analyzed 0 graph)) =
      observeSpectrum state (modelSynthesis state graph))
    (hModel : processOneSpectrum state = modelSynthesis state graph) :
    actual = observeSpectrum state (processOneSpectrum state) :=
  hProcess.trans ((congrArg (observeFrame ∘ renderOne) hGraph).trans
    (hRender.trans (congrArg (observeSpectrum state) hModel.symm)))

private theorem frame_observations (state : Scene ℝ ℝ) (math : state.spectrum.math = exactMath)
    (heap : PVDR.QueueMembership exactQueue)
    (inverse : ∀ spectra, state.inverse spectra 0 =
      fun sample => (DSP.Resynthesis.ifft (m := 16384) (spectra 0)).sample sample.val) :
    observeFrame (processOne.run state).2 = observeSpectrum state (processOneSpectrum state) := by
  let analyzed := (analyzeOne.run state).2
  have analyzedEq := frameAnalysis state math
  have facts := frameGraphFacts state heap
  exact pipelineObserve state analyzed (modelGraph state) (observeFrame (processOne.run state).2)
    (congrArg observeFrame (processOne_run state))
    (frameGraph state analyzed math analyzedEq.1 analyzedEq.2.1)
    (frameRender state analyzed (modelGraph state) math analyzedEq.1 analyzedEq.2.1 analyzedEq.2.2
      facts.1 facts.2 inverse)
    (synthesize_model state)

private theorem observeComponents (before after : Scene ℝ ℝ) (result : SpectralResult)
    (related : observeFrame after = observeSpectrum before result) :
    exactChannel after = result.1.1 ∧ after.rng = result.2 ∧
    ∀ sample : FrameIndex, after.frame 0 sample =
      (DSP.Resynthesis.ifft (m := 16384) result.1.2).sample sample.val * before.window sample :=
  ⟨congrArg Prod.fst related, congrArg (fun o : MonoObservation => o.2.1) related,
    fun sample => congrArg (fun o : MonoObservation => o.2.2 sample) related⟩

/-- In the exact one-channel interpretation, the generated frame agrees with
the mathematical spectral step followed by the finite inverse DFT and the
supplied window. The result includes every frame sample and all retained
channel and random-source state. -/
theorem processOne_refines (state : Scene ℝ ℝ) (math : state.spectrum.math = exactMath)
    (heap : PVDR.QueueMembership exactQueue)
    (inverse : ∀ spectra, state.inverse spectra 0 =
      fun sample => (DSP.Resynthesis.ifft (m := 16384) (spectra 0)).sample sample.val) :
    let result := processOneSpectrum state
    let actual := (processOne.run state).2
    exactChannel actual = result.1.1 ∧ actual.rng = result.2 ∧
    ∀ sample : FrameIndex, actual.frame 0 sample =
      (DSP.Resynthesis.ifft (m := 16384) result.1.2).sample sample.val * state.window sample :=
  observeComponents state (processOne.run state).2 (processOneSpectrum state)
    (frame_observations state math heap inverse)

end

end DSP.Resynthesis.FrameStorage

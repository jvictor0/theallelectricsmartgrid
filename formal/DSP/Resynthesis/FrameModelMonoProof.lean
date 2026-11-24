import DSP.Resynthesis.Grain

namespace DSP.Resynthesis

variable {R P Q RNG : Type} {n m : ℕ}
variable [Add R] [Sub R] [Mul R] [Div R] [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfNat R 3]
  [OfScientific R] [LT R] [LE R] [DecidableLT R] [DecidableLE R] [Max R] [IfLess R]
variable [Add P] [Sub P] [Mul P] [Div P] [Neg P] [OfNat P 0] [OfNat P 1]
  [OfScientific P] [LT P] [DecidableLT P] [IfLess P]

private theorem run_finish_pair {S A B C : Type} (action : StateM S (A × B))
    (finish : A → B → C) (state : S) :
    ((do
      let (a, b) ← action
      return finish a b : StateM S C).run state) =
      (finish (action.run state).1.1 (action.run state).1.2, (action.run state).2) := rfl

/-- Spectral processing retains the gate, slew magnitude and phase tables
and returns the spectrum synthesized from the analyzed graph. -/
theorem processSpectrum_run (math : GrainMath R P) (queue : PVDR.Queue R Q n)
    (nextPhase : StateM RNG R) (state : ChannelState R P n) (control : Controls R)
    (previous current : DFT R n) (rng : RNG) :
    let analysis := analysisInput math previous current
    let result := (PVDR.analyze math.widen (math.natPhase (n / 4)) math.thresholds queue
      nextPhase state.gates analysis).run rng
    let mags := slewMagnitudes math control.slewUp state.magnitude analysis.magnitude
    let phases := fun osc => fixupPhases math (oscillatorDetune control osc)
      analysis result.1.1 result.1.2 (state.phase osc)
    (processSpectrum math queue nextPhase state control previous current).run rng =
      ((⟨result.1.1, mags, phases⟩,
        synthesizeSpectrum math control analysis.omega mags phases result.1.2), result.2) := by
  let analysis := analysisInput math previous current
  let mags := slewMagnitudes math control.slewUp state.magnitude analysis.magnitude
  let action := PVDR.analyze math.widen (math.natPhase (n / 4)) math.thresholds queue
    nextPhase state.gates analysis
  let finish (gates : BinTable GateState n) (graph : PVDRGraph P n) :=
    let phases := fun osc => fixupPhases math (oscillatorDetune control osc)
      analysis gates graph (state.phase osc)
    ((⟨gates, mags, phases⟩ : ChannelState R P n),
      synthesizeSpectrum math control analysis.omega mags phases graph)
  exact run_finish_pair action finish rng

/-- With one lane, the packed model performs exactly one spectral process and
passes its result through the supplied inverse and final window. -/
theorem process_mono_run (fourier : Fourier R n m 1) (math : GrainMath R P)
    (queue : PVDR.Queue R Q n) (nextPhase : StateM RNG R)
    (state : State R P n 1) (input : GrainInput R n 1) (rng : RNG) :
    let packed := (process fourier math queue nextPhase state input).run rng
    let mono := (processSpectrum math queue nextPhase (state 0) (input.controls 0)
      (fourier.fft input.previous 0) (fourier.fft input.current 0)).run rng
    ((packed.1.1 0, packed.1.2 0), packed.2) =
      ((mono.1.1, math.hann (fourier.ifft (fun _ => mono.1.2) 0)), mono.2) := by
  have mapId {A B : Type} (f : A → B) (x : Id A) : f <$> x = f x := rfl
  simp [process, Vector.ofFnM_succ, mapId]

end DSP.Resynthesis

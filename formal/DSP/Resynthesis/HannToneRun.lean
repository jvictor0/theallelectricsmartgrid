import DSP.Processor.Run
import DSP.Resynthesis.GrainProofs
import DSP.Resynthesis.HannToneOverlap

namespace DSP.Resynthesis

noncomputable section

/-- The read head can choose any analysis start per grain. Both local grids
sample the source at unit host-rate spacing, one analysis hop apart. -/
def hannToneInput {n : ℕ} (amplitude phase : ℝ) (bin : ℕ) (control : Controls ℝ)
    (start : ℝ) : GrainInput ℝ n 1 where
  previous _ := hannSinusoid amplitude bin ((bin : ℝ) / n * (start - ↑(n / 4)) + phase)
  current _ := hannSinusoid amplitude bin ((bin : ℝ) / n * start + phase)
  controls _ := control

/-- The actual packed grain processor with one lane, run on a sinusoidal
source and returning the updated tracker, draw cursor, and output frame. -/
def hannToneStep {n m : ℕ} [NeZero n] [NeZero m] {Q RNG : Type} (queue : PVDR.Queue ℝ Q n)
    (nextPhase : StateM RNG ℝ) (amplitude phase : ℝ) (bin : ℕ) (control : Controls ℝ) :
    Processor (ChannelState ℝ ℝ n × RNG) ℝ (Buffer ℝ m) := fun state start =>
  let result := ((process exactFourier exactMath queue nextPhase (fun _ => state.1)
    (hannToneInput amplitude phase bin control start)).run state.2)
  ((result.1.1 0, result.2), result.1.2 0)

theorem hannToneStep_eq {n m : ℕ} [NeZero n] [NeZero m] {Q RNG : Type} (queue : PVDR.Queue ℝ Q n)
    (nextPhase : StateM RNG ℝ) (amplitude phase : ℝ) (bin : ℕ) (control : Controls ℝ)
    (state : ChannelState ℝ ℝ n × RNG) (start : ℝ) :
    hannToneStep (m := m) queue nextPhase amplitude phase bin control state start =
      let input := hannToneInput (n := n) amplitude phase bin control start
      let result := ((processSpectrum exactMath queue nextPhase state.1 control
        (fft (input.previous 0)) (fft (input.current 0))).run state.2)
      ((result.1.1, result.2), exactMath.hann (ifft result.1.2)) := by
  have mapId {A B : Type} (f : A → B) (x : Id A) : f <$> x = f x := rfl
  simp [hannToneStep, process, Vector.ofFnM_succ, exactFourier, hannToneInput, mapId]

theorem fixupPhases_root_of_mem {n : ℕ} (math : GrainMath ℝ ℝ) (detune : ℝ)
    (input : PVDRInput ℝ ℝ n) (gates : BinTable GateState n) (graph : PVDRGraph ℝ n)
    (phases : BinTable ℝ n) {bin : ActiveBin n} (unique : graph.order.Nodup)
    (member : bin ∈ graph.order) (root : (graph.link.get bin).root = bin) :
    (fixupPhases math detune input gates graph phases).get bin =
      if gates.get bin == .triggered then math.widen (input.phase.get bin)
      else phases.get bin + (graph.link.get bin).delta * math.widen detune := by
  obtain ⟨before, after, order⟩ := List.append_of_mem member
  have split := List.nodup_append.mp (order ▸ unique)
  have absent_before : bin ∉ before := fun mem => split.2.2 bin mem bin (by simp) rfl
  have absent_after := (List.nodup_cons.mp split.2.1).1
  exact fixupPhases_root math detune input gates graph phases bin before after order absent_before absent_after root

/-- Connect the per-grain phase advance to the four overlapping frames. The
phase on the first grain anchors the output; subsequent read starts enter
only the incoming frames. -/
theorem steadyTone_overlap_of_step {n outputHop : ℕ} {RNG : Type}
    (root : ActiveBin n) (step : Processor (ChannelState ℝ ℝ n × RNG) ℝ (Buffer ℝ (4 * outputHop)))
    (initial : ChannelState ℝ ℝ n × RNG) (start anchor : ℕ → ℝ)
    (amplitude ratio advance : ℝ) (target : ℕ)
    (startsLow : initial.1.gates.get root = .low)
    (advanceShift : ratio * advance = (target : ℝ) / 4)
    (stepMatches : ∀ (state : ChannelState ℝ ℝ n × RNG) grain,
      let next := step state (start grain)
      next.1.1.gates.get root ≠ .low ∧
        (next.1.1.phase 0).get root = (if state.1.gates.get root = .low then anchor grain
          else (state.1.phase 0).get root + advance) ∧
        next.2 = exactMath.hann (hannSinusoid amplitude target (ratio * (next.1.1.phase 0).get root)))
    (grain : ℕ) (offset : Fin outputHop) (ready : 3 ≤ grain) :
    steadyOverlapAdd (Processor.run step start initial) grain offset =
      sinusoid amplitude ((target : ℝ) / (4 * outputHop)) (ratio * anchor 0)
        (grain * outputHop + offset.val : ℕ) := by
  have phaseState (i : ℕ) :
      (Processor.runState step start initial (i + 1)).1.gates.get root ≠ .low ∧
        ((Processor.runState step start initial (i + 1)).1.phase 0).get root =
          anchor 0 + (i : ℝ) * advance := by
    induction i with
    | zero =>
      have first := stepMatches initial 0
      simpa [startsLow] using And.intro first.1 first.2.1
    | succ i ih =>
      have next := stepMatches (Processor.runState step start initial (i + 1)) (i + 1)
      refine ⟨next.1, ?_⟩
      rw [Processor.runState_succ, next.2.1, if_neg ih.1, ih.2]
      push_cast
      ring
  apply steadyOverlapAdd_hannSinusoid (Processor.run step start initial) amplitude (ratio * anchor 0)
    target grain offset ready
  intro i _
  have state := (phaseState i).2
  rw [Processor.runState_succ] at state
  rw [Processor.run, (stepMatches (Processor.runState step start initial i) i).2.2, state]
  have outputHopPos : 0 < outputHop := Nat.zero_lt_of_lt offset.isLt
  have outputHopNe : (outputHop : ℝ) ≠ 0 := by exact_mod_cast outputHopPos.ne'
  congr 2
  calc
    ratio * (anchor 0 + (i : ℝ) * advance) = ratio * anchor 0 + (i : ℝ) * (ratio * advance) := by ring
    _ = _ := by
      rw [advanceShift]
      push_cast
      field_simp

end

end DSP.Resynthesis

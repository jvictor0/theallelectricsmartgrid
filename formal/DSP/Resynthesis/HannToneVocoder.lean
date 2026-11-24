import DSP.Resynthesis.HannToneRun
import DSP.Resynthesis.HannToneAnalysis
import DSP.Resynthesis.HannToneInverse
import DSP.Resynthesis.HannTonePVDR
import DSP.Resynthesis.HannToneSynthesis
import Mathlib.Tactic.FinCases

namespace DSP.Resynthesis

noncomputable section

def PitchRatio.factor (shift : PitchRatio) : ℝ :=
  (shift.numerator : ℝ) / (shift.denominator : ℝ)

/-- Phase chosen by the first triggered grain, in turns. -/
def hannToneAnchor {n : ℕ} (amplitude phase : ℝ) (root : ActiveBin n) (start : ℝ) : ℝ :=
  exactMath.phase (exactMath.polar (amplitude / 4) ((root.coefficient.val : ℝ) / n * start + phase))

private def queueMembership {n : ℕ} {Q : Type} {queue : PVDR.Queue ℝ Q n}
    (law : PVDR.QueueAccounting queue) : PVDR.QueueMembership queue where
  contains q entry := entry ∈ law.contents q
  empty entry := by simp [law.empty]
  push q added entry mem := List.mem_cons.mp ((law.push q added).mem_iff.mp mem)
  popped q entry rest h := (law.popSome q entry rest h).mem_iff.mpr (by simp)
  rest q entry rest h other mem := (law.popSome q entry rest h).mem_iff.mpr (List.mem_cons_of_mem _ mem)

private theorem immediateMagnitude {n : ℕ} (cutoff : ℝ) (immediate : 0.5 ≤ cutoff)
    (previous target : BinTable ℝ n) (bin : ActiveBin n) :
    (slewMagnitudes exactMath cutoff previous target).get bin = target.get bin := by
  have get (f : ActiveBin n → ℝ) : (BinTable.ofFn f).get bin = f bin := by
    simp [BinTable.ofFn, BinTable.get]
  rw [slewMagnitudes, if_pos immediate]
  change (BinTable.ofFn fun k => slewMagnitude (previous.get k) (target.get k) 1).get bin = target.get bin
  simp only [get, slewMagnitude, IfLess.ifLess, mul_one]
  split
  · ring
  · rfl

/-- A single grain of an interior bin-centered sinusoid. The old root phase
advances by the input pitch times the analysis hop after the first trigger. -/
theorem hannToneStep_sinusoid {n m : ℕ} [NeZero n] [NeZero m] {Q RNG : Type}
    {queue : PVDR.Queue ℝ Q n} (law : PVDR.QueueMaximum queue) (nextPhase : StateM RNG ℝ)
    {amplitude : ℝ} (positive : 0 < amplitude) (phase start : ℝ) (root : ActiveBin n)
    (target : ℕ) (inputLow : 1 < root.coefficient.val) (inputHigh : root.coefficient.val + 1 < n / 2)
    (targetLow : 1 < target) (targetHigh : target + 1 < n / 2) (outputGrid : n ≤ m)
    (control : Controls ℝ) (wet : control.fade = 1) (solo : control.unisonGain = 0)
    (immediate : 0.5 ≤ control.slewUp) (targetEq : control.shift.factor * root.coefficient.val = target)
    (state : ChannelState ℝ ℝ n × RNG) :
    let next := hannToneStep (m := m) queue nextPhase amplitude phase root.coefficient.val control state start
    next.1.1.gates.get root = (if state.1.gates.get root = .low then .triggered else .high) ∧
      (next.1.1.phase 0).get root = (if state.1.gates.get root = .low then
        hannToneAnchor amplitude phase root start else (state.1.phase 0).get root +
          (root.coefficient.val : ℝ) / n * ↑(n / 4)) ∧
      next.2 = exactMath.hann (hannSinusoid amplitude target (control.shift.factor * (next.1.1.phase 0).get root)) := by
  let bin := root.coefficient.val
  let frequency : ℝ := (bin : ℝ) / n
  let hop : ℝ := ↑(n / 4)
  let capture := hannToneInput (n := n) amplitude phase bin control start
  let previous := fft (capture.previous 0)
  let current := fft (capture.current 0)
  let input := analysisInput exactMath previous current
  let band := interiorBand bin inputLow inputHigh
  let left := band 0
  let right := band 2
  have middle : band 1 = root := by
    apply Fin.ext
    change bin - 2 + 1 = root.val
    dsimp [bin, ActiveBin.coefficient]
    change 1 < root.val + 1 at inputLow
    omega
  have rootPos : 0 < root.val := by change 1 < root.val + 1 at inputLow; omega
  have leftIndex : left.coefficient.val + 1 = bin := by
    change (interiorBand bin inputLow inputHigh 0).coefficient.val + 1 = bin
    rw [interiorBand_coefficient]
    change bin - 1 + 0 + 1 = bin
    omega
  have rightIndex : right.coefficient.val = bin + 1 := by
    change (interiorBand bin inputLow inputHigh 2).coefficient.val = bin + 1
    rw [interiorBand_coefficient]
    change bin - 1 + 2 = bin + 1
    omega
  have rootLeft : root ≠ left := by intro eq; rw [← eq] at leftIndex; change bin + 1 = bin at leftIndex; omega
  have rootRight : root ≠ right := by intro eq; rw [← eq] at rightIndex; change bin = bin + 1 at rightIndex; omega
  have leftVal : left.val = root.val - 1 := by
    change left.val + 1 + 1 = root.val + 1 at leftIndex
    omega
  have rightVal : right.val = root.val + 1 := by
    change right.val + 1 = root.val + 1 + 1 at rightIndex
    omega
  have above : root.above = some right := by
    have good : root.val + 1 < n / 2 - 1 := rightVal ▸ right.isLt
    simp only [ActiveBin.above, dif_pos good, Option.some.injEq]
    exact Fin.ext rightVal.symm
  have below : root.below = some left := by
    simp only [ActiveBin.below, dif_pos rootPos, Option.some.injEq]
    exact Fin.ext leftVal.symm
  obtain ⟨prevMag, currMag, freq, angle, polarity⟩ :=
    analysisInput_hannSinusoid positive phase start bin inputLow inputHigh
  have magnitudes (b : ActiveBin n) : input.magnitude.get b =
      if b = root then amplitude / 4 else if b = left ∨ b = right then amplitude / 8 else 0 := by
    have centerEq : b.coefficient.val = bin ↔ b = root := by
      simp [bin, ActiveBin.coefficient, Fin.ext_iff]
    have leftEq : b.coefficient.val + 1 = bin ↔ b = left := by
      rw [← leftIndex]
      simp [ActiveBin.coefficient, Fin.ext_iff]
    have rightEq : b.coefficient.val = bin + 1 ↔ b = right := by
      rw [← rightIndex]
      simp [ActiveBin.coefficient, Fin.ext_iff]
    exact (currMag b).trans (if_congr centerEq rfl (if_congr (or_congr leftEq rightEq) rfl rfl))
  have previousMagnitudes (b : ActiveBin n) : input.prevMagnitude.get b = input.magnitude.get b :=
    (prevMag b).trans (currMag b).symm
  let result := ((PVDR.analyze id hop exactMath.thresholds queue nextPhase state.1.gates input).run state.2)
  let gates := result.1.1
  let graph := result.1.2
  let magnitude := slewMagnitudes exactMath control.slewUp state.1.magnitude input.magnitude
  let fixed := fun i => fixupPhases exactMath (oscillatorDetune control i) input gates graph (state.1.phase i)
  have invariant := PVDR.analyze_hann_tone law hop amplitude positive nextPhase state.1.gates input
    state.2 left root right above below magnitudes previousMagnitudes
  obtain ⟨gate, centerLink, leftLink, rightLink, otherSmall⟩ := invariant
  change gates.get root = _ at gate
  change graph.link.get root = _ at centerLink
  change graph.link.get left = _ at leftLink
  change graph.link.get right = _ at rightLink
  have order := PVDR.analyze_order_perm_finRange id hop exactMath.thresholds queue law.toQueueAccounting
    nextPhase state.1.gates input state.2
  change graph.order.Perm _ at order
  have inOrder (b : ActiveBin n) : b ∈ graph.order := order.mem_iff.mpr (List.mem_finRange b)
  have unique : graph.order.Nodup := order.nodup_iff.mpr (List.nodup_finRange _)
  have rootOfRoot : (graph.link.get root).root = root := by rw [centerLink]
  have centerPhase : (fixed 0).get root =
      if state.1.gates.get root = .low then hannToneAnchor amplitude phase root start
      else (state.1.phase 0).get root + frequency * hop := by
    change (fixupPhases exactMath (oscillatorDetune control 0) input gates graph (state.1.phase 0)).get root = _
    rw [fixupPhases_root_of_mem _ _ _ _ _ _ unique (inOrder root) rootOfRoot, centerLink, gate]
    have haveFrequency : input.omega.get root = frequency := freq root rfl
    have haveAngle : input.phase.get root = hannToneAnchor amplitude phase root start := angle root rfl
    rw [haveFrequency, haveAngle]
    split
    · simp [exactMath, show (GateState.triggered == GateState.triggered) = true by rfl]
    · simp [oscillatorDetune, detuneFor, exactMath, show (GateState.high == GateState.triggered) = false by rfl]
  have follower (b : ActiveBin n) (different : root ≠ b) (link : (graph.link.get b).root = root) :
      (fixed 0).get b = (fixed 0).get root + (input.phase.get b - input.phase.get root) := by
    have steps := analyze_fixup_follower exactMath hop (oscillatorDetune control 0) queue
      (queueMembership law.toQueueAccounting) nextPhase state.1.gates input (state.1.phase 0) state.2 b (inOrder b)
    change (graph.link.get b).root ≠ b → (fixed 0).get b =
      (fixed 0).get (graph.link.get b).root + (input.phase.get b - input.phase.get (graph.link.get b).root) at steps
    rw [link] at steps
    exact steps different
  have slew (b : ActiveBin n) : magnitude.get b = input.magnitude.get b :=
    immediateMagnitude _ immediate _ _ _
  have inBandSmall (b : ActiveBin n) : (graph.link.get b).small = false ↔ ∃ i : Fin 3, b = band i := by
    constructor
    · intro h
      by_cases l : b = left
      · exact ⟨0, l⟩
      by_cases c : b = root
      · exact ⟨1, c.trans middle.symm⟩
      by_cases r : b = right
      · exact ⟨2, r⟩
      have := otherSmall b l c r
      change (graph.link.get b).small = true at this
      rw [this] at h
      contradiction
    · rintro ⟨i, rfl⟩
      fin_cases i
      · change (graph.link.get left).small = false
        rw [leftLink]
      · change (graph.link.get (band 1)).small = false
        rw [middle, centerLink]
      · change (graph.link.get right).small = false
        rw [rightLink]
  have inBandRoot (i : Fin 3) : (graph.link.get (band i)).root = band 1 := by
    rw [middle]
    fin_cases i
    · change (graph.link.get left).root = root
      rw [leftLink]
    · change (graph.link.get (band 1)).root = root
      rw [middle, centerLink]
    · change (graph.link.get right).root = root
      rw [rightLink]
  have inBandMagnitude (i : Fin 3) : magnitude.get (band i) = if i = 1 then amplitude / 4 else amplitude / 8 := by
    rw [slew, magnitudes]
    fin_cases i
    · change (if left = root then _ else if left = left ∨ left = right then _ else _) =
        if (0 : Fin 3) = 1 then _ else _
      simp [rootLeft.symm]
    · change (if band 1 = root then _ else if band 1 = left ∨ band 1 = right then _ else _) =
        if (1 : Fin 3) = 1 then _ else _
      simp [middle]
    · change (if right = root then _ else if right = left ∨ right = right then _ else _) =
        if (2 : Fin 3) = 1 then _ else _
      simp [rootRight.symm]
  have inBandPolarity (i : Fin 3) (side : i ≠ 1) (q : ℝ) :
      exactMath.polar (amplitude / 8) (q + (graph.link.get (band i)).delta) =
        exactMath.polar (-amplitude / 8) q := by
    fin_cases i
    · change exactMath.polar (amplitude / 8) (q + (graph.link.get left).delta) = _
      rw [leftLink]
      exact polarity root left rfl (Or.inl leftIndex) q
    · exact (side rfl).elim
    · change exactMath.polar (amplitude / 8) (q + (graph.link.get right).delta) = _
      rw [rightLink]
      exact polarity root right rfl (Or.inr rightIndex) q
  have rendered : synthesizeSpectrum exactMath control input.omega magnitude fixed graph =
      hannSinusoidSpectrum amplitude target (control.shift.factor * (fixed 0).get root) := by
    by_cases unity : control.shift = .unity
    · have ratioOne : control.shift.factor = 1 := by simp [PitchRatio.factor, unity, PitchRatio.unity]
      have targetMatches : bin = target := by
        have : (bin : ℝ) = target := by simpa [ratioOne, bin] using targetEq
        exact_mod_cast this
      have quiet (b : ActiveBin n) (outside : ¬ ∃ i : Fin 3, b = band i) : magnitude.get b = 0 := by
        rw [slew, magnitudes, if_neg (fun eq => outside ⟨1, eq.trans middle.symm⟩),
          if_neg (fun side => outside (side.elim (fun eq => ⟨0, eq⟩) (fun eq => ⟨2, eq⟩)))]
      have sidePhase (i : Fin 3) (side : i ≠ 1) :
          exactMath.polar (amplitude / 8) ((fixed 0).get (band i)) =
            exactMath.polar (-amplitude / 8) ((fixed 0).get (band 1)) := by
        have projected : (graph.link.get (band i)).root = root := (inBandRoot i).trans middle
        have distinct : root ≠ band i := by
          fin_cases i
          · exact rootLeft
          · exact (side rfl).elim
          · exact rootRight
        have offsets : (graph.link.get (band i)).delta = input.phase.get (band i) - input.phase.get root := by
          fin_cases i
          · exact congrArg PhaseLink.delta leftLink
          · exact (side rfl).elim
          · exact congrArg PhaseLink.delta rightLink
        rw [follower _ distinct projected, ← offsets, ← middle]
        exact inBandPolarity i side ((fixed 0).get (band 1))
      have dry := synthesizeSpectrum_unity_hann control input.omega magnitude fixed graph amplitude
        inputLow inputHigh wet solo unity inBandMagnitude quiet sidePhase
      change _ = hannSinusoidSpectrum amplitude bin ((fixed 0).get (band 1)) at dry
      rw [middle, targetMatches] at dry
      rw [ratioOne, one_mul]
      exact dry
    · have frequencyShifted : input.omega.get (band 1) * control.shift.factor = (target : ℝ) / n := by
        rw [middle, (show input.omega.get root = frequency from freq root rfl)]
        change (bin : ℝ) / n * control.shift.factor = (target : ℝ) / n
        rw [div_mul_eq_mul_div, mul_comm (bin : ℝ), targetEq]
      have shifted := synthesizeSpectrum_shifted_hann control input.omega magnitude fixed graph amplitude
        inputLow inputHigh targetLow targetHigh wet solo unity unique inOrder inBandSmall inBandRoot
        frequencyShifted inBandMagnitude inBandPolarity
      change _ = hannSinusoidSpectrum amplitude target ((fixed 0).get (band 1) * control.shift.factor) at shifted
      rw [middle, mul_comm ((fixed 0).get root)] at shifted
      exact shifted
  have spectrumRun : ((processSpectrum exactMath queue nextPhase state.1 control previous current).run state.2) =
      ((⟨gates, magnitude, fixed⟩, synthesizeSpectrum exactMath control input.omega magnitude fixed graph), result.2) := rfl
  dsimp only
  rw [hannToneStep_eq]
  change (let res := ((processSpectrum exactMath queue nextPhase state.1 control previous current).run state.2)
    res.1.1.gates.get root = (if state.1.gates.get root = .low then .triggered else .high) ∧
      (res.1.1.phase 0).get root = (if state.1.gates.get root = .low then
        hannToneAnchor amplitude phase root start else (state.1.phase 0).get root + frequency * hop) ∧
      exactMath.hann (ifft (m := m) res.1.2) = exactMath.hann
        (hannSinusoid amplitude target (control.shift.factor * (res.1.1.phase 0).get root)))
  rw [spectrumRun]
  dsimp only
  refine ⟨gate, centerPhase, ?_⟩
  rw [rendered, ifft_hannSinusoidSpectrum _ _ target targetLow targetHigh outputGrid]

/-- The exact phase vocoder produces a stationary sinusoid from a stationary
sinusoid with arbitrary analysis starts. The input tone and the shifted tone
land on interior bins, and overlap-add has four active grains. -/
theorem steadyVocoder_sinusoid {n outputHop : ℕ} [NeZero n] [NeZero (4 * outputHop)] {Q RNG : Type}
    {queue : PVDR.Queue ℝ Q n} (law : PVDR.QueueMaximum queue) (nextPhase : StateM RNG ℝ) (rng : RNG)
    {amplitude : ℝ} (positive : 0 < amplitude) (phase : ℝ) (start : ℕ → ℝ) (root : ActiveBin n)
    (target : ℕ) (inputLow : 1 < root.coefficient.val) (inputHigh : root.coefficient.val + 1 < n / 2)
    (targetLow : 1 < target) (targetHigh : target + 1 < n / 2)
    (inputGrid : n = 4 * (n / 4)) (outputGrid : n ≤ 4 * outputHop)
    (control : Controls ℝ) (wet : control.fade = 1) (solo : control.unisonGain = 0)
    (immediate : 0.5 ≤ control.slewUp) (targetEq : control.shift.factor * root.coefficient.val = target)
    (grain : ℕ) (offset : Fin outputHop) (ready : 3 ≤ grain) :
    let step := hannToneStep (m := 4 * outputHop) queue nextPhase amplitude phase root.coefficient.val control
    steadyOverlapAdd (Processor.run step start (ChannelState.clear, rng)) grain offset =
      sinusoid amplitude ((target : ℝ) / (4 * outputHop))
        (control.shift.factor * hannToneAnchor amplitude phase root (start 0))
        (grain * outputHop + offset.val : ℕ) := by
  dsimp only
  apply steadyTone_overlap_of_step root _ _ start (fun i => hannToneAnchor amplitude phase root (start i))
    amplitude control.shift.factor ((root.coefficient.val : ℝ) / n * ↑(n / 4)) target _ _ _ grain offset ready
  · simp [ChannelState.clear, BinTable.ofFn, BinTable.get]
  · have grid : (n : ℝ) = 4 * (↑(n / 4) : ℝ) := by exact_mod_cast inputGrid
    have nonzero : (n : ℝ) ≠ 0 := by exact_mod_cast (NeZero.ne n)
    calc
      _ = (control.shift.factor * root.coefficient.val) * (↑(n / 4) / (n : ℝ)) := by ring
      _ = _ := by
        rw [targetEq]
        field_simp
        rw [grid]
        ring
  · intro state i
    have step := hannToneStep_sinusoid (m := 4 * outputHop) law nextPhase positive phase (start i) root target
      inputLow inputHigh targetLow targetHigh outputGrid control wet solo immediate targetEq state
    refine ⟨?_, step.2.1, step.2.2⟩
    rw [step.1]
    split <;> decide

end

end DSP.Resynthesis

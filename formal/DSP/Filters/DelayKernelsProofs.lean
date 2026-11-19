import DSP.Filters.DelayKernels
import DSP.Filters.TunedDecayComb.Phase
import DSP.Filters.TunedDecayComb.ResponseProofs
import DSP.Nonlinear.Real

noncomputable section
namespace DSP

instance : DelayMath ℝ :=
  ⟨fun x => Real.exp (Real.log 2 * x), fun x => (⌊x⌋ : ℤ),
    fun y x => Complex.arg ⟨x, y⟩ / (2 * Real.pi), fun x => Real.exp (-(Real.log 1000 * x))⟩

namespace DelayKernels

def Coefficients.ofPrepared {capacity : ℕ} (c : TunedDecayComb.Coefficients capacity ℝ) : Coefficients ℝ :=
  ⟨c.age.val + 1, c.a, c.b, c.r, c.delayGain, c.g, c.k, c.h, c.gain, c.sign,
    if c.bypass then 1 else 0⟩

theorem combStep_refines {capacity : ℕ} (c : TunedDecayComb.Coefficients capacity ℝ)
    (s : TunedDecayComb.Branch ℝ) (x delayed : ℝ) :
    (combStep ⟨Coefficients.ofPrepared c, s⟩ ⟨x, delayed⟩).memory = TunedDecayComb.branchStep c s x delayed := by
  cases hb : c.bypass <;>
    norm_num [combStep, Coefficients.ofPrepared, TunedDecayComb.branchStep, IfLess.ifLess, hb]

theorem allPassStep_refines {capacity : ℕ} (c : SchroederAllPass.Coefficients capacity ℝ)
    (s : SchroederAllPass.State capacity ℝ) (x : ℝ) :
    let next := allPassStep ⟨c.gain, s.y, 0⟩ ⟨x, s.buffer.read c.age⟩
    SchroederAllPass.step c s ⟨x⟩ = ⟨s.buffer.push next.written, next.y⟩ := rfl

theorem lowpass_refines {capacity : ℕ} (c : TunedDecayComb.Coefficients capacity ℝ)
    (q : ComplexValue ℝ) : lowpass (Coefficients.ofPrepared c) q = TunedDecayComb.Snapshot.lowpass c q := by
  cases hb : c.bypass <;>
    norm_num [lowpass, Coefficients.ofPrepared, TunedDecayComb.Snapshot.lowpass, IfLess.ifLess, hb,
      ComplexValue.ofReal]

theorem delay_phase (N : ℕ) (f : ℝ) :
    (⟨Trig.cos2pi (-((N : ℝ) * f)), Trig.sin2pi (-((N : ℝ) * f))⟩ : ComplexValue ℝ).value =
      ResponseFrequency.unitDelay f ^ N := by
  rw [TunedDecayComb.unitDelay_nat]
  apply Complex.ext <;>
    simp [ComplexValue.value, ResponseFrequency.unitDelay_re, ResponseFrequency.unitDelay_im,
      cos2pi_real, sin2pi_real, mul_comm, mul_left_comm, mul_assoc]

theorem loop_refines {capacity : ℕ} (c : TunedDecayComb.Coefficients capacity ℝ) (f : ℝ)
    (l : ComplexValue ℝ) :
    (loop (Coefficients.ofPrepared c) (ResponseFrequency.ofCyclesPerSample f) l).value =
      (TunedDecayComb.Snapshot.loop c (ResponseFrequency.ofCyclesPerSample f).zInverse l).value := by
  simp only [loop, TunedDecayComb.Snapshot.loop, ComplexValue.value_mul,
    ComplexValue.value_scale, ComplexValue.value_div, ComplexValue.value_add,
    ComplexValue.value_ofReal, ComplexValue.value_pow, Coefficients.ofPrepared]
  have hd := delay_phase (c.age.val + 1) f
  push_cast at hd
  simp only [ResponseFrequency.ofCyclesPerSample_cyclesPerSample]
  rw [hd]
  have hf : (ResponseFrequency.ofCyclesPerSample f).zInverse.value = ResponseFrequency.unitDelay f :=
    ResponseFrequency.ofCyclesPerSample_delay f
  rw [hf]

theorem response_refines {capacity : ℕ} (c₀ c₁ : TunedDecayComb.Coefficients capacity ℝ) (m f : ℝ) :
    let result := (response ⟨Coefficients.ofPrepared c₀, Coefficients.ofPrepared c₁, m⟩ (ResponseFrequency.ofCyclesPerSample f)).2
    result.numerator.value / result.denominator.value =
      TunedDecayComb.mixedResponse (c₀.map Complex.ofReal) (c₁.map Complex.ofReal) (m : ℂ)
        (ResponseFrequency.unitDelay f) := by
  dsimp only [response]
  rw [lowpass_refines, lowpass_refines]
  simp only [ComplexValue.value_add, ComplexValue.value_scale, ComplexValue.value_sub,
    ComplexValue.value_ofReal, loop_refines]
  rw [TunedDecayComb.Snapshot.loop_value _ _ _ (TunedDecayComb.Snapshot.lowpass_value _ _),
    TunedDecayComb.Snapshot.loop_value _ _ _ (TunedDecayComb.Snapshot.lowpass_value _ _),
    TunedDecayComb.Snapshot.lowpass_value, TunedDecayComb.Snapshot.lowpass_value]
  simp only [Complex.ofReal_sub, Complex.ofReal_one, TunedDecayComb.mixedResponse]
  have hf : (ResponseFrequency.ofCyclesPerSample f).zInverse.value = ResponseFrequency.unitDelay f :=
    ResponseFrequency.ofCyclesPerSample_delay f
  rw [hf]

theorem prepare_refines {capacity : ℕ} (p : TunedDecayComb.LogicalSpec) (hp : p.Valid capacity)
    (old : Coefficients ℝ) :
    prepare old ⟨p.frequency,p.feedbackSign,p.log2DecaySeconds,p.dampOctaves,p.sampleRate⟩ =
      Coefficients.ofPrepared (p.prepare hp) := by
  have hcut : IfLess.ifLess (p.frequency * DelayMath.exp2 p.dampOctaves) (0.49 : ℝ)
      (p.frequency * DelayMath.exp2 p.dampOctaves) 0.49 = p.cutoff := by
    simp only [IfLess.ifLess, DelayMath.exp2, TunedDecayComb.LogicalSpec.cutoff]
    norm_num only
    rw [min_comm, min_def]
    split_ifs <;> first | rfl | linarith
  have hb : IfLess.ifLess p.feedbackSign (0 : ℝ) 0 (IfLess.ifLess 0 p.feedbackSign 0 1) =
      if p.feedbackSign = 0 then (1 : ℝ) else 0 := by
    rcases hp.2.2.2.2.2.2 with h | h | h <;> norm_num [IfLess.ifLess, h]
  have hphase : IfLess.ifLess (if p.feedbackSign = 0 then (1 : ℝ) else 0) (1 / 2)
      (-(DelayMath.phaseTurns (p.k * Real.sqrt p.u) (1 - p.u))) 0 = p.phase / (2 * Real.pi) := by
    by_cases hs : p.feedbackSign = 0
    · norm_num [IfLess.ifLess, hs, TunedDecayComb.LogicalSpec.phase]
    · simp [IfLess.ifLess, hs, TunedDecayComb.LogicalSpec.phase, DelayMath.phaseTurns, neg_div]
  have hd : (p.cycles + p.phase / (2 * Real.pi)) / p.frequency = p.delay := by
    unfold TunedDecayComb.LogicalSpec.delay
    have hf := hp.2.1.ne'
    have hpi := Real.pi_ne_zero
    field_simp
  have hn : IfLess.ifLess (1 : ℝ) (DelayMath.floor p.delay - 1)
      (DelayMath.floor p.delay - 1) 1 = (p.samples : ℝ) := by
    have hpos := (p.delay_bounds hp).1
    have hf : 1 ≤ ⌊p.delay⌋₊ := (Nat.le_floor_iff (by linarith)).mpr (by simpa using hpos.le)
    change (if 1 < ((⌊p.delay⌋ : ℤ) : ℝ) - 1 then ((⌊p.delay⌋ : ℤ) : ℝ) - 1 else 1) =
      ((max 1 (⌊p.delay⌋₊ - 1) : ℕ) : ℝ)
    rw [← natCast_floor_eq_intCast_floor (by linarith : 0 ≤ p.delay), Nat.cast_max,
      Nat.cast_sub hf, Nat.cast_one]
    rw [max_def]
    split_ifs <;> first | rfl | linarith
  have hr : DelayMath.decay (1 / (p.sampleRate * p.duration)) = p.radius := by
    change Real.exp _ = Real.exp _
    congr 1
    ring
  have hgain : DelayMath.decay ((p.samples : ℝ) / (p.sampleRate * p.duration)) = p.radius ^ p.samples := by
    change Real.exp _ = Real.exp _ ^ p.samples
    rw [← Real.exp_nat_mul]
    congr 1
    ring
  have hnage : (p.prepare hp).age.val + 1 = p.samples :=
    DelayBuffer.ofDelay_samples p.samples (p.samples_fraction_bounds hp).1
      (p.samples_fraction_bounds hp).2.1.le
  let phase := IfLess.ifLess (if p.feedbackSign = 0 then (1 : ℝ) else 0) (1 / 2)
    (-(DelayMath.phaseTurns (p.k * Real.sqrt p.u) (1 - p.u))) 0
  let delay := (p.cycles + phase) / p.frequency
  let whole := IfLess.ifLess (1 : ℝ) (DelayMath.floor delay - 1) (DelayMath.floor delay - 1) 1
  let td := Real.tan (Real.pi * (p.frequency * (delay - whole)))
  let a := (p.t - td) / (p.t + td)
  have hdelay : delay = p.delay := by dsimp only [delay, phase]; rw [hphase, hd]
  have hwhole : whole = (p.samples : ℝ) := by dsimp only [whole]; rw [hdelay, hn]
  have ha : a = p.a := by
    dsimp only [a, td]
    rw [hdelay, hwhole]
    simp [TunedDecayComb.LogicalSpec.a, TunedDecayComb.LogicalSpec.td,
      TunedDecayComb.LogicalSpec.fraction, mul_assoc]
  dsimp only [prepare]
  rw [hcut, hb]
  simp only [← pow_two, tanPi_real, sqrt_real]
  change Coefficients.mk whole a
    (Real.sqrt (IfLess.ifLess 0 (1 - a ^ 2) (1 - a ^ 2) 0))
    (DelayMath.decay (1 / (p.sampleRate * p.duration)))
    (DelayMath.decay (whole / (p.sampleRate * p.duration)))
    p.g p.k (1 / (1 + p.k * p.g + p.g ^ 2)) p.gain p.feedbackSign
    (if p.feedbackSign = 0 then 1 else 0) = _
  simp only [hwhole, ha, hr, hgain]
  unfold Coefficients.ofPrepared
  have hncast : ((p.prepare hp).age.val : ℝ) + 1 = (p.samples : ℝ) := by exact_mod_cast hnage
  rw [hncast]
  simp only [TunedDecayComb.LogicalSpec.prepare, beq_iff_eq, IfLess.ifLess]
  congr 2
  rw [max_def]
  split_ifs <;> first | rfl | linarith

end DelayKernels
end DSP

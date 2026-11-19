import DSP.Filters.TunedDecayComb.Tuning

noncomputable section
namespace DSP.TunedDecayComb

open ResponseFrequency

theorem unitDelay_normSq (f : ℝ) : Complex.normSq (unitDelay f) = 1 := by
  simp [Complex.normSq_apply, unitDelay_re, unitDelay_im, ← sq, Real.cos_sq_add_sin_sq]

theorem unitDelay_add (f g : ℝ) : unitDelay (f + g) = unitDelay f * unitDelay g := by
  unfold unitDelay
  rw [← Complex.exp_add]
  congr 1
  push_cast
  ring

theorem unitDelay_nat (f : ℝ) (n : ℕ) : unitDelay f ^ n = unitDelay (f * n) := by
  induction n with
  | zero => simp [unitDelay]
  | succ n ih =>
    rw [pow_succ, ih]
    rw [Nat.cast_add, Nat.cast_one, mul_add, mul_one, unitDelay_add]

theorem unitDelay_cayley (f : ℝ) (hf : Tuning.ValidCutoff f) :
    unitDelay f = (1 - Complex.I * (Tuning.warp f : ℂ)) /
      (1 + Complex.I * (Tuning.warp f : ℂ)) := by
  have hc : (Real.cos (Real.pi * f) : ℂ) ≠ 0 :=
    Complex.ofReal_ne_zero.mpr (ne_of_gt (Tuning.cos_pos f hf))
  have he : unitDelay f =
      Complex.exp (-((Real.pi * f : ℝ) : ℂ) * Complex.I) /
      Complex.exp (((Real.pi * f : ℝ) : ℂ) * Complex.I) := by
    rw [← Complex.exp_sub]
    unfold unitDelay
    congr 1
    push_cast
    ring
  rw [he]
  have hexp : Complex.exp (-((Real.pi * f : ℝ) : ℂ) * Complex.I) =
      (Real.cos (Real.pi * f) : ℂ) - Complex.I * Real.sin (Real.pi * f) := by
    rw [Complex.exp_mul_I]
    simp
    ring
  have hexp' : Complex.exp (((Real.pi * f : ℝ) : ℂ) * Complex.I) =
      (Real.cos (Real.pi * f) : ℂ) + Complex.I * Real.sin (Real.pi * f) := by
    rw [Complex.exp_mul_I]
    simp
    ring
  have hd := Complex.exp_ne_zero (((Real.pi * f : ℝ) : ℂ) * Complex.I)
  rw [hexp'] at hd
  rw [hexp, hexp', Tuning.warp, Real.tan_eq_sin_div_cos]
  rw [Complex.ofReal_div]
  have htden : 1 + Complex.I * ((Real.sin (Real.pi * f) : ℂ) / Real.cos (Real.pi * f)) ≠ 0 := by
    intro h
    apply hd
    have he := congrArg (fun z : ℂ => z * (Real.cos (Real.pi * f) : ℂ)) h
    field_simp at he
    linear_combination he
  field_simp

theorem allpass_fraction_phase (f delta : ℝ)
    (hf : Tuning.ValidCutoff f) (hd : Tuning.ValidCutoff (f * delta)) :
    NormalizedAllPass.response
      (((Tuning.warp f - Tuning.warp (f * delta)) /
        (Tuning.warp f + Tuning.warp (f * delta)) : ℝ) : ℂ) 1 (unitDelay f) =
      unitDelay (f * delta) := by
  have ht := Tuning.warp_pos f hf
  have hu := Tuning.warp_pos (f * delta) hd
  have hsum : Tuning.warp f + Tuning.warp (f * delta) ≠ 0 := by positivity
  have ha : |(Tuning.warp f - Tuning.warp (f * delta)) /
      (Tuning.warp f + Tuning.warp (f * delta))| < 1 := by
    rw [abs_lt]
    constructor
    · apply (lt_div_iff₀ (by positivity)).mpr; linarith
    · apply (div_lt_iff₀ (by positivity)).mpr; linarith
  have hap := NormalizedAllPass.unit_denominator_ne_zero
    ((Tuning.warp f - Tuning.warp (f * delta)) / (Tuning.warp f + Tuning.warp (f * delta)))
    1 (unitDelay f) (by simpa using ha) (unitDelay_normSq f)
  have hsumC : (Tuning.warp f : ℂ) + Tuning.warp (f * delta) ≠ 0 := by exact_mod_cast hsum
  have hden (t : ℝ) : 1 + Complex.I * (t : ℂ) ≠ 0 := by
    intro he
    have h := congrArg Complex.re he
    simp at h
  have hdT := hden (Tuning.warp f)
  have hdU := hden (Tuning.warp (f * delta))
  simp only [Complex.ofReal_one, mul_one] at hap
  rw [NormalizedAllPass.response, one_mul, mul_one, unitDelay_cayley (f * delta) hd]
  apply (div_eq_div_iff hap hdU).mpr
  rw [unitDelay_cayley f hf]
  push_cast
  have hdT' : 1 + (Tuning.warp f : ℂ) * Complex.I ≠ 0 := by simpa [mul_comm] using hdT
  field_simp [hdT', hdT, hsumC]
  ring

def prototype {capacity : ℕ} (c : Coefficients capacity ℝ) : Coefficients capacity ℝ :=
  { c with r := 1, delayGain := 1 }

theorem frequency_cayley (f : ℝ) (hf : Tuning.ValidCutoff f) :
    1 + unitDelay f ≠ 0 ∧ (1 - unitDelay f) / (1 + unitDelay f) = Complex.I * (Tuning.warp f : ℂ) := by
  have hd : 1 + Complex.I * (Tuning.warp f : ℂ) ≠ 0 := by
    intro h
    have := congrArg Complex.re h
    simp at this
  have hsum : 1 + unitDelay f = 2 / (1 + Complex.I * (Tuning.warp f : ℂ)) := by
    rw [unitDelay_cayley f hf]
    field_simp
    ring
  have hone : 1 + unitDelay f ≠ 0 := by rw [hsum]; exact div_ne_zero (by norm_num) hd
  refine ⟨hone, ?_⟩
  apply (div_eq_iff hone).mpr
  rw [unitDelay_cayley f hf]
  field_simp
  ring

theorem prepared_allpass_phase {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity) :
    NormalizedAllPass.response ((p.prepare hp).a : ℂ) 1 (unitDelay p.frequency) =
      unitDelay (p.frequency * p.fraction) := by
  have hf : Tuning.ValidCutoff p.frequency := ⟨hp.2.1, by linarith [hp.2.2.1]⟩
  obtain ⟨_, _, hd, hd2⟩ := p.samples_fraction_bounds hp
  have hfd : Tuning.ValidCutoff (p.frequency * p.fraction) :=
    ⟨mul_pos hp.2.1 hd, by nlinarith [hp.2.1, hp.2.2.1]⟩
  simpa [LogicalSpec.prepare, LogicalSpec.a, LogicalSpec.t, LogicalSpec.td, Tuning.warp, mul_assoc]
    using allpass_fraction_phase p.frequency p.fraction hf hfd

/-- The lowpass has exactly unit magnitude at the requested fundamental, with
phase equal to the negative atan2 of its normalized complex denominator. -/
theorem prepared_lowpass_phase {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity)
    (hs : p.feedbackSign ≠ 0) :
    lowpassResponse ((prototype (p.prepare hp)).map Complex.ofReal) (unitDelay p.frequency) =
      Complex.exp ((p.phase : ℂ) * Complex.I) := by
  obtain ⟨hg, hk, hv, hk2, hv2, hgt⟩ := p.lowpass_properties hp
  obtain ⟨hu, _⟩ := p.u_bounds hp
  have ht := p.t_pos hp
  have hsqrt := Real.sq_sqrt hu.le
  have hts : p.g * Real.sqrt p.u = p.t := by
    have he : (p.g * Real.sqrt p.u) ^ 2 = p.t ^ 2 := by rw [mul_pow, hsqrt]; exact hgt
    nlinarith [mul_pos hg (Real.sqrt_pos.mpr hu)]
  let Z : ℂ := ⟨1 - p.u, p.k * Real.sqrt p.u⟩
  have hZnorm : ‖Z‖ = p.gain := by
    have he : Complex.normSq Z = p.gain ^ 2 := by
      simp only [Z, Complex.normSq_apply, ← sq, mul_pow, hsqrt]
      linear_combination p.u * hk2 - hv2
    rw [Complex.normSq_eq_norm_sq] at he
    nlinarith [norm_nonneg Z]
  have hZ : Z ≠ 0 := by intro h; rw [h, norm_zero] at hZnorm; linarith
  have hgC : (p.g : ℂ) ≠ 0 := Complex.ofReal_ne_zero.mpr hg.ne'
  have hvC : (p.gain : ℂ) ≠ 0 := Complex.ofReal_ne_zero.mpr hv.ne'
  have hden : (Complex.I * (p.t : ℂ)) ^ 2 + (p.k * p.g : ℝ) *
      (Complex.I * (p.t : ℂ)) + (p.g ^ 2 : ℝ) = (p.g ^ 2 : ℝ) * Z := by
    apply Complex.ext
    · simp [Z, Complex.mul_re, Complex.mul_im, pow_two]
      nlinarith [hgt]
    · simp [Z, Complex.mul_re, Complex.mul_im, pow_two]
      linear_combination -(p.k * p.g) * hts
  have hf : Tuning.ValidCutoff p.frequency := ⟨hp.2.1, by linarith [hp.2.2.1]⟩
  obtain ⟨hone, hcay⟩ := frequency_cayley p.frequency hf
  have hproto := prototype_cayley p.g p.k (unitDelay p.frequency) hone
  rw [hcay] at hproto
  change (Complex.I * (p.t : ℂ)) ^ 2 + (p.k * p.g : ℝ) *
    (Complex.I * (p.t : ℂ)) + (p.g ^ 2 : ℝ) = _ at hproto
  rw [hden] at hproto
  have hLP : lowpassResponse ((prototype (p.prepare hp)).map Complex.ofReal) (unitDelay p.frequency) =
      (p.gain : ℂ) / Z := by
    unfold lowpassResponse
    rw [complex_lowpass_denominator]
    simp only [prototype, LogicalSpec.prepare, Coefficients.map, beq_iff_eq, hs,
      if_false, Complex.ofReal_one, one_mul]
    have hquot : (p.gain * p.g ^ 2 : ℝ) /
        (prototypeDenominator p.g p.k (unitDelay p.frequency) / (1 + unitDelay p.frequency) ^ 2) =
        (p.gain : ℂ) / Z := by
      rw [← hproto]
      push_cast
      field_simp
    rw [div_div_eq_mul_div] at hquot
    simpa using hquot
  rw [hLP]
  have harg := Complex.norm_mul_exp_arg_mul_I Z
  rw [hZnorm] at harg
  rw [← harg, div_mul_eq_div_div, div_self hvC, one_div, ← Complex.exp_neg]
  congr 1
  simp [LogicalSpec.phase, hs, Z]

theorem prepared_lowpass_unity {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity)
    (hs : p.feedbackSign ≠ 0) :
    ‖lowpassResponse ((prototype (p.prepare hp)).map Complex.ofReal) (unitDelay p.frequency)‖ = 1 := by
  rw [prepared_lowpass_phase p hp hs]
  simp

/-- The damping control places the undamped lowpass at half power at its
clamped octave frequency. The peak at the fundamental has unit power. -/
theorem prepared_lowpass_cutoff_half_power {capacity : ℕ} (p : LogicalSpec)
    (hp : p.Valid capacity) (hs : p.feedbackSign ≠ 0) :
    Complex.normSq (lowpassResponse ((prototype (p.prepare hp)).map Complex.ofReal)
      (unitDelay p.cutoff)) = 1 / 2 := by
  obtain ⟨hg, hk, hv, hk2, hv2, hgt⟩ := p.lowpass_properties hp
  have ht := p.t_pos hp
  have hshape : p.shape ^ 2 = 1 + (p.ratio ^ 2 - 1) ^ 2 :=
    Real.sq_sqrt (by positivity)
  have hgshape : p.g ^ 2 = p.t ^ 2 * p.shape := by
    rw [LogicalSpec.g, mul_pow, Real.sq_sqrt (by linarith [p.shape_gt_one hp])]
  have hratio : Tuning.warp p.cutoff = p.ratio * p.t := by
    rw [LogicalSpec.ratio, div_mul_cancel₀ _ ht.ne']
  have hnum : (p.gain * p.g ^ 2) ^ 2 = (Tuning.warp p.cutoff ^ 2 - p.t ^ 2) ^ 2 := by
    calc
      _ = (1 - p.u ^ 2) * (p.g ^ 2) ^ 2 := by rw [mul_pow, hv2]
      _ = (p.g ^ 2) ^ 2 - (p.g ^ 2 * p.u) ^ 2 := by ring
      _ = (p.t ^ 2) ^ 2 * (p.shape ^ 2 - 1) := by rw [hgt, hgshape]; ring
      _ = _ := by rw [hshape, hratio]; ring
  let s : ℂ := Complex.I * (Tuning.warp p.cutoff : ℂ)
  let den : ℂ := s ^ 2 + (p.k * p.g : ℝ) * s + (p.g ^ 2 : ℝ)
  have hgap := analog_lowpass_gap p.g p.k p.gain p.u s hk2 hv2
  have hden : Complex.normSq den = 2 * (p.gain * p.g ^ 2) ^ 2 := by
    have he : s.re = 0 := by simp [s]
    have hi : s.im = Tuning.warp p.cutoff := by simp [s]
    rw [he, hi, hgt] at hgap
    change Complex.normSq den - (p.gain * p.g ^ 2) ^ 2 = _ at hgap
    nlinarith [hnum]
  have hcut : Tuning.ValidCutoff p.cutoff :=
    ⟨lt_trans hp.2.1 (p.cutoff_bounds hp).1, (p.cutoff_bounds hp).2⟩
  obtain ⟨hone, hcay⟩ := frequency_cayley p.cutoff hcut
  have hproto := prototype_cayley p.g p.k (unitDelay p.cutoff) hone
  rw [hcay] at hproto
  have hLP : lowpassResponse ((prototype (p.prepare hp)).map Complex.ofReal) (unitDelay p.cutoff) =
      (p.gain * p.g ^ 2 : ℝ) / den := by
    unfold lowpassResponse
    rw [complex_lowpass_denominator]
    simp only [prototype, LogicalSpec.prepare, Coefficients.map, beq_iff_eq, hs,
      if_false, Complex.ofReal_one, one_mul]
    change _ = (p.gain * p.g ^ 2 : ℝ) /
      ((Complex.I * (Tuning.warp p.cutoff : ℂ)) ^ 2 + (p.k * p.g : ℝ) *
        (Complex.I * (Tuning.warp p.cutoff : ℂ)) + (p.g ^ 2 : ℝ))
    rw [hproto, div_div_eq_mul_div]
    push_cast
    rfl
  rw [hLP, Complex.normSq_div, hden]
  simp only [Complex.normSq_ofReal]
  have hn : p.gain * p.g ^ 2 ≠ 0 := (mul_pos hv (sq_pos_of_pos hg)).ne'
  field_simp


/-- The integer delay and fractional allpass exactly compensate the lowpass
phase at the fundamental. Negative feedback uses half a cycle. -/
theorem prepared_fundamental_loop {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity)
    (hs : p.feedbackSign ≠ 0) :
    returnResponse ((prototype (p.prepare hp)).map Complex.ofReal) (unitDelay p.frequency) = 1 := by
  have hn : (p.prepare hp).age.val + 1 = p.samples := by
    exact DelayBuffer.ofDelay_samples p.samples (p.samples_fraction_bounds hp).1
      (p.samples_fraction_bounds hp).2.1.le
  have he : p.frequency * p.samples + p.frequency * p.fraction =
      (2 * Real.pi * p.cycles + p.phase) / (2 * Real.pi) := by
    unfold LogicalSpec.fraction LogicalSpec.delay
    have hf := hp.2.1.ne'
    have hpi := Real.pi_ne_zero
    field_simp
    ring
  have hexp : unitDelay (p.frequency * p.samples) * unitDelay (p.frequency * p.fraction) *
      Complex.exp ((p.phase : ℂ) * Complex.I) =
      Complex.exp (-((2 * Real.pi * p.cycles : ℝ) : ℂ) * Complex.I) := by
    rw [← unitDelay_add, he, unitDelay, ← Complex.exp_add]
    congr 1
    push_cast
    have hpi : (Real.pi : ℂ) ≠ 0 := Complex.ofReal_ne_zero.mpr Real.pi_ne_zero
    field_simp
    ring
  have hsign : (p.feedbackSign : ℂ) *
      Complex.exp (-((2 * Real.pi * p.cycles : ℝ) : ℂ) * Complex.I) = 1 := by
    rcases hp.2.2.2.2.2.2 with h | h | h
    · simp only [LogicalSpec.cycles, h]
      norm_num
      rw [show (2 : ℂ) * Real.pi * (1 / 2) * Complex.I = (Real.pi : ℂ) * Complex.I by ring]
      simp [Complex.exp_neg, Complex.exp_pi_mul_I]
    · exact (hs h).elim
    · simp only [LogicalSpec.cycles, h]
      norm_num
      rw [Complex.exp_neg, Complex.exp_two_pi_mul_I]
      simp
  have hl := prepared_lowpass_phase p hp hs
  have ha := prepared_allpass_phase p hp
  unfold returnResponse
  rw [hl]
  change Complex.exp ((p.phase : ℂ) * Complex.I) * (p.feedbackSign : ℂ) *
    NormalizedAllPass.response ((p.prepare hp).a : ℂ) 1 (unitDelay p.frequency) * 1 *
      unitDelay p.frequency ^ ((p.prepare hp).age.val + 1) = 1
  rw [ha, hn, unitDelay_nat]
  calc
    _ = (p.feedbackSign : ℂ) * (unitDelay (p.frequency * p.samples) *
        unitDelay (p.frequency * p.fraction) * Complex.exp ((p.phase : ℂ) * Complex.I)) := by ring
    _ = 1 := by rw [hexp, hsign]

theorem prepared_dry_response {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity)
    (hs : p.feedbackSign = 0) (q : ℂ) : settledResponse ((p.prepare hp).map Complex.ofReal) q = 1 := by
  simp [settledResponse, returnResponse, lowpassResponse, LogicalSpec.prepare, Coefficients.map, hs]

theorem closedDenominator_radial {capacity : ℕ} (c : Coefficients capacity ℝ) (q : ℂ)
    (hr : c.delayGain = c.r ^ (c.age.val + 1)) :
    closedDenominator (c.map Complex.ofReal) q =
      closedDenominator ((prototype c).map Complex.ofReal) ((c.r : ℂ) * q) := by
  cases hb : c.bypass <;>
    simp [closedDenominator, lowpassDenominator, Coefficients.map, prototype, hr, hb, mul_pow]
  all_goals ring

/-- The tuned fundamental attains the radius bound, so its selected T60 is
exact in the frozen real model. Its angle is preserved by radial damping. -/
theorem prepared_fundamental_pole {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity)
    (hs : p.feedbackSign ≠ 0) :
    let z := (p.radius : ℂ) * (unitDelay p.frequency)⁻¹
    closedDenominator ((p.prepare hp).map Complex.ofReal) z⁻¹ = 0 ∧ ‖z‖ = p.radius := by
  obtain ⟨_, hc, ha, hr0, _, hr⟩ := p.prepare_properties hp
  have hrad : (p.radius : ℂ) ≠ 0 := Complex.ofReal_ne_zero.mpr hr0.ne'
  have hq : unitDelay p.frequency ≠ 0 := Complex.exp_ne_zero _
  have hqc : ‖unitDelay p.frequency‖ = 1 := by
    have h := unitDelay_normSq p.frequency
    rw [Complex.normSq_eq_norm_sq] at h
    nlinarith [norm_nonneg (unitDelay p.frequency)]
  have hc' : PeakNormalized (prototype (p.prepare hp)) := hc
  have hLP := lowpass_disk_bound (prototype (p.prepare hp)) (unitDelay p.frequency) hc'
    (by simpa [prototype] using (unitDelay_normSq p.frequency).le)
  have hAP := NormalizedAllPass.unit_denominator_ne_zero (p.prepare hp).a 1
    (unitDelay p.frequency) (by simpa using ha) (unitDelay_normSq p.frequency)
  have hreg : Regular ((prototype (p.prepare hp)).map Complex.ofReal) (unitDelay p.frequency) :=
    ⟨by simpa [prototype, Coefficients.map] using hAP, hLP.1⟩
  have hz : closedDenominator ((prototype (p.prepare hp)).map Complex.ofReal) (unitDelay p.frequency) = 0 := by
    rw [closedDenominator_factor _ _ hreg, prepared_fundamental_loop p hp hs, sub_self, mul_zero]
  constructor
  · rw [closedDenominator_radial _ _ hr]
    have he : ((p.prepare hp).r : ℂ) * ((p.radius : ℂ) * (unitDelay p.frequency)⁻¹)⁻¹ =
        unitDelay p.frequency := by
      change (p.radius : ℂ) * ((p.radius : ℂ) * (unitDelay p.frequency)⁻¹)⁻¹ = _
      field_simp
    rwa [he]
  · rw [norm_mul, norm_inv, hqc, inv_one, mul_one, Complex.norm_real, Real.norm_eq_abs]
    exact abs_of_nonneg hr0.le

/-- Numerator after clearing the internal lattice and lowpass denominators. -/
def closedNumerator {K : Type} [Field K] {capacity : ℕ}
    (c : Coefficients capacity K) (q : K) : K :=
  (1 + c.a * c.r * q) * (if c.bypass then 1 else c.gain * c.g ^ 2 * (1 + c.r * q) ^ 2)

theorem closedNumerator_factor {K : Type} [Field K] {capacity : ℕ}
    (c : Coefficients capacity K) (q : K) (hc : Regular c q) :
    closedNumerator c q = (1 + c.a * c.r * q) *
      (if c.bypass then 1 else lowpassDenominator c q) * lowpassResponse c q := by
  cases hb : c.bypass with
  | true => simp [closedNumerator, lowpassResponse, hb]
  | false =>
    simp only [closedNumerator, lowpassResponse, hb, Bool.false_eq_true, if_false]
    field_simp [hc.2 hb]

theorem closedNumerator_radial {capacity : ℕ} (c : Coefficients capacity ℝ) (q : ℂ) :
    closedNumerator (c.map Complex.ofReal) q =
      closedNumerator ((prototype c).map Complex.ofReal) ((c.r : ℂ) * q) := by
  cases hb : c.bypass <;>
    simp only [closedNumerator, Coefficients.map, prototype, hb, Bool.false_eq_true,
      if_false, if_true, Complex.ofReal_one, mul_one, one_mul]
  all_goals ring

theorem settledResponse_fraction {K : Type} [Field K] {capacity : ℕ}
    (c : Coefficients capacity K) (q : K) (hc : Regular c q) :
    settledResponse c q = closedNumerator c q / closedDenominator c q := by
  rw [closedNumerator_factor c q hc, closedDenominator_factor c q hc]
  have hd : (if c.bypass then 1 else lowpassDenominator c q) ≠ 0 := by
    cases hb : c.bypass with
    | true => simp
    | false => simpa [hb] using hc.2 hb
  exact (mul_div_mul_left _ _ (mul_ne_zero hc.1 hd)).symm

/-- The fundamental denominator root is not cancelled by the numerator. -/
theorem prepared_fundamental_uncancelled {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity)
    (hs : p.feedbackSign ≠ 0) :
    let z := (p.radius : ℂ) * (unitDelay p.frequency)⁻¹
    closedNumerator ((p.prepare hp).map Complex.ofReal) z⁻¹ ≠ 0 := by
  obtain ⟨_, hc, ha, hr0, _, _⟩ := p.prepare_properties hp
  have hrad : (p.radius : ℂ) ≠ 0 := Complex.ofReal_ne_zero.mpr hr0.ne'
  have hq : unitDelay p.frequency ≠ 0 := Complex.exp_ne_zero _
  have hLP := lowpass_disk_bound (prototype (p.prepare hp)) (unitDelay p.frequency) hc
    (by simpa [prototype] using (unitDelay_normSq p.frequency).le)
  have hAP := NormalizedAllPass.unit_denominator_ne_zero (p.prepare hp).a 1
    (unitDelay p.frequency) (by simpa using ha) (unitDelay_normSq p.frequency)
  have hreg : Regular ((prototype (p.prepare hp)).map Complex.ofReal) (unitDelay p.frequency) :=
    ⟨by simpa [prototype, Coefficients.map] using hAP, hLP.1⟩
  dsimp only
  rw [closedNumerator_radial]
  have he : ((p.prepare hp).r : ℂ) * ((p.radius : ℂ) * (unitDelay p.frequency)⁻¹)⁻¹ =
      unitDelay p.frequency := by
    change (p.radius : ℂ) * ((p.radius : ℂ) * (unitDelay p.frequency)⁻¹)⁻¹ = _
    field_simp
  rw [he, closedNumerator_factor _ _ hreg, prepared_lowpass_phase p hp hs]
  apply mul_ne_zero
  · apply mul_ne_zero hreg.1
    have hb : (p.prepare hp).bypass = false := by
      simp [LogicalSpec.prepare, hs]
    simpa [prototype, Coefficients.map, hb] using hreg.2 (by exact hb)
  · exact Complex.exp_ne_zero _

end DSP.TunedDecayComb

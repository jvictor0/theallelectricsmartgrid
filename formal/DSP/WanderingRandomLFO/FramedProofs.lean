import DSP.Filters.SquiggleBoyQuadReverb.WanderingProofs

namespace DSP.WanderingRandomLFO

open SmoothRandomLFO Set

noncomputable section

/-- The exact real value of the clock's open 52-bit uniform conversion. -/
def gridUnit (bits : UInt64) : ℝ :=
  unitFromHighBits ((bits >>> 12).toNat : ℝ)

theorem gridUnit_bounds (bits : UInt64) :
    (2 ^ 53 : ℝ)⁻¹ ≤ gridUnit bits ∧ gridUnit bits ≤ 1 := by
  let high := (bits >>> 12).toNat
  have hhigh : high < 2 ^ 52 := by
    simp only [high, UInt64.toNat_shiftRight]
    change bits.toNat >>> 12 < 2 ^ 52
    rw [Nat.shiftRight_eq_div_pow]
    exact (Nat.div_lt_iff_lt_mul (by decide)).mpr (by simpa using bits.toNat_lt)
  have form : gridUnit bits = ((high : ℝ) + 0.5) / (2 ^ 52 : ℝ) := by
    norm_num [gridUnit, unitFromHighBits, high]
  rw [form]
  constructor
  · calc
      _ = (1 / 2 : ℝ) / 2 ^ 52 := by norm_num
      _ ≤ _ := div_le_div_of_nonneg_right (by linarith [Nat.cast_nonneg (α := ℝ) high])
        (by positivity)
  · apply (div_le_one (by norm_num : (0 : ℝ) < 2 ^ 52)).2
    have hb : (high : ℝ) + 1 ≤ (2 : ℝ) ^ 52 := by exact_mod_cast hhigh
    linarith

/-- An upper bound on all exact speeds, including the initial speed and the
largest logarithm on the production draw grid. -/
def gridSpeedLimit (lambda : ℝ) : ℝ := 37 / max 1 lambda

theorem gridDraw_mem_Icc (lambda : ℝ) (bits : UInt64) :
    minValue 1 (-Real.log (gridUnit bits) / max 1 lambda) ∈ Icc 0 (gridSpeedLimit lambda) := by
  obtain ⟨hlo, hhi⟩ := gridUnit_bounds bits
  have hu : 0 < gridUnit bits := lt_of_lt_of_le (by positivity) hlo
  have hrate : 0 < max 1 lambda := lt_of_lt_of_le zero_lt_one (le_max_left _ _)
  have hloglo := Real.log_le_log (by positivity : 0 < (2 ^ 53 : ℝ)⁻¹) hlo
  rw [Real.log_inv, Real.log_pow] at hloglo
  norm_num at hloglo
  have hloghi := Real.log_nonpos hu.le hhi
  unfold gridSpeedLimit
  change (if -Real.log (gridUnit bits) / max 1 lambda < (1 : ℝ) then
    -Real.log (gridUnit bits) / max 1 lambda else 1) ∈ Icc 0 (37 / max 1 lambda)
  have hn : -Real.log (gridUnit bits) / max 1 lambda ∈ Icc 0 (37 / max 1 lambda) :=
    ⟨div_nonneg (neg_nonneg.mpr hloghi) hrate.le,
     div_le_div_of_nonneg_right (by linarith [Real.log_two_lt_d9]) hrate.le⟩
  split
  · exact hn
  · next h => exact ⟨zero_le_one, (le_of_not_gt h).trans hn.2⟩

/-- Speed held during each frame, with an update immediately before the first
advance. Index zero is the speed before the first update. -/
def gridSpeed (lambda alpha : ℝ) (draws : ℕ → UInt64) : ℕ → ℝ
  | 0 => 1 / max 1 lambda
  | n + 1 => smoothVelocity alpha (gridSpeed lambda alpha draws n)
      (minValue 1 (-Real.log (gridUnit (draws n)) / max 1 lambda))

theorem gridSpeed_mem_Icc (lambda alpha : ℝ) (draws : ℕ → UInt64)
    (ha : alpha ∈ Icc 0 1) (n : ℕ) : gridSpeed lambda alpha draws n ∈ Icc 0 (gridSpeedLimit lambda) := by
  induction n with
  | zero =>
    have hrate : 0 < max 1 lambda := lt_of_lt_of_le zero_lt_one (le_max_left _ _)
    exact ⟨by simp only [gridSpeed]; positivity,
      div_le_div_of_nonneg_right (by norm_num) hrate.le⟩
  | succ n ih =>
    exact SquiggleBoyQuadReverb.smoothVelocity_mem_Icc ha ih (gridDraw_mem_Icc lambda (draws n))

/-- The generated frame arithmetic, with speed measured in knots per engine
sample and start measured relative to the next knot. -/
def exactFrame (f : ℝ → ℝ) (start speed index : ℝ) : ℝ :=
  let left := f start
  let right := f (phaseAfter start speed (frameSamples : ℝ))
  reconstruct left (reconstructionSlope left right (frameSamples : ℝ)) index

theorem exactFrame_eq_frameValue (f : ℝ → ℝ) (start speed index : ℝ) (hs : speed ≠ 0) :
    exactFrame f start speed index =
      frameValue f start (phaseAfter start speed (frameSamples : ℝ)) (phaseAfter start speed index) := by
  norm_num [exactFrame, frameValue, frameSlope, phaseAfter, reconstruct, reconstructionSlope,
    frameSamples]
  field_simp [hs]
  ring

/-- At a speed of at most one knot per frame, the exact frame can encounter
one knot. This bound holds whether it crosses the knot or stays on one cubic. -/
theorem exactFrame_error_le {p0 p1 p2 p3 next start speed index : ℝ}
    (h0 : |p0| ≤ 1) (h1 : |p1| ≤ 1) (h2 : |p2| ≤ 1) (h3 : |p3| ≤ 1)
    (hn : |next| ≤ 1) (ha : start ∈ Icc (-1) 0) (hv : 0 ≤ speed)
    (hframe : speed * (frameSamples : ℝ) ≤ 1) (hi : index ∈ Icc 0 (frameSamples : ℝ)) :
    let f := joinValue (unitSegment p0 p1 p2 p3) next 1
    |exactFrame f start speed index - f (phaseAfter start speed index)| ≤
      (speed * (frameSamples : ℝ)) ^ 2 := by
  dsimp only
  let s := unitSegment p0 p1 p2 p3
  let f := joinValue s next 1
  change |exactFrame f start speed index - f (phaseAfter start speed index)| ≤ _
  rcases hv.eq_or_lt with hzero | hpos
  · simp [← hzero, exactFrame, reconstruct, reconstructionSlope, phaseAfter]
  have hF : 0 < (frameSamples : ℝ) := by norm_num [frameSamples]
  have hap : start < phaseAfter start speed (frameSamples : ℝ) := by
    unfold phaseAfter
    linarith [mul_pos hpos hF]
  have hit : phaseAfter start speed index ∈ Icc start (phaseAfter start speed (frameSamples : ℝ)) :=
    ⟨le_add_of_nonneg_right (mul_nonneg hv hi.1),
      add_le_add_right (mul_le_mul_of_nonneg_left hi.2 hv) _⟩
  have within : Icc start (phaseAfter start speed (frameSamples : ℝ)) ⊆ Icc (-s.width) s.next := by
    apply Icc_subset_Icc
    · exact ha.1
    · change start + speed * (frameSamples : ℝ) ≤ 1
      linarith [ha.2]
  have bs : SegmentBounded s 1 :=
    ⟨h0, h1, h2, h3, le_rfl, le_rfl, le_rfl, le_rfl, le_rfl⟩
  obtain ⟨hd, hdd⟩ := join_hasDerivs s next 1 (by norm_num [s, unitSegment])
    (by norm_num [s, unitSegment]) (by norm_num [s, unitSegment])
  rw [exactFrame_eq_frameValue _ _ _ _ hpos.ne']
  have bound := frameValue_error_le hap hit (fun t _ => hd t) (fun t _ => hdd t)
    (fun t ht => abs_join_acceleration_le one_pos bs hn le_rfl (within ht))
    (by norm_num : 0 ≤ (4 : ℝ) / 1 ^ 2)
  convert bound using 1
  unfold phaseAfter
  ring

theorem gridFrame_error_le {lambda alpha p0 p1 p2 p3 next start index : ℝ}
    (draws : ℕ → UInt64) (n : ℕ) (ha : alpha ∈ Icc 0 1)
    (hlambda : 37 * (frameSamples : ℝ) ≤ lambda)
    (h0 : |p0| ≤ 1) (h1 : |p1| ≤ 1) (h2 : |p2| ≤ 1) (h3 : |p3| ≤ 1)
    (hn : |next| ≤ 1) (hs : start ∈ Icc (-1) 0) (hi : index ∈ Icc 0 (frameSamples : ℝ)) :
    let f := joinValue (unitSegment p0 p1 p2 p3) next 1
    let speed := gridSpeed lambda alpha draws (n + 1)
    |exactFrame f start speed index - f (phaseAfter start speed index)| ≤
      (37 * (frameSamples : ℝ) / lambda) ^ 2 := by
  dsimp only
  have hF : 0 < (frameSamples : ℝ) := by norm_num [frameSamples]
  have hlarge : (1 : ℝ) ≤ lambda := by
    norm_num [frameSamples] at hlambda
    linarith
  have hpos : 0 < lambda := zero_lt_one.trans_le hlarge
  have hv := gridSpeed_mem_Icc lambda alpha draws ha (n + 1)
  have hwidth : gridSpeed lambda alpha draws (n + 1) * (frameSamples : ℝ) ≤
      37 * (frameSamples : ℝ) / lambda := by
    calc
      _ ≤ gridSpeedLimit lambda * (frameSamples : ℝ) := mul_le_mul_of_nonneg_right hv.2 hF.le
      _ = _ := by rw [gridSpeedLimit, max_eq_right hlarge]; ring
  have hone : 37 * (frameSamples : ℝ) / lambda ≤ 1 := (div_le_one hpos).2 hlambda
  exact (exactFrame_error_le h0 h1 h2 h3 hn hs hv.1 (hwidth.trans hone) hi).trans
    (pow_le_pow_left₀ (mul_nonneg hv.1 hF.le) hwidth 2)

theorem reverbFrame_error_le {alpha p0 p1 p2 p3 next start index : ℝ}
    (draws : ℕ → UInt64) (n : ℕ) (ha : alpha ∈ Icc 0 1)
    (h0 : |p0| ≤ 1) (h1 : |p1| ≤ 1) (h2 : |p2| ≤ 1) (h3 : |p3| ≤ 1)
    (hn : |next| ≤ 1) (hs : start ∈ Icc (-1) 0) (hi : index ∈ Icc 0 (frameSamples : ℝ)) :
    let f := joinValue (unitSegment p0 p1 p2 p3) next 1
    let speed := gridSpeed (squiggleBoyReverbLambdaSamples : ℝ) alpha draws (n + 1)
    |exactFrame f start speed index - f (phaseAfter start speed index)| ≤ 0.000000381 ∧
    let offset := WanderingSchroederAllPassGeneric.offsetSamples (squiggleBoyReverbWanderSpec.depth : ℝ)
    |offset (exactFrame f start speed index) - offset (f (phaseAfter start speed index))| ≤ 0.000049 := by
  dsimp only
  have h := gridFrame_error_le (lambda := (squiggleBoyReverbLambdaSamples : ℝ)) draws n ha
    (by norm_num [frameSamples, squiggleBoyReverbLambdaSamples]) h0 h1 h2 h3 hn hs hi
  have hsmall := h.trans (by norm_num [frameSamples, squiggleBoyReverbLambdaSamples] :
    (37 * (frameSamples : ℝ) / (squiggleBoyReverbLambdaSamples : ℝ)) ^ 2 ≤ 0.000000381)
  refine ⟨hsmall, ?_⟩
  have hd : (squiggleBoyReverbWanderSpec.depth : ℝ) = 128 := rfl
  rw [hd]
  exact (WanderingSchroederAllPassGeneric.abs_offsetSamples_sub_le (by norm_num) _ _).trans
    ((mul_le_mul_of_nonneg_left hsmall (by norm_num)).trans (by norm_num))

end

end DSP.WanderingRandomLFO

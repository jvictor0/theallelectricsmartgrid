import PhaseTime.InverseGrid

namespace PhaseTime.AnalysisStability

/-! Timestamp distance is not itself an audio error. This model establishes
exact common-shift invariance for a stationary resolved partial, and conditional
stability of normalized phase-controlled features over both analysis windows.
It does not prove general STFT/PVDR continuity, transient safety, unchanged gate
decisions, or inaudibility. The confinement criterion below supports FixedCell startup and coalescing;
that module gives the corresponding storage transitions. -/

noncomputable def blend (a b alpha : ℝ) : ℝ := a + alpha * (b - a)

theorem blend_inside (a b alpha : ℝ) (hab : a ≤ b)
    (ha : 0 ≤ alpha) (hb : alpha ≤ 1) :
    a ≤ blend a b alpha ∧ blend a b alpha ≤ b := by
  unfold blend
  constructor <;> nlinarith

def Confined (F : ℝ → ℝ) (first last lo hi : ℝ) : Prop :=
  ∀ t, first ≤ t → t ≤ last → lo ≤ F t ∧ F t ≤ hi

-- Monotonicity and exact latest-visit identity are deliberately not required.
-- @cpp grid_lookup: linear timestamp interpolation, under a weaker range certificate.
theorem confined_interpolation (F : ℝ → ℝ) (first last lo hi alpha q : ℝ)
    (h : Confined F first last lo hi) (ht : first ≤ last)
    (ha : 0 ≤ alpha) (hb : alpha ≤ 1) (hq : lo ≤ q ∧ q ≤ hi) :
    |F (blend first last alpha) - q| ≤ hi - lo := by
  have inside := blend_inside first last alpha ht ha hb
  have hf := h _ inside.1 inside.2
  apply abs_le.mpr
  constructor <;> linarith [hq.1, hq.2, hf.1, hf.2]

-- Both shifted analysis windows must be covered, not just their starting points.
theorem shifted_windows_confined (F : ℝ → ℝ) (t u firstOffset lastOffset r lo hi : ℝ)
    (htu : t ≤ u) (hr : firstOffset ≤ r ∧ r ≤ lastOffset)
    (h : Confined F (t + firstOffset) (u + lastOffset) lo hi) :
    |F (t + r) - F (u + r)| ≤ hi - lo := by
  have ht := h (t + r) (by linarith [hr.1]) (by linarith [hr.2])
  have hu := h (u + r) (by linarith [hr.1]) (by linarith [hr.2])
  apply abs_le.mpr
  constructor <;> linarith [ht.1, ht.2, hu.1, hu.2]

noncomputable def feature (n : Nat) (weight offset : Nat → ℝ)
    (F A : ℝ → ℝ) (t : ℝ) : ℝ :=
  ∑ i ∈ Finset.range n, weight i * A (F (t + offset i))

-- Applies to an ideal averaged envelope or frequency control. A native FFT
-- magnitude is not silently identified with this ideal feature.
-- @cpp analysis_pair: the same start-time displacement shifts both Hann windows.
theorem phase_feature_bound (n : Nat) (weight offset : Nat → ℝ)
    (F A : ℝ → ℝ) (t u gain epsilon : ℝ)
    (hw : ∀ i ∈ Finset.range n, 0 ≤ weight i)
    (hs : ∑ i ∈ Finset.range n, weight i = 1)
    (hg : 0 ≤ gain) (hA : ∀ x y, |A x - A y| ≤ gain * |x - y|)
    (hf : ∀ i ∈ Finset.range n, |F (t + offset i) - F (u + offset i)| ≤ epsilon) :
    |feature n weight offset F A t - feature n weight offset F A u| ≤ gain * epsilon := by
  unfold feature
  rw [← Finset.sum_sub_distrib]
  calc
    |∑ i ∈ Finset.range n, (weight i * A (F (t + offset i)) -
      weight i * A (F (u + offset i)))|
        ≤ ∑ i ∈ Finset.range n, |weight i * A (F (t + offset i)) -
          weight i * A (F (u + offset i))| := Finset.abs_sum_le_sum_abs _ _
    _ ≤ ∑ i ∈ Finset.range n, weight i * (gain * epsilon) := by
      apply Finset.sum_le_sum
      intro i hi
      rw [← mul_sub, abs_mul, abs_of_nonneg (hw i hi)]
      apply mul_le_mul_of_nonneg_left _ (hw i hi)
      exact le_trans (hA _ _) (mul_le_mul_of_nonneg_left (hf i hi) hg)
    _ = gain * epsilon := by rw [← Finset.sum_mul, hs, one_mul]

theorem confined_feature_bound (n : Nat) (weight offset : Nat → ℝ)
    (F A : ℝ → ℝ) (t u firstOffset lastOffset lo hi gain : ℝ)
    (htu : t ≤ u)
    (ho : ∀ i ∈ Finset.range n, firstOffset ≤ offset i ∧ offset i ≤ lastOffset)
    (hc : Confined F (t + firstOffset) (u + lastOffset) lo hi)
    (hw : ∀ i ∈ Finset.range n, 0 ≤ weight i)
    (hs : ∑ i ∈ Finset.range n, weight i = 1)
    (hg : 0 ≤ gain) (hA : ∀ x y, |A x - A y| ≤ gain * |x - y|) :
    |feature n weight offset F A t - feature n weight offset F A u| ≤ gain * (hi - lo) := by
  exact phase_feature_bound n weight offset F A t u gain (hi - lo) hw hs hg hA
    (fun i hmem => shifted_windows_confined F t u firstOffset lastOffset (offset i) lo hi htu (ho i hmem) hc)

noncomputable def carrierPhase (omega initial t : ℝ) : ℝ := omega * t + initial

noncomputable def advance (phase : ℝ → ℝ) (t hop : ℝ) : ℝ := phase t - phase (t - hop)

theorem carrier_advance (omega initial t hop : ℝ) :
    advance (carrierPhase omega initial) t hop = omega * hop := by
  unfold advance carrierPhase
  ring

theorem common_shift_advance (omega initial t hop delta : ℝ) :
    advance (carrierPhase omega initial) (t + delta) hop =
      advance (carrierPhase omega initial) t hop := by
  rw [carrier_advance, carrier_advance]

-- Wrapping can be any deterministic function: the equality holds even if the
-- carrier aliases in the phase estimator. It does not assert alias-free pitch.
-- @cpp analysis_frequency: subtract the expected advance, wrap, divide by H.
noncomputable def estimate (wrap : ℝ → ℝ) (phase : ℝ → ℝ)
    (t hop binOmega : ℝ) : ℝ :=
  wrap (advance phase t hop - binOmega * hop) / hop + binOmega

theorem stationary_estimate_shift (wrap : ℝ → ℝ) (omega initial t hop delta binOmega : ℝ) :
    estimate wrap (carrierPhase omega initial) (t + delta) hop binOmega =
      estimate wrap (carrierPhase omega initial) t hop binOmega := by
  simp only [estimate, common_shift_advance]

-- Resolved bins belonging to one stationary partial share the carrier speed;
-- their relative phases are unchanged by a common shift as well.
theorem stationary_relative_phase (omega a b t delta : ℝ) :
    carrierPhase omega a (t + delta) - carrierPhase omega b (t + delta) =
      carrierPhase omega a t - carrierPhase omega b t := by
  unfold carrierPhase
  ring

-- A High leader advances existing synthesis state. Triggered leaders instead
-- reset to an absolute analysis phase, which is outside this invariance claim.
-- @cpp analysis_steady_step: non-triggered leader phase propagation.
noncomputable def steadyStep (wrap : ℝ → ℝ) (phase : ℝ → ℝ)
    (t hop binOmega detune synthesisPhase : ℝ) : ℝ :=
  synthesisPhase + detune * hop * estimate wrap phase t hop binOmega

theorem stationary_step_shift (wrap : ℝ → ℝ)
    (omega initial t hop delta binOmega detune synthesisPhase : ℝ) :
    steadyStep wrap (carrierPhase omega initial) (t + delta) hop binOmega detune synthesisPhase =
      steadyStep wrap (carrierPhase omega initial) t hop binOmega detune synthesisPhase := by
  simp only [steadyStep, stationary_estimate_shift]

end PhaseTime.AnalysisStability

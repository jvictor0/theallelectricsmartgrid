import DSP.Filters.SquiggleBoyQuadReverb.Frozen.CubicSpectrum
import DSP.Filters.SquiggleBoyQuadReverb.Frozen.MixerNorm
import DSP.Filters.SquiggleBoyQuadReverb.Frozen.WanderExpectation

namespace DSP.SquiggleBoyQuadReverb.Frozen

open DelayBuffer MeasureTheory WanderingSchroederAllPass
open scoped BigOperators Matrix.Norms.L2Operator

noncomputable section

/-- The fractional part and the four ages for an ordinary cubic stencil. -/
def Tap.Consecutive {n : ℕ} (t : Tap n ℝ) : Prop :=
  ∀ i, t.fraction i ∈ Set.Icc 0 1 ∧
    ∃ base : ℕ, ∀ j : Fin 4, (t.age i j).val + 1 = base + (3 - j.val)

theorem Tap.Consecutive.norm_le_one {n : ℕ} {t : Tap n ℝ} (h : t.Consecutive)
    {q : ℂ} (hq : ‖q‖ = 1) (i : Fin 4) : ‖tapResponse t.complex q i‖ ≤ 1 := by
  obtain ⟨hf, base, hage⟩ := h i
  exact norm_tapResponse_le_one_of_consecutive t q i hq hf base hage

private def apfBase (spec : TapSpec) (i : Fin 4) : ℕ :=
  ((![spec.first - spec.depth, spec.second - spec.depth,
    spec.third - spec.depth, spec.fourth - spec.depth] : Fin 4 → UInt64) i).toNat

private theorem tapBase_real (spec : TapSpec) (i : Fin 4) :
    (tapBase spec : Lanes ℝ) i = (apfBase spec i : ℝ) := by
  fin_cases i <;> rfl

private theorem apfTap_consecutive {n : ℕ} (hn : 0 < n) (spec : TapSpec) (spline : Lanes ℝ)
    (hfit : ∀ i, 3 ≤ apfBase spec i ∧ apfBase spec i + 2 * spec.depth.toNat < n)
    (hcapacity : n ≤ UInt32.size) : (apfTap hn spec spline).Consecutive := by
  intro i
  let v := WanderingSchroederAllPassGeneric.offsetSamples (spec.depth.toNat : ℝ) (spline i)
  have hv : 0 ≤ v ∧ v ≤ 2 * (spec.depth.toNat : ℝ) :=
    WanderingSchroederAllPassGeneric.offsetSamples_bounds (by positivity) (spline i)
  have hround : (fractionalPosition v).1 = (⌈v⌉₊ : ℝ) := by
    change -((⌊-v⌋ : ℤ) : ℝ) = _
    rw [Int.floor_neg, Int.cast_neg, neg_neg, natCast_ceil_eq_intCast_ceil hv.1]
  have hceil : ⌈v⌉₊ ≤ 2 * spec.depth.toNat := Nat.ceil_le.mpr (by push_cast; exact hv.2)
  let oldest := apfBase spec i + ⌈v⌉₊
  have ho : 3 ≤ oldest ∧ oldest < n := by
    obtain ⟨hl, hu⟩ := hfit i
    dsimp only [oldest]
    omega
  have hAge : ((tapPosition spec spline).1.lane i).toNat = oldest := by
    change (UInt32.ofNat ⌊(tapBase spec : Lanes ℝ) i + (fractionalPosition v).1⌋₊).toNat = _
    rw [tapBase_real, hround, ← Nat.cast_add, Nat.floor_natCast]
    exact UInt32.toNat_ofNat_of_lt' (ho.2.trans_le hcapacity)
  refine ⟨?_, oldest - 2, ?_⟩
  · change (fractionalPosition v).1 - v ∈ Set.Icc 0 1
    rw [hround]
    exact ⟨sub_nonneg.mpr (Nat.le_ceil v), sub_le_iff_le_add'.mpr (Nat.ceil_lt_add_one hv.1).le⟩
  · intro j
    have hj : (UInt64.ofNat j.val).toNat = j.val :=
      UInt64.toNat_ofNat_of_lt' (j.isLt.trans_le (by decide))
    change (((tapPosition spec spline).1.lane i).toNat + n -
      (UInt64.ofNat j.val).toNat % n) % n + 1 = _
    rw [hAge, hj, Nat.mod_eq_of_lt (show j.val < n by omega),
      show oldest + n - j.val = n + (oldest - j.val) by omega,
      Nat.add_mod_left, Nat.mod_eq_of_lt (show oldest - j.val < n by omega)]
    omega

private theorem introTap_fits : ∀ stage : Fin 8,
    (∀ i, 3 ≤ apfBase (Runtime.introTap stage) i ∧
      apfBase (Runtime.introTap stage) i + 2 * (Runtime.introTap stage).depth.toNat <
        squiggleBoyReverbIntroAPFCapacity stage) ∧
      squiggleBoyReverbIntroAPFCapacity stage ≤ UInt32.size := by decide

private theorem diffusionTaps_consecutive (order : List (Fin 8)) (s : Diffusion order)
    (j : Fin order.length) : (diffusionTaps order s j).Consecutive := by
  induction order with
  | nil => exact j.elim0
  | cons i order ih =>
    refine Fin.cases ?_ (fun k => ?_) j
    · have hi : 0 < squiggleBoyReverbIntroAPFCapacity i :=
        (Nat.zero_le _).trans_lt ((introTap_fits i).1 0).2
      exact apfTap_consecutive hi (Runtime.introTap i) _ (introTap_fits i).1 (introTap_fits i).2
    · exact ih s.2 k

/-- The installed all-pass stencils stay within their buffers at every clock value. -/
theorem wanderingTaps_consecutive (s : FrozenState) :
    (prepared s).first.Consecutive ∧ (prepared s).second.Consecutive ∧
      ∀ j, ((prepared s).intro j).Consecutive :=
  ⟨apfTap_consecutive (by decide : 0 < squiggleBoyReverbFirstCapacity)
      (Runtime.feedbackTap 0) _ (by decide) (by decide),
    apfTap_consecutive (by decide : 0 < squiggleBoyReverbSecondCapacity)
      (Runtime.feedbackTap 1) _ (by decide) (by decide),
    diffusionTaps_consecutive _ s.diffusion⟩

/-- The single-pass gain envelope from the fixed controls at this frequency. -/
def driveBound (s : FrozenState) (q : ℂ) : ℝ :=
  ‖dampingResponse (prepared s).complex q‖ * ‖(prepared s).complex.satGain‖

/-- The feedback gain envelope from the fixed controls at this frequency. -/
def loopBound (s : FrozenState) (q : ℂ) : ℝ :=
  driveBound s q * ‖(prepared s).complex.feedback‖

theorem norm_wanderParts_le (s : FrozenState) (q : ℂ) (hq : ‖q‖ = 1)
    (hmain : (prepared s).main.Consecutive) (x : WanderSample) :
    ‖forward (wanderSpec s x) q‖ ≤ driveBound s q ∧
      ‖loopReturn (wanderSpec s x) q‖ ≤ loopBound s q ∧
      ‖directTransfer (wanderSpec s x) q‖ ≤ driveBound s q := by
  obtain ⟨hfirst, hsecond, hintro⟩ := wanderingTaps_consecutive (withWander s x)
  let p := wanderSpec s x
  have hc := wanderSpec_fixedControls s x q
  have hd := congrArg Prod.fst hc
  have hs := congrArg (fun a => a.2.1) hc
  have hm := congrArg (fun a => a.2.2.1) hc
  have hf := congrArg (fun a => a.2.2.2) hc
  dsimp only at hd hs hm hf
  have hMain : ∀ i, ‖tapResponse p.main q i‖ ≤ 1 := by
    intro i
    simpa only [p, hm, Spec.complex] using hmain.norm_le_one hq i
  have hBounds := transfer_bounds_of_passive_taps p q hq
    (norm_nonneg (dampingResponse p q)) norm_mixMatrix.le hMain
    (hfirst.norm_le_one hq) (hsecond.norm_le_one hq)
    (fun j => (hintro j).norm_le_one hq) (norm_le_pi_norm (dampingResponse p q))
  simpa only [p, loopBound, driveBound, hd, hs, hf] using hBounds

private theorem ae_wander_bounds (s : FrozenState) (q : ℂ) (hq : ‖q‖ = 1)
    (hmain : (prepared s).main.Consecutive) :
    ∀ᵐ x ∂wanderLaw, ‖loopReturn (wanderSpec s x) q‖ ≤ loopBound s q ∧
      ‖directTransfer (wanderSpec s x) q‖ ≤ driveBound s q :=
  Filter.Eventually.of_forall fun x => (norm_wanderParts_le s q hq hmain x).2

theorem loopBound_nonneg (s : FrozenState) (q : ℂ) : 0 ≤ loopBound s q := by
  unfold loopBound driveBound
  positivity

theorem expectedWanderTransfer_eq_tsum_of_consecutive (s : FrozenState) (q : ℂ)
    (hq : ‖q‖ = 1) (hstable : loopBound s q < 1) (hmain : (prepared s).main.Consecutive) :
    expectedWanderTransfer s q = ∑' n, ∫ x, returnTerm (wanderSpec s x) q n ∂wanderLaw := by
  have h := ae_wander_bounds s q hq hmain
  exact expectedWanderTransfer_eq_tsum s q (loopBound_nonneg s q) hstable
    (h.mono fun _ hx => hx.1) (h.mono fun _ hx => hx.2)

/-- A reverb-specific error bound using the fixed damping, saturation slope
and feedback at a frequency with a uniform contraction margin. -/
theorem expectedWanderTransfer_truncation_le_of_consecutive (s : FrozenState) (q : ℂ)
    (hq : ‖q‖ = 1) (hstable : loopBound s q < 1)
    (hmain : (prepared s).main.Consecutive) (N : ℕ) :
    ‖expectedWanderTransfer s q -
      ∑ n ∈ Finset.range N, ∫ x, returnTerm (wanderSpec s x) q n ∂wanderLaw‖ ≤
        loopBound s q ^ N * driveBound s q / (1 - loopBound s q) := by
  have h := ae_wander_bounds s q hq hmain
  exact expectedTransfer_truncation_le (μ := wanderLaw) (p := wanderSpec s) (q := q)
    (loopBound_nonneg s q) hstable (measurable_wanderLoopReturn s q)
    (measurable_wanderDirectTransfer s q) (h.mono fun _ hx => hx.1)
    (h.mono fun _ hx => hx.2) N

end

end DSP.SquiggleBoyQuadReverb.Frozen

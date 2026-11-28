import DSP.PolyXFader
import DSP.FMA.Algebra
import DSP.IfLess.Real
import Mathlib.Algebra.Order.Archimedean.Real.Basic
import Mathlib.Algebra.Order.Floor.Ring
import Mathlib.Analysis.Complex.Exponential
import Mathlib.Analysis.SpecialFunctions.Pow.Real
import Mathlib.Tactic.Linarith
import Mathlib.Tactic.NormNum
import Mathlib.Tactic.Ring

namespace DSP.PolyXFader

open scoped BigOperators

noncomputable section

def realFloor (x : ℝ) : ℝ := (⌊x⌋ : ℝ)

theorem fractional_bounds (phase : ℝ) :
    0 ≤ fractional realFloor phase ∧ fractional realFloor phase < 1 := by
  have lower := Int.floor_le phase
  have upper := Int.lt_floor_add_one phase
  unfold fractional realFloor
  constructor <;> linarith

theorem fractional_add_int (phase : ℝ) (cycles : ℤ) :
    fractional realFloor (phase + cycles) = fractional realFloor phase := by
  unfold fractional realFloor
  rw [Int.floor_add_intCast, Int.cast_add]
  ring

/-- The actual shaping definition reduces the phase before applying the
multiplier, so even the partial lobe repeats after any integer phase change. -/
theorem shapePrepared_periodic (p : PreparedWaveform ℝ ℝ) (curves : ℝ → ℝ × ℝ)
    (phase : ℝ) (cycles : ℤ) :
    shapePreparedWith realFloor realFloor id curves p (phase + cycles) =
      shapePreparedWith realFloor realFloor id curves p phase := by
  have h : phase + (cycles : ℝ) + p.phaseShift + 0.75 =
      phase + p.phaseShift + 0.75 + cycles := by ring
  simp only [shapePreparedWith, h, fractional_add_int]

theorem holdAndBlend_dry (trigger live latched held alpha amplitude : ℝ) :
    let next := holdAndBlend trigger live latched held alpha 0 amplitude
    next.raw = live ∧
      next.latched = IfLess.ifLess 0 trigger live latched ∧
      next.held = OnePoleZOHLowPass.onePoleStep alpha next.latched held := by
  simp [holdAndBlend]

private theorem linkProduct_le (links : List ℕ) (h : ∀ m ∈ links, m ≤ 5) :
    links.prod ≤ 5 ^ links.length := by
  induction links with
  | nil => simp
  | cons m links ih =>
    rw [List.prod_cons, List.length_cons, pow_succ']
    exact Nat.mul_le_mul (h m (by simp)) (ih (by
      intro i hi
      exact h i (by simp [hi])))

/-- Each topology link advances strictly through six loop nodes. The five
available links may each use the largest grid multiplier, five. -/
theorem gridTopologyRatio_le (links : List ℕ) (nodes : Fin (links.length + 1) → Fin 6)
    (ordered : StrictMono nodes) (h : ∀ m ∈ links, m ≤ 5) : links.prod ≤ 3125 := by
  have hcard := Fintype.card_le_of_injective nodes ordered.injective
  simp only [Fintype.card_fin] at hcard
  have hlen : links.length ≤ 5 := by omega
  exact (linkProduct_le links h).trans (Nat.pow_le_pow_right (by decide) hlen)

/-- The fastest grid topology has five multiplier-five links. Internal tempo
is host Nyquist divided by 8192 and the engine oversamples by four. -/
theorem nominalSlewLimit_eq :
    ((48000 : ℝ) / 2 / 8192) * 5 ^ (5 : ℕ) / (48000 * 4) = 3125 / 65536 := by
  norm_num

/-- The exact exponential control maps the normalized encoder range to 1–16. -/
theorem mappedMult_of_normalized (normalized : ℝ) (h : normalized ∈ Set.Icc 0 1) :
    MappedMult ((maxMappedMult : ℝ) ^ normalized) := by
  change 1 ≤ (16 : ℝ) ^ normalized ∧ (16 : ℝ) ^ normalized ≤ 16
  constructor
  · simpa using Real.rpow_le_rpow_of_exponent_le (show (1 : ℝ) ≤ 16 by norm_num) h.1
  · simpa using Real.rpow_le_rpow_of_exponent_le (show (1 : ℝ) ≤ 16 by norm_num) h.2

/-- Each multiplier cap comes from the nominal rate of the loop it limits. -/
theorem waveMultiplier_rate_le (requested limit rate cap : ℝ)
    (hrate : 0 ≤ rate) (hcap : limit * rate ≤ cap) :
    waveMultiplier requested limit * rate ≤ cap := by
  have hmin : waveMultiplier requested limit = min requested limit := by
    rw [waveMultiplier, minOf]
    change (if requested < limit then requested else limit) = _
    split
    · exact (min_eq_left (le_of_lt ‹_›)).symm
    · exact (min_eq_right (le_of_not_gt ‹_›)).symm
  rw [hmin]
  exact (mul_le_mul_of_nonneg_right (min_le_right _ _) hrate).trans hcap

theorem waveMultiplier_nominalRate_le (requested rate cap : ℝ) (hrate : 0 < rate) :
    waveMultiplier requested (cap / rate) * rate ≤ cap := by
  apply waveMultiplier_rate_le requested (cap / rate) rate cap hrate.le
  rw [div_mul_cancel₀ _ hrate.ne']

theorem minimumWaveMultLimit_le (budget ratio fastest : ℝ)
    (hb : 0 ≤ budget) (hr : 0 < ratio) (hf : ratio ≤ fastest) :
    budget / fastest ≤ budget / ratio :=
  div_le_div_of_nonneg_left hb hr hf

theorem waveMultiplier_eq_of_below_minimum (requested minimum loopLimit : ℝ)
    (hreq : requested ≤ minimum) (hmin : minimum ≤ loopLimit) :
    waveMultiplier requested loopLimit = requested := by
  rw [waveMultiplier, minOf]
  change (if requested < loopLimit then requested else loopLimit) = requested
  split
  · rfl
  · exact le_antisymm (le_of_not_gt ‹_›) (hreq.trans hmin)

theorem waveMultiplier_eq_of_mapped (requested loopLimit : ℝ)
    (hreq : MappedMult requested) (hlimit : (maxMappedMult : ℝ) ≤ loopLimit) :
    waveMultiplier requested loopLimit = requested :=
  waveMultiplier_eq_of_below_minimum requested (maxMappedMult : ℝ) loopLimit hreq.2 hlimit

theorem includeLoopSpeed_eq (weight speed fastest : ℝ) :
    includeLoopSpeed weight speed fastest = if 0 < weight then max fastest speed else fastest := by
  rw [includeLoopSpeed, maxOf]
  change (if 0 < weight then (if fastest < speed then speed else fastest) else fastest) = _
  split <;> rename_i h
  · split
    · exact (max_eq_right (le_of_lt ‹_›)).symm
    · exact (max_eq_left (le_of_not_gt ‹_›)).symm
  · rfl

theorem includeLoopSpeed_mem (weight speed fastest limit : ℝ)
    (hs : speed ∈ Set.Icc 0 limit) (hf : fastest ∈ Set.Icc 0 limit) :
    includeLoopSpeed weight speed fastest ∈ Set.Icc 0 limit := by
  rw [includeLoopSpeed_eq]
  split
  · exact ⟨hf.1.trans (le_max_left _ _), max_le hf.2 hs.2⟩
  · exact hf

/-- An uncapped multiplier is common to every loop on its lane, so it can be
applied after selecting the maximum ratio. -/
theorem includeLoopSpeed_uniformMult (weight ratio fastest mult : ℝ) (hm : 0 ≤ mult) :
    includeLoopSpeed weight (ratio * mult) (fastest * mult) =
      includeLoopSpeed weight ratio fastest * mult := by
  simp only [includeLoopSpeed_eq]
  split
  · by_cases h : ratio ≤ fastest
    · rw [max_eq_left h, max_eq_left (mul_le_mul_of_nonneg_right h hm)]
    · have h' := le_of_not_ge h
      rw [max_eq_right h', max_eq_right (mul_le_mul_of_nonneg_right h' hm)]
  · rfl

theorem slewCycles_eq_min (limit speed travel : ℝ) :
    slewCycles limit speed travel = min (8 * speed * travel) limit := by
  rw [slewCycles, minOf]
  change (if 8 * speed * travel < limit then 8 * speed * travel
    else limit) = _
  split
  · exact (min_eq_left (le_of_lt ‹_›)).symm
  · exact (min_eq_right (le_of_not_gt ‹_›)).symm

/-- The wrapper passes a nonnegative phase-travel magnitude, and geometric
loop speeds are nonnegative. -/
theorem slewOmega_mem (limit stepScale speed travel : ℝ)
    (hl : 0 ≤ limit) (hs : 0 ≤ stepScale) (hstep : limit * stepScale ≤ 3125 / 65536)
    (hr : 0 ≤ speed) (ht : 0 ≤ travel) :
    slewCycles limit speed travel * stepScale * 6.283185307179586 ∈
      Set.Icc 0 (3 / 10 : ℝ) := by
  rw [slewCycles_eq_min]
  constructor
  · exact mul_nonneg (mul_nonneg (le_min (mul_nonneg (mul_nonneg (by norm_num) hr) ht) hl) hs)
      (by norm_num)
  · exact (mul_le_mul_of_nonneg_right
      ((mul_le_mul_of_nonneg_right (min_le_right _ _) hs).trans hstep) (by norm_num)).trans
      (by norm_num)

theorem slewAlpha_mem (omega : ℝ) (h : omega ∈ Set.Icc 0 (3 / 10 : ℝ)) :
    slewAlpha omega ∈ Set.Icc 0 omega := by
  have h0 := h.1
  have h1 := h.2
  constructor
  · rw [show slewAlpha omega = omega * (1 - omega / 2) +
      omega ^ 3 * (1 / 6 - omega / 24) + omega ^ 5 * (1 / 120 - omega / 720) by
        simp only [slewAlpha, fma_eq]
        ring]
    exact add_nonneg (add_nonneg (mul_nonneg h0 (by linarith))
      (mul_nonneg (pow_nonneg h0 _) (by linarith)))
      (mul_nonneg (pow_nonneg h0 _) (by linarith))
  · apply sub_nonneg.mp
    rw [show omega - slewAlpha omega = omega ^ 2 * (1 / 2 - omega / 6) +
      omega ^ 4 * (1 / 24 - omega / 120) + omega ^ 6 / 720 by
        simp only [slewAlpha, fma_eq]
        ring]
    exact add_nonneg (add_nonneg (mul_nonneg (pow_nonneg h0 _) (by linarith))
      (mul_nonneg (pow_nonneg h0 _) (by linarith)))
      (div_nonneg (pow_nonneg h0 _) (by norm_num))

/-- Absolute approximation error of the executable polynomial in real
arithmetic. Native floating-point rounding is tested separately. -/
theorem slewAlpha_error (omega : ℝ) (h : omega ∈ Set.Icc 0 (3 / 10 : ℝ)) :
    |slewAlpha omega - (1 - Real.exp (-omega))| ≤ 1 / 20000000 := by
  have h0 := h.1
  have h1 := h.2
  have habs : |(-omega : ℝ)| ≤ 1 := by rw [abs_neg, abs_of_nonneg h0]; linarith
  have herr := Real.exp_bound habs (n := 7) (by norm_num)
  rw [abs_neg, abs_of_nonneg h0] at herr
  have poly : (∑ m ∈ Finset.range 7, (-omega) ^ m / (m.factorial : ℝ)) =
      1 - slewAlpha omega := by
    norm_num [Finset.sum_range_succ, Nat.factorial_succ, slewAlpha, fma_eq]
    ring
  rw [poly] at herr
  have hp := pow_le_pow_left₀ h0 h1 7
  calc
    |slewAlpha omega - (1 - Real.exp (-omega))| =
        |Real.exp (-omega) - (1 - slewAlpha omega)| := by congr 1; ring
    _ ≤ omega ^ 7 * ((8 : ℝ) / (5040 * 7)) := by norm_num at herr ⊢; exact herr
    _ ≤ (3 / 10 : ℝ) ^ 7 * (8 / (5040 * 7)) :=
      mul_le_mul_of_nonneg_right hp (by norm_num)
    _ ≤ 1 / 20000000 := by norm_num

theorem doubleSlewAlpha_exact (omega : ℝ) :
    doubleSlewAlpha (1 - Real.exp (-omega)) = 1 - Real.exp (-(2 * omega)) := by
  rw [doubleSlewAlpha, show -(2 * omega) = -omega + -omega by ring, Real.exp_add]
  ring

theorem doubleSlewAlpha_mem (alpha : ℝ) (h : alpha ∈ Set.Icc 0 (1 : ℝ)) :
    doubleSlewAlpha alpha ∈ Set.Icc 0 (1 : ℝ) := by
  have h0 := h.1
  have h1 := h.2
  constructor
  · exact mul_nonneg h0 (by linarith)
  · unfold doubleSlewAlpha
    nlinarith [sq_nonneg (1 - alpha)]

theorem doubleSlewAlpha_error (a b : ℝ) (ha : a ∈ Set.Icc 0 (1 : ℝ))
    (hb : b ∈ Set.Icc 0 (1 : ℝ)) :
    |doubleSlewAlpha a - doubleSlewAlpha b| ≤ 2 * |a - b| := by
  have h0 : 0 ≤ 2 - a - b := by linarith [ha.2, hb.2]
  have h1 : 2 - a - b ≤ 2 := by linarith [ha.1, hb.1]
  rw [show doubleSlewAlpha a - doubleSlewAlpha b = (2 - a - b) * (a - b) by
    unfold doubleSlewAlpha
    ring, abs_mul, abs_of_nonneg h0]
  exact mul_le_mul_of_nonneg_right h1 (abs_nonneg _)

end
end DSP.PolyXFader

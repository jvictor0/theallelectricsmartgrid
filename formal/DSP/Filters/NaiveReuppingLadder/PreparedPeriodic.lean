import DSP.Filters.NaiveReuppingLadder.PeriodicWaveUniqueness
import DSP.Transcendental.Real

noncomputable section
namespace DSP.NaiveReuppingLadder.PeriodicWave
open DetectorDynamics BandPassedEnvelopeFollower.Periodic
open scoped NNReal

private theorem real_min (a b : ℝ) : minValue a b = min a b := by
  change (if b < a then b else a) = min a b
  split <;> rename_i h
  · exact (min_eq_right h.le).symm
  · exact (min_eq_left (le_of_not_gt h)).symm

private theorem real_max (a b : ℝ) : maxValue a b = max a b := by
  change (if a < b then b else a) = max a b
  split <;> rename_i h
  · exact (max_eq_right h.le).symm
  · exact (max_eq_left (le_of_not_gt h)).symm

/-- For every positive cutoff and drive the actual coefficient setter has a
nonempty resonance-knob interval with a unique input-periodic history. The
same interval works for EVERY finite input period and word, including all sine
amplitudes. The interval is expressed in the original normalized knob. -/
theorem prepared_resonance_range (cutoff drive : ℝ) (hc : 0 < cutoff) (hg : 0 < drive)
    (template : NaiveReuppingLadder ℝ) :
    ∃ g a rMax : ℝ, 0 < g ∧ 0 < a ∧ a ≤ 1 ∧ 0 < rMax ∧
      ∀ (r : ℝ) (_ : 0 ≤ r) (_ : r < rMax) (N : ℕ) (_ : 0 < N)
        (u : ℕ → ℝ) (_ : Function.Periodic u N),
        let p := computeCoefficients ⟨cutoff,r,drive⟩ template
        Tuned p.inputEnvelope g a ∧ Tuned p.feedbackEnvelope g a ∧
        (∃ s : Setup, s.k = knobs p ∧ s.g = g ∧ s.a = a ∧ s.u = u ∧ s.N = N ∧ s.L 0 < 1) ∧
        ∃! z : Joint, prefixRun (knobs p) g a u z N = z := by
  let c : ℝ := minValue 0.5 cutoff
  let t := Trig.sin2pi (c*0.5)
  let root := SquareRoot.sqrt (1+t*t)
  let alpha := t*2*(root-t)
  let ratio := t*2*(root+t)
  let v := minValue (ratio*0.5) 1
  let cap : ℝ := 318.30883898555044
  let g := v / SquareRoot.sqrt (maxValue ((1-v)*(1+v)) (v*v*(1/(cap*cap))))
  let a := t*(0.43497944204608224 / 6.283185307179586476925286766559)
  have hc₀ : 0 < c := by dsimp [c]; rw [real_min]; exact lt_min (by norm_num) hc
  have hc₁ : c ≤ 0.5 := by dsimp [c]; rw [real_min]; exact min_le_left _ _
  have ht : 0 < t := by
    dsimp only [t,sin2pi_real]
    apply Real.sin_pos_of_pos_of_lt_pi <;> nlinarith [Real.pi_pos,mul_pos Real.pi_pos hc₀,
      mul_nonneg Real.pi_pos.le (sub_nonneg.mpr hc₁)]
  have ht₁ : t ≤ 1 := Real.sin_le_one _
  have hroot : root^2 = 1+t*t := Real.sq_sqrt (by nlinarith [sq_nonneg t])
  have hroot₀ : 0 ≤ root := Real.sqrt_nonneg _
  have hgt : t < root := by
    by_contra! h
    nlinarith [mul_nonneg (sub_nonneg.mpr h) (add_nonneg ht.le hroot₀)]
  have hα : 0 < alpha := mul_pos (mul_pos ht (by norm_num)) (sub_pos.mpr hgt)
  have hα₁ : alpha ≤ 1 := by dsimp [alpha]; nlinarith [sq_nonneg (root-t)]
  have hv : 0 < v := by
    have hratio : 0 < ratio := mul_pos (mul_pos ht (by norm_num)) (by linarith)
    dsimp only [v]; rw [real_min]; exact lt_min (mul_pos hratio (by norm_num)) zero_lt_one
  have hG : 0 < g := by
    apply div_pos hv (Real.sqrt_pos.mpr ?_)
    rw [real_max]
    exact lt_of_lt_of_le (mul_pos (mul_pos hv hv) (by norm_num [cap])) (le_max_right _ _)
  have ha : 0 < a := mul_pos ht (by norm_num)
  have ha₁ : a ≤ 1 := by
    dsimp only [a]
    exact (mul_le_mul_of_nonneg_right ht₁ (by norm_num)).trans (by norm_num)
  let den := Real.tanh drive
  have hsat : 0 < den := by
    dsimp [den]; rw [Real.tanh_eq_sinh_div_cosh]
    exact div_pos (Real.sinh_pos_iff.mpr hg) (Real.cosh_pos _)
  let slope := drive/den
  let D := slope*slope*slope*(dampingEpsilon : ℝ)
  have hD : 0 ≤ D := by
    have hs := div_pos hg hsat
    exact mul_nonneg (mul_nonneg (mul_nonneg hs.le hs.le) hs.le) (by norm_num [dampingEpsilon])
  let B := (|drive|/den)^3 * (1+2*D*tapGain g^4*(1/den)^2/(0+powerFloor))
  have hB : 0 < B := by
    have hS := div_pos (abs_pos.mpr hg.ne') hsat
    have hF := floor_pos
    dsimp only [B]
    positivity
  refine ⟨g,a,(1/B)/3.9,hG,ha,ha₁,div_pos (one_div_pos.mpr hB) (by norm_num),?_⟩
  intro r hr hlimit N hN u hu
  let p := computeCoefficients ⟨cutoff,r,drive⟩ template
  have hK : 0 ≤ p.baseFeedback := by
    change 0 ≤ minValue (r*3.9) (0.9 / (_*_+0.00000001))
    rw [real_min]
    exact le_min (mul_nonneg hr (by norm_num)) (div_nonneg (by norm_num)
      (add_nonneg (mul_self_nonneg _) (by norm_num)))
  let s : Setup := ⟨knobs p,g,a,u,N,hN,hu,hG,ha,ha₁,hα,hα₁,hK,hD,hsat⟩
  have hres : s.k.K ≤ r*3.9 := by
    change minValue (r*3.9) _ ≤ _
    rw [real_min]; exact min_le_left _ _
  have hsmall := (s.small_gain_iff_feedback_lt 0 le_rfl hg.ne').mpr
    (hres.trans_lt ((lt_div_iff₀ (by norm_num : (0:ℝ) < 3.9)).mp hlimit))
  exact ⟨tuned_prepared template.inputEnvelope g a hG.le,tuned_prepared template.feedbackEnvelope g a hG.le,
    ⟨s,rfl,rfl,rfl,rfl,rfl,hsmall⟩, s.unique_periodic_history 0 le_rfl s.dry_nonneg hsmall⟩

end DSP.NaiveReuppingLadder.PeriodicWave

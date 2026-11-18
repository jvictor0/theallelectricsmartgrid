import DSP.ResponseMeasurement

/-! Uniform waveform estimates pass through long-time measurements. -/

noncomputable section
namespace DSP.ResponseMeasurement
open Filter Finset
open scoped Topology

theorem average_norm_le (signal : ℕ → ℂ) (bound : ℝ) (hb : 0 ≤ bound)
    (hs : ∀ n, ‖signal n‖ ≤ bound) (count : ℕ) : ‖average signal count‖ ≤ bound := by
  by_cases hn : count = 0
  · simpa [average,hn] using hb
  have hc : 0 < (count : ℝ) := by exact_mod_cast Nat.pos_of_ne_zero hn
  have hsum : ‖∑ n ∈ range count, signal n‖ ≤ (count : ℝ)*bound := by
    apply (norm_sum_le _ _).trans
    simpa using sum_le_sum (fun n (_ : n ∈ range count) => hs n)
  calc
    _ = (count : ℝ)⁻¹ * ‖∑ n ∈ range count, signal n‖ := by
      rw [average,norm_smul,Real.norm_of_nonneg (inv_nonneg.mpr hc.le)]
    _ ≤ (count : ℝ)⁻¹*((count : ℝ)*bound) := mul_le_mul_of_nonneg_left hsum (inv_nonneg.mpr hc.le)
    _ = bound := by field_simp

theorem coherent_norm_le (z : ℂ) (hz : ‖z‖ = 1) (signal : ℕ → ℝ)
    (bound : ℝ) (hb : 0 ≤ bound) (hs : ∀ n, |signal n| ≤ bound) (count : ℕ) :
    ‖coherent z signal count‖ ≤ 2*bound := by
  have h := average_norm_le (fun n => (signal n : ℂ)*star z^n) bound hb
    (fun n => by simpa [norm_mul,norm_pow,hz] using hs n) count
  simpa only [coherent,norm_mul,Complex.norm_ofNat] using mul_le_mul_of_nonneg_left h (by norm_num : (0:ℝ) ≤ 2)

/-- Uniform limits of sequences with convergent means also have convergent
means. The approximating sequence can depend on the requested accuracy. -/
theorem average_exists_of_approximation (signal : ℕ → ℂ)
    (h : ∀ eta > 0, ∃ reference : ℕ → ℂ,
      (∃ a, Tendsto (average reference) atTop (𝓝 a)) ∧ ∀ n, ‖signal n-reference n‖ ≤ eta) :
    ∃ a, Tendsto (average signal) atTop (𝓝 a) := by
  apply cauchySeq_tendsto_of_complete
  rw [Metric.cauchySeq_iff]
  intro epsilon he
  obtain ⟨reference,⟨a,ha⟩,hb⟩ := h (epsilon/3) (by positivity)
  obtain ⟨N,hN⟩ := Metric.cauchySeq_iff.mp ha.cauchySeq (epsilon/3) (by positivity)
  refine ⟨N,?_⟩
  intro m hm n hn
  have hmn := hN m hm n hn
  have hb' (i : ℕ) : dist (average signal i) (average reference i) ≤ epsilon/3 := by
    rw [dist_eq_norm,← average_sub]
    exact average_norm_le _ _ (by positivity) hb i
  have ht := dist_triangle (average signal m) (average reference m) (average signal n)
  have ht' := dist_triangle (average reference m) (average reference n) (average signal n)
  have hm' := hb' m
  have hn' := hb' n
  rw [dist_comm (average signal n)] at hn'
  linarith

theorem smallSignal_of_uniform_linearization (z : ℂ) (hz : ‖z‖ = 1)
    (output : ℝ → ℕ → ℝ) (linear : ℕ → ℝ) (H : ℂ)
    (hlinear : HasCoherentResponse z linear H)
    (hsettled : ∀ᶠ epsilon in 𝓝[>] 0, ∃ a, HasCoherentResponse z (output epsilon) a)
    (hsmall : ∀ eta > 0, ∃ delta > 0, ∀ epsilon, 0 < epsilon → epsilon < delta →
      ∀ n, |output epsilon n-epsilon*linear n| ≤ eta*epsilon) :
    HasSmallSignalResponse z output H := by
  classical
  let settled := fun e => if h : ∃ a, HasCoherentResponse z (output e) a then h.choose else 0
  have hs : ∀ᶠ e in 𝓝[>] 0, HasCoherentResponse z (output e) (settled e) := by
    filter_upwards [hsettled] with e he
    simpa only [settled,dif_pos he] using he.choose_spec
  refine ⟨settled,hs,?_⟩
  rw [Metric.tendsto_nhds]
  intro eta heta
  obtain ⟨delta,hd,hbound⟩ := hsmall (eta/4) (by positivity)
  filter_upwards [hs,self_mem_nhdsWithin,
    (show ∀ᶠ e in 𝓝[>] (0:ℝ), e < delta from
      nhdsWithin_le_nhds (gt_mem_nhds hd))] with e he hepos hesmall
  change 0 < e at hepos
  have hscale : Tendsto (coherent z (fun n => e*linear n)) atTop (𝓝 ((e:ℂ)*H)) := by
    change Tendsto (fun count => coherent z (fun n => e*linear n) count) _ _
    simpa only [coherent_scale] using hlinear.const_mul (e:ℂ)
  have hdiff : Tendsto (coherent z (fun n => output e n-e*linear n)) atTop (𝓝 (settled e-(e:ℂ)*H)) := by
    change Tendsto (fun count => coherent z (fun n => output e n-e*linear n) count) _ _
    simpa only [coherent_sub] using he.sub hscale
  have hlim : ‖settled e-(e:ℂ)*H‖ ≤ 2*(eta/4*e) := by
    apply le_of_tendsto hdiff.norm
    exact Filter.Eventually.of_forall (coherent_norm_le z hz _ _ (by positivity) (hbound e hepos hesmall))
  have he0 : (e:ℂ) ≠ 0 := by exact_mod_cast ne_of_gt hepos
  rw [dist_eq_norm]
  have heq : settled e/(e:ℂ)-H = (settled e-(e:ℂ)*H)/(e:ℂ) := by field_simp
  rw [heq,norm_div,Complex.norm_real,Real.norm_eq_abs,abs_of_pos hepos]
  apply (div_le_div_of_nonneg_right hlim hepos.le).trans_lt
  rw [div_lt_iff₀ hepos]
  nlinarith [mul_pos heta hepos]

end DSP.ResponseMeasurement

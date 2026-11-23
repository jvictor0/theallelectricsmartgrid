import DSP.ResponseMeasurement.Approximation
import DSP.ResponseMeasurement.Modulation

/-! Explicit response curves of finite Fourier approximations. Digital
frequency coincidences are kept exactly, including conjugate aliases. The
carriers can represent input harmonics, autonomous tones, or mixed modes.
No uniform-phase/time-average identification is assumed. -/

noncomputable section
namespace DSP.ResponseMeasurement.FiniteSpectrum
open Filter Finset
open scoped Topology
variable {ι : Type*} [Fintype ι]

def signal (peak carrier : ι → ℂ) (n : ℕ) : ℝ :=
  ∑ i, (peak i*(carrier i)^n).re

def resonance (z : ℂ) : ℂ := if z = 1 then 1 else 0

def coherentCurve (peak carrier : ι → ℂ) (target : ℂ) : ℂ :=
  ∑ i, (peak i*resonance (carrier i*star target)+star (peak i)*resonance (star (carrier i)*star target))

def powerCurve (peak carrier : ι → ℂ) : ℝ :=
  (∑ i, ∑ j, (peak i*peak j*resonance (carrier i*carrier j)+
    peak i*star (peak j)*resonance (carrier i*star (carrier j))).re)/2

def prediction (peak carrier : ι → ℂ) (target : ℂ) : Prediction :=
  ⟨coherentCurve peak carrier target,powerCurve peak carrier⟩

def amplitudeBound (peak : ι → ℂ) : ℝ := ∑ i, ‖peak i‖

/-- Away from frequency coincidences the explicit power curve is just the
sum of the individual tone powers. The hypotheses exclude DC, Nyquist and
conjugate duplicates as well as equal carriers. -/
theorem powerCurve_separated (peak carrier : ι → ℂ)
    (hc : ∀ i, ‖carrier i‖ = 1) (hi : Function.Injective carrier)
    (hpair : ∀ i j, carrier i*carrier j ≠ 1) :
    powerCurve peak carrier = (∑ i, ‖peak i‖^2)/2 := by
  classical
  have he (i j : ι) : carrier i*star (carrier j) = 1 ↔ i = j := by
    constructor
    · intro h
      apply hi
      have h' := congrArg (fun z => z*carrier j) h
      have hu : star (carrier j)*carrier j = 1 := by
        rw [mul_comm]; exact unit_mul_star _ (hc j)
      simpa only [mul_assoc,hu,mul_one,one_mul] using h'
    · intro h; subst j; exact unit_mul_star _ (hc i)
  unfold powerCurve
  congr 1
  apply Finset.sum_congr rfl
  intro i _
  simp only [resonance,hpair,if_false,mul_zero,zero_add,he]
  simp only [mul_one,mul_zero,apply_ite,Complex.zero_re]
  rw [Finset.sum_ite_eq]
  simp only [Finset.mem_univ,if_true,Complex.star_def,Complex.mul_conj,
    Complex.ofReal_re,Complex.normSq_eq_norm_sq]

/-- A retained tone is exactly the coherent curve when no other retained
frequency or conjugate aliases onto it. -/
theorem coherentCurve_separated (peak carrier : ι → ℂ) (target : ℂ) (selected : ι)
    (hf : ∀ i, carrier i*star target = 1 ↔ i = selected)
    (hb : ∀ i, star (carrier i)*star target ≠ 1) :
    coherentCurve peak carrier target = peak selected := by
  classical
  simp only [coherentCurve,resonance,hf,hb,if_false,mul_zero,add_zero,mul_ite,mul_one]
  simp

theorem signal_bound (peak carrier : ι → ℂ) (hc : ∀ i, ‖carrier i‖ = 1) (n : ℕ) :
    |signal peak carrier n| ≤ amplitudeBound peak := by
  apply (abs_sum_le_sum_abs _ _).trans
  apply sum_le_sum
  intro i _
  simpa only [norm_mul,norm_pow,hc,one_pow,mul_one] using Complex.abs_re_le_norm (peak i*carrier i^n)

theorem mode_average (z : ℂ) (hz : ‖z‖ = 1) :
    Tendsto (average (fun n => z^n)) atTop (𝓝 (resonance z)) := by
  by_cases h : z = 1
  · subst z
    apply tendsto_const_nhds.congr'
    filter_upwards [eventually_gt_atTop 0] with N hN
    have hn : (N:ℂ) ≠ 0 := by exact_mod_cast (Nat.ne_of_gt hN)
    simp [average,resonance,Complex.real_smul,hn]
  · simpa [resonance,h] using average_mode_tendsto_zero z hz h

theorem coherent_sum (target : ℂ) (f : ι → ℕ → ℝ) (N : ℕ) :
    coherent target (fun n => ∑ i, f i n) N = ∑ i, coherent target (f i) N := by
  simp only [coherent,average,Complex.ofReal_sum,Finset.sum_mul]
  rw [Finset.sum_comm]
  simp only [Finset.smul_sum,Finset.mul_sum]

theorem coherent_limit (peak carrier : ι → ℂ) (hc : ∀ i, ‖carrier i‖ = 1)
    (target : ℂ) (ht : ‖target‖ = 1) :
    HasCoherentResponse target (signal peak carrier) (coherentCurve peak carrier target) := by
  have h := tendsto_finsetSum univ (fun i _ =>
    ((mode_average (carrier i*star target) (by simp [hc i,ht])).const_mul (peak i)).add
      ((mode_average (star (carrier i)*star target) (by simp [hc i,ht])).const_mul (star (peak i))))
  change Tendsto (fun N => coherent target (fun n => ∑ i, (peak i*carrier i^n).re) N) _ _
  simp_rw [coherent_sum,coherent_mode]
  exact h

theorem average_re (f : ℕ → ℂ) (N : ℕ) :
    DSP.Coboundary.average (fun n => (f n).re) N = (average f N).re := by
  simp [DSP.Coboundary.average,average,Complex.real_smul]

theorem average_sum_real (f : ι → ℕ → ℝ) (N : ℕ) :
    DSP.Coboundary.average (fun n => ∑ i, f i n) N =
      ∑ i, DSP.Coboundary.average (f i) N := by
  simp only [DSP.Coboundary.average]
  rw [Finset.sum_comm,Finset.smul_sum]

theorem product_signal (peak carrier : ι → ℂ) (n : ℕ) :
    signal peak carrier n^2 = ∑ i, ∑ j,
      ((peak i*peak j*(carrier i*carrier j)^n).re+
        (peak i*star (peak j)*(carrier i*star (carrier j))^n).re)/2 := by
  rw [signal,pow_two,Finset.sum_mul]
  apply sum_congr rfl
  intro i _
  rw [Finset.mul_sum]
  apply sum_congr rfl
  intro j _
  rw [re_product]
  congr 2 <;> congr 1 <;> simp only [mul_pow,star_mul,star_pow] <;> ring

theorem pair_limit (a b z w : ℂ) (hz : ‖z‖ = 1) (hw : ‖w‖ = 1) :
    Tendsto (fun N => DSP.Coboundary.average
      (fun n => ((a*b*(z*w)^n).re+(a*star b*(z*star w)^n).re)/2) N)
      atTop (𝓝 ((a*b*resonance (z*w)+a*star b*resonance (z*star w)).re/2)) := by
  have hc := ((mode_average (z*w) (by simp [hz,hw])).const_mul (a*b)).add
    ((mode_average (z*star w) (by simp [hz,hw])).const_mul (a*star b))
  have h := (Complex.continuous_re.continuousAt.tendsto.comp hc).div_const 2
  convert h using 1
  funext N
  change DSP.Coboundary.average
    (fun n => ((a*b*(z*w)^n).re+(a*star b*(z*star w)^n).re)/2) N =
    (a*b*average (fun n => (z*w)^n) N+a*star b*average (fun n => (z*star w)^n) N).re/2
  have hadd (f g : ℕ → ℂ) : average (fun n => f n+g n) N = average f N+average g N := by
    simp only [average,Finset.sum_add_distrib,smul_add]
  have hmul (c : ℂ) (f : ℕ → ℂ) : average (fun n => c*f n) N = c*average f N := by
    simp only [average,← Finset.mul_sum,Complex.real_smul]
    ring
  rw [← hmul,← hmul,← hadd,← average_re]
  simp only [DSP.Coboundary.average,Complex.add_re,smul_eq_mul]
  rw [← Finset.sum_div]
  ring

theorem power_limit (peak carrier : ι → ℂ) (hc : ∀ i, ‖carrier i‖ = 1) :
    HasMeanSquare (signal peak carrier) (powerCurve peak carrier) := by
  have h := tendsto_finsetSum univ (fun i _ => tendsto_finsetSum univ
    (fun j _ => pair_limit (peak i) (peak j) (carrier i) (carrier j) (hc i) (hc j)))
  have he (N : ℕ) : meanSquare (signal peak carrier) N =
      ∑ i, ∑ j, DSP.Coboundary.average (fun n =>
        ((peak i*peak j*(carrier i*carrier j)^n).re+
          (peak i*star (peak j)*(carrier i*star (carrier j))^n).re)/2) N := by
    change DSP.Coboundary.average (fun n => signal peak carrier n^2) N = _
    simp_rw [product_signal,average_sum_real]
  change Tendsto (fun N => meanSquare (signal peak carrier) N) _ _
  simp_rw [he]
  simpa only [powerCurve,← Finset.sum_div] using h

/-- A finite spectrum gives an explicit curve and a uniform approximation
budget for any nearby full waveform, without requiring that waveform's
measurements themselves have a limit. -/
theorem approximates (actual : ℕ → ℝ) (peak carrier : ι → ℂ)
    (hc : ∀ i, ‖carrier i‖ = 1) (target : ℂ) (ht : ‖target‖ = 1) (error : ℝ)
    (he : ∀ n, |actual n-signal peak carrier n| ≤ error) :
    Approximates actual target (prediction peak carrier target)
      (2*error) (error*(2*amplitudeBound peak+error)) :=
  approximates_of_uniform_waveform_error actual (signal peak carrier) target ht
    (prediction peak carrier target) error (amplitudeBound peak) he (signal_bound peak carrier hc)
    (coherent_limit peak carrier hc target ht) (power_limit peak carrier hc)

end DSP.ResponseMeasurement.FiniteSpectrum

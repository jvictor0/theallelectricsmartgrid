import DSP.Filters.Responses.Realizes
import DSP.Analysis.LinearTracking
import DSP.Stability.Bilinear

/-! Time-domain stability of the shared SVF recurrence. Coordinates project
the actual integrator histories; no second processing algorithm is assumed. -/
noncomputable section
namespace DSP.TPTSVF
open Filter
open scoped Topology

abbrev History (K : Type) := Fin 2 → K

def histories {K : Type} (s : TPTSVF K) : History K := ![s.ic1,s.ic2]

def withHistory {K : Type} (s : TPTSVF K) (x : History K) : TPTSVF K :=
  {s with ic1 := x 0, ic2 := x 1}

def historyUpdate {K : Type} [Field K] (s : TPTSVF K) : History K × K →ₗ[K] History K where
  toFun v := histories (step (withHistory s v.1) ⟨v.2⟩)
  map_add' v w := by ext i; fin_cases i <;> simp [histories,step,withHistory,fma_eq] <;> ring
  map_smul' c v := by ext i; fin_cases i <;> simp [histories,step,withHistory,fma_eq] <;> ring

def historyMatrix {K : Type} [Field K] (s : TPTSVF K) : History K →ₗ[K] History K :=
  (historyUpdate s).comp (LinearMap.inl K (History K) K)

def digitalDenominator {K : Type} [Field K] (g k z : K) : K :=
  (z-1)^2+k*g*(z-1)*(z+1)+g^2*(z+1)^2

theorem Tuned.a1_eq {K : Type} [Field K] {s : TPTSVF K} {g : K} (ht : s.Tuned g) :
    s.a1 = 1/(1+g*(g+s.k)) := by
  have hd : 1+g*(g+s.k) ≠ 0 := by intro h; have hs := ht.solve; rw [h,mul_zero] at hs; exact zero_ne_one hs
  exact (eq_div_iff hd).mpr ht.solve

theorem characteristic_of_mode {K : Type} [Field K] (s : TPTSVF K) (g z : K)
    (ht : s.Tuned g) (x : History K) (hx : x ≠ 0)
    (hm : historyMatrix s x = z • x) : digitalDenominator g s.k z = 0 := by
  let d := 1+g*(g+s.k)
  have hd : d ≠ 0 := by
    intro h
    have hs := ht.solve
    change s.a1*d=1 at hs
    rw [h,mul_zero] at hs
    exact zero_ne_one hs
  have h0 := congrFun hm 0
  have h1 := congrFun hm 1
  change (FMA.fma (FMA.fma s.a2 (0-x 1) (s.a1*x 0)) 2 (-(x 0)))=z*x 0 at h0
  change (FMA.fma (FMA.fma s.a3 (0-x 1) (FMA.fma s.a2 (x 0) (x 1))) 2 (-(x 1)))=z*x 1 at h1
  simp only [fma_eq] at h0 h1
  simp only [ht.a3,ht.a2,ht.a1_eq] at h0 h1
  change (g*(1/d)*(0-x 1)+(1/d)*x 0)*2+ -x 0=z*x 0 at h0
  change (g*(g*(1/d))*(0-x 1)+(g*(1/d)*x 0+x 1))*2+ -x 1=z*x 1 at h1
  field_simp [hd] at h0 h1
  have he0 : (2-d-d*z)*x 0-2*g*x 1=0 := by linear_combination h0
  have he1 : 2*g*x 0+(d-2*g^2-d*z)*x 1=0 := by linear_combination h1
  have hp0 : d*digitalDenominator g s.k z*x 0=0 := by
    dsimp [digitalDenominator,d] at *
    linear_combination (1+g*(g+s.k)-2*g^2-(1+g*(g+s.k))*z)*he0+2*g*he1
  have hp1 : d*digitalDenominator g s.k z*x 1=0 := by
    dsimp [digitalDenominator,d] at *
    linear_combination -2*g*he0+(2-(1+g*(g+s.k))-(1+g*(g+s.k))*z)*he1
  by_contra hp
  apply hx
  ext i
  fin_cases i
  · exact (mul_eq_zero.mp hp0).resolve_left (mul_ne_zero hd hp)
  · exact (mul_eq_zero.mp hp1).resolve_left (mul_ne_zero hd hp)

theorem digital_roots_lt_one (g k : ℝ) (hg : 0 < g) (hk : 0 < k) (z : ℂ)
    (hz : digitalDenominator (g:ℂ) (k:ℂ) z = 0) : ‖z‖ < 1 := by
  have hz1 : z+1 ≠ 0 := by
    intro h
    have he : z = -1 := by linear_combination h
    norm_num [digitalDenominator,he] at hz
  have hp : ((z-1)/(z+1))^2+(k*g:ℝ)*((z-1)/(z+1))+(g*g:ℝ)=0 := by
    field_simp
    push_cast
    dsimp [digitalDenominator] at hz
    linear_combination hz
  have h := (Stability.svf g k hg hk).bilinear ((z-1)/(z+1)) (by simpa [pow_two] using hp)
  have he : (1+(z-1)/(z+1))/(1-(z-1)/(z+1))=z := by field_simp; ring
  rwa [he] at h

def complexCoefficients (s : TPTSVF ℝ) : TPTSVF ℂ :=
  ⟨s.k,s.a1,s.a2,s.a3,0,0,0,0,0,0,0⟩

def embedHistory : History ℝ →ₗ[ℝ] History ℂ where
  toFun x := fun i => (x i : ℂ)
  map_add' _ _ := by ext; simp
  map_smul' _ _ := by ext; simp

theorem embedHistory_norm (x : History ℝ) : ‖embedHistory x‖ = ‖x‖ := by
  simp [embedHistory,Pi.norm_def]

theorem embedHistory_commutes (s : TPTSVF ℝ) (x : History ℝ) :
    embedHistory (historyMatrix s x) = historyMatrix (complexCoefficients s) (embedHistory x) := by
  ext i
  fin_cases i <;> simp [embedHistory,historyMatrix,historyUpdate,histories,step,withHistory,fma_eq,complexCoefficients]

set_option maxHeartbeats 800000 in
theorem spectrum_lt_one (s : TPTSVF ℝ) (g : ℝ) (ht : s.Tuned g) (hg : 0 < g) (hk : 0 < s.k) :
    ∀ z ∈ spectrum ℂ (historyMatrix (complexCoefficients s)).toContinuousLinearMap, ‖z‖ < 1 := by
  intro z hz
  let A := historyMatrix (complexCoefficients s)
  have he : Module.End.HasEigenvalue A z := by
    apply Module.End.HasEigenvalue.of_mem_spectrum
    change z ∈ spectrum ℂ ((Module.End.toContinuousLinearMap (𝕜 := ℂ) (History ℂ)) A) at hz
    rwa [AlgEquiv.spectrum_eq] at hz
  obtain ⟨x,hx,hne⟩ := he.exists_hasEigenvector
  have ht' : (complexCoefficients s).Tuned (g:ℂ) := by
    constructor
    · change (s.a1:ℂ)*(1+(g:ℂ)*(g+s.k))=1
      exact_mod_cast ht.solve
    · change (s.a2:ℂ)=(g:ℂ)*s.a1
      exact_mod_cast ht.a2
    · change (s.a3:ℂ)=(g:ℂ)*s.a2
      exact_mod_cast ht.a3
  have hm : historyMatrix (complexCoefficients s) x = z • x := Module.End.mem_eigenspace_iff.mp hx
  have hp := characteristic_of_mode (complexCoefficients s) (g:ℂ) z ht' x hne hm
  apply digital_roots_lt_one g s.k hg hk z
  exact hp

def historyGauge (s : TPTSVF ℝ) (g : ℝ) (ht : s.Tuned g) (hg : 0 < g) (hk : 0 < s.k) :
    StabilityGauge (historyMatrix s) :=
  (StabilityGauge.ofSpectrum (historyMatrix (complexCoefficients s)).toContinuousLinearMap
    (spectrum_lt_one s g ht hg hk)).comap embedHistory embedHistory_norm (embedHistory_commutes s)

end DSP.TPTSVF

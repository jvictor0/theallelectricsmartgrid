import DSP.Filters.ConservativeReuppingLadder.Contracts
import DSP.Filters.ConservativeReuppingLadder.Linearization
import DSP.Analysis.StabilityGauge
import Mathlib.Analysis.Calculus.ContDiff.RCLike

/-! Four-dimensional history coordinates for analysis of the actual processor.
This is a projection of `step`, not a second executable implementation. -/

noncomputable section
namespace DSP.ConservativeReuppingLadder

abbrev History (K : Type) := Fin 4 → K

def histories {K : Type} (s : ConservativeReuppingLadder K) : History K :=
  ![s.stage1,s.stage2,s.stage3,s.stage4]

def withHistory {K : Type} [Zero K] (s : ConservativeReuppingLadder K) (x : History K) :
    ConservativeReuppingLadder K :=
  {s with stage1 := x 0, stage2 := x 1, stage3 := x 2, stage4 := x 3, input := 0, output := 0}

def linearUpdate {K : Type} [Field K] (s : ConservativeReuppingLadder K) :
    History K × K →ₗ[K] History K where
  toFun v := histories (linearStep (withHistory s v.1) ⟨v.2⟩)
  map_add' v w := by
    ext i
    fin_cases i <;>
      simp [histories,linearStep,stepWith,withHistory,OnePoleZOHLowPass.onePoleStep_eq,
        fma_eq,NormalizedTanh.slopeZero] <;> ring
  map_smul' c v := by
    ext i
    fin_cases i <;>
      simp [histories,linearStep,stepWith,withHistory,OnePoleZOHLowPass.onePoleStep_eq,
        fma_eq,NormalizedTanh.slopeZero] <;> ring

def stateMatrix {K : Type} [Field K] (s : ConservativeReuppingLadder K) :
    History K →ₗ[K] History K :=
  (linearUpdate s).comp (LinearMap.inl K (History K) K)

def complexCoefficients (s : ConservativeReuppingLadder ℝ) : ConservativeReuppingLadder ℂ :=
  ⟨s.alpha,⟨s.saturation.gain,s.saturation.tanhGain⟩,s.k,0,0,0,0,0,0⟩

def embedHistory : History ℝ →ₗ[ℝ] History ℂ where
  toFun x := fun i => (x i : ℂ)
  map_add' _ _ := by ext; simp
  map_smul' _ _ := by ext; simp

theorem embedHistory_norm (x : History ℝ) : ‖embedHistory x‖ = ‖x‖ := by
  simp [embedHistory, Pi.norm_def]

theorem embedHistory_commutes (s : ConservativeReuppingLadder ℝ) (x : History ℝ) :
    embedHistory (stateMatrix s x) =
      (stateMatrix (complexCoefficients s)) (embedHistory x) := by
  ext i
  fin_cases i <;>
    simp [embedHistory,stateMatrix,linearUpdate,complexCoefficients,histories,linearStep,
      stepWith,withHistory,OnePoleZOHLowPass.onePoleStep_eq,fma_eq,NormalizedTanh.slopeZero]

theorem prepared_spectrum_lt_one (spec : LogicalSpec ℝ) (s : ConservativeReuppingLadder ℝ)
    (hc : 0 < spec.cutoff) (hr0 : 0 ≤ spec.resonance) (hr1 : spec.resonance ≤ 1)
    (hg : 0 < spec.gain) :
    ∀ z ∈ spectrum ℂ (stateMatrix (complexCoefficients (computeCoefficients spec s))).toContinuousLinearMap,
      ‖z‖ < 1 := by
  let f := computeCoefficients spec s
  let A := stateMatrix (complexCoefficients f)
  intro z hz
  have he : Module.End.HasEigenvalue A z := by
    apply Module.End.HasEigenvalue.of_mem_spectrum
    change z ∈ spectrum ℂ ((Module.End.toContinuousLinearMap (𝕜 := ℂ) (History ℂ)) A) at hz
    rwa [AlgEquiv.spectrum_eq] at hz
  obtain ⟨x,hx,hne⟩ := he.exists_hasEigenvector
  have hm : A x = z • x := Module.End.mem_eigenspace_iff.mp hx
  have hmode := characteristic_of_mode (withHistory (complexCoefficients f) x) z
    (congrFun hm 0) (congrFun hm 1) (congrFun hm 2) (congrFun hm 3)
    (by
      by_contra! h
      apply hne
      ext i
      fin_cases i <;> simp_all [withHistory])
  apply prepared_roots_lt_one spec s hc hr0 hr1 hg z
  simpa [characteristic,withHistory,complexCoefficients,NormalizedTanh.slopeZero] using hmode

def preparedGauge (spec : LogicalSpec ℝ) (s : ConservativeReuppingLadder ℝ)
    (hc : 0 < spec.cutoff) (hr0 : 0 ≤ spec.resonance) (hr1 : spec.resonance ≤ 1)
    (hg : 0 < spec.gain) : StabilityGauge (stateMatrix (computeCoefficients spec s)) :=
  (StabilityGauge.ofSpectrum
    (stateMatrix (complexCoefficients (computeCoefficients spec s))).toContinuousLinearMap
    (prepared_spectrum_lt_one spec s hc hr0 hr1 hg)).comap embedHistory embedHistory_norm
      (embedHistory_commutes (computeCoefficients spec s))

def stateUpdate (s : ConservativeReuppingLadder ℝ) (v : History ℝ × ℝ) : History ℝ :=
  histories (step (withHistory s v.1) ⟨v.2⟩)

theorem stateUpdate_zero (s : ConservativeReuppingLadder ℝ) : stateUpdate s 0 = 0 := by
  ext i
  fin_cases i <;> simp [stateUpdate,histories,step,stepWith,withHistory,
    OnePoleZOHLowPass.onePoleStep_eq,fma_eq,NormalizedTanh.apply]

@[fun_prop]
theorem tanh_contDiff : ContDiff ℝ 1 Real.tanh := by
  have he : Real.tanh = Real.sinh / Real.cosh := funext Real.tanh_eq_sinh_div_cosh
  rw [he]
  exact Real.contDiff_sinh.div Real.contDiff_cosh (fun x => ne_of_gt (Real.cosh_pos x))

theorem stateUpdate_contDiff (s : ConservativeReuppingLadder ℝ) : ContDiff ℝ 1 (stateUpdate s) := by
  apply contDiff_pi.mpr
  intro i
  fin_cases i <;>
    simp [stateUpdate,histories,step,stepWith,withHistory,NormalizedTanh.apply,
      Tanh.tanh,OnePoleZOHLowPass.onePoleStep_eq,fma_eq] <;> fun_prop

theorem onePole_fderiv {V : Type*} [NormedAddCommGroup V] [NormedSpace ℝ V]
    {f h : V → ℝ} {df dh : V →L[ℝ] ℝ} (a : ℝ)
    (hf : HasFDerivAt f df 0) (hh : HasFDerivAt h dh 0) :
    HasFDerivAt (fun v => OnePoleZOHLowPass.onePoleStep a (f v) (h v))
      (a • df+(1-a) • dh) 0 := by
  simp only [OnePoleZOHLowPass.onePoleStep_eq,fma_eq]
  exact (hf.const_mul a).fun_add (hh.const_mul (1-a))

theorem saturation_fderiv {V : Type*} [NormedAddCommGroup V] [NormedSpace ℝ V]
    (c : NormalizedTanh.Coefficients ℝ) {f : V → ℝ} {df : V →L[ℝ] ℝ}
    (hz : f 0 = 0) (hf : HasFDerivAt f df 0) :
    HasFDerivAt (fun v => NormalizedTanh.apply c (f v)) (NormalizedTanh.slopeZero c • df) 0 := by
  have h := NormalizedTanh.hasDerivAt_zero c
  rw [← hz] at h
  exact h.comp_hasFDerivAt 0 hf

theorem stateUpdate_hasFDerivAt (s : ConservativeReuppingLadder ℝ) :
    HasFDerivAt (stateUpdate s) (linearUpdate s).toContinuousLinearMap 0 := by
  let proj (i : Fin 4) : History ℝ × ℝ →L[ℝ] ℝ :=
    (ContinuousLinearMap.proj i).comp (ContinuousLinearMap.fst ℝ (History ℝ) ℝ)
  let inp : History ℝ × ℝ →L[ℝ] ℝ := ContinuousLinearMap.snd ℝ (History ℝ) ℝ
  let d := inp-s.k • proj 3
  let f1 := fun v => OnePoleZOHLowPass.onePoleStep s.alpha (d v) (proj 0 v)
  let f2 := fun v => OnePoleZOHLowPass.onePoleStep s.alpha (NormalizedTanh.apply s.saturation (f1 v)) (proj 1 v)
  let f3 := fun v => OnePoleZOHLowPass.onePoleStep s.alpha (NormalizedTanh.apply s.saturation (f2 v)) (proj 2 v)
  let f4 := fun v => OnePoleZOHLowPass.onePoleStep s.alpha (NormalizedTanh.apply s.saturation (f3 v)) (proj 3 v)
  have hz : f1 0 = 0 ∧ f2 0 = 0 ∧ f3 0 = 0 := by
    simp [f1,f2,f3,OnePoleZOHLowPass.onePoleStep_eq,fma_eq,NormalizedTanh.apply]
  have h1 := onePole_fderiv s.alpha d.hasFDerivAt (proj 0).hasFDerivAt
  have h2 := onePole_fderiv s.alpha (saturation_fderiv s.saturation hz.1 h1) (proj 1).hasFDerivAt
  have h3 := onePole_fderiv s.alpha (saturation_fderiv s.saturation hz.2.1 h2) (proj 2).hasFDerivAt
  have h4 := onePole_fderiv s.alpha (saturation_fderiv s.saturation hz.2.2 h3) (proj 3).hasFDerivAt
  apply hasFDerivAt_pi'.mpr
  intro i
  fin_cases i
  · convert! h1 using 1
  · convert! h2 using 1
  · convert! h3 using 1
  · convert! h4 using 1

theorem stateUpdate_hasStrictFDerivAt (s : ConservativeReuppingLadder ℝ) :
    HasStrictFDerivAt (stateUpdate s) (linearUpdate s).toContinuousLinearMap 0 :=
  (stateUpdate_contDiff s).contDiffAt.hasStrictFDerivAt' (stateUpdate_hasFDerivAt s) (by norm_num)

end DSP.ConservativeReuppingLadder

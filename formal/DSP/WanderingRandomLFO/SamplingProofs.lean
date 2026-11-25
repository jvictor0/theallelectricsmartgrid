import DSP.WanderingRandomLFO.ProbabilityModel
import Mathlib.MeasureTheory.Function.Floor
import Mathlib.Probability.Independence.Basic

namespace DSP.WanderingRandomLFO

open SmoothRandomLFO MeasureTheory ProbabilityTheory Set

noncomputable section

private theorem unitUniform_eq_restrict_Ioc : unitUniform = volume.restrict (Ioc 0 1) :=
  restrict_Ico_eq_restrict_Ioc

/-- The inverse exponential transform in the exact clock uses a uniform draw. -/
theorem uniformClockDraw_map (rate : ℝ) (hr : 0 < rate) :
    unitUniform.map (fun u => -Real.log u / rate) = expMeasure rate := by
  let draw := fun u : ℝ => -Real.log u / rate
  have hd : Measurable draw := by dsimp [draw]; fun_prop
  letI : IsProbabilityMeasure (expMeasure rate) := isProbabilityMeasure_expMeasure hr
  letI : IsProbabilityMeasure (unitUniform.map draw) := Measure.isProbabilityMeasure_map hd.aemeasurable
  apply Measure.eq_of_cdf
  ext x
  rw [cdf_eq_real, cdf_expMeasure_eq hr, map_measureReal_apply hd measurableSet_Iic,
    unitUniform_eq_restrict_Ioc, measureReal_restrict_apply (hd measurableSet_Iic)]
  split_ifs with hx
  · have hset : draw ⁻¹' Iic x ∩ Ioc 0 1 = Icc (Real.exp (-(rate*x))) 1 := by
      ext u
      constructor
      · rintro ⟨hu, hU⟩
        refine ⟨(Real.log_le_log_iff (Real.exp_pos _) hU.1).mp ?_, hU.2⟩
        rw [Real.log_exp]
        have hh := (div_le_iff₀ hr).mp (show -Real.log u / rate ≤ x from hu)
        linarith
      · rintro ⟨hu, hU⟩
        have hpos := (Real.exp_pos _).trans_le hu
        refine ⟨(div_le_iff₀ hr).mpr ?_, hpos, hU⟩
        have hh := Real.log_le_log (Real.exp_pos _) hu
        rw [Real.log_exp] at hh
        linarith
    rw [hset, measureReal_def, Real.volume_Icc, ENNReal.toReal_ofReal]
    exact sub_nonneg.mpr (Real.exp_le_one_iff.mpr (neg_nonpos.mpr (mul_nonneg hr.le hx)))
  · have hset : draw ⁻¹' Iic x ∩ Ioc 0 1 = ∅ := by
      apply eq_empty_iff_forall_notMem.mpr
      rintro u ⟨hu, hU⟩
      exact hx ((div_nonneg (neg_nonneg.mpr (Real.log_nonpos hU.1.le hU.2)) hr.le).trans hu)
    rw [hset, measureReal_empty]

theorem fractionalPhase_map_volume :
    (volume : Measure Phase).map fractionalPhase = unitUniform := by
  have hw : unitUniform.map ((↑) : ℝ → Phase) = (volume : Measure Phase) := by
    rw [unitUniform_eq_restrict_Ioc]
    simpa using (AddCircle.measurePreserving_mk (1 : ℝ) 0).map_eq
  rw [← hw, Measure.map_map measurable_fractionalPhase AddCircle.measurable_mk']
  calc
    _ = unitUniform.map id := by
      apply Measure.map_congr
      filter_upwards [ae_restrict_mem measurableSet_Ico (μ := (volume : Measure ℝ))
        (s := Ico (0 : ℝ) 1)] with x hx
      have hx' : x ∈ Ico (0 : ℝ) (0+1) := by simpa using hx
      change (((AddCircle.equivIco 1 0) (x : Phase)).val : ℝ) = x
      rw [AddCircle.equivIco_coe_eq hx']
    _ = _ := Measure.map_id

/-- The limiting law is the cubic blend of four independent uniforms at an
independent uniform phase, scaled to the bipolar LFO output. -/
theorem stationaryAmplitudeLaw_eq :
    stationaryAmplitudeLaw =
      (unitUniform.prod knotUniforms).map (fun x : ℝ × (Fin 4 → ℝ) =>
        2 * (((1-x.1)^3 * x.2 0 + (4-6*x.1^2+3*x.1^3)*x.2 1 +
          (1+3*x.1+3*x.1^2-3*x.1^3)*x.2 2 + x.1^3*x.2 3) / 6) - 1) := by
  unfold stationaryAmplitudeLaw amplitudeLaw
  have hp : unitUniform.prod knotUniforms = (volume.prod knotUniforms).map
      (Prod.map fractionalPhase id) := by
    rw [← Measure.map_prod_map _ _ measurable_fractionalPhase measurable_id,
      fractionalPhase_map_volume, Measure.map_id]
  rw [hp, Measure.map_map (by fun_prop) (measurable_fractionalPhase.prodMap measurable_id)]
  congr 1
  funext x
  simp [bipolarSample, normalizedSpline_eq]

private def knotWindow (k : ℕ) (p : ℕ → ℝ) (j : Fin 4) : ℝ := p (k + j.val)

private theorem measurable_knotWindow (k : ℕ) : Measurable (knotWindow k) :=
  measurable_pi_iff.mpr fun _ => measurable_pi_apply _

private theorem measurable_selectKnots {Ω : Type*} [MeasurableSpace Ω]
    (index : Ω → ℕ) (hindex : Measurable index) :
    Measurable (fun x : Ω × (ℕ → ℝ) => knotWindow (index x.1) x.2) := by
  intro s hs
  have heq : (fun x : Ω × (ℕ → ℝ) => knotWindow (index x.1) x.2) ⁻¹' s =
      ⋃ k : ℕ, ((index ∘ Prod.fst) ⁻¹' {k}) ∩ ((knotWindow k ∘ Prod.snd) ⁻¹' s) := by
    ext x
    simp
  rw [heq]
  exact MeasurableSet.iUnion (fun k => ((hindex.comp measurable_fst) (measurableSet_singleton k)).inter
    (((measurable_knotWindow k).comp measurable_snd) hs))

/-- The clock can choose a knot window at random. Its four values still have
the four-coordinate product law because the knot stream is iid and independent
of the clock. -/
theorem iidKnotWindow_map {Ω : Type*} [MeasurableSpace Ω] (μ : Measure Ω)
    [IsProbabilityMeasure μ] (knots : Measure (ℕ → ℝ)) [IsProbabilityMeasure knots]
    (hiid : iIndepFun (fun j (p : ℕ → ℝ) => p j) knots)
    (hunit : ∀ j : ℕ, knots.map (fun p : ℕ → ℝ => p j) = unitUniform)
    (phase : Ω → Phase) (index : Ω → ℕ) (hphase : Measurable phase) (hindex : Measurable index) :
    (μ.prod knots).map (fun x : Ω × (ℕ → ℝ) =>
      (phase x.1, fun j : Fin 4 => x.2 (index x.1 + j.val))) =
      (μ.map phase).prod knotUniforms := by
  have hwindow (k : ℕ) : knots.map (knotWindow k) = knotUniforms := by
    have hinj : Function.Injective (fun j : Fin 4 => k + j.val) := fun _ _ h =>
      Fin.ext (Nat.add_left_cancel h)
    change knots.map (fun p : ℕ → ℝ => fun j : Fin 4 => p (k + j.val)) = _
    simpa only [hunit, knotUniforms] using
      (hiid.precomp hinj).map_fun_eq_pi_map (fun _ => (measurable_pi_apply _).aemeasurable)
  have hprod : Measurable (fun x : Ω × (ℕ → ℝ) => (phase x.1, knotWindow (index x.1) x.2)) :=
    (hphase.comp measurable_fst).prodMk (measurable_selectKnots index hindex)
  refine Measure.ext_of_lintegral _ fun f hf => ?_
  change (∫⁻ z, f z ∂((μ.prod knots).map (fun x => (phase x.1, knotWindow (index x.1) x.2)))) = _
  rw [lintegral_map hf hprod,
    lintegral_prod (fun x : Ω × (ℕ → ℝ) => f (phase x.1, knotWindow (index x.1) x.2))
      (hf.comp hprod).aemeasurable,
    lintegral_prod _ hf.aemeasurable, lintegral_map hf.lintegral_prod_right' hphase]
  refine lintegral_congr fun x => ?_
  have hinner : Measurable (fun y : Fin 4 → ℝ => f (phase x, y)) :=
    hf.comp (measurable_const.prodMk measurable_id)
  change (∫⁻ y, f (phase x, knotWindow (index x) y) ∂knots) = _
  rw [← lintegral_map hinner (measurable_knotWindow (index x)), hwindow]

private theorem fractionalPhase_coe (t : ℝ) : fractionalPhase (t : Phase) = Int.fract t := by
  have ht : Int.fract t ∈ Ico (0 : ℝ) (0 + 1) :=
    ⟨Int.fract_nonneg t, by simpa using Int.fract_lt_one t⟩
  rw [← AddCircle.coe_fract t]
  exact congrArg Subtype.val (AddCircle.equivIco_coe_eq ht)

/-- Read the exact spline directly from an iid sequence of normalized knot
draws, using the same bipolar conversion and cubic as the runtime. -/
def idealSplineSample (t : ℝ) (p : ℕ → ℝ) : ℝ :=
  let k := ⌊t⌋₊
  let point (j : ℕ) := 2 * p (k + j) - 1
  evaluate (prepare (unitSegment (point 0) (point 1) (point 2) (point 3))) (Int.fract t)

private theorem idealSplineSample_eq (t : ℝ) (p : ℕ → ℝ) :
    idealSplineSample t p = bipolarSample ((t : Phase), knotWindow ⌊t⌋₊ p) := by
  unfold bipolarSample
  rw [fractionalPhase_coe]
  change normalizedSpline (Int.fract t) (fun j => 2 * knotWindow ⌊t⌋₊ p j - 1) = _
  simp only [normalizedSpline_eq]
  ring

/-- A clock independent of the knot stream has the one-sample law used in
the convergence theorem. The unwrapped phase selects the knot window. -/
theorem iidSplineSample_map {Ω : Type*} [MeasurableSpace Ω] (μ : Measure Ω)
    [IsProbabilityMeasure μ] (knots : Measure (ℕ → ℝ)) [IsProbabilityMeasure knots]
    (hiid : iIndepFun (fun j (p : ℕ → ℝ) => p j) knots)
    (hunit : ∀ j : ℕ, knots.map (fun p : ℕ → ℝ => p j) = unitUniform)
    (time : Ω → ℝ) (htime : Measurable time) :
    (μ.prod knots).map (fun x : Ω × (ℕ → ℝ) => idealSplineSample (time x.1) x.2) =
      amplitudeLaw (μ.map (fun x => (time x : Phase))) := by
  let phase (x : Ω) := (time x : Phase)
  let index (x : Ω) := ⌊time x⌋₊
  have hp : Measurable phase := AddCircle.measurable_mk'.comp htime
  have hi : Measurable index := Nat.measurable_floor.comp htime
  have hw := iidKnotWindow_map μ knots hiid hunit phase index hp hi
  have hm : Measurable (fun x : Ω × (ℕ → ℝ) =>
      (phase x.1, fun j : Fin 4 => x.2 (index x.1 + j.val))) :=
    (hp.comp measurable_fst).prodMk (measurable_selectKnots index hi)
  unfold amplitudeLaw
  change _ = ((μ.map phase).prod knotUniforms).map bipolarSample
  rw [← hw, Measure.map_map measurable_bipolarSample hm]
  congr 1
  funext x
  exact idealSplineSample_eq (time x.1) x.2

theorem everySampleLaw_eq_iidSplineSample (lambda : ℝ) (sample : ℕ)
    (knots : Measure (ℕ → ℝ)) [IsProbabilityMeasure knots]
    (hiid : iIndepFun (fun j (p : ℕ → ℝ) => p j) knots)
    (hunit : ∀ j : ℕ, knots.map (fun p : ℕ → ℝ => p j) = unitUniform) :
    let frame := sample / frameSamples
    let i := (sample % frameSamples : ℕ)
    everySampleLaw lambda sample = ((phaseDraws lambda frame).prod knots).map
      (fun x => idealSplineSample (samplePhaseTime lambda exactAlpha frame i x.1) x.2) :=
  (iidSplineSample_map _ knots hiid hunit _ (measurable_samplePhaseTime _ _ _ _)).symm

end

end DSP.WanderingRandomLFO

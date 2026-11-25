import DSP.WanderingRandomLFO.PhaseLaw

namespace DSP.WanderingRandomLFO

open MeasureTheory ProbabilityTheory

noncomputable section

private theorem measurable_phaseSum (coeff : ℕ → ℝ) (k : ℕ) :
    Measurable (fun draw : Fin k → ℝ => (↑(∑ j : Fin k, coeff j * min 1 (draw j)) : Phase)) := by
  apply AddCircle.measurable_mk'.comp
  exact Finset.measurable_sum _ fun j _ =>
    measurable_const.mul (measurable_const.min (measurable_pi_apply j))

private theorem iid_map_phaseSum (lambda : ℝ) (coeff : ℕ → ℝ) (k : ℕ) :
    (Measure.pi (fun _ : Fin k => expMeasure (max 1 lambda))).map
      (fun draw : Fin k → ℝ => (↑(∑ j : Fin k, coeff j * min 1 (draw j)) : Phase)) =
    convolvedPhase (fun j => phaseStepLaw lambda (coeff j)) k := by
  let μ := expMeasure (max 1 lambda)
  letI : IsProbabilityMeasure μ :=
    isProbabilityMeasure_expMeasure (zero_lt_one.trans_le (le_max_left _ _))
  change (Measure.pi (fun _ : Fin k => μ)).map _ = _
  induction k with
  | zero =>
    simp only [Finset.univ_eq_empty, Finset.sum_empty, AddCircle.coe_zero, convolvedPhase,
      Measure.map_const, measure_univ, one_smul]
  | succ k ih =>
    let e := MeasurableEquiv.piFinSuccAbove (fun _ : Fin (k + 1) => ℝ) (Fin.last k)
    have he : MeasurePreserving e (Measure.pi (fun _ : Fin (k + 1) => μ))
        (μ.prod (Measure.pi (fun _ : Fin k => μ))) :=
      measurePreserving_piFinSuccAbove (fun _ => μ) (Fin.last k)
    let f : ℝ → Phase := fun x => (↑(coeff k * min 1 x) : Phase)
    let g : (Fin k → ℝ) → Phase := fun draw => (↑(∑ j : Fin k, coeff j * min 1 (draw j)) : Phase)
    have hf : Measurable f := AddCircle.measurable_mk'.comp (by fun_prop)
    have hg : Measurable g := measurable_phaseSum coeff k
    haveI : SFinite (convolvedPhase (fun j => phaseStepLaw lambda (coeff j)) k) := by
      rw [← ih]
      infer_instance
    rw [convolvedPhase, Measure.conv_comm, ← ih]
    change _ = (μ.map f ∗ (Measure.pi (fun _ : Fin k => μ)).map g)
    rw [Measure.conv, Measure.map_prod_map _ _ hf hg, ← he.map_eq,
      Measure.map_map measurable_add (hf.prodMap hg),
      Measure.map_map (measurable_add.comp (hf.prodMap hg)) e.measurable]
    congr 1
    funext draw
    simp [e, f, g, Function.comp_def, MeasurableEquiv.piFinSuccAbove,
      Fin.sum_univ_castSucc, Fin.init, add_comm]

theorem phaseDraws_map_sum (lambda : ℝ) (n : ℕ) (coeff : ℕ → ℝ) (shift : ℝ) :
    (phaseDraws lambda n).map
      (fun draw : Fin (n+1) → ℝ => ((shift + ∑ j : Fin (n+1), coeff j * min 1 (draw j) : ℝ) :
        Phase)) =
    convolvedPhase (fun j => phaseStepLaw lambda (coeff j)) (n+1) ∗
      Measure.dirac (shift : Phase) := by
  have hsum := iid_map_phaseSum lambda coeff (n + 1)
  change (phaseDraws lambda n).map _ = _ at hsum
  rw [← hsum, Measure.conv_dirac, Measure.map_map (by fun_prop)
    (measurable_phaseSum coeff (n+1))]
  simp only [Function.comp_def, AddCircle.coe_add, add_comm]

end

end DSP.WanderingRandomLFO

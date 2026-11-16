import DSP.IIR.Realization
import DSP.Transfer.Rational

namespace DSP.StateSpace

open Matrix Polynomial

variable {n : ℕ} {R S : Type} [CommRing R] [CommRing S]

def map (f : R →+* S) (s : StateSpace n R) : StateSpace n S where
  A i j := f (s.A i j)
  B i := f (s.B i)
  C i := f (s.C i)
  D := f s.D

def delayMatrix (s : StateSpace n R) (q : R) : Matrix (Fin n) (Fin n) R :=
  1 - q • Matrix.of s.A

noncomputable def delayDenominatorAt (s : StateSpace n R) (q : R) : R := (s.delayMatrix q).det
noncomputable def delayNumeratorAt (s : StateSpace n R) (q : R) : R :=
  s.D * s.delayDenominatorAt q + q * (s.C ⬝ᵥ ((s.delayMatrix q).adjugate *ᵥ s.B))

noncomputable def delayDenominator (s : StateSpace n R) : R[X] :=
  (s.map Polynomial.C).delayDenominatorAt X
noncomputable def delayNumerator (s : StateSpace n R) : R[X] :=
  (s.map Polynomial.C).delayNumeratorAt X

theorem map_delayMatrix (f : R →+* S) (s : StateSpace n R) (q : R) :
    (s.delayMatrix q).map f = (s.map f).delayMatrix (f q) := by
  ext i j
  by_cases h : i = j
  · subst j; simp [delayMatrix, map]
  · simp [delayMatrix, map, h]

theorem map_delayDenominatorAt (f : R →+* S) (s : StateSpace n R) (q : R) :
    f (s.delayDenominatorAt q) = (s.map f).delayDenominatorAt (f q) := by
  unfold delayDenominatorAt
  rw [RingHom.map_det, RingHom.mapMatrix_apply, map_delayMatrix]

theorem map_delayNumeratorAt (f : R →+* S) (s : StateSpace n R) (q : R) :
    f (s.delayNumeratorAt q) = (s.map f).delayNumeratorAt (f q) := by
  simp only [delayNumeratorAt, map_add, map_mul, map_delayDenominatorAt, RingHom.map_dotProduct]
  congr 1
  congr 1
  change (f ∘ s.C) ⬝ᵥ (f ∘ ((s.delayMatrix q).adjugate *ᵥ s.B)) = _
  have h : f ∘ ((s.delayMatrix q).adjugate *ᵥ s.B) =
      ((s.map f).delayMatrix (f q)).adjugate *ᵥ (s.map f).B := by
    funext i
    rw [Function.comp_apply, RingHom.map_mulVec]
    change (f.mapMatrix (s.delayMatrix q).adjugate *ᵥ (f ∘ s.B)) i = _
    rw [RingHom.map_adjugate, RingHom.mapMatrix_apply, map_delayMatrix]
    rfl
  rw [h]
  rfl

@[simp] theorem delayDenominator_constant (s : StateSpace n R) :
    s.delayDenominator.coeff 0 = 1 := by
  change Polynomial.constantCoeff ((s.map Polynomial.C).delayDenominatorAt X) = 1
  rw [map_delayDenominatorAt]
  simp [delayDenominatorAt, delayMatrix]

/-- Eliminate internal state without dividing by a determinant. -/
theorem delay_crossMultiply (s : StateSpace n R) (q u : R) (v : Fin n → R)
    (h : v = q • s.advance v u) :
    s.delayDenominatorAt q * s.observe v u = s.delayNumeratorAt q * u := by
  have hm : s.delayMatrix q *ᵥ v = (q * u) • s.B := by
    rw [delayMatrix, sub_mulVec, one_mulVec, smul_mulVec]
    conv_lhs => arg 1; rw [h, advance_eq]
    ext i
    simp only [Pi.add_apply, Pi.sub_apply, Pi.smul_apply, smul_eq_mul]
    change q * ((s.A *ᵥ v) i + s.B i * u) - q * (s.A *ᵥ v) i = q * u * s.B i
    ring
  have hv : s.delayDenominatorAt q • v = (q * u) • ((s.delayMatrix q).adjugate *ᵥ s.B) := by
    have he := congrArg (fun w => (s.delayMatrix q).adjugate *ᵥ w) hm
    rw [mulVec_mulVec, adjugate_mul, smul_mulVec, one_mulVec, mulVec_smul] at he
    exact he
  have hy := congrArg (fun w => s.C ⬝ᵥ w) hv
  simp only [dotProduct_smul, smul_eq_mul] at hy
  rw [observe_eq, delayNumeratorAt]
  linear_combination hy

/-- With zero initial state, the vector generating series obeys Q=q(AQ+BU). -/
theorem run_series_state (s : StateSpace n R) (u : ℕ → R) :
    let v : Fin n → PowerSeries R := fun i => IIR.series (fun t => Processor.runState s.processor u (fun _ => 0) t i)
    v = (PowerSeries.X : PowerSeries R) • (s.map PowerSeries.C).advance v (IIR.series u) := by
  dsimp only
  funext i
  apply PowerSeries.ext
  intro t
  cases t with
  | zero => simp [IIR.series, Processor.runState]
  | succ t =>
    simp only [Pi.smul_apply, smul_eq_mul, PowerSeries.coeff_succ_X_mul,
      advance, dot, orderedSum_eq_sum, map, map_add,
      map_sum, PowerSeries.coeff_C_mul, IIR.series, PowerSeries.coeff_mk]
    simp [Processor.runState_succ, processor, advance, dot, orderedSum_eq_sum]

theorem run_series_output (s : StateSpace n R) (u : ℕ → R) :
    IIR.series (Processor.run s.processor u (fun _ => 0)) =
      (s.map PowerSeries.C).observe
        (fun i => IIR.series (fun t => Processor.runState s.processor u (fun _ => 0) t i))
        (IIR.series u) := by
  apply PowerSeries.ext
  intro t
  simp only [observe, dot, orderedSum_eq_sum, map, map_add, map_sum,
    PowerSeries.coeff_C_mul, IIR.series, PowerSeries.coeff_mk]
  simp [Processor.run, processor, observe, dot, orderedSum_eq_sum]

theorem run_seriesEquation (s : StateSpace n R) (u : ℕ → R) :
    (s.delayDenominator : PowerSeries R) * IIR.series (Processor.run s.processor u (fun _ => 0)) =
      (s.delayNumerator : PowerSeries R) * IIR.series u := by
  let f := Polynomial.coeToPowerSeries.ringHom (R := R)
  have hd : (s.delayDenominator : PowerSeries R) = (s.map PowerSeries.C).delayDenominatorAt PowerSeries.X := by
    simpa [delayDenominator, f, map] using map_delayDenominatorAt f (s.map Polynomial.C) X
  have hn : (s.delayNumerator : PowerSeries R) = (s.map PowerSeries.C).delayNumeratorAt PowerSeries.X := by
    simpa [delayNumerator, f, map] using map_delayNumeratorAt f (s.map Polynomial.C) X
  rw [hd, hn, run_series_output]
  exact delay_crossMultiply _ _ _ _ (s.run_series_state u)

noncomputable def toIIR {K : Type} [Field K] (s : StateSpace n K) :=
  IIR.ofPolynomials s.delayNumerator s.delayDenominator

theorem toIIR_correct {K : Type} [Field K] (s : StateSpace n K) (u : ℕ → K) :
    Processor.run (IIR.processor s.toIIR) u IIR.zeroState =
      Processor.run s.processor u (fun _ => 0) := by
  symm
  apply (IIR.realize_series s.delayNumerator s.delayDenominator (by simp) u _).mpr
  exact s.run_seriesEquation u

end DSP.StateSpace

import DSP.Transfer.Realization

namespace DSP.RationalCoefficients

open Matrix

variable {n : ℕ} {K : Type} [NontriviallyNormedField K] [NormedSpace ℝ K]

/-- The scalar equation A(D)v=u, y=B(D)v, with derivatives supplied as a jet.
The denominator is monic of order n+1; d is the numerator's top coefficient. -/
def JetSolution (c : RationalCoefficients n K) (u y : ℝ → K)
    (v : ℝ → Fin (n + 2) → K) : Prop :=
  (∀ t (i : Fin (n + 1)), HasDerivAt (fun r => v r i.castSucc) (v t i.succ) t) ∧
  (∀ t, v t (Fin.last (n + 1)) + ∑ i, c.a i * v t i.castSucc = u t) ∧
  ∀ t, y t = c.d * v t (Fin.last (n + 1)) + ∑ i, c.b i * v t i.castSucc

def StateSolution (c : RationalCoefficients n K) (u y : ℝ → K)
    (q : ℝ → Fin (n + 1) → K) : Prop :=
  (∀ t i, HasDerivAt (fun r => q r i) (c.system.advance (q t) (u t) i) t) ∧
  ∀ t, y t = c.system.observe (q t) (u t)

def extendJet (c : RationalCoefficients n K) (u : ℝ → K) (q : ℝ → Fin (n + 1) → K) :
    ℝ → Fin (n + 2) → K :=
  fun t => Fin.lastCases (u t - ∑ i, c.a i * q t i) (q t)

omit [NormedSpace ℝ K] in
theorem advance_castSucc (c : RationalCoefficients n K) (q : Fin (n + 1) → K)
    (u : K) (i : Fin n) : c.system.advance q u i.castSucc = q i.succ := by
  simp [StateSpace.advance_eq, system, mulVec, dotProduct]

omit [NormedSpace ℝ K] in
theorem advance_last (c : RationalCoefficients n K) (q : Fin (n + 1) → K) (u : K) :
    c.system.advance q u (Fin.last n) = u - ∑ i, c.a i * q i := by
  simp [StateSpace.advance_eq, system, mulVec, dotProduct, Finset.sum_neg_distrib, sub_eq_add_neg, add_comm]

omit [NormedSpace ℝ K] in
theorem observe_companion (c : RationalCoefficients n K) (q : Fin (n + 1) → K) (u : K) :
    c.system.observe q u = c.d * (u - ∑ i, c.a i * q i) + ∑ i, c.b i * q i := by
  simp only [StateSpace.observe_eq, system, dotProduct, sub_mul, Finset.sum_sub_distrib,
    mul_assoc, ← Finset.mul_sum]
  ring

theorem stateSolution_iff_jet (c : RationalCoefficients n K) (u y : ℝ → K)
    (q : ℝ → Fin (n + 1) → K) :
    c.StateSolution u y q ↔ c.JetSolution u y (c.extendJet u q) := by
  constructor
  · rintro ⟨hd, hy⟩
    refine ⟨fun t i => ?_, fun t => ?_, fun t => ?_⟩
    · refine Fin.lastCases ?_ (fun j => ?_) i
      · simpa [extendJet, advance_last] using hd t (Fin.last n)
      · simpa only [extendJet, Fin.lastCases_castSucc, Fin.succ_castSucc, advance_castSucc] using hd t j.castSucc
    · simp [extendJet]
    · simpa [extendJet, observe_companion] using hy t
  · rintro ⟨hd, _, hy⟩
    refine ⟨fun t i => ?_, fun t => ?_⟩
    · refine Fin.lastCases ?_ (fun j => ?_) i
      · simpa [extendJet, advance_last] using hd t (Fin.last n)
      · simpa only [extendJet, Fin.lastCases_castSucc, Fin.succ_castSucc, advance_castSucc] using hd t j.castSucc
    · simpa [extendJet, observe_companion] using hy t

theorem jet_to_stateSolution (c : RationalCoefficients n K) (u y : ℝ → K)
    (v : ℝ → Fin (n + 2) → K) (h : c.JetSolution u y v) :
    c.StateSolution u y (fun t i => v t i.castSucc) := by
  have he : c.extendJet u (fun t i => v t i.castSucc) = v := by
    funext t i
    refine Fin.lastCases ?_ (fun _ => ?_) i
    · simpa [extendJet] using (eq_sub_iff_add_eq.mpr (h.2.1 t)).symm
    · simp [extendJet]
  apply (c.stateSolution_iff_jet u y _).mpr
  rwa [he]

theorem ode_hasTransfer (c : RationalCoefficients n ℂ) (s gain : ℂ)
    (hs : c.evalDenominator s ≠ 0) :
    (∃ v : Fin (n + 1) → ℂ,
      c.JetSolution (expMode s) (fun t => gain * expMode s t)
        (c.extendJet (expMode s) (fun t => expMode s t • v))) ↔
      gain = c.evalNumerator s / c.evalDenominator s := by
  simp only [← stateSolution_iff_jet]
  exact c.realizeAnalog_transfer s gain hs

end DSP.RationalCoefficients

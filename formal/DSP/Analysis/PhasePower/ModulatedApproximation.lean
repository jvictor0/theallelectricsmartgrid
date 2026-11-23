import DSP.Analysis.PhasePower.Approximation
import DSP.ResponseMeasurement.SlowModulation

/-! Compose the constructed slow-phase correction with explicit fast-carrier
corrections. This is the error budget for a finite HB observer decomposition;
the fast terms need not have zero discrete average. -/

noncomputable section
namespace DSP.PhasePower.Discrete
open DSP.ResponseMeasurement Finset
variable {ι : Type*} [Fintype ι]

theorem modulated_curve (q : Quadratic) (c : ℝ) (hc : 0 ≤ c) (N : ℕ)
    (h : ℝ) (hh : 0 < h) (phase observation : ℕ → ℂ)
    (eulerError stepBound observationError : ℝ)
    (hz : ∀ n, ‖phase n‖ = 1)
    (he : ∀ n, ‖phase (n+1)-phase n-(h:ℂ)*flow c (phase n)‖ ≤ eulerError)
    (hs : ∀ n, ‖phase (n+1)-phase n‖ ≤ stepBound)
    (carrier : ι → ℂ) (amplitude : ι → ℕ → ℂ) (bound variation : ι → ℝ)
    (hu : ∀ i, ‖carrier i‖ = 1) (hn : ∀ i, carrier i ≠ 1)
    (hb : ∀ i n, ‖amplitude i n‖ ≤ bound i)
    (hv : ∀ i n, ‖amplitude i (n+1)-amplitude i n‖ ≤ variation i)
    (ho : ∀ n, ‖observation n-q.eval (phase n)-∑ i, amplitude i n*(carrier i)^n‖ ≤ observationError) :
    EventuallyNear (DSP.Coboundary.average observation) (q.eval (center c))
      (observationError+error q c N h eulerError stepBound+∑ i, variation i/‖1-carrier i‖) := by
  let slow := fun n => -potential q c N (phase n)/(h:ℂ)
  let fast := fun n => ∑ i, SlowModulation.potential (carrier i) (amplitude i) n
  apply eventuallyNear_of_coboundary observation (fun n => slow n+fast n) _ _
    (size0 q c N/h+∑ i, bound i/‖1-carrier i‖)
  · intro n
    have hslow := correction_residual q c hc N h hh _ _ (hz n) (hz (n+1)) _ _ (he n) (hs n)
    have hfast : ‖(∑ i, amplitude i n*(carrier i)^n)-(fast n-fast (n+1))‖ ≤
        ∑ i, variation i/‖1-carrier i‖ := by
      simp only [fast,← Finset.sum_sub_distrib]
      apply (norm_sum_le _ _).trans
      exact sum_le_sum (fun i _ => SlowModulation.residual_bound _ (hn i) (hu i) _ _ n (hv i n))
    have hid : observation n-q.eval (center c)-((slow n+fast n)-(slow (n+1)+fast (n+1))) =
        (observation n-q.eval (phase n)-∑ i, amplitude i n*(carrier i)^n)+
          (q.eval (phase n)-q.eval (center c)-(slow n-slow (n+1)))+
          ((∑ i, amplitude i n*(carrier i)^n)-(fast n-fast (n+1))) := by ring
    rw [hid]
    exact (norm_add_le _ _).trans (add_le_add
      ((norm_add_le _ _).trans (add_le_add (ho n) hslow)) hfast)
  · intro n
    apply (norm_add_le _ _).trans
    apply add_le_add (correction_bound q c N h hh _ (hz n).le)
    apply (norm_sum_le _ _).trans
    exact sum_le_sum (fun i _ => SlowModulation.potential_bound _ (hu i) _ _ n (hb i n))

end DSP.PhasePower.Discrete

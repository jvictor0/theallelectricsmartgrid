import DSP.Analysis.PhasePower.Discrete

noncomputable section
namespace DSP.PhasePower.Discrete
open DSP.ResponseMeasurement

/-- Euler defect is measured in the complex unit phase. It includes the
curvature of a sampled phase rotation as well as phase-model error. -/
def error (q : Quadratic) (c : ℝ) (N : ℕ) (h eulerError stepBound : ℝ) : ℝ :=
  tail q c N+size1 q c N*eulerError/h+size2 q c N*stepBound^2/(2*h)

theorem one_step (q : Quadratic) (c : ℝ) (hc : 0 ≤ c) (N : ℕ)
    (h : ℝ) (hh : 0 < h) (z w : ℂ) (hz : ‖z‖ = 1) (hw : ‖w‖ = 1)
    (eulerError stepBound : ℝ) (he : ‖w-z-(h:ℂ)*flow c z‖ ≤ eulerError)
    (hs : ‖w-z‖ ≤ stepBound) :
    ‖q.eval z-q.eval (center c)-(potential q c N w-potential q c N z)/(h:ℂ)‖ ≤
      error q c N h eulerError stepBound := by
  have hB1 : 0 ≤ size1 q c N := by unfold size1 FinitePrimitive.bound1; positivity
  have hB2 : 0 ≤ size2 q c N := by unfold size2 FinitePrimitive.bound2; positivity
  have he0 : 0 ≤ eulerError := (norm_nonneg _).trans he
  have hstep : ‖w-z‖^2 ≤ stepBound^2 := pow_le_pow_left₀ (norm_nonneg _) hs 2
  have hTaylor := FinitePrimitive.remainder (coefficient q c N) degree z w hz.le hw.le
  have hSlope := FinitePrimitive.slope_bound (coefficient q c N) degree z hz.le
  have hTail := tail_bound q c hc N z hz
  change ‖potential q c N w-potential q c N z-derivative q c N z*(w-z)‖ ≤ _ at hTaylor
  change ‖derivative q c N z‖ ≤ size1 q c N at hSlope
  have hid : potential q c N w-potential q c N z-(h:ℂ)*(q.eval z-q.eval (center c)) =
      (potential q c N w-potential q c N z-derivative q c N z*(w-z))+
        derivative q c N z*(w-z-(h:ℂ)*flow c z)+
        (h:ℂ)*(derivative q c N z*flow c z-(q.eval z-q.eval (center c))) := by ring
  have hb : ‖potential q c N w-potential q c N z-(h:ℂ)*(q.eval z-q.eval (center c))‖ ≤
      size2 q c N*stepBound^2/2+size1 q c N*eulerError+h*tail q c N := by
    rw [hid]
    apply (norm_add_le _ _).trans
    apply add_le_add
    · apply (norm_add_le _ _).trans
      apply add_le_add
      · exact hTaylor.trans (div_le_div_of_nonneg_right
          (mul_le_mul_of_nonneg_left hstep hB2) (by norm_num))
      · rw [norm_mul]
        exact mul_le_mul hSlope he (norm_nonneg _) hB1
    · rw [norm_mul,Complex.norm_real,Real.norm_eq_abs,abs_of_pos hh]
      exact mul_le_mul_of_nonneg_left hTail hh.le
  have hn : (h:ℂ) ≠ 0 := by exact_mod_cast hh.ne'
  have heq : q.eval z-q.eval (center c)-(potential q c N w-potential q c N z)/(h:ℂ) =
      -(potential q c N w-potential q c N z-(h:ℂ)*(q.eval z-q.eval (center c)))/(h:ℂ) := by
    field_simp
    ring
  rw [heq,norm_div,norm_neg,Complex.norm_real,Real.norm_eq_abs,abs_of_pos hh]
  exact (div_le_div_of_nonneg_right hb hh.le).trans_eq (by unfold error; field_simp; ring)

theorem correction_bound (q : Quadratic) (c : ℝ) (N : ℕ) (h : ℝ) (hh : 0 < h)
    (z : ℂ) (hz : ‖z‖ ≤ 1) :
    ‖-potential q c N z/(h:ℂ)‖ ≤ size0 q c N/h := by
  rw [norm_div,norm_neg,Complex.norm_real,Real.norm_eq_abs,abs_of_pos hh]
  exact div_le_div_of_nonneg_right
    (FinitePrimitive.value_bound (coefficient q c N) degree z hz) hh.le

theorem correction_residual (q : Quadratic) (c : ℝ) (hc : 0 ≤ c) (N : ℕ)
    (h : ℝ) (hh : 0 < h) (z w : ℂ) (hz : ‖z‖ = 1) (hw : ‖w‖ = 1)
    (eulerError stepBound : ℝ) (he : ‖w-z-(h:ℂ)*flow c z‖ ≤ eulerError)
    (hs : ‖w-z‖ ≤ stepBound) :
    ‖q.eval z-q.eval (center c)-(-potential q c N z/(h:ℂ)- -potential q c N w/(h:ℂ))‖ ≤
      error q c N h eulerError stepBound := by
  convert one_step q c hc N h hh z w hz hw eulerError stepBound he hs using 1
  congr 1
  ring

/-- The finite-window version exposes the startup cost. -/
theorem finite_phase_bound (q : Quadratic) (c : ℝ) (hc : 0 ≤ c) (N : ℕ)
    (h : ℝ) (hh : 0 < h) (phase : ℕ → ℂ) (eulerError stepBound : ℝ)
    (hz : ∀ n, ‖phase n‖ = 1)
    (he : ∀ n, ‖phase (n+1)-phase n-(h:ℂ)*flow c (phase n)‖ ≤ eulerError)
    (hs : ∀ n, ‖phase (n+1)-phase n‖ ≤ stepBound) (count : ℕ) (hcount : 0 < count) :
    ‖DSP.Coboundary.average (fun n => q.eval (phase n)) count-q.eval (center c)‖ ≤
      error q c N h eulerError stepBound+2*(size0 q c N/h)/(count:ℝ) := by
  exact DSP.Coboundary.finite_bound _ (fun n => -potential q c N (phase n)/(h:ℂ)) _ _ _ count hcount
    (fun n _ => correction_residual q c hc N h hh _ _ (hz n) (hz (n+1)) _ _ (he n) (hs n))
    (correction_bound q c N h hh _ (hz 0).le) (correction_bound q c N h hh _ (hz count).le)

/-- An explicit finite polynomial now supplies the correction. Only the
phase-trajectory defect and the observable mismatch remain hypotheses. -/
theorem approximates_observable (q : Quadratic) (c : ℝ) (hc : 0 ≤ c) (N : ℕ)
    (h : ℝ) (hh : 0 < h) (phase observation : ℕ → ℂ)
    (eulerError stepBound observationError : ℝ)
    (hz : ∀ n, ‖phase n‖ = 1)
    (he : ∀ n, ‖phase (n+1)-phase n-(h:ℂ)*flow c (phase n)‖ ≤ eulerError)
    (hs : ∀ n, ‖phase (n+1)-phase n‖ ≤ stepBound)
    (ho : ∀ n, ‖observation n-q.eval (phase n)‖ ≤ observationError) :
    EventuallyNear (DSP.Coboundary.average observation) (q.eval (center c))
      (observationError+error q c N h eulerError stepBound) := by
  apply eventuallyNear_of_coboundary observation
    (fun n => -potential q c N (phase n)/(h:ℂ)) (q.eval (center c))
    (observationError+error q c N h eulerError stepBound) (size0 q c N/h)
  · intro n
    have hbound := one_step q c hc N h hh (phase n) (phase (n+1)) (hz n) (hz (n+1))
      eulerError stepBound (he n) (hs n)
    have hid : observation n-q.eval (center c)-
        (-potential q c N (phase n)/(h:ℂ)- -potential q c N (phase (n+1))/(h:ℂ)) =
        (observation n-q.eval (phase n))+
          (q.eval (phase n)-q.eval (center c)-
            (potential q c N (phase (n+1))-potential q c N (phase n))/(h:ℂ)) := by ring
    rw [hid]
    exact (norm_add_le _ _).trans (add_le_add (ho n) hbound)
  · intro n
    exact correction_bound q c N h hh _ (hz n).le

end DSP.PhasePower.Discrete

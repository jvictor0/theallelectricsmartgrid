import DSP.ResponseMeasurement.Approximation

/-! Averaging a fast carrier with a slowly changing amplitude. The correction
is explicit and finite. A nonconstant amplitude leaves a quantified residual;
its fast oscillation cannot simply be declared to average to zero. -/

noncomputable section
namespace DSP.ResponseMeasurement.SlowModulation

def potential (carrier : ℂ) (amplitude : ℕ → ℂ) (n : ℕ) : ℂ :=
  amplitude n*carrier^n/(1-carrier)

theorem residual_identity (carrier : ℂ) (hc : carrier ≠ 1) (amplitude : ℕ → ℂ) (n : ℕ) :
    amplitude n*carrier^n-(potential carrier amplitude n-potential carrier amplitude (n+1)) =
      (amplitude (n+1)-amplitude n)*carrier^(n+1)/(1-carrier) := by
  have hn : 1-carrier ≠ 0 := sub_ne_zero.mpr hc.symm
  unfold potential
  field_simp
  simp only [pow_succ]
  ring

theorem residual_bound (carrier : ℂ) (hc : carrier ≠ 1) (hu : ‖carrier‖ = 1)
    (amplitude : ℕ → ℂ) (variation : ℝ) (n : ℕ)
    (he : ‖amplitude (n+1)-amplitude n‖ ≤ variation) :
    ‖amplitude n*carrier^n-(potential carrier amplitude n-potential carrier amplitude (n+1))‖ ≤
      variation/‖1-carrier‖ := by
  rw [residual_identity carrier hc,norm_div,norm_mul,norm_pow,hu,one_pow,mul_one]
  exact div_le_div_of_nonneg_right he (norm_nonneg _)

theorem potential_bound (carrier : ℂ) (hu : ‖carrier‖ = 1)
    (amplitude : ℕ → ℂ) (bound : ℝ) (n : ℕ) (hb : ‖amplitude n‖ ≤ bound) :
    ‖potential carrier amplitude n‖ ≤ bound/‖1-carrier‖ := by
  rw [potential,norm_div,norm_mul,norm_pow,hu,one_pow,mul_one]
  exact div_le_div_of_nonneg_right hb (norm_nonneg _)

/-- A fast sideband's surviving average is bounded by amplitude variation
per sample divided by its distance from digital DC. Rational carriers are allowed. -/
theorem average_bound (carrier : ℂ) (hc : carrier ≠ 1) (hu : ‖carrier‖ = 1)
    (amplitude : ℕ → ℂ) (bound variation : ℝ)
    (hb : ∀ n, ‖amplitude n‖ ≤ bound)
    (he : ∀ n, ‖amplitude (n+1)-amplitude n‖ ≤ variation) :
    EventuallyNear (DSP.Coboundary.average (fun n => amplitude n*carrier^n)) 0
      (variation/‖1-carrier‖) := by
  apply eventuallyNear_of_coboundary _ (potential carrier amplitude) 0 _ (bound/‖1-carrier‖)
  · intro n
    simpa only [sub_zero] using residual_bound carrier hc hu amplitude variation n (he n)
  · exact fun n => potential_bound carrier hu amplitude bound n (hb n)

end DSP.ResponseMeasurement.SlowModulation

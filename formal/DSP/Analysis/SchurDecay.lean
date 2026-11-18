import Mathlib.Analysis.Normed.Algebra.GelfandFormula
import Mathlib.Analysis.SpecificLimits.Normed
import Mathlib.Analysis.Normed.Module.FiniteDimension
import Mathlib.LinearAlgebra.Eigenspace.Basic

/-! Convert strict spectral stability into time-domain estimates. -/

noncomputable section
namespace DSP.SchurDecay
open Filter
open scoped Topology

theorem exists_contracting_power {A : Type*} [NormedRing A] [NormedAlgebra ℂ A]
    [CompleteSpace A] [Nontrivial A] (a : A)
    (hs : ∀ z ∈ spectrum ℂ a, ‖z‖ < 1) :
    ∃ n : ℕ, 0 < n ∧ ‖a^n‖ < 1 := by
  have hr : spectralRadius ℂ a < 1 := by
    apply spectrum.spectralRadius_lt_of_forall_lt a (r := 1)
    intro z hz
    exact_mod_cast hs z hz
  have ht := spectrum.pow_norm_pow_one_div_tendsto_nhds_spectralRadius a
  have he := ht.eventually (gt_mem_nhds hr)
  obtain ⟨n, hn, hpow⟩ := (he.and (eventually_gt_atTop 0)).exists
  refine ⟨n, hpow, ?_⟩
  have hn' : ‖a^n‖ ^ (1/(n : ℝ)) < 1 := by simpa using hn
  have hn0 : 0 < (n : ℝ) := by exact_mod_cast hpow
  exact (Real.rpow_lt_one_iff' (norm_nonneg _) (by positivity)).mp hn'

end DSP.SchurDecay

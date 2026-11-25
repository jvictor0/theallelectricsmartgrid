import DSP.Filters.SquiggleBoyQuadReverb.Frozen.ContractBounds

namespace DSP.SquiggleBoyQuadReverb.Frozen

open Matrix
open scoped Matrix.Norms.L2Operator

noncomputable section

private theorem mix_entry (i j : Fin 4) :
    (LinearMap.toMatrix' (mix : Lanes ℂ →ₗ[ℂ] Lanes ℂ)) i j =
      squiggleBoyReverbMix (Pi.single j 1) i := by
  rw [LinearMap.toMatrix'_apply]
  rfl

private theorem mix_conjTranspose_mul_self :
    let A : QuadMatrix := LinearMap.toMatrix' mix
    Aᴴ * A = 1 := by
  dsimp only
  ext i j
  simp only [Matrix.mul_apply, Matrix.conjTranspose_apply, mix_entry]
  fin_cases i <;> fin_cases j <;>
    norm_num [Fin.sum_univ_four, Matrix.one_apply, squiggleBoyReverbMix, givensPair,
      Pi.single_apply, Matrix.cons_val_two, Matrix.cons_val_three,
      Matrix.vecHead, Matrix.vecTail, Function.comp_apply, Fin.ext_iff, map_ofNat]

/-- The runtime's rational Givens mixer preserves the Euclidean norm. -/
theorem norm_mixMatrix : ‖(LinearMap.toMatrix' (mix : Lanes ℂ →ₗ[ℂ] Lanes ℂ))‖ = 1 := by
  let A : QuadMatrix := LinearMap.toMatrix' mix
  have hu : Aᴴ * A = 1 := mix_conjTranspose_mul_self
  have h := Matrix.l2_opNorm_conjTranspose_mul_self A
  rw [hu, norm_one] at h
  change ‖A‖ = 1
  nlinarith [norm_nonneg A]

end

end DSP.SquiggleBoyQuadReverb.Frozen

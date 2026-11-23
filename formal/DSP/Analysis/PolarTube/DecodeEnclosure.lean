import DSP.Analysis.PolarTube.Decode
import DSP.Analysis.IntervalArithmetic

/-! Entrywise enclosures for the curved input chart and its Jacobian. These
are used to initialize centered block replay from the actual chart, retaining
the variation of both the Fourier profile and the polar direction. -/

namespace DSP.PolarTube
open LeanCert.Core DSP.IntervalArithmetic
open scoped Matrix

def decodeBox (p : Fin 3 → IntervalRat) (r : Fin 3 → IntervalRat)
    (cosine sine : IntervalRat) : Fin 4 → IntervalRat :=
  ![(p 0 |>.add (r 0)).mul cosine, (p 0 |>.add (r 0)).mul sine,
    (p 1).add (r 1), (p 2).add (r 2)]

def decodeJacobianBox (p dp : Fin 3 → IntervalRat) (r0 cosine sine : IntervalRat) :
    Fin 4 → Fin 4 → IntervalRat :=
  ![![(dp 0 |>.mul cosine).sub ((p 0 |>.add r0).mul sine), cosine,
        IntervalRat.singleton 0, IntervalRat.singleton 0],
    ![(dp 0 |>.mul sine).add ((p 0 |>.add r0).mul cosine), sine,
        IntervalRat.singleton 0, IntervalRat.singleton 0],
    ![dp 1, IntervalRat.singleton 0, IntervalRat.singleton 1, IntervalRat.singleton 0],
    ![dp 2, IntervalRat.singleton 0, IntervalRat.singleton 0, IntervalRat.singleton 1]]

noncomputable section

def decodeJacobian (profile : Real.Angle → Normal) (dprofile : ℝ → Normal) (x : State) :
    Matrix (Fin 4) (Fin 4) ℝ :=
  let θ := x 0
  let r := profile (θ:Real.Angle) 0+x 1
  ![![dprofile θ 0*Real.cos θ-r*Real.sin θ, Real.cos θ, 0, 0],
    ![dprofile θ 0*Real.sin θ+r*Real.cos θ, Real.sin θ, 0, 0],
    ![dprofile θ 1, 0, 1, 0], ![dprofile θ 2, 0, 0, 1]]

theorem decodeJacobian_mul (profile : Real.Angle → Normal) (dprofile : ℝ → Normal) (x v : State) :
    decodeJacobian profile dprofile x *ᵥ v = decodeVariation profile dprofile x v := by
  funext i
  fin_cases i <;> simp [decodeJacobian, decodeVariation, Matrix.mulVec, dotProduct, Fin.sum_univ_succ] <;> ring

theorem decodeBox_mem (profile : Real.Angle → Normal) (x : State)
    (p r : Fin 3 → IntervalRat) (cosine sine : IntervalRat)
    (hp : ∀ j, profile (x 0:Real.Angle) j ∈ p j)
    (hr : ∀ j : Fin 3, x j.succ ∈ r j)
    (hc : Real.cos (x 0) ∈ cosine) (hs : Real.sin (x 0) ∈ sine) :
    ∀ i, liftDecode profile x i ∈ decodeBox p r cosine sine i := by
  intro i
  fin_cases i
  · exact IntervalRat.mem_mul (IntervalRat.mem_add (hp 0) (hr 0)) hc
  · exact IntervalRat.mem_mul (IntervalRat.mem_add (hp 0) (hr 0)) hs
  · exact IntervalRat.mem_add (hp 1) (hr 1)
  · exact IntervalRat.mem_add (hp 2) (hr 2)

theorem decodeJacobianBox_mem (profile : Real.Angle → Normal) (dprofile : ℝ → Normal) (x : State)
    (p dp : Fin 3 → IntervalRat) (r0 cosine sine : IntervalRat)
    (hp : ∀ j, profile (x 0:Real.Angle) j ∈ p j) (hdp : ∀ j, dprofile (x 0) j ∈ dp j)
    (hr : x 1 ∈ r0) (hc : Real.cos (x 0) ∈ cosine) (hs : Real.sin (x 0) ∈ sine) :
    ∀ i j, decodeJacobian profile dprofile x i j ∈ decodeJacobianBox p dp r0 cosine sine i j := by
  have hz : (0:ℝ) ∈ IntervalRat.singleton 0 := by simpa using IntervalRat.mem_singleton 0
  have ho : (1:ℝ) ∈ IntervalRat.singleton 1 := by simpa using IntervalRat.mem_singleton 1
  have h0 := IntervalRat.mem_sub (IntervalRat.mem_mul (hdp 0) hc)
    (IntervalRat.mem_mul (IntervalRat.mem_add (hp 0) hr) hs)
  have h1 := IntervalRat.mem_add (IntervalRat.mem_mul (hdp 0) hs)
    (IntervalRat.mem_mul (IntervalRat.mem_add (hp 0) hr) hc)
  intro i j
  fin_cases i <;> fin_cases j
  all_goals first
    | exact h0
    | exact h1
    | exact hc
    | exact hs
    | exact hz
    | exact ho
    | exact hdp 1
    | exact hdp 2

theorem hasDerivAt_matrix_decode (profile : Real.Angle → Normal) (dprofile : ℝ → Normal)
    (inverse : Matrix (Fin 4) (Fin 4) ℝ) (x : ℝ → State) (t : ℝ) (v : State)
    (hx : ∀ j, HasDerivAt (fun r => x r j) (v j) t)
    (hp : ∀ j, HasDerivAt (fun θ : ℝ => profile (θ:Real.Angle) j) (dprofile (x t 0) j) (x t 0)) :
    ∀ i, HasDerivAt (fun r => (inverse *ᵥ liftDecode profile (x r)) i)
      (((inverse*decodeJacobian profile dprofile (x t)) *ᵥ v) i) t := by
  have h := hasDerivAt_liftDecode profile dprofile x t v hx hp
  intro i
  rw [← Matrix.mulVec_mulVec, decodeJacobian_mul]
  change HasDerivAt (fun r => ∑ j, inverse i j*liftDecode profile (x r) j)
    (∑ j, inverse i j*decodeVariation profile dprofile (x t) v j) t
  exact HasDerivAt.fun_sum (fun j _ => (h j).const_mul (inverse i j))

end
end DSP.PolarTube

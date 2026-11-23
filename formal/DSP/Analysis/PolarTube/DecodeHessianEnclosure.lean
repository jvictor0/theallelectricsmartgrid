import DSP.Analysis.PolarTube.DecodeEnclosure

/-! Initial Hessian boxes for the curved decode. Only phase/phase and
phase/radius entries are nonzero; the retained profile curvature is essential
for the cancellation in the transverse block derivatives. -/

namespace DSP.PolarTube
open LeanCert.Core DSP.IntervalArithmetic

abbrev HessianBox := Fin 4 → Fin 4 → Fin 4 → IntervalRat

def decodeHessianBox (p dp ddp : Fin 3 → IntervalRat) (r cosine sine : IntervalRat) : HessianBox :=
  let rho := (p 0).add r
  let diagonal : Fin 4 → IntervalRat :=
    ![(((ddp 0).mul cosine).sub (((dp 0).mul sine).scale 2)).sub (rho.mul cosine),
      (((ddp 0).mul sine).add (((dp 0).mul cosine).scale 2)).sub (rho.mul sine),ddp 1,ddp 2]
  let cross : Fin 4 → IntervalRat := ![sine.neg,cosine,IntervalRat.singleton 0,IntervalRat.singleton 0]
  fun i j k => if j=0 ∧ k=0 then diagonal i
    else if (j=0 ∧ k=1) ∨ (j=1 ∧ k=0) then cross i else IntervalRat.singleton 0

noncomputable section

def decodeHessian (profile : Real.Angle → Normal) (dp ddp : ℝ → Normal) (x : State) :
    Fin 4 → Fin 4 → Fin 4 → ℝ :=
  let θ := x 0
  let rho := profile (θ:Real.Angle) 0+x 1
  let diagonal : State :=
    ![ddp θ 0*Real.cos θ-2*(dp θ 0*Real.sin θ)-rho*Real.cos θ,
      ddp θ 0*Real.sin θ+2*(dp θ 0*Real.cos θ)-rho*Real.sin θ,ddp θ 1,ddp θ 2]
  let cross : State := ![-Real.sin θ,Real.cos θ,0,0]
  fun i j k => if j=0 ∧ k=0 then diagonal i
    else if (j=0 ∧ k=1) ∨ (j=1 ∧ k=0) then cross i else 0

theorem decodeMixedVariation_basis (profile : Real.Angle → Normal) (dp ddp : ℝ → Normal)
    (x : State) (i j k : Fin 4) :
    decodeMixedVariation profile dp ddp x (Pi.single j 1) (Pi.single k 1) 0 i =
      decodeHessian profile dp ddp x i j k := by
  fin_cases i <;> fin_cases j <;> fin_cases k
  all_goals norm_num [decodeMixedVariation,decodeHessian,Pi.single_apply]
  all_goals ring

theorem decodeHessianBox_mem (profile : Real.Angle → Normal) (dp ddp : ℝ → Normal)
    (x : State) (pb dpb ddpb : Fin 3 → IntervalRat) (r cosine sine : IntervalRat)
    (hp : ∀ j, profile (x 0:Real.Angle) j ∈ pb j)
    (hd : ∀ j, dp (x 0) j ∈ dpb j) (hdd : ∀ j, ddp (x 0) j ∈ ddpb j)
    (hr : x 1 ∈ r) (hc : Real.cos (x 0) ∈ cosine) (hs : Real.sin (x 0) ∈ sine) :
    ∀ i j k, decodeMixedVariation profile dp ddp x (Pi.single j 1) (Pi.single k 1) 0 i ∈
      decodeHessianBox pb dpb ddpb r cosine sine i j k := by
  have hz : (0:ℝ) ∈ IntervalRat.singleton 0 := by simpa using IntervalRat.mem_singleton 0
  have hρ := IntervalRat.mem_add (hp 0) hr
  have h00 := IntervalRat.mem_sub (IntervalRat.mem_sub (IntervalRat.mem_mul (hdd 0) hc)
    (IntervalRat.mem_scale 2 (IntervalRat.mem_mul (hd 0) hs))) (IntervalRat.mem_mul hρ hc)
  have h10 := IntervalRat.mem_sub (IntervalRat.mem_add (IntervalRat.mem_mul (hdd 0) hs)
    (IntervalRat.mem_scale 2 (IntervalRat.mem_mul (hd 0) hc))) (IntervalRat.mem_mul hρ hs)
  have hn := IntervalRat.mem_neg hs
  intro i j k
  rw [decodeMixedVariation_basis]
  simp only [decodeHessian,decodeHessianBox]
  split_ifs
  · fin_cases i
    · simpa using h00
    · simpa using h10
    · exact hdd 1
    · exact hdd 2
  · fin_cases i
    · exact hn
    · exact hc
    · exact hz
    · exact hz
  · exact hz

end
end DSP.PolarTube

import DSP.Filters.TPTSVF.Dynamics
import DSP.ResponseMeasurement.Tracking

noncomputable section
namespace DSP.TPTSVF
open Filter ResponseMeasurement
open scoped Topology

def readout {K : Type} [Field K] (s : TPTSVF K) : History K × K →ₗ[K] History K where
  toFun v := let f := step (withHistory s v.1) ⟨v.2⟩; ![f.lowPass,f.allPass]
  map_add' v w := by ext i; fin_cases i <;> simp [step,withHistory,fma_eq] <;> ring
  map_smul' c v := by ext i; fin_cases i <;> simp [step,withHistory,fma_eq] <;> ring

def sinusoidMode {K : Type} [Field K] (g k z : K) : History K :=
  ![2*g*(z-1)/digitalDenominator g k z,2*g^2*(z+1)/digitalDenominator g k z]

def modeResponse {K : Type} [Field K] (g k z : K) : History K :=
  let s : Snapshot K := ⟨g,k⟩
  ![s.lowPassTransfer (z-1) (z+1),s.allPassTransfer (z-1) (z+1)]

theorem sinusoidMode_step {K : Type} [Field K] (s : TPTSVF K) (g z : K) (ht : s.Tuned g)
    (hd : digitalDenominator g s.k z ≠ 0) :
    historyUpdate s (sinusoidMode g s.k z,1) = z • sinusoidMode g s.k z := by
  have hden : 1+g*(g+s.k) ≠ 0 := by
    intro h
    have hs := ht.solve
    rw [h,mul_zero] at hs
    exact zero_ne_one hs
  ext i
  fin_cases i <;> change _ = z * _
  all_goals
    simp [historyUpdate,LinearMap.coe_mk,AddHom.coe_mk,histories,step,withHistory,
      fma_eq,sinusoidMode,Matrix.cons_val_zero,Matrix.cons_val_one]
    simp only [ht.a3,ht.a2,ht.a1_eq]
    field_simp [hd,hden]
    dsimp [digitalDenominator]
    ring

theorem modeResponse_eq {K : Type} [Field K] (g k z : K) :
    modeResponse g k z = ![g^2*(z+1)^2/digitalDenominator g k z,
      ((z-1)^2-k*g*(z-1)*(z+1)+g^2*(z+1)^2)/digitalDenominator g k z] := by
  ext i
  fin_cases i <;> simp [modeResponse,Snapshot.lowPassTransfer,Snapshot.allPassTransfer,
    Snapshot.weightedTransfer,Response.svf]
  all_goals
    congr 1 <;> (try dsimp [digitalDenominator]) <;> ring

theorem sinusoidMode_readout {K : Type} [Field K] (s : TPTSVF K) (g z : K) (ht : s.Tuned g)
    (hd : digitalDenominator g s.k z ≠ 0) :
    readout s (sinusoidMode g s.k z,1) = modeResponse g s.k z := by
  have hden : 1+g*(g+s.k) ≠ 0 := by
    intro h
    have hs := ht.solve
    rw [h,mul_zero] at hs
    exact zero_ne_one hs
  rw [modeResponse_eq]
  ext i
  fin_cases i
  all_goals
    simp [readout,step,withHistory,fma_eq,sinusoidMode]
    simp only [ht.a3,ht.a2,ht.a1_eq]
    field_simp [hd,hden]
    dsimp [digitalDenominator]
    ring

def realHistory : History ℂ →ₗ[ℝ] History ℝ where
  toFun x := fun i => (x i).re
  map_add' _ _ := by ext; simp
  map_smul' _ _ := by ext; simp

theorem realHistory_update (s : TPTSVF ℝ) (x : History ℂ) (u : ℂ) :
    realHistory (historyUpdate (complexCoefficients s) (x,u)) =
      historyUpdate s (realHistory x,u.re) := by
  ext i
  fin_cases i <;> simp [realHistory,historyUpdate,complexCoefficients,histories,step,
    withHistory,fma_eq,Complex.mul_re]

theorem realHistory_readout (s : TPTSVF ℝ) (x : History ℂ) (u : ℂ) :
    realHistory (readout (complexCoefficients s) (x,u)) = readout s (realHistory x,u.re) := by
  ext i
  fin_cases i <;> simp [realHistory,readout,complexCoefficients,step,withHistory,fma_eq,Complex.mul_re]

theorem histories_trajectory (s : TPTSVF ℝ) (u : ℕ → ℝ) (n : ℕ) :
    histories (trajectory s u n) = LocalDynamics.orbit (historyUpdate s).toContinuousLinearMap u (histories s) n := by
  induction n with
  | zero => rfl
  | succ n ih =>
    rw [LocalDynamics.orbit,← ih]
    have hc := states_coefficients s u n
    ext i
    fin_cases i <;> simp [trajectory,Step.states_succ,historyUpdate,histories,step,withHistory,
      hc.2.1,hc.2.2.1,hc.2.2.2]

theorem trajectory_readout (s : TPTSVF ℝ) (u : ℕ → ℝ) (n : ℕ) :
    ![(trajectory s u (n+1)).lowPass,(trajectory s u (n+1)).allPass] =
      readout s (histories (trajectory s u n),u n) := by
  have hc := states_coefficients s u n
  ext i
  fin_cases i <;> simp [trajectory,Step.states_succ,readout,histories,step,withHistory,
    hc.1,hc.2.1,hc.2.2.1,hc.2.2.2]

theorem settles_readout (s : TPTSVF ℝ) (g : ℝ) (ht : s.Tuned g) (hg : 0 < g) (hk : 0 < s.k)
    (z : ℂ) (hz : ‖z‖=1) (i : Fin 2) :
    Settles step s (fun v => (![v.lowPass,v.allPass] : History ℝ) i) z
      (modeResponse (g:ℂ) (s.k:ℂ) z i) := by
  intro u A hu
  have hd : digitalDenominator (g:ℂ) (s.k:ℂ) z ≠ 0 := by
    intro h
    have hlt := digital_roots_lt_one g s.k hg hk z h
    rw [hz] at hlt
    exact lt_irrefl 1 hlt
  have ht' : (complexCoefficients s).Tuned (g:ℂ) := by
    constructor
    · change (s.a1:ℂ)*(1+(g:ℂ)*(g+s.k))=1
      exact_mod_cast ht.solve
    · change (s.a2:ℂ)=(g:ℂ)*s.a1
      exact_mod_cast ht.a2
    · change (s.a3:ℂ)=(g:ℂ)*s.a2
      exact_mod_cast ht.a3
  let mode := sinusoidMode (g:ℂ) (s.k:ℂ) z
  let ref := fun n => realHistory ((A*z^n) • mode)
  have href (n : ℕ) : historyUpdate s (ref n,(A*z^n).re)=ref (n+1) := by
    rw [← realHistory_update]
    have he : ((A*z^n) • mode,A*z^n) = (A*z^n) • (mode,(1:ℂ)) := by simp
    rw [he,map_smul]
    have hm : historyUpdate (complexCoefficients s) (mode,1) = z • mode :=
      sinusoidMode_step (complexCoefficients s) (g:ℂ) z ht' hd
    rw [hm]
    simp only [ref,smul_smul,pow_succ,mul_assoc]
  have hstate := LocalDynamics.linear_tracks_perturbed (historyUpdate s).toContinuousLinearMap
    (historyGauge s g ht hg hk) u (fun n => (A*z^n).re) (histories s) ref href hu
  have hpair := hstate.prodMk_nhds hu
  have ho := ((readout s).toContinuousLinearMap.continuous.tendsto 0).comp hpair
  simp only [map_zero,Function.comp_def] at ho
  have hout (n : ℕ) : readout s (ref n,(A*z^n).re)=
      realHistory ((A*z^n) • modeResponse (g:ℂ) (s.k:ℂ) z) := by
    rw [← realHistory_readout]
    have he : ((A*z^n) • mode,A*z^n) = (A*z^n) • (mode,(1:ℂ)) := by simp
    rw [he,map_smul]
    have hm : readout (complexCoefficients s) (mode,1) = modeResponse (g:ℂ) (s.k:ℂ) z :=
      sinusoidMode_readout (complexCoefficients s) (g:ℂ) z ht' hd
    rw [hm]
  have hi := (continuous_apply i).tendsto 0 |>.comp ho
  change Tracks z _ _
  apply hi.congr
  intro n
  dsimp only [Function.comp_def]
  rw [← histories_trajectory,← Prod.mk_sub_mk,map_sub]
  change (readout s (histories (trajectory s u n),u n) i)-readout s (ref n,(A*z^n).re) i = _
  rw [hout]
  rw [← trajectory_readout]
  simp only [realHistory,LinearMap.coe_mk,AddHom.coe_mk,Pi.smul_apply,smul_eq_mul,observed,trajectory]
  congr 2
  ring

end DSP.TPTSVF

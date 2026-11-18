import DSP.Filters.ConservativeReuppingLadder
import DSP.Filters.OnePoleZOHLowPass.Transfer

/-! Exact transfer of the tangent recurrence, with the actual feedback delay.
Linearization.lean connects this recurrence to derivatives of the nonlinear
processor. This file does not assert that the nonlinear filter is LTI. -/

noncomputable section
namespace DSP.ConservativeReuppingLadder
open Bilinear

def linearStep {K : Type} [Field K] (s : ConservativeReuppingLadder K) (x : Sample K) :
    ConservativeReuppingLadder K := stepWith (fun v => NormalizedTanh.slopeZero s.saturation * v) s x

/-- q denotes a unit delay, not a bilinear/analog variable. -/
def transfer {K : Type} [Field K] (alpha slope k q : K) : K :=
  ((1+k)*slope^4*alpha^4) / ((1-(1-alpha)*q)^4+k*slope^3*alpha^4*q)

def AtRest {K : Type} [Zero K] (s : ConservativeReuppingLadder K) : Prop :=
  s.stage1 = 0 ∧ s.stage2 = 0 ∧ s.stage3 = 0 ∧ s.stage4 = 0

theorem linear_coefficients {K : Type} [Field K] (s : ConservativeReuppingLadder K)
    (u : ℕ → K) (n : ℕ) :
    let st := Step.states linearStep (fun i => ⟨u i⟩) s n
    st.alpha = s.alpha ∧ st.k = s.k ∧ st.saturation = s.saturation := by
  dsimp only
  induction n with
  | zero => exact ⟨rfl,rfl,rfl⟩
  | succ n ih => exact ih

/-- Stream elimination for the shared Euler primitive, retaining its updated
state observation. This helper also applies to the internally driven stages. -/
theorem euler_stream {K : Type} [Field K] (alpha : K) (history input : ℕ → K)
    (hz : history 0 = 0)
    (hstep : ∀ n, history (n+1) = OnePoleZOHLowPass.onePoleStep alpha (input n) (history n)) :
    (1-const (1-alpha)*delay)*ser (fun n => history (n+1)) = const alpha*ser input := by
  have h := ser_congr hstep
  simp only [OnePoleZOHLowPass.onePoleStep_eq, fma_eq] at h
  rw [ser_add, ser_mul, ser_mul] at h
  have hs := ser_shift history
  rw [hz, const_zero, zero_add] at hs
  rw [hs] at h
  linear_combination h

theorem realizes_linear {K : Type} [Field K] (s : ConservativeReuppingLadder K) (hs : AtRest s) :
    Realizes linearStep s (·.output)
      (transfer (const s.alpha) (const (NormalizedTanh.slopeZero s.saturation)) (const s.k) delay) := by
  intro u
  let st := Step.states linearStep (fun i => ⟨u i⟩) s
  let slope := NormalizedTanh.slopeZero s.saturation
  let a := const s.alpha
  let p := const (1-s.alpha)
  let c := const slope
  let k := const s.k
  have hc := linear_coefficients s u
  have h1 := euler_stream s.alpha (fun n => (st n).stage1)
    (fun n => u n-s.k*(st n).stage4) hs.1 (fun n => by
      simp only [st, Step.states_succ, linearStep, stepWith, (hc n).1, (hc n).2.1])
  have h2 := euler_stream s.alpha (fun n => (st n).stage2)
    (fun n => slope*(st (n+1)).stage1) hs.2.1 (fun n => by
      change OnePoleZOHLowPass.onePoleStep (st n).alpha
        (NormalizedTanh.slopeZero (st n).saturation * (st (n+1)).stage1) (st n).stage2 = _
      rw [(hc n).1, (hc n).2.2])
  have h3 := euler_stream s.alpha (fun n => (st n).stage3)
    (fun n => slope*(st (n+1)).stage2) hs.2.2.1 (fun n => by
      change OnePoleZOHLowPass.onePoleStep (st n).alpha
        (NormalizedTanh.slopeZero (st n).saturation * (st (n+1)).stage2) (st n).stage3 = _
      rw [(hc n).1, (hc n).2.2])
  have h4 := euler_stream s.alpha (fun n => (st n).stage4)
    (fun n => slope*(st (n+1)).stage3) hs.2.2.2 (fun n => by
      change OnePoleZOHLowPass.onePoleStep (st n).alpha
        (NormalizedTanh.slopeZero (st n).saturation * (st (n+1)).stage3) (st n).stage4 = _
      rw [(hc n).1, (hc n).2.2])
  rw [ser_mul] at h2 h3 h4
  rw [ser_sub, ser_mul] at h1
  have hdelay := ser_shift (fun n => (st n).stage4)
  change ser (fun n => (st n).stage4) = const s.stage4 + _ at hdelay
  rw [hs.2.2.2, const_zero, zero_add] at hdelay
  rw [hdelay] at h1
  have hout : ser (response linearStep u s (·.output)) =
      c*(k+1)*ser (fun n => (st (n+1)).stage4) := by
    have he (n : ℕ) : response linearStep u s (·.output) n =
        (slope*(s.k+1))*(st (n+1)).stage4 := by
      change (NormalizedTanh.slopeZero (st n).saturation * (st (n+1)).stage4)*((st n).k+1) = _
      rw [(hc n).2.1, (hc n).2.2]
      dsimp only [slope]
      ring
    rw [ser_congr he, ser_mul]
    simp [c, k, const]
  have hd : (1-p*delay)^4+k*c^3*a^4*delay ≠ 0 := by
    have he := embed_ne_zero ((1-PowerSeries.C (1-s.alpha)*PowerSeries.X)^4+
      PowerSeries.C s.k * PowerSeries.C slope^3 * PowerSeries.C s.alpha^4 * PowerSeries.X)
      (by simp)
    simpa [a,p,c,k,const,delay] using he
  have he : ((1-p*delay)^4+k*c^3*a^4*delay)*ser (fun n => (st (n+1)).stage4) =
      c^3*a^4*ser u := by
    linear_combination (1-p*delay)^3*h4 + a*c*(1-p*delay)^2*h3 +
      (a*c)^2*(1-p*delay)*h2 + (a*c)^3*h1
  rw [hout]
  change c*(k+1)*_ = transfer a c k delay * ser u
  have hp : 1-a = p := by simp [a,p,const]
  rw [transfer, hp, div_mul_eq_mul_div, eq_div_iff hd]
  linear_combination c*(k+1)*he

/-- Every nonzero homogeneous history mode satisfies the same quartic.
Thus the denominator is also tied to internal state modes, not merely to an
input/output identity that might hide unstable cancelled modes. -/
theorem characteristic_of_mode {K : Type} [Field K] (s : ConservativeReuppingLadder K) (z : K)
    (h1 : (linearStep s ⟨0⟩).stage1 = z*s.stage1)
    (h2 : (linearStep s ⟨0⟩).stage2 = z*s.stage2)
    (h3 : (linearStep s ⟨0⟩).stage3 = z*s.stage3)
    (h4 : (linearStep s ⟨0⟩).stage4 = z*s.stage4)
    (hn : s.stage1 ≠ 0 ∨ s.stage2 ≠ 0 ∨ s.stage3 ≠ 0 ∨ s.stage4 ≠ 0) :
    (z-(1-s.alpha))^4+s.k*NormalizedTanh.slopeZero s.saturation^3*s.alpha^4*z^3 = 0 := by
  let l := z-(1-s.alpha)
  let b := s.alpha*NormalizedTanh.slopeZero s.saturation*z
  let a := s.alpha*s.k
  have e1 : l*s.stage1+a*s.stage4 = 0 := by
    simp only [linearStep,stepWith,OnePoleZOHLowPass.onePoleStep_eq,fma_eq] at h1
    dsimp only [l,a]
    linear_combination -h1
  have e2 : l*s.stage2-b*s.stage1 = 0 := by
    change OnePoleZOHLowPass.onePoleStep s.alpha
      (NormalizedTanh.slopeZero s.saturation*(linearStep s ⟨0⟩).stage1) s.stage2 = _ at h2
    rw [h1] at h2
    simp only [OnePoleZOHLowPass.onePoleStep_eq,fma_eq] at h2
    dsimp only [l,b]
    linear_combination -h2
  have e3 : l*s.stage3-b*s.stage2 = 0 := by
    change OnePoleZOHLowPass.onePoleStep s.alpha
      (NormalizedTanh.slopeZero s.saturation*(linearStep s ⟨0⟩).stage2) s.stage3 = _ at h3
    rw [h2] at h3
    simp only [OnePoleZOHLowPass.onePoleStep_eq,fma_eq] at h3
    dsimp only [l,b]
    linear_combination -h3
  have e4 : l*s.stage4-b*s.stage3 = 0 := by
    change OnePoleZOHLowPass.onePoleStep s.alpha
      (NormalizedTanh.slopeZero s.saturation*(linearStep s ⟨0⟩).stage3) s.stage4 = _ at h4
    rw [h3] at h4
    simp only [OnePoleZOHLowPass.onePoleStep_eq,fma_eq] at h4
    dsimp only [l,b]
    linear_combination -h4
  have eqn : l^4+a*b^3 = 0 := by
    rcases hn with hn|hn|hn|hn
    · apply (mul_eq_zero.mp (show (l^4+a*b^3)*s.stage1 = 0 by
        linear_combination l^3*e1-a*l^2*e4-a*b*l*e3-a*b^2*e2)).resolve_right hn
    · apply (mul_eq_zero.mp (show (l^4+a*b^3)*s.stage2 = 0 by
        linear_combination l^3*e2+b*l^2*e1-a*b*l*e4-a*b^2*e3)).resolve_right hn
    · apply (mul_eq_zero.mp (show (l^4+a*b^3)*s.stage3 = 0 by
        linear_combination l^3*e3+b*l^2*e2+b^2*l*e1-a*b^2*e4)).resolve_right hn
    · apply (mul_eq_zero.mp (show (l^4+a*b^3)*s.stage4 = 0 by
        linear_combination l^3*e4+b*l^2*e3+b^2*l*e2+b^3*e1)).resolve_right hn
  dsimp only [l,a,b] at eqn
  linear_combination eqn

end DSP.ConservativeReuppingLadder

import DSP.Filters.ConservativeReuppingLadder.Uniform
import DSP.ResponseMeasurement.Uniform

/-! Time-domain calibration of the tangent transfer on real sinusoids. -/

noncomputable section
namespace DSP.ConservativeReuppingLadder
open Filter
open scoped Topology

def realHistory : History ℂ →ₗ[ℝ] History ℝ where
  toFun x := fun i => (x i).re
  map_add' _ _ := by ext; simp
  map_smul' _ _ := by ext; simp

theorem realHistory_update (s : ConservativeReuppingLadder ℝ) (x : History ℂ) (u : ℂ) :
    realHistory (linearUpdate (complexCoefficients s) (x,u)) = linearUpdate s (realHistory x,u.re) := by
  ext i
  fin_cases i <;> simp [realHistory,linearUpdate,complexCoefficients,histories,linearStep,
    stepWith,withHistory,OnePoleZOHLowPass.onePoleStep_eq,fma_eq,NormalizedTanh.slopeZero,
    Complex.mul_re,Complex.sub_re,Complex.add_re]

def sinusoidMode (s : ConservativeReuppingLadder ℝ) (z : ℂ) : History ℂ :=
  let a : ℂ := s.alpha
  let b : ℂ := (NormalizedTanh.slopeZero s.saturation : ℝ)
  let d := z-(1-a)
  let den := characteristic s.alpha (s.k*NormalizedTanh.slopeZero s.saturation^3) z
  ![a*d^3/den,b*a^2*z*d^2/den,b^2*a^3*z^2*d/den,b^3*a^4*z^3/den]

theorem sinusoidMode_step (s : ConservativeReuppingLadder ℝ) (z : ℂ)
    (hd : characteristic s.alpha (s.k*NormalizedTanh.slopeZero s.saturation^3) z ≠ 0) :
    linearUpdate (complexCoefficients s) (sinusoidMode s z,1) = z • sinusoidMode s z := by
  let a : ℂ := s.alpha
  let b : ℂ := (NormalizedTanh.slopeZero s.saturation : ℝ)
  let k : ℂ := s.k
  let den := characteristic s.alpha (s.k*NormalizedTanh.slopeZero s.saturation^3) z
  let v := sinusoidMode s z
  have hd' : den ≠ 0 := hd
  have he : den = (z-(1-a))^4+k*b^3*a^4*z^3 := by
    simp [den,characteristic,a,b,k]
  have hv : v = ![a*(z-(1-a))^3/den,b*a^2*z*(z-(1-a))^2/den,
      b^2*a^3*z^2*(z-(1-a))/den,b^3*a^4*z^3/den] := rfl
  have h1 : a*(1-k*v 3)+(1-a)*v 0 = z*v 0 := by
    rw [hv]
    change a*(1-k*(b^3*a^4*z^3/den))+(1-a)*(a*(z-(1-a))^3/den) = z*(a*(z-(1-a))^3/den)
    field_simp [hd']
    rw [he]
    ring
  have h2 : a*(b*(z*v 0))+(1-a)*v 1 = z*v 1 := by
    rw [hv]
    change a*(b*(z*(a*(z-(1-a))^3/den)))+(1-a)*(b*a^2*z*(z-(1-a))^2/den) =
      z*(b*a^2*z*(z-(1-a))^2/den)
    field_simp [hd']
    ring
  have h3 : a*(b*(z*v 1))+(1-a)*v 2 = z*v 2 := by
    rw [hv]
    change a*(b*(z*(b*a^2*z*(z-(1-a))^2/den)))+(1-a)*(b^2*a^3*z^2*(z-(1-a))/den) =
      z*(b^2*a^3*z^2*(z-(1-a))/den)
    field_simp [hd']
    ring
  have h4 : a*(b*(z*v 2))+(1-a)*v 3 = z*v 3 := by
    rw [hv]
    change a*(b*(z*(b^2*a^3*z^2*(z-(1-a))/den)))+(1-a)*(b^3*a^4*z^3/den) = z*(b^3*a^4*z^3/den)
    field_simp [hd']
    ring
  have hb : NormalizedTanh.slopeZero (complexCoefficients s).saturation = b := by
    simp [complexCoefficients,b,NormalizedTanh.slopeZero]
  ext i
  fin_cases i <;> simp only [linearUpdate,histories,linearStep,stepWith,withHistory,
    OnePoleZOHLowPass.onePoleStep_eq,fma_eq,hb]
  · exact h1
  · change a*(b*(a*(1-k*v 3)+(1-a)*v 0))+(1-a)*v 1 = _
    rw [h1]
    exact h2
  · change a*(b*(a*(b*(a*(1-k*v 3)+(1-a)*v 0))+(1-a)*v 1))+(1-a)*v 2 = _
    rw [h1,h2]
    exact h3
  · change a*(b*(a*(b*(a*(b*(a*(1-k*v 3)+(1-a)*v 0))+(1-a)*v 1))+(1-a)*v 2))+(1-a)*v 3 = _
    rw [h1,h2,h3]
    exact h4

def sinusoidReference (s : ConservativeReuppingLadder ℝ) (z : ℂ) (n : ℕ) : History ℝ :=
  realHistory (z^n • sinusoidMode s z)

theorem sinusoidReference_step (s : ConservativeReuppingLadder ℝ) (z : ℂ)
    (hd : characteristic s.alpha (s.k*NormalizedTanh.slopeZero s.saturation^3) z ≠ 0) (n : ℕ) :
    linearUpdate s (sinusoidReference s z n,(z^n).re) = sinusoidReference s z (n+1) := by
  have hp : (z^n • sinusoidMode s z,z^n) = z^n • (sinusoidMode s z,(1:ℂ)) := by simp
  rw [sinusoidReference,← realHistory_update,hp,map_smul,sinusoidMode_step s z hd]
  simp only [sinusoidReference,smul_smul,pow_succ]

theorem sinusoidReference_output (s : ConservativeReuppingLadder ℝ) (z : ℂ) (hz : z ≠ 0) (n : ℕ) :
    linearObservation s (sinusoidReference s z (n+1)) =
      (transfer (s.alpha:ℂ) ((NormalizedTanh.slopeZero s.saturation:ℝ):ℂ) (s.k:ℂ) z⁻¹ * z^n).re := by
  let a : ℂ := s.alpha
  let b : ℂ := (NormalizedTanh.slopeZero s.saturation : ℝ)
  let k : ℂ := s.k
  let den := characteristic s.alpha (s.k*NormalizedTanh.slopeZero s.saturation^3) z
  have he : transfer a b k z⁻¹ = (k+1)*b^4*a^4*z^4/den := by
    have heq : (1-(1-a)*z⁻¹)^4+k*b^3*a^4*z⁻¹ = den/z^4 := by
      dsimp only [den,characteristic,a,b,k]
      push_cast
      field_simp
    rw [transfer,heq]
    field_simp
    ring
  rw [he]
  change (s.k+1)*NormalizedTanh.slopeZero s.saturation *
    (z^(n+1)*(sinusoidMode s z 3)).re = _
  have hc : (s.k+1)*NormalizedTanh.slopeZero s.saturation *
      (z^(n+1)*(sinusoidMode s z 3)).re =
      (((k+1)*b)* (z^(n+1)*(sinusoidMode s z 3))).re := by
    dsimp only [k,b]
    simp [Complex.mul_re]
  rw [hc]
  congr 1
  simp [sinusoidMode,pow_succ,a,b,k,den]
  ring

theorem prepared_linear_tracks (spec : LogicalSpec ℝ) (s : ConservativeReuppingLadder ℝ)
    (hc : 0 < spec.cutoff) (hr0 : 0 ≤ spec.resonance) (hr1 : spec.resonance ≤ 1)
    (hg : 0 < spec.gain) (z : ℂ) (hz : ‖z‖ = 1) :
    let f := computeCoefficients spec s
    Tendsto (fun n => (linearTrajectory (reset f) (fun i => (z^i).re) (n+1)).output-
      (transfer (f.alpha:ℂ) ((NormalizedTanh.slopeZero f.saturation:ℝ):ℂ) (f.k:ℂ) z⁻¹*z^n).re)
      atTop (𝓝 0) := by
  dsimp only
  let f := computeCoefficients spec s
  have hz0 : z ≠ 0 := by intro h; simp [h] at hz
  have hd : characteristic f.alpha (f.k*NormalizedTanh.slopeZero f.saturation^3) z ≠ 0 := by
    intro h
    have ht := prepared_roots_lt_one spec s hc hr0 hr1 hg z h
    rw [hz] at ht
    exact lt_irrefl 1 ht
  have ht := LocalDynamics.linear_tracks (linearUpdate f).toContinuousLinearMap
    (preparedGauge spec s hc hr0 hr1 hg) (fun n => (z^n).re) 0 (sinusoidReference f z)
    (sinusoidReference_step f z hd)
  have ho := ((linearObservation f).continuous.tendsto 0).comp ht
  simp only [Function.comp_def,map_sub,map_zero] at ho
  have hs := ho.comp (tendsto_add_atTop_nat 1)
  simpa only [Function.comp_def,← linearOutput_eq_observation,sinusoidReference_output f z hz0] using hs

end DSP.ConservativeReuppingLadder

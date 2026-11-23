import DSP.Analysis.PhasePower.Approximation
import DSP.ResponseMeasurement.FiniteSpectrum

/-! The concrete first-order driven-orbit approximation used by the research
probe. Data come from a phase-fixed autonomous HB orbit and its two forced
first variations, not from a fit to measured response. Their validity for the
full ladder is not asserted by merely packaging them in this structure.
Both observables are quadratic polynomials of the mismatch phase; this makes
the constructed discrete averaging bounds applicable. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance.DrivenApproximation
open DSP.PhasePower.Discrete DSP.ResponseMeasurement

structure Data where
  orbitPhasor : ℂ
  cosPhasor : ℂ
  sinPhasor : ℂ
  orbitPower : ℝ
  crossCos : ℝ
  crossSin : ℝ
  cosPower : ℝ
  sinPower : ℝ
  cosSinPower : ℝ

def coherentPolynomial (d : Data) (A : ℝ) : Quadratic :=
  ⟨(A:ℂ)/2*(d.cosPhasor+Complex.I*d.sinPhasor),d.orbitPhasor,
    (A:ℂ)/2*(d.cosPhasor-Complex.I*d.sinPhasor)⟩

def powerPolynomial (d : Data) (A : ℝ) : Quadratic :=
  ⟨(d.orbitPower+A^2/2*(d.cosPower+d.sinPower):ℝ),
    2*(A:ℂ)*((d.crossCos:ℂ)-Complex.I*(d.crossSin:ℂ)),
    (A:ℂ)^2/2*((d.cosPower-d.sinPower:ℝ):ℂ)-Complex.I*(A:ℂ)^2*(d.cosSinPower:ℂ)⟩

def prediction (d : Data) (A : ℝ) (moment : ℂ) : Prediction :=
  ⟨(coherentPolynomial d A).eval moment,((powerPolynomial d A).eval moment).re⟩

/-- Canonical phase convention: drift = detuning + width*sin(phase).
Locked and running formulas use the same phase origin. A physical phase
orientation can be applied to the polynomial coefficients before evaluation. -/
def phaseMoment (ratio : ℝ) : ℂ :=
  if |ratio| ≤ 1 then DSP.PhasePower.lockedMoment ratio
  else if 0 ≤ ratio then center (DSP.PhasePower.runningCoherence ratio)
  else star (center (DSP.PhasePower.runningCoherence (-ratio)))

def curve (d : Data) (amplitude detuning width : ℝ) : Prediction :=
  prediction d amplitude (phaseMoment (detuning/width))

/-- Prepared by the autonomous HB orbit and the bordered cosine/sine
first-variation solves. Frequencies here are radians per engine sample. -/
structure Prepared where
  data : Data
  harmonic : ℕ
  oscillatorFrequency : ℝ
  frequencyCos : ℝ
  frequencySin : ℝ

def Prepared.sensitivity (p : Prepared) : ℂ := ⟨p.frequencyCos,p.frequencySin⟩
def Prepared.orientation (p : Prepared) : ℂ := p.sensitivity/(‖p.sensitivity‖:ℂ)
def Prepared.ratio (p : Prepared) (amplitude inputFrequency : ℝ) : ℝ :=
  (inputFrequency/(p.harmonic:ℝ)-p.oscillatorFrequency)/(amplitude*‖p.sensitivity‖)

/-- Physical phase convention of the existing HB derivative solve. The
conjugation accounts for the direction of input/oscillator phase mismatch. -/
def physicalMoment (orientation : ℂ) (ratio : ℝ) : ℂ :=
  -Complex.I*orientation*star (phaseMoment ratio)

/-- A concrete frequency/amplitude -> response formula once the HB orbit
and first variations are prepared. Requires harmonic > 0, amplitude > 0,
and nonzero sensitivity for its intended interpretation. -/
def Prepared.response (p : Prepared) (amplitude inputFrequency : ℝ) : Prediction :=
  prediction p.data amplitude (physicalMoment p.orientation (p.ratio amplitude inputFrequency))

def canonicalPolynomial (q : Quadratic) (orientation : ℂ) : Quadratic :=
  let turn := -Complex.I*orientation
  ⟨star q.constant,star (q.linear*turn),star (q.quadratic*turn^2)⟩

theorem canonicalPolynomial_eval (q : Quadratic) (orientation z : ℂ) :
    star ((canonicalPolynomial q orientation).eval z) = q.eval (-Complex.I*orientation*star z) := by
  simp only [canonicalPolynomial,Quadratic.eval,star_add,star_mul,star_pow,star_star]
  ring

theorem physicalMoment_locked (orientation : ℂ) (r : ℝ) (hr : |r| ≤ 1) :
    physicalMoment orientation r = orientation*((r:ℂ)+Complex.I*(Real.sqrt (1-r^2):ℂ)) := by
  simp only [physicalMoment,phaseMoment,if_pos hr,DSP.PhasePower.lockedMoment,
    star_sub,star_neg,star_mul,Complex.star_def,Complex.conj_ofReal,Complex.conj_I]
  ring_nf
  simp only [Complex.I_sq]
  ring

theorem physicalMoment_running (orientation : ℂ) (r : ℝ) (hr : 1 < r) :
    physicalMoment orientation r = orientation*(DSP.PhasePower.runningCoherence r:ℂ) := by
  have hr0 : 0 < r := by linarith
  simp only [physicalMoment,phaseMoment,abs_of_pos hr0,if_neg (not_le.mpr hr),
    if_pos hr0.le,center,DSP.PhasePower.runningMoment,star_mul,star_neg,
    Complex.star_def,Complex.conj_ofReal,Complex.conj_I]
  ring_nf
  simp only [Complex.I_sq]
  ring

/-- The explicit physical-convention curve has a proved sampled-phase
error, with the same observable coefficients as the HB proposal. This is a
theorem about the reduced phase trajectory, not an assertion that the full
ladder follows that trajectory. -/
theorem physical_running_curve (q : Quadratic) (orientation : ℂ) (r : ℝ) (hr : 1 < r)
    (N : ℕ) (h : ℝ) (hh : 0 < h) (initial : ℝ) :
    EventuallyNear (DSP.Coboundary.average (fun n => q.eval
      (-Complex.I*orientation*star (rotation (trajectory (DSP.PhasePower.runningCoherence r) h initial n)))))
      (q.eval (physicalMoment orientation r))
      (samplingError (canonicalPolynomial q orientation) (DSP.PhasePower.runningCoherence r) N h) := by
  have ht := running_curve (canonicalPolynomial q orientation) r hr N h hh initial
  have hm : physicalMoment orientation r = -Complex.I*orientation*
      star (DSP.PhasePower.runningMoment (DSP.PhasePower.runningCoherence r)) := by
    have hr0 : 0 < r := by linarith
    simp only [physicalMoment,phaseMoment,abs_of_pos hr0,if_neg (not_le.mpr hr),if_pos hr0.le,center]
  have havg (f : ℕ → ℂ) (count : ℕ) :
      DSP.Coboundary.average (fun n => star (f n)) count = star (DSP.Coboundary.average f count) := by
    simp only [DSP.Coboundary.average,star_smul,star_sum,star_trivial]
  intro slack hs
  filter_upwards [ht slack hs] with count hcount
  have he (z : ℂ) : q.eval (-Complex.I*orientation*star z) =
      star ((canonicalPolynomial q orientation).eval z) := (canonicalPolynomial_eval q orientation z).symm
  simp only [hm,he,havg,← star_sub,norm_star]
  exact hcount

/-- Real-part projection gives the actual scalar power-observer bound. -/
theorem physical_running_real_curve (q : Quadratic) (orientation : ℂ) (r : ℝ) (hr : 1 < r)
    (N : ℕ) (h : ℝ) (hh : 0 < h) (initial : ℝ) :
    EventuallyNear (DSP.Coboundary.average (fun n => (q.eval
      (-Complex.I*orientation*star (rotation (trajectory (DSP.PhasePower.runningCoherence r) h initial n)))).re))
      (q.eval (physicalMoment orientation r)).re
      (samplingError (canonicalPolynomial q orientation) (DSP.PhasePower.runningCoherence r) N h) := by
  have ht := physical_running_curve q orientation r hr N h hh initial
  intro slack hs
  filter_upwards [ht slack hs] with count hcount
  change |DSP.Coboundary.average _ count-(q.eval (physicalMoment orientation r)).re| ≤ _
  rw [FiniteSpectrum.average_re]
  have hprojection := Complex.abs_re_le_norm
    (DSP.Coboundary.average (fun n => q.eval
      (-Complex.I*orientation*star (rotation (trajectory (DSP.PhasePower.runningCoherence r) h initial n)))) count-
      q.eval (physicalMoment orientation r))
  apply le_trans _ hcount
  simpa only [Complex.sub_re,DSP.Coboundary.average,average] using hprojection

theorem coherent_observer (d : Data) (A : ℝ) (z : ℂ) (hz : ‖z‖ = 1) :
    (coherentPolynomial d A).eval z =
      z*(d.orbitPhasor+(A:ℂ)*(d.cosPhasor*z.re+d.sinPhasor*z.im)) := by
  rw [DSP.PhasePower.driven_observer z d.orbitPhasor d.cosPhasor d.sinPhasor A hz]
  simp only [coherentPolynomial,Quadratic.eval]
  ring

theorem power_observer (d : Data) (A : ℝ) (z : ℂ) (hz : ‖z‖ = 1) :
    ((powerPolynomial d A).eval z).re =
      d.orbitPower+2*A*(d.crossCos*z.re+d.crossSin*z.im)+
        A^2*(d.cosPower*z.re^2+d.sinPower*z.im^2+2*d.cosSinPower*z.re*z.im) := by
  have hu : z.re^2+z.im^2 = 1 := by
    have h : z.re*z.re+z.im*z.im = 1 := by
      rw [← Complex.normSq_apply,Complex.normSq_eq_norm_sq,hz]; norm_num
    nlinarith
  simp [powerPolynomial,Quadratic.eval,Complex.mul_re,Complex.mul_im,pow_two]
  linear_combination -(A^2/2*(d.cosPower+d.sinPower))*hu

end DSP.NaiveReuppingLadder.HarmonicBalance.DrivenApproximation

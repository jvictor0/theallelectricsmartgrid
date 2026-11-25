import DSP.Filters.NaiveReuppingLadder.Runtime
import DSP.Filters.ReuppingLadder.TuningProofs
import Mathlib.Tactic.Linarith
import Mathlib.Tactic.Positivity
import Mathlib.Tactic.FieldSimp
import Mathlib.Tactic.Ring
import Mathlib.Tactic.NormNum

/-! Uniform boundedness of the exact-real interpretation of the generated ladder.
The detector uses a common quadratic energy for all permitted integrator gains,
so the same memory bounds apply across cutoff changes. -/

namespace DSP.NaiveReuppingLadder.Bounds

noncomputable section

def energy (p q : ℝ) : ℝ := p^2 + p*q + q^2

private def svfTuned (g : ℝ) (f : TPTSVF ℝ) : Prop :=
  f.k = 1 ∧ f.a1 = 1 / (1 + g*(g+1)) ∧ f.a2 = g*f.a1 ∧ f.a3 = g*f.a2

-- Conservative bounds based on the detector integrator gain cap, rounded up.
def radius (U : ℝ) : ℝ := (3 + 7*319)*U
def tapBound (U : ℝ) : ℝ := 3*radius U

private def svfMemory (U : ℝ) (f : TPTSVF ℝ) : Prop :=
  energy f.ic1 f.ic2 ≤ 3*(radius U)^2

def detectorTuned (f : BandPassedEnvelopeFollower ℝ) : Prop :=
  (∃ g, 0 ≤ g ∧ g ≤ 319 ∧ svfTuned g f.stage1 ∧ svfTuned g f.stage2) ∧
  0 ≤ f.powerAlpha ∧ f.powerAlpha ≤ 1

def detectorMemory (U : ℝ) (f : BandPassedEnvelopeFollower ℝ) : Prop :=
  svfMemory U f.stage1 ∧ svfMemory (tapBound U) f.stage2 ∧
  0 ≤ f.power ∧ f.power ≤ (tapBound (tapBound U))^2

def tuned (s : NaiveReuppingLadder ℝ) : Prop :=
  0 ≤ s.alpha ∧ s.alpha ≤ 1 ∧ (1/8 : ℝ) ≤ s.saturation.tanhGain ∧
  0 ≤ s.baseFeedback ∧ s.baseFeedback ≤ 3.9 ∧ 0 ≤ s.dampingScale ∧
  detectorTuned s.inputEnvelope ∧ detectorTuned s.feedbackEnvelope

def memory (U : ℝ) (s : NaiveReuppingLadder ℝ) : Prop :=
  |s.stage1| ≤ U + 3.9*8 ∧ |s.stage2| ≤ 8 ∧ |s.stage3| ≤ 8 ∧ |s.stage4| ≤ 8 ∧
  detectorMemory U s.inputEnvelope ∧ detectorMemory 8 s.feedbackEnvelope

def validSpec (spec : LogicalSpec ℝ) : Prop :=
  0 < spec.cutoff ∧ 0 ≤ spec.resonance ∧ spec.resonance ≤ 1 ∧
  (1/4 : ℝ) ≤ spec.gain ∧ spec.gain ≤ 5

private def detectorGain (p : ℝ) : ℝ :=
  let d := minValue (p * 0.5) 1
  let maxGain : ℝ := 318.30883898555044
  d / SquareRoot.sqrt (maxValue ((1-d)*(1+d)) (d*d*(1/(maxGain*maxGain))))

private theorem min_real (a b : ℝ) : minValue a b = min a b := by
  by_cases h : b < a
  · simp [minValue, IfLess.ifLess, h, min_eq_right h.le]
  · simp [minValue, IfLess.ifLess, h, min_eq_left (le_of_not_gt h)]

private theorem max_real (a b : ℝ) : maxValue a b = max a b := by
  by_cases h : a < b
  · simp [maxValue, IfLess.ifLess, h, max_eq_right h.le]
  · simp [maxValue, IfLess.ifLess, h, max_eq_left (le_of_not_gt h)]

private theorem detector_min_pos (p : ℝ) (hp : 0 < p) : 0 < minValue (p * 0.5) 1 := by
  rw [min_real]
  positivity

theorem detector_radicand_pos (p : ℝ) (hp : 0 < p) :
    let d := minValue (p * 0.5) 1
    let maxGain : ℝ := 318.30883898555044
    0 < maxValue ((1-d)*(1+d)) (d*d*(1/(maxGain*maxGain))) := by
  dsimp only
  let d := minValue (p * 0.5) 1
  let c : ℝ := 318.30883898555044
  change 0 < maxValue ((1-d)*(1+d)) (d*d*(1/(c*c)))
  rw [max_real]
  have hd : 0 < d := detector_min_pos p hp
  have hc : 0 < c := by dsimp [c]; norm_num
  exact lt_of_lt_of_le (show 0 < d*d*(1/(c*c)) by positivity) (le_max_right _ _)

private theorem detector_gain (p : ℝ) (hp : 0 < p) : 0 ≤ detectorGain p ∧ detectorGain p ≤ 319 := by
  let d := minValue (p * 0.5) 1
  let c : ℝ := 318.30883898555044
  let r := maxValue ((1-d)*(1+d)) (d*d*(1/(c*c)))
  change 0 ≤ d / Real.sqrt r ∧ d / Real.sqrt r ≤ 319
  have hd : 0 < d := detector_min_pos p hp
  have hc : 0 < c := by dsimp [c]; norm_num
  have hr : 0 < r := detector_radicand_pos p hp
  have hs : 0 < Real.sqrt r := Real.sqrt_pos.2 hr
  refine ⟨div_nonneg hd.le hs.le, ?_⟩
  have hsq : (d / c)^2 ≤ r := by
    have heq : (d/c)^2 = d*d*(1/(c*c)) := by ring
    rw [heq]
    unfold r
    rw [max_real]
    exact le_max_right _ _
  have hds : d/c ≤ Real.sqrt r := by
    calc
      d/c = Real.sqrt ((d/c)^2) := (Real.sqrt_sq (div_nonneg hd.le hc.le)).symm
      _ ≤ Real.sqrt r := Real.sqrt_le_sqrt hsq
  have hcd : d ≤ Real.sqrt r * c := (div_le_iff₀ hc).1 hds
  have hg : d / Real.sqrt r ≤ c := (div_le_iff₀ hs).2 (by simpa [mul_comm] using hcd)
  exact hg.trans (by dsimp [c]; norm_num)

private theorem svf_midpoint (f : TPTSVF ℝ) (g : ℝ) (x : Sample ℝ) (hg : 0 ≤ g)
    (ht : svfTuned g f) :
    let z := f.step x
    z.ic1 = 2*z.bandPass-f.ic1 ∧ z.ic2 = 2*z.lowPass-f.ic2 ∧
    f.ic1 = z.bandPass - g*(x.x-z.bandPass-z.lowPass) ∧
    f.ic2 = z.lowPass-g*z.bandPass := by
  obtain ⟨_, ha1, ha2, ha3⟩ := ht
  have hd : 0 < 1 + g*(g+1) := by positivity
  simp only [TPTSVF.step, fma_eq, ha3, ha2, ha1]
  refine ⟨?_, ?_, ?_, ?_⟩
  · ring
  · ring
  · field_simp [hd.ne']; ring
  · ring

private theorem energy_axes {p q U : ℝ} (hU : 0 ≤ U) (hE : energy p q ≤ 4*U^2) :
    |p| ≤ 3*U ∧ |q| ≤ 3*U := by
  have axis (a b : ℝ) (h : energy a b ≤ 4*U^2) : |a| ≤ 3*U := by
    have h₁ : a^2 ≤ (3*U)^2 := by
      unfold energy at h
      nlinarith [sq_nonneg (a+b), sq_nonneg b, sq_nonneg U]
    exact (sq_le_sq₀ (abs_nonneg a) (by positivity)).mp (by rwa [sq_abs])
  exact ⟨axis p q hE, axis q p (by unfold energy at *; linarith [mul_comm p q])⟩

private theorem svf_step
    (f : TPTSVF ℝ) (g U : ℝ) (x : Sample ℝ)
    (hg : 0 ≤ g) (hg' : g ≤ 319) (hU : 0 ≤ U) (hx : |x.x| ≤ U)
    (ht : svfTuned g f) (hm : svfMemory U f) :
    svfTuned g (f.step x) ∧ svfMemory U (f.step x) ∧ |(f.step x).bandPass| ≤ tapBound U := by
  let z := f.step x
  let b := z.bandPass
  let l := z.lowPass
  obtain ⟨h1,h2,ho1,ho2⟩ := svf_midpoint f g x hg ht
  change z.ic1 = 2*b-f.ic1 at h1
  change z.ic2 = 2*l-f.ic2 at h2
  change f.ic1 = b-g*(x.x-b-l) at ho1
  change f.ic2 = l-g*b at ho2
  have hn1 : z.ic1 = b+g*(x.x-b-l) := by rw [h1,ho1]; ring
  have hn2 : z.ic2 = l+g*b := by rw [h2,ho2]; ring
  -- The trapezoidal update dissipates this energy unless the input supplies it.
  have henergy : energy z.ic1 z.ic2 - energy f.ic1 f.ic2 =
      2*g*((2*b+l)*x.x-energy b l) := by rw [hn1,hn2,ho1,ho2]; unfold energy; ring
  have hR : 0 ≤ radius U := by unfold radius; positivity
  have hUSq : x.x^2 ≤ U^2 := by rw [← sq_abs x.x]; exact (sq_le_sq₀ (abs_nonneg _) hU).mpr hx
  have hm' : svfMemory U z := by
    unfold svfMemory at *
    -- A small midpoint bounds the next state directly. A large one dissipates energy.
    by_cases hmid : energy b l ≤ 4*U^2
    · obtain ⟨hb,hl⟩ := energy_axes hU hmid
      have hinput : |x.x-b-l| ≤ 7*U := by
        apply abs_le.mpr
        have hx' := abs_le.mp hx
        have hb' := abs_le.mp hb
        have hl' := abs_le.mp hl
        constructor <;> linarith
      have hbnd (v M : ℝ) (hv : |v| ≤ M) : |g*v| ≤ 319*M := by
        rw [abs_mul, abs_of_nonneg hg]
        exact mul_le_mul hg' hv (abs_nonneg _) (by norm_num)
      have hZ1 : |z.ic1| ≤ radius U := by
        rw [hn1]
        have h := (abs_add_le b (g*(x.x-b-l))).trans (add_le_add hb (hbnd _ _ hinput))
        unfold radius
        linarith
      have hZ2 : |z.ic2| ≤ radius U := by
        rw [hn2]
        have h := (abs_add_le l (g*b)).trans (add_le_add hl (hbnd _ _ hb))
        unfold radius
        linarith
      have hs1 := (sq_le_sq₀ (abs_nonneg _) hR).mpr hZ1
      have hs2 := (sq_le_sq₀ (abs_nonneg _) hR).mpr hZ2
      rw [sq_abs] at hs1 hs2
      unfold energy
      nlinarith [sq_nonneg (z.ic1-z.ic2)]
    · have hwork : (2*b+l)*x.x ≤ energy b l := by
        unfold energy at *
        nlinarith [sq_nonneg (b+l/2-2*x.x), sq_nonneg l]
      have hd := mul_nonpos_of_nonneg_of_nonpos (by positivity : 0 ≤ 2*g) (sub_nonpos.mpr hwork)
      linarith
  have axes (p q : ℝ) (h : energy p q ≤ 3*(radius U)^2) : |p| ≤ 3*radius U :=
    (energy_axes hR (h.trans (by nlinarith [sq_nonneg (radius U)]))).1
  have hout : |b| ≤ tapBound U := by
    have u := abs_le.mp (axes f.ic1 f.ic2 hm)
    have v := abs_le.mp (axes z.ic1 z.ic2 hm')
    apply abs_le.mpr
    unfold tapBound
    constructor <;> linarith
  exact ⟨ht, hm', hout⟩

private theorem detector_step
    (f : BandPassedEnvelopeFollower ℝ) (U : ℝ) (x : Sample ℝ)
    (hU : 0 ≤ U) (hx : |x.x| ≤ U) (ht : detectorTuned f) (hm : detectorMemory U f) :
    detectorTuned (f.step x) ∧ detectorMemory U (f.step x) := by
  obtain ⟨⟨g,hg,hg',ht1,ht2⟩,ha,ha'⟩ := ht
  obtain ⟨hm1,hm2,hp,hp'⟩ := hm
  obtain ⟨ht1',hm1',hbp1⟩ := svf_step f.stage1 g U x hg hg' hU hx ht1 hm1
  have hB : 0 ≤ tapBound U := by unfold tapBound radius; positivity
  obtain ⟨ht2',hm2',hbp2⟩ := svf_step f.stage2 g (tapBound U)
    ⟨(f.stage1.step x).bandPass⟩ hg hg' hB hbp1 ht2 hm2
  let b := (f.stage2.step ⟨(f.stage1.step x).bandPass⟩).bandPass
  have hb : b*b ≤ (tapBound (tapBound U))^2 := by
    have := mul_self_le_mul_self (abs_nonneg b) hbp2
    simpa only [← sq, sq_abs] using this
  refine ⟨⟨⟨g,hg,hg',ht1',ht2'⟩,ha,ha'⟩, hm1', hm2', ?_⟩
  change 0 ≤ OnePoleZOHLowPass.relax f.powerAlpha (b*b) f.power ∧
    OnePoleZOHLowPass.relax f.powerAlpha (b*b) f.power ≤ (tapBound (tapBound U))^2
  rw [show OnePoleZOHLowPass.relax f.powerAlpha (b*b) f.power =
      (1-f.powerAlpha)*f.power + f.powerAlpha*(b*b) by unfold OnePoleZOHLowPass.relax; ring]
  refine ⟨add_nonneg (mul_nonneg (sub_nonneg.mpr ha') hp) (mul_nonneg ha (mul_self_nonneg b)), ?_⟩
  calc
    _ ≤ (1-f.powerAlpha)*(tapBound (tapBound U))^2 +
        f.powerAlpha*(tapBound (tapBound U))^2 :=
      add_le_add (mul_le_mul_of_nonneg_left hp' (sub_nonneg.mpr ha'))
        (mul_le_mul_of_nonneg_left hb ha)
    _ = _ := by ring

private theorem prepared_detector (f : BandPassedEnvelopeFollower ℝ) (g a : ℝ)
    (hg : 0 ≤ g) (hg' : g ≤ 319) (ha : 0 ≤ a) (ha' : a ≤ 1) :
    detectorTuned (f.computeCoefficients ⟨g,a⟩) := by
  unfold detectorTuned
  refine ⟨⟨g,hg,hg',?_,?_⟩,ha,ha'⟩ <;>
    simp [BandPassedEnvelopeFollower.computeCoefficients, svfTuned, TPTSVF.prepareFromSnapshot, fma_eq, add_comm]

private def powerAlpha (cutoff : ℝ) : ℝ :=
  Trig.sin2pi (cutoff*0.5) * (0.43497944204608224 / 6.283185307179586476925286766559)

private theorem gain_lower (g : ℝ) (hg : (1/4 : ℝ) ≤ g) :
    (1/8 : ℝ) ≤ Real.tanh g := by
  have he : (5/4 : ℝ) ≤ Real.exp g := by linarith [Real.add_one_le_exp g]
  have he0 := Real.exp_pos g
  have he' : Real.exp (-g) ≤ (4/5 : ℝ) := by
    rw [Real.exp_neg, ← one_div]
    exact (div_le_iff₀ he0).mpr (by linarith)
  rw [Real.tanh_eq, le_div_iff₀ (by positivity)]
  linarith

private theorem phase_range (c : ℝ) (hc : 0 < c) (hc' : c ≤ 0.5) :
    let phase := 2*Real.pi*(c*0.5)
    0 < phase ∧ phase ≤ Real.pi/2 := by
  dsimp
  exact ⟨by positivity, by nlinarith [mul_le_mul_of_nonneg_left hc' Real.pi_pos.le]⟩

private theorem alpha_range (c : ℝ) (hc : 0 < c) (hc' : c ≤ 0.5) :
    0 ≤ powerAlpha c ∧ powerAlpha c ≤ 1 := by
  obtain ⟨hp,hp'⟩ := phase_range c hc hc'
  have hx := Real.sin_pos_of_pos_of_lt_pi hp (by linarith [Real.pi_pos])
  have hy := Real.sin_le_one (2*Real.pi*(c*0.5))
  unfold powerAlpha
  change 0 ≤ Real.sin (2*Real.pi*(c*0.5)) * _ ∧ Real.sin (2*Real.pi*(c*0.5)) * _ ≤ _
  constructor
  · positivity
  · exact (mul_le_mul_of_nonneg_right hy (by norm_num)).trans (by norm_num)

private theorem poleRatio_bound (cutoff : ℝ) (hc : 0 < cutoff) :
    (ReuppingLadder.prepareTuning cutoff).poleRatio ≤ 2*(Real.sqrt 2+1) := by
  let c := minValue (0.5 : ℝ) cutoff
  have hc0 : 0 < c := by dsimp [c]; rw [min_real]; positivity
  have hc1 : c ≤ 0.5 := by dsimp [c]; rw [min_real]; exact min_le_left ..
  let sine := Real.sin (2*Real.pi*(c*0.5))
  let root := Real.sqrt (1+sine*sine)
  obtain ⟨hp, hp'⟩ := phase_range c hc0 hc1
  have hs0 : 0 ≤ sine := (Real.sin_pos_of_pos_of_lt_pi hp (by linarith [Real.pi_pos])).le
  have hs1 : sine ≤ 1 := Real.sin_le_one _
  have hs2 := (sq_le_sq₀ hs0 zero_le_one).mpr hs1
  have hr : root ≤ Real.sqrt 2 := Real.sqrt_le_sqrt (by nlinarith)
  change sine*2*(root+sine) ≤ _
  exact mul_le_mul (by linarith) (add_le_add hr hs1)
    (add_nonneg (Real.sqrt_nonneg _) hs0) (by norm_num)

theorem criticalFeedback_margin (spec : LogicalSpec ℝ) (s : NaiveReuppingLadder ℝ)
    (hc : 0 < spec.cutoff) (hr : spec.resonance ≤ 1) :
    let next := computeCoefficients spec s
    (0.1 : ℝ) ≤ next.criticalFeedback - next.baseFeedback := by
  let t := ReuppingLadder.prepareTuning spec.cutoff
  have hp0 : 0 < t.poleRatio := (ReuppingLadder.prepareTuning_properties spec.cutoff hc).2.2.1
  have hp1 : t.poleRatio ≤ 2*(Real.sqrt 2+1) := poleRatio_bound spec.cutoff hc
  have hk : (4 : ℝ) ≤ t.criticalFeedback := by
    change 4 ≤ ReuppingLadder.criticalForRatio t.poleRatio
    unfold ReuppingLadder.criticalForRatio
    by_cases h : t.poleRatio < 2
    · simp only [IfLess.ifLess, if_pos h]
      nlinarith [sq_nonneg t.poleRatio]
    · simp only [IfLess.ifLess, if_neg h, maxValue]
      have hsq := Real.sq_sqrt (by norm_num : (0 : ℝ) ≤ 2)
      have hroot : 1 ≤ Real.sqrt 2 := by nlinarith [Real.sqrt_nonneg 2]
      have hm := mul_le_mul_of_nonneg_left hp1 (sub_nonneg.mpr hroot)
      have hratio : Real.sqrt 2 - 1 ≤ 2/t.poleRatio :=
        (le_div_iff₀ hp0).mpr (by nlinarith)
      have hpow : (Real.sqrt 2)^4 ≤ (1+2/t.poleRatio)^4 :=
        pow_le_pow_left₀ (Real.sqrt_nonneg 2) (by linarith) _
      have hfour : (Real.sqrt 2)^4 = 4 := by rw [show (Real.sqrt 2)^4 = ((Real.sqrt 2)^2)^2 by ring, hsq]; norm_num
      rw [hfour] at hpow
      convert hpow using 1; ring
  have hb : (computeCoefficients spec s).baseFeedback ≤ 3.9 := by
    simp only [computeCoefficients, updateFeedbackCoefficients]
    rw [min_real]
    exact (min_le_left _ _).trans (by simpa using mul_le_mul_of_nonneg_right hr (by norm_num : (0 : ℝ) ≤ 3.9))
  change (0.1 : ℝ) ≤ t.criticalFeedback - (computeCoefficients spec s).baseFeedback
  linarith

theorem prepare_tuned
    (spec : LogicalSpec ℝ) (s : NaiveReuppingLadder ℝ) (hs : validSpec spec) :
    tuned (computeCoefficients spec s) := by
  obtain ⟨hc,hr0,hr1,hg,_⟩ := hs
  let t := ReuppingLadder.prepareTuning spec.cutoff
  obtain ⟨ha,ha',hp,_⟩ := ReuppingLadder.prepareTuning_properties spec.cutoff hc
  have hc0 : 0 < t.cutoff := by change 0 < minValue (0.5 : ℝ) spec.cutoff; rw [min_real]; positivity
  have hc1 : t.cutoff ≤ 0.5 := by change minValue (0.5 : ℝ) spec.cutoff ≤ 0.5; rw [min_real]; exact min_le_left ..
  let g := detectorGain t.poleRatio
  let a := powerAlpha t.cutoff
  obtain ⟨hd0,hd1⟩ := detector_gain t.poleRatio hp
  obtain ⟨he0,he1⟩ := alpha_range t.cutoff hc0 hc1
  let c : NormalizedTanh.Coefficients ℝ := NormalizedTanh.prepare ⟨spec.gain⟩
  have htg : (1/8 : ℝ) ≤ c.tanhGain := gain_lower _ hg
  have hsl : 0 ≤ NormalizedTanh.slopeZero c := by
    unfold NormalizedTanh.slopeZero
    exact div_nonneg (by change 0 ≤ spec.gain; linarith) (by linarith)
  let perStage := t.alpha / (2-t.alpha) * NormalizedTanh.slopeZero c
  let safe := (0.9 : ℝ) / ((perStage*perStage)*(perStage*perStage)+0.00000001)
  have hsafe : 0 ≤ safe := by
    unfold safe
    exact div_nonneg (by norm_num) (add_nonneg (mul_self_nonneg _) (by norm_num))
  change 0 ≤ t.alpha ∧ t.alpha ≤ 1 ∧ (1/8 : ℝ) ≤ c.tanhGain ∧
    0 ≤ minValue (spec.resonance * 3.9) safe ∧ minValue (spec.resonance * 3.9) safe ≤ 3.9 ∧
    0 ≤ NormalizedTanh.slopeZero c * NormalizedTanh.slopeZero c * NormalizedTanh.slopeZero c * dampingEpsilon ∧
    detectorTuned (s.inputEnvelope.computeCoefficients ⟨g,a⟩) ∧
    detectorTuned (s.feedbackEnvelope.computeCoefficients ⟨g,a⟩)
  rw [min_real]
  refine ⟨ha.le, ha'.le, htg, le_min (by positivity) hsafe, ?_, ?_,
    prepared_detector _ g a hd0 hd1 he0 he1, prepared_detector _ g a hd0 hd1 he0 he1⟩
  · exact (min_le_left _ _).trans (by linarith)
  · unfold dampingEpsilon
    positivity

private theorem blend_abs_le {a x z B : ℝ} (ha : 0 ≤ a) (ha' : a ≤ 1)
    (hx : |x| ≤ B) (hz : |z| ≤ B) : |a*x + (1-a)*z| ≤ B := by
  calc
    _ ≤ |a*x| + |(1-a)*z| := abs_add_le ..
    _ = a*|x| + (1-a)*|z| := by rw [abs_mul, abs_mul, abs_of_nonneg ha, abs_of_nonneg (sub_nonneg.mpr ha')]
    _ ≤ a*B + (1-a)*B := add_le_add (mul_le_mul_of_nonneg_left hx ha)
      (mul_le_mul_of_nonneg_left hz (sub_nonneg.mpr ha'))
    _ = B := by ring

theorem feedback_bounds (s : NaiveReuppingLadder ℝ)
    (hpower : 0 ≤ s.inputEnvelope.power) (hk0 : 0 ≤ s.baseFeedback)
    (hdamp : 0 ≤ s.dampingScale) :
    0 ≤ effectiveFeedback s ∧ effectiveFeedback s ≤ s.baseFeedback := by
  have hf : 0 < (powerFloor : ℝ) := by norm_num [powerFloor]
  have hden : 0 < s.inputEnvelope.power + powerFloor := add_pos_of_nonneg_of_pos hpower hf
  have hm (x : ℝ) : 0 ≤ maxValue 0 x := by
    unfold maxValue
    change 0 ≤ (if 0 < x then x else 0)
    split <;> linarith
  have hex : 0 ≤ maxValue (0 : ℝ)
      (s.feedbackEnvelope.power - s.referencePowerGain * s.inputEnvelope.power) /
      (s.inputEnvelope.power + powerFloor) := div_nonneg (hm _) hden.le
  have hprod := mul_nonneg hdamp hex
  unfold effectiveFeedback
  refine ⟨div_nonneg hk0 (by linarith), (div_le_iff₀ (by linarith)).mpr ?_⟩
  nlinarith [mul_nonneg hk0 hprod]

private theorem saturate_abs_le (c : NormalizedTanh.Coefficients ℝ) (x : ℝ)
    (h : (1/8 : ℝ) ≤ c.tanhGain) : |NormalizedTanh.apply c x| ≤ 8 := by
  simp only [NormalizedTanh.apply, tanh_real, abs_div, abs_of_nonneg (by linarith : 0 ≤ c.tanhGain)]
  apply (div_le_iff₀ (by linarith : 0 < c.tanhGain)).mpr
  linarith [(Real.abs_tanh_lt_one (x*c.gain)).le]

theorem step_safe
    (U : ℝ) (hU : 0 ≤ U) (s : NaiveReuppingLadder ℝ) (x : Sample ℝ)
    (hx : |x.x| ≤ U) (ht : tuned s) (hm : memory U s) :
    tuned (s.step x) ∧ memory U (s.step x) ∧ |(s.step x).output| ≤ 39.2 := by
  obtain ⟨ha0,ha1,hsat,hk0,hk1,hd,hti,htf⟩ := ht
  obtain ⟨hy1,hy2,hy3,hy4,hmi,hmf⟩ := hm
  obtain ⟨hk, hkb⟩ := feedback_bounds s hmi.2.2.1 hk0 hd
  have hfb : |x.x - effectiveFeedback s * s.stage4| ≤ U + 3.9*8 := by
    calc
      _ ≤ |x.x| + |effectiveFeedback s * s.stage4| := by simpa only [sub_eq_add_neg, abs_neg] using abs_add_le x.x (-(effectiveFeedback s * s.stage4))
      _ = |x.x| + effectiveFeedback s * |s.stage4| := by rw [abs_mul, abs_of_nonneg hk]
      _ ≤ U + 3.9*8 := add_le_add hx
        (mul_le_mul (hkb.trans hk1) hy4 (abs_nonneg _) (by norm_num))
  have pole (z y M : ℝ) (hz : |z| ≤ M) (hy : |y| ≤ M) :
      |OnePoleZOHLowPass.onePoleStep s.alpha z y| ≤ M := by
    rw [OnePoleZOHLowPass.onePoleStep_eq, fma_eq]
    exact blend_abs_le ha0 ha1 hz hy
  have h1 : |(s.step x).stage1| ≤ U + 3.9*8 := pole _ _ _ hfb hy1
  have h2 : |(s.step x).stage2| ≤ 8 := pole _ _ _ (saturate_abs_le _ _ hsat) hy2
  have h3 : |(s.step x).stage3| ≤ 8 := pole _ _ _ (saturate_abs_le _ _ hsat) hy3
  have h4 : |(s.step x).stage4| ≤ 8 := pole _ _ _ (saturate_abs_le _ _ hsat) hy4
  obtain ⟨hti', hmi'⟩ := detector_step s.inputEnvelope U x hU hx hti hmi
  obtain ⟨htf', hmf'⟩ := detector_step s.feedbackEnvelope 8 ⟨(s.step x).stage4⟩ (by norm_num) h4 htf hmf
  refine ⟨⟨ha0,ha1,hsat,hk0,hk1,hd,hti',htf'⟩, ⟨h1,h2,h3,h4,hmi',hmf'⟩, ?_⟩
  change |NormalizedTanh.apply s.saturation (s.step x).stage4 * (effectiveFeedback s+1)| ≤ 39.2
  rw [abs_mul, abs_of_nonneg (by linarith : 0 ≤ effectiveFeedback s + 1)]
  exact (mul_le_mul (saturate_abs_le _ _ hsat) (by linarith : effectiveFeedback s+1 ≤ 4.9)
    (by linarith) (by norm_num : (0 : ℝ) ≤ 8)).trans (by norm_num)

theorem tuned_reset (s : NaiveReuppingLadder ℝ) (h : tuned s) : tuned s.reset := h

theorem memory_reset (s : NaiveReuppingLadder ℝ) (U : ℝ) (hU : 0 ≤ U) :
    memory U s.reset := by
  unfold memory detectorMemory svfMemory energy
  simp [NaiveReuppingLadder.reset, BandPassedEnvelopeFollower.reset,
    BandPassedEnvelopeFollower.resetSVF]
  exact ⟨by linarith, ⟨sq_nonneg _, sq_nonneg _, sq_nonneg _⟩, sq_nonneg _, sq_nonneg _, sq_nonneg _⟩

/-- For fixed valid controls and bounded input, reset clears all memories and
the actual compact processor keeps the ladder and detector memories bounded
uniformly in time. All arithmetic is interpreted over exact reals. -/
theorem bounded_export
    (spec : LogicalSpec ℝ) (hspec : validSpec spec)
    (s : NaiveReuppingLadder ℝ) (U : ℝ) (hU : 0 ≤ U) (input : Nat → Sample ℝ)
    (hinput : ∀ n, |(input n).x| ≤ U) :
    let initial := (computeCoefficients spec s).reset
    ∀ n, tuned (Processor.runState Runtime.model input initial n) ∧
      memory U (Processor.runState Runtime.model input initial n) ∧
      |(Processor.run Runtime.process input (Runtime.save initial) n).x| ≤ 39.2 := by
  intro initial n
  have hinv : ∀ i, tuned (Processor.runState Runtime.model input initial i) ∧
      memory U (Processor.runState Runtime.model input initial i) := by
    intro i
    induction i with
    | zero => exact ⟨tuned_reset _ (prepare_tuned spec s hspec), memory_reset _ U hU⟩
    | succ i ih =>
      obtain ⟨ht, hm, _⟩ := step_safe U hU _ _ (hinput i) ih.1 ih.2
      exact ⟨ht, hm⟩
  have hrun := Processor.run_simulation _ _ _ Runtime.simulation input initial (Runtime.save initial) rfl
  rw [← hrun]
  exact ⟨(hinv n).1, (hinv n).2, (step_safe U hU _ _ (hinput n)
    (hinv n).1 (hinv n).2).2.2⟩

/-- Retuning leaves every memory in the invariant intact. -/
theorem memory_retune (s : NaiveReuppingLadder ℝ) (spec : LogicalSpec ℝ) (U : ℝ)
    (h : memory U s) : memory U (computeCoefficients spec s) := h

/-- A control update followed by a sample, expressed solely with the two
operations exported to C++. -/
def retunedProcess : Processor (Runtime.State ℝ) (LogicalSpec ℝ × Sample ℝ) (Sample ℝ) :=
  fun s x => Runtime.process (Runtime.prepareCoefficients x.1 s).1 x.2

private def retunedModel : Processor (NaiveReuppingLadder ℝ) (LogicalSpec ℝ × Sample ℝ) (Sample ℝ) :=
  fun s x => Runtime.model (computeCoefficients x.1 s) x.2

private theorem retuned_simulation :
    Processor.Simulation retunedModel retunedProcess (fun s m => m = Runtime.save s) := by
  intro s m hm x
  subst m
  change _ ∧ (Runtime.model (computeCoefficients x.1 s) x.2).2 =
    (Runtime.process (Runtime.prepareCoefficients x.1 (Runtime.save s)).1 x.2).2
  simp only [retunedProcess, Runtime.prepare_save, retunedModel]
  exact Runtime.simulation (computeCoefficients x.1 s) _ rfl x.2

/-- Bounded memories and audio output from reset, even with a different valid
cutoff, resonance, and drive every sample. The output gain bound is uniform
over the full app control range under the exact-real interpretation. -/
theorem bounded_retuned_export (s : NaiveReuppingLadder ℝ) (U : ℝ) (hU : 0 ≤ U)
    (input : Nat → LogicalSpec ℝ × Sample ℝ)
    (hinput : ∀ n, validSpec (input n).1 ∧ |(input n).2.x| ≤ U) (n : Nat) :
    let initial := (Runtime.reset (Runtime.save s)).1
    memory U (Runtime.restore (Processor.runState retunedProcess input initial n)) ∧
      |(Processor.run retunedProcess input initial n).x| ≤ 39.2 := by
  intro initial
  have hinit : initial = Runtime.save s.reset := by dsimp [initial]; rw [Runtime.reset_save]
  have hinv : ∀ i, memory U (Processor.runState retunedModel input s.reset i) := by
    intro i
    induction i with
    | zero => exact memory_reset s U hU
    | succ i ih =>
      exact (step_safe U hU _ _ (hinput i).2 (prepare_tuned _ _ (hinput i).1)
        (memory_retune _ _ U ih)).2.1
  have hstate := Processor.runState_simulation _ _ _ retuned_simulation input s.reset initial hinit n
  have hrun := Processor.run_simulation _ _ _ retuned_simulation input s.reset initial hinit
  rw [hstate, ← hrun]
  exact ⟨hinv n, (step_safe U hU _ _ (hinput n).2 (prepare_tuned _ _ (hinput n).1)
    (memory_retune _ _ U (hinv n))).2.2⟩

end
end DSP.NaiveReuppingLadder.Bounds

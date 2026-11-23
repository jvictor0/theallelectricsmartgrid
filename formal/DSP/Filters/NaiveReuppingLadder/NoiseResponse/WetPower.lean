import DSP.Filters.NaiveReuppingLadder.NoiseResponse.ResonanceCap
import DSP.Filters.NaiveReuppingLadder.Exploration
import DSP.FMA.Algebra
import DSP.IfLess.Real
import Mathlib.Analysis.InnerProductSpace.PiL2
import Mathlib.Tactic

noncomputable section
namespace DSP.NaiveReuppingLadder.NoiseResponse.WetPower
open Finset

def raw (x : ℝ) := x * (27+x^2) / (27+9*x^2)
def clip (x : ℝ) := min 1 (max (-1) x)
def sat (g x : ℝ) := clip (raw (g*x)) / clip (raw g)

theorem raw_lip (x y : ℝ) : |raw x - raw y| ≤ |x-y| := by
  let D := (3+x^2)*(3+y^2)
  have hD : 0 < D := by dsimp [D]; positivity
  have h : |3*(3-x*y)| ≤ D := by
    apply abs_le.mpr
    dsimp [D]
    constructor <;> nlinarith [sq_nonneg (x*y),sq_nonneg (x+y),sq_nonneg (x-y),sq_nonneg x,sq_nonneg y]
  have fac : raw x - raw y = (x-y)*(1/9+(8/9)*(3*(3-x*y)/D)) := by
    dsimp [raw,D]
    field_simp; ring
  rw [fac,abs_mul]
  have hq : |3*(3-x*y)/D| ≤ 1 := by
    rw [abs_div,abs_of_pos hD,div_le_one hD]; exact h
  have hc : |1/9+(8/9:ℝ)*(3*(3-x*y)/D)| ≤ 1 := by
    refine (abs_add_le ..).trans ?_
    rw [abs_mul,abs_of_nonneg (by norm_num : (0:ℝ) ≤ 8/9),abs_of_nonneg (by norm_num : (0:ℝ) ≤ 1/9)]
    linarith
  nlinarith [mul_le_mul_of_nonneg_left hc (abs_nonneg (x-y))]

theorem clip_lip (x y : ℝ) : |clip x - clip y| ≤ |x-y| := by
  unfold clip
  calc
    _ ≤ max |(1:ℝ)-1| |max (-1) x - max (-1) y| := abs_min_sub_min_le_max ..
    _ ≤ |x-y| := by
      apply max_le
      · simp
      · simpa [max_comm] using abs_max_sub_max_le_abs x y (-1)

theorem rational_stock (x : ℝ) : DSP.Tanh.rational x = clip (raw x) := by
  have h : x*(x*x+27)/(x*9*x+27) = raw x := by unfold raw; ring_nf
  simp only [DSP.Tanh.rational,DSP.fma_eq,h,clip,DSP.minValue,DSP.maxValue,DSP.IfLess.ifLess]
  rw [show (if -1 < raw x then raw x else -1) = max (-1) (raw x) by split_ifs with hlt; exact (max_eq_right hlt.le).symm; exact (max_eq_left (le_of_not_gt hlt)).symm]
  split_ifs with hh
  · exact (min_eq_right hh.le).symm
  · exact (min_eq_left (le_of_not_gt hh)).symm

theorem normalized_lip {g : ℝ} (hg : 0 ≤ g) (hT : 0 < clip (raw g)) (x y : ℝ) :
    |sat g x - sat g y| ≤ (g/clip (raw g))*|x-y| := by
  unfold sat
  rw [←sub_div,abs_div,abs_of_pos hT,div_mul_eq_mul_div]
  apply div_le_div_of_nonneg_right _ hT.le
  calc
    _ ≤ |g*x-g*y| := (clip_lip ..).trans (raw_lip ..)
    _ = _ := by rw [←mul_sub,abs_mul,abs_of_nonneg hg]

private theorem cubic_pos {x : ℝ} (hx : 0 ≤ x) :
    |clip (raw x)-x| ≤ (8/27)*x^3 := by
  have hp : 0 < 27+9*x^2 := by positivity
  have hr : 0 ≤ raw x := by unfold raw; positivity
  have diff : x-raw x = 8*x^3/(27+9*x^2) := by unfold raw; field_simp; ring
  have hrx : raw x ≤ x := by linarith [diff,div_nonneg (by positivity : 0 ≤ 8*x^3) hp.le]
  have hsat : clip (raw x) = min 1 (raw x) := by simp [clip,max_eq_right (by linarith : -1 ≤ raw x)]
  rw [hsat,abs_of_nonpos (by linarith [min_le_right (1:ℝ) (raw x)] : min 1 (raw x)-x ≤ 0)]
  by_cases h : raw x ≤ 1
  · rw [min_eq_right h,neg_sub,diff]
    apply (div_le_iff₀ hp).2
    nlinarith [mul_nonneg (pow_nonneg hx 3) (sq_nonneg x)]
  · rw [min_eq_left (le_of_not_ge h)]
    have big : 1 < x := lt_of_lt_of_le (lt_of_not_ge h) hrx
    nlinarith [mul_nonneg (show 0 ≤ 2*x+6 by linarith) (sq_nonneg (2*x-3))]

/-- Cubic distortion around the actual rational saturator's tangent, including hard clipping. -/
theorem cubic_remainder {g : ℝ} (hg : 0 ≤ g) (hT : 0 < clip (raw g)) (x : ℝ) :
    |sat g x - (g/clip (raw g))*x| ≤ ((8/27)*g^3/clip (raw g))*|x|^3 := by
  have odd (z : ℝ) : clip (raw (-z)) = -clip (raw z) := by
    have h : raw (-z) = -raw z := by unfold raw; ring_nf
    rw [h]; unfold clip; simp only [min_def,max_def]; split_ifs <;> linarith
  have all (z : ℝ) : |clip (raw z)-z| ≤ (8/27)*|z|^3 := by
    by_cases hz : 0 ≤ z
    · simpa [abs_of_nonneg hz] using cubic_pos hz
    · have hn := cubic_pos (neg_nonneg.mpr (le_of_not_ge hz))
      rw [odd,show -clip (raw z) - -z = -(clip (raw z)-z) by ring,abs_neg] at hn
      simpa [abs_of_neg (lt_of_not_ge hz)] using hn
  dsimp [sat]
  rw [div_mul_eq_mul_div,←sub_div,abs_div,abs_of_pos hT,div_mul_eq_mul_div]
  apply div_le_div_of_nonneg_right _ hT.le
  simpa [abs_mul,abs_of_nonneg hg,mul_pow,mul_assoc] using all (g*x)

/-- Finite-record energy norm. Dividing both sides of the final bound by √n gives RMS. -/
def energy (n : ℕ) (x : ℕ → ℝ) : ℝ := √(∑ t ∈ range n, (x t)^2)

private theorem energy_eq (n : ℕ) (x : ℕ → ℝ) :
    energy n x = ‖(WithLp.toLp 2 (fun i : Fin n => x i) : EuclideanSpace ℝ (Fin n))‖ := by
  rw [energy,EuclideanSpace.norm_eq]
  change _ = √(∑ i : Fin n, ‖x i‖^2)
  simp only [Real.norm_eq_abs,sq_abs]
  rw [Fin.sum_univ_eq_sum_range (fun t => (x t)^2)]

private theorem triangle (n : ℕ) (x y : ℕ → ℝ) :
    energy n (fun t => x t+y t) ≤ energy n x + energy n y := by
  simp only [energy_eq]
  exact norm_add_le (WithLp.toLp 2 (fun i : Fin n => x i)) (WithLp.toLp 2 (fun i : Fin n => y i))

private theorem scaled {n : ℕ} {x y : ℕ → ℝ} {s : ℝ} (hs : 0 ≤ s)
    (h : ∀ t, |x t| ≤ s*|y t|) : energy n x ≤ s*energy n y := by
  have sqs : ∑ t ∈ range n,(x t)^2 ≤ s^2 * ∑ t ∈ range n,(y t)^2 := by
    rw [mul_sum]
    apply sum_le_sum; intro t _
    simpa [mul_pow,sq_abs] using pow_le_pow_left₀ (abs_nonneg _) (h t) 2
  simpa [energy,Real.sqrt_mul (sq_nonneg s),Real.sqrt_sq hs] using Real.sqrt_le_sqrt sqs

/-- One-pole output cannot have more energy than its input starting at zero.
    The unsummed storage term also handles the one-sample feedback delay. -/
theorem pole_energy {a : ℝ} (ha : 0 < a) (hu : a ≤ 1) (v x : ℕ → ℝ) (zero : v 0=0)
    (step : ∀ t, v (t+1)=(1-a)*v t+a*x t) (n : ℕ) :
    energy n (fun t => v (t+1)) ≤ energy n x := by
  apply Real.sqrt_le_sqrt
  have bound (m : ℕ) : a*(∑ t ∈ range m,(v (t+1))^2)+(1-a)*(v m)^2 ≤ a*∑ t ∈ range m,(x t)^2 := by
    induction m with
    | zero => simp [zero]
    | succ m ih =>
      simp only [sum_range_succ,step m]
      nlinarith [mul_nonneg (mul_nonneg ha.le (sub_nonneg.mpr hu)) (sq_nonneg (v m-x m))]
  nlinarith [bound n,mul_nonneg (sub_nonneg.mpr hu) (sq_nonneg (v n))]

private theorem delay_energy {v : ℕ → ℝ} (zero : v 0=0) (n : ℕ) :
    energy n v ≤ energy n (fun t => v (t+1)) := by
  apply Real.sqrt_le_sqrt
  have eq := sum_range_succ' (fun t => (v t)^2) n
  rw [sum_range_succ,zero,zero_pow (by decide),add_zero] at eq
  linarith [sq_nonneg (v n)]

abbrev Signal := ℕ → ℝ
def next (x : Signal) : Signal := fun t => x (t+1)
def gap (n : ℕ) (u v : Signal) := energy n (fun t => u t-v t)

private theorem gap_triangle (n : ℕ) (u v w : Signal) : gap n u w ≤ gap n u v + gap n v w := by
  have h := triangle n (fun t => u t-v t) (fun t => v t-w t)
  simpa only [sub_add_sub_cancel,gap] using h

private theorem norm_le_gap (n : ℕ) (u v : Signal) : energy n u ≤ gap n u v + energy n v := by
  simpa only [sub_add_cancel,gap] using triangle n (fun t => u t-v t) v

structure Pole (a : ℝ) (u v : Signal) : Prop where
  zero : v 0 = 0
  step : ∀ t, v (t+1) = (1-a)*v t + a*u t

private theorem pole_gap {a : ℝ} (ha : 0 < a) (hu : a ≤ 1) {u v x y : Signal}
    (l : Pole a u v) (r : Pole a x y) (n : ℕ) : gap n (next v) (next y) ≤ gap n u x := by
  apply pole_energy ha hu (fun t => v t-y t)
  · simp [l.zero,r.zero]
  · intro t; rw [l.step,r.step]; ring

/-- Signals a..d are stored *before* sample t. The next stage consumes their updated values. -/
structure Four where
  a : Signal
  b : Signal
  c : Signal
  d : Signal

structure Cascade (alpha : ℝ) (F : ℝ → ℝ) (x : Signal) (v : Four) : Prop where
  first : Pole alpha x v.a
  second : Pole alpha (fun t => F (next v.a t)) v.b
  third : Pole alpha (fun t => F (next v.b t)) v.c
  fourth : Pole alpha (fun t => F (next v.c t)) v.d

def residual (n : ℕ) (F : ℝ → ℝ) (s : ℝ) (z : Signal) :=
  gap n (fun t => F (next z t)) (fun t => s*next z t)

private theorem sat_gap {F : ℝ → ℝ} {s : ℝ} (hs : 0 ≤ s)
    (lip : ∀ x y, |F x-F y| ≤ s*|x-y|) (n : ℕ) (u v : Signal) :
    gap n (fun t => F (next u t)) (fun t => s*next v t) ≤
      s*gap n (next u) (next v) + residual n F s v :=
  (gap_triangle n _ (fun t => F (next v t)) _).trans
    (add_le_add (scaled hs (fun t => lip (next u t) (next v t))) le_rfl)

/-- The explicit fourth-stage error budget. Rᵢ are tangent defects on the
    computable linear reference; U is the energy of its fourth state. -/
def stageBudget (K s U R₁ R₂ R₃ : ℝ) :=
  (s^3*K*U + s^2*R₁ + s*R₂ + R₃)/(1-K*s^3)

def outputBudget (K s U R₁ R₂ R₃ R₄ : ℝ) :=
  (1+K)*(s*stageBudget K s U R₁ R₂ R₃ + R₄) + K*s*U

/-- Pathwise, for every input and record length. Feedback reads the old fourth
    state and may vary arbitrarily inside its cap, including with both detectors active. -/
theorem wet_energy {F : ℝ → ℝ} {alpha s K : ℝ} (ha : 0 < alpha) (hu : alpha ≤ 1)
    (hs : 0 ≤ s) (hK : 0 ≤ K) (loop : K*s^3 < 1) (lip : ∀ x y, |F x-F y| ≤ s*|x-y|)
    (x k : Signal) (cap : ∀ t, |k t| ≤ K) (wet ref : Four)
    (w : Cascade alpha F (fun t => x t-k t*wet.d t) wet)
    (l : Cascade alpha (fun z => s*z) x ref) (n : ℕ) :
    gap n (fun t => F (next wet.d t)*(1+k t)) (fun t => s*next ref.d t) ≤
      outputBudget K s (energy n (next ref.d)) (residual n F s ref.a) (residual n F s ref.b)
        (residual n F s ref.c) (residual n F s ref.d) := by
  let Δ (u v : Signal) := gap n (next u) (next v)
  let U := energy n (next ref.d)
  let R := residual n F s
  have kprod (v : Signal) : energy n (fun t => k t*v t) ≤ K*energy n v := by
    apply scaled hK; intro t; rw [abs_mul]; exact mul_le_mul_of_nonneg_right (cap t) (abs_nonneg _)
  have d₁ : Δ wet.a ref.a ≤ K*(Δ wet.d ref.d+U) := by
    apply (pole_gap ha hu w.first l.first n).trans
    have h : gap n (fun t => x t-k t*wet.d t) x = energy n (fun t => k t*wet.d t) := by
      unfold gap energy; congr 1; apply sum_congr rfl
      intro t _; ring
    rw [h]
    exact (kprod wet.d).trans (mul_le_mul_of_nonneg_left
      ((delay_energy w.fourth.zero n).trans (norm_le_gap n (next wet.d) (next ref.d))) hK)
  have d₂ := (pole_gap ha hu w.second l.second n).trans (sat_gap hs lip n wet.a ref.a)
  have d₃ := (pole_gap ha hu w.third l.third n).trans (sat_gap hs lip n wet.b ref.b)
  have d₄ := (pole_gap ha hu w.fourth l.fourth n).trans (sat_gap hs lip n wet.c ref.c)
  change Δ wet.b ref.b ≤ s*Δ wet.a ref.a+R ref.a at d₂
  change Δ wet.c ref.c ≤ s*Δ wet.b ref.b+R ref.b at d₃
  change Δ wet.d ref.d ≤ s*Δ wet.c ref.c+R ref.c at d₄
  have bound : Δ wet.d ref.d ≤ stageBudget K s U (R ref.a) (R ref.b) (R ref.c) := by
    apply (le_div_iff₀ (sub_pos.mpr loop)).2
    linarith only [mul_le_mul_of_nonneg_left d₁ (pow_nonneg hs 3),
      mul_le_mul_of_nonneg_left d₂ (sq_nonneg s),mul_le_mul_of_nonneg_left d₃ hs,d₄]
  let fw := fun t => F (next wet.d t)
  let sl := fun t => s*next ref.d t
  have satdiff : gap n fw sl ≤ s*stageBudget K s U (R ref.a) (R ref.b) (R ref.c)+R ref.d :=
    (sat_gap hs lip n wet.d ref.d).trans (add_le_add (mul_le_mul_of_nonneg_left bound hs) le_rfl)
  have linear : energy n sl ≤ s*U := scaled hs (fun t => by simp [sl,abs_mul,abs_of_nonneg hs])
  have out := triangle n (fun t => k t*fw t) (fun t => fw t-sl t)
  have eq (t : ℕ) : fw t*(1+k t)-sl t=k t*fw t+(fw t-sl t) := by ring
  change energy n (fun t => fw t*(1+k t)-sl t) ≤ _
  simp_rw [eq]
  apply out.trans
  have kw := (kprod fw).trans (mul_le_mul_of_nonneg_left ((norm_le_gap n fw sl).trans
    (add_le_add satdiff linear)) hK)
  change _ ≤ (1+K)*(s*stageBudget K s U (R ref.a) (R ref.b) (R ref.c)+R ref.d)+K*s*U
  change energy n (fun t => k t*fw t) + gap n fw sl ≤ _
  linarith only [kw,satdiff]

private def memory (a : ℝ) (x : Signal) : Signal
  | 0 => 0
  | t+1 => (1-a)*memory a x t + a*x t

private theorem memory_pole (a : ℝ) (x : Signal) : Pole a x (memory a x) := ⟨rfl,fun _ => rfl⟩

def linearReference (a s : ℝ) (x : Signal) : Four :=
  let advance := fun v : Signal => memory a (fun t => s*next v t)
  let A := memory a x
  let B := advance A
  let C := advance B
  ⟨A,B,C,advance C⟩

private theorem linear_cascade (a s : ℝ) (x : Signal) :
    Cascade a (fun z => s*z) x (linearReference a s x) :=
  ⟨memory_pole ..,memory_pole ..,memory_pole ..,memory_pole ..⟩

local instance : Tanh ℝ := ⟨Tanh.rational⟩

def actual (prepared : NaiveReuppingLadder ℝ) (x : Signal) :=
  trajectory (fun t => ⟨x t⟩) prepared.reset

private theorem coefficients (p : NaiveReuppingLadder ℝ) (x : Signal) (t : ℕ) :
    let z := actual p x t
    z.alpha=p.alpha ∧ z.saturation=p.saturation ∧ z.baseFeedback=p.baseFeedback ∧
      z.dampingScale=p.dampingScale ∧ z.inputEnvelope.powerAlpha=p.inputEnvelope.powerAlpha := by
  induction t with
  | zero => exact ⟨rfl,rfl,rfl,rfl,rfl⟩
  | succ t ih => exact ih

private theorem actual_cap (p : NaiveReuppingLadder ℝ) (x : Signal)
    (hK : 0 ≤ p.baseFeedback) (hd : 0 ≤ p.dampingScale)
    (hp : 0 ≤ p.inputEnvelope.powerAlpha) (hp' : p.inputEnvelope.powerAlpha ≤ 1) (t : ℕ) :
    |(actual p x t).effectiveFeedback| ≤ p.baseFeedback := by
  have pos (j : ℕ) : 0 ≤ (actual p x j).inputEnvelope.power := by
    induction j with
    | zero => exact le_rfl
    | succ j ih =>
      have eq := (coefficients p x j).2.2.2.2
      exact Exploration.detector_power_nonnegative _ _ (eq ▸ hp) (eq ▸ hp') ih
  have eq := coefficients p x t
  have bounds := Exploration.runtime_feedback_bounds _ (eq.2.2.1 ▸ hK) (eq.2.2.2.1 ▸ hd) (pos t)
  rw [abs_of_nonneg bounds.1,←eq.2.2.1]; exact bounds.2

private def stages (z : ℕ → NaiveReuppingLadder ℝ) : Four :=
  ⟨fun t => (z t).stage1,fun t => (z t).stage2,fun t => (z t).stage3,fun t => (z t).stage4⟩

private theorem apply_sat (p : NaiveReuppingLadder ℝ)
    (hT : p.saturation.tanhGain=clip (raw p.saturation.gain)) (z : ℝ) :
    NormalizedTanh.apply p.saturation z = sat p.saturation.gain z := by
  change Tanh.rational (z*p.saturation.gain)/p.saturation.tanhGain = _
  rw [rational_stock,hT,mul_comm]; rfl

private theorem actual_cascade (p : NaiveReuppingLadder ℝ) (x : Signal)
    (hT : p.saturation.tanhGain=clip (raw p.saturation.gain)) :
    let z := actual p x
    Cascade p.alpha (sat p.saturation.gain) (fun t => x t-(z t).effectiveFeedback*(z t).stage4) (stages z) := by
  let z := actual p x
  have one (t : ℕ) (u v : ℝ) : OnePoleZOHLowPass.onePoleStep (z t).alpha u v =
      (1-p.alpha)*v+p.alpha*u := by simp [z,(coefficients p x t).1,add_comm]
  have saturation (t : ℕ) (u : ℝ) : NormalizedTanh.apply (z t).saturation u = sat p.saturation.gain u := by
    rw [(coefficients p x t).2.1]; exact apply_sat p hT u
  refine ⟨⟨rfl,?_⟩,⟨rfl,?_⟩,⟨rfl,?_⟩,⟨rfl,?_⟩⟩ <;> intro t
  · exact one t _ _
  · change OnePoleZOHLowPass.onePoleStep (z t).alpha (NormalizedTanh.apply (z t).saturation (z (t+1)).stage1) _ = _
    rw [one,saturation]; rfl
  · change OnePoleZOHLowPass.onePoleStep (z t).alpha (NormalizedTanh.apply (z t).saturation (z (t+1)).stage2) _ = _
    rw [one,saturation]; rfl
  · change OnePoleZOHLowPass.onePoleStep (z t).alpha (NormalizedTanh.apply (z t).saturation (z (t+1)).stage3) _ = _
    rw [one,saturation]; rfl

/-- Direct bound on the same detector-on step used by the ladder. Prepared coefficients
    are interpreted in exact real arithmetic with the executable rational nonlinearity. -/
theorem physical_record (p : NaiveReuppingLadder ℝ) (x : Signal) (n : ℕ)
    (ha : 0 < p.alpha) (hu : p.alpha ≤ 1) (hg : 0 ≤ p.saturation.gain)
    (hT : p.saturation.tanhGain=clip (raw p.saturation.gain)) (ht : 0 < clip (raw p.saturation.gain))
    (hk : 0 ≤ p.baseFeedback) (hd : 0 ≤ p.dampingScale)
    (hp : 0 ≤ p.inputEnvelope.powerAlpha) (hp' : p.inputEnvelope.powerAlpha ≤ 1)
    (loop : p.baseFeedback*(p.saturation.gain/clip (raw p.saturation.gain))^3 < 1) :
    let s := p.saturation.gain/clip (raw p.saturation.gain)
    let ref := linearReference p.alpha s x
    let R := residual n (sat p.saturation.gain) s
    gap n (fun t => (actual p x (t+1)).output) (fun t => s*next ref.d t) ≤
      outputBudget p.baseFeedback s (energy n (next ref.d)) (R ref.a) (R ref.b) (R ref.c) (R ref.d) := by
  let z := actual p x
  let k : Signal := fun t => (z t).effectiveFeedback
  have out (t : ℕ) : (z (t+1)).output = sat p.saturation.gain (next (stages z).d t)*(1+k t) := by
    change NormalizedTanh.apply (z t).saturation (z (t+1)).stage4 * (k t+1) = _
    rw [(coefficients p x t).2.1,apply_sat p hT,add_comm (k t) 1]; rfl
  change gap n (fun t => (z (t+1)).output) _ ≤ _
  simp_rw [out]
  exact wet_energy ha hu (div_nonneg hg ht.le) hk loop (normalized_lip hg ht) x k
    (actual_cap p x hk hd hp hp') _ _ (actual_cascade p x hT) (linear_cascade ..) n

/-- A single-stage RMS invariant permits coefficient and state-update rounding errors.
    The input and previous-state bounds need hold only at the current sample. -/
theorem rms_stage_invariant {a d before input localError after : ℝ}
    (ha : 0 < a) (hu : a ≤ 1) (old : before ≤ d)
    (recurrence : after ≤ (1-a)*before+a*input+localError)
    (verified : input+localError/a ≤ d) : after ≤ d := by
  have hc := (div_le_iff₀ ha).mp (show localError/a ≤ d-input by linarith)
  linarith [mul_le_mul_of_nonneg_left old (sub_nonneg.mpr hu)]

/-- A bound on the band-amplitude discrepancy gives a two-sided bound even
    when the true band power vanishes. The norm can include both FFT frames
    and the noise probability space. -/
theorem band_power_bracket {E : Type*} [SeminormedAddCommGroup E] (wet reference : E)
    {lo hi err : ℝ} (l : lo ≤ ‖reference‖) (h : ‖reference‖ ≤ hi)
    (e : ‖wet-reference‖ ≤ err) :
    (max 0 (lo-err))^2 ≤ ‖wet‖^2 ∧ ‖wet‖^2 ≤ (hi+err)^2 := by
  have d := (abs_norm_sub_norm_le wet reference).trans e
  have bounds := abs_le.mp d
  exact ⟨pow_le_pow_left₀ (le_max_left _ _) (max_le (norm_nonneg _) (by linarith)) 2,
    pow_le_pow_left₀ (norm_nonneg _) (by linarith) 2⟩

end DSP.NaiveReuppingLadder.NoiseResponse.WetPower

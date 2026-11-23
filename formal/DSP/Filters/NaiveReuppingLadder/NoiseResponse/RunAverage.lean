import DSP.Filters.NaiveReuppingLadder.NoiseResponse.MarkovPower
import Mathlib.MeasureTheory.Integral.DominatedConvergence
import Mathlib.Analysis.PSeries
import Mathlib.Analysis.Asymptotics.SpecificAsymptotics

noncomputable section
open Finset Filter MeasureTheory
open scoped Topology

namespace NoiseDisplay.RunAverage

variable {S A : Type*} [Fintype A]
variable (next : S → A → S) (w : A → ℝ)

/-- A moment of the sum of centered edge rewards conditional on
    starting at i and having accumulated x so far. -/
def moment (power : ℕ) (d : S → A → ℝ) : ℕ → S → ℝ → ℝ
  | 0, _, x => x^power
  | n+1, i, x => ∑ a, w a * moment power d n (next i a) (x+d i a)

theorem centered_second (hw : ∀ a, 0 ≤ w a) (one : ∑ a, w a = 1)
    (d : S → A → ℝ) (center : ∀ i, ∑ a, w a*d i a = 0)
    (V : ℝ) (var : ∀ i, ∑ a, w a*(d i a)^2 ≤ V) (n : ℕ) (i : S) (x : ℝ) :
    moment next w 2 d n i x ≤ x^2 + n*V := by
  induction n generalizing i x with
  | zero => simp [moment]
  | succ n ih =>
    calc
      _ ≤ ∑ a, w a*((x+d i a)^2+n*V) :=
        sum_le_sum fun a _ => mul_le_mul_of_nonneg_left (ih _ _) (hw a)
      _ = (x^2+n*V)*(∑ a, w a) + (2*x)*(∑ a, w a*d i a) + ∑ a, w a*(d i a)^2 := by
        simp only [mul_sum, ←sum_add_distrib]
        exact sum_congr rfl (fun _ _ => by ring)
      _ ≤ _ := by rw [one, center i]; push_cast; linarith [var i]

/-- The centered increment uses the Poisson correction for the conditional
    mean reward. It includes the randomness of the measurement on the edge. -/
theorem centered_poisson (one : ∑ a, w a = 1) (r : S → A → ℝ) (h v : S → ℝ)
    (eqn : ∀ i, ∑ a, w a*r i a = h i + v i - ∑ a, w a*v (next i a)) (i : S) :
    ∑ a, w a*(r i a - h i + v (next i a) - v i) = 0 := by
  simp only [mul_sub,mul_add,sum_sub_distrib,sum_add_distrib,←sum_mul,one,one_mul]
  linarith [eqn i]

/-- Expected total squared movement of the harmonic part. On a finite state
    set its distinct values have a positive gap, so a bounded expectation here
    forces only finitely many nonzero movements almost surely. -/
def totalMoves (h : S → ℝ) : ℕ → S → ℝ
  | 0, _ => 0
  | n+1, i => ∑ a, w a*((h (next i a)-h i)^2 + totalMoves h n (next i a))

theorem harmonic_moves (hw : ∀ a, 0 ≤ w a) (one : ∑ a, w a = 1)
    (h : S → ℝ) (harmonic : ∀ i, ∑ a, w a*h (next i a) = h i)
    (B : ℝ) (bound : ∀ i, (h i)^2 ≤ B) (n : ℕ) (i : S) :
    totalMoves next w h n i ≤ B - (h i)^2 := by
  induction n generalizing i with
  | zero => simpa [totalMoves] using sub_nonneg.mpr (bound i)
  | succ n ih =>
    calc
      _ ≤ ∑ a, w a*((h (next i a)-h i)^2 + (B-(h (next i a))^2)) :=
        sum_le_sum fun a _ => mul_le_mul_of_nonneg_left (add_le_add_right (ih _) _) (hw a)
      _ = (B+(h i)^2)*(∑ a, w a) - (2*h i)*(∑ a, w a*h (next i a)) := by
        simp only [mul_sum, ←sum_sub_distrib]
        exact sum_congr rfl (fun _ _ => by ring)
      _ = _ := by rw [harmonic,one]; ring

/-- The fourth moment is summable after division by n^4. The increments may
    depend arbitrarily on the state and only their conditional means vanish. -/
theorem centered_fourth (hw : ∀ a, 0 ≤ w a) (one : ∑ a, w a = 1)
    (d : S → A → ℝ) (center : ∀ i, ∑ a, w a*d i a = 0)
    (unit : ∀ i a, |d i a| ≤ 1) (n : ℕ) (i : S) (x : ℝ) :
    moment next w 4 d n i x ≤ x^4 + 8*n*x^2 + 4*(n:ℝ)^2 := by
  induction n generalizing i x with
  | zero => simp [moment]
  | succ n ih =>
    let b := (n : ℝ)
    let c := x^4 + 8*b*x^2 + 4*b^2 + 8*x^2 + 3 + 8*b
    let l := 4*x^3 + 16*b*x
    have step (a : A) :
        (x+d i a)^4 + 8*b*(x+d i a)^2 + 4*b^2 ≤ c + l*d i a := by
      let z := d i a
      have h := abs_le.mp (unit i a)
      have z2 : z^2 ≤ 1 := by dsimp [z]; nlinarith [mul_nonneg (by linarith : 0 ≤ 1+d i a) (by linarith : 0 ≤ 1-d i a)]
      have z4 : z^4 ≤ 1 := by nlinarith [mul_nonneg (sub_nonneg.mpr z2) (by positivity : 0 ≤ 1+z^2)]
      have h1 := mul_nonneg (sq_nonneg x) (sub_nonneg.mpr z2)
      have h2 := mul_nonneg (Nat.cast_nonneg n (α:=ℝ)) (sub_nonneg.mpr z2)
      change (x+z)^4 + 8*b*(x+z)^2 + 4*b^2 ≤ c + l*z
      dsimp [c,l,b]
      nlinarith [sq_nonneg (x*z-z^2)]
    calc
      _ ≤ ∑ a, w a*(c+l*d i a) :=
        sum_le_sum fun a _ => mul_le_mul_of_nonneg_left ((ih _ _).trans (step a)) (hw a)
      _ = c*(∑ a, w a) + l*(∑ a, w a*d i a) := by
        simp only [mul_sum,←sum_add_distrib]; exact sum_congr rfl (fun _ _ => by ring)
      _ ≤ _ := by rw [center,one]; dsimp [c,b]; push_cast; linarith

section Paths
variable {Ω : Type*} [MeasurableSpace Ω] {μ : Measure Ω}

/-- A summable sequence of expected nonnegative costs is summable on almost
    every path. This applies both to normalized sampling error and state jumps. -/
theorem ae_summable_cost (f : ℕ → Ω → ℝ) (pos : ∀ n ω, 0 ≤ f n ω)
    (hi : ∀ n, Integrable (f n) μ) (hs : Summable (fun n => ∫ ω, f n ω ∂μ)) :
    ∀ᵐ ω ∂μ, Summable (fun n => f n ω) := by
  let g (n : ℕ) (ω : Ω) : NNReal := ⟨f n ω, pos n ω⟩
  have cv (n : ℕ) (ω : Ω) : (g n ω : ENNReal) = ENNReal.ofReal (f n ω) :=
    (ENNReal.ofReal_coe_nnreal (p:=g n ω)).symm
  have eq (n : ℕ) : (∫⁻ ω, (g n ω : ENNReal) ∂μ) = ENNReal.ofReal (∫ ω, f n ω ∂μ) := by
    simp only [cv]
    exact (ofReal_integral_eq_lintegral_ofReal (hi n) (Eventually.of_forall (pos n))).symm
  have meas (n : ℕ) : AEMeasurable (fun ω => (g n ω : ENNReal)) μ := by
    simp only [cv]
    exact ENNReal.continuous_ofReal.measurable.comp_aemeasurable (hi n).aemeasurable
  have fin : (∫⁻ ω, ∑' n, (g n ω : ENNReal) ∂μ) ≠ ⊤ := by
    rw [lintegral_tsum meas]
    simp_rw [eq]
    rw [←ENNReal.ofReal_tsum_of_nonneg (fun n => integral_nonneg (pos n)) hs]
    exact ENNReal.ofReal_ne_top
  filter_upwards [ae_lt_top' (AEMeasurable.tsum meas) fin] with ω hω
  exact NNReal.summable_coe.mpr (ENNReal.tsum_coe_ne_top_iff_summable.mp hω.ne)

/-- A fourth-moment bound growing as n² implies the normalized sum vanishes
    almost surely, even when successive increments are dependent. -/
theorem ae_sampling_noise (M : ℕ → Ω → ℝ) (C : ℝ)
    (hi : ∀ n, Integrable (fun ω => (M n ω)^4) μ)
    (moment_bound : ∀ n, (∫ ω, (M n ω)^4 ∂μ) ≤ C*(n:ℝ)^2) :
    ∀ᵐ ω ∂μ, Tendsto (fun n : ℕ => M n ω/n) atTop (𝓝 0) := by
  let f (n : ℕ) (ω : Ω) := (M n ω)^4/(n:ℝ)^4
  have pos (n : ℕ) (ω : Ω) : 0 ≤ f n ω := by dsimp [f]; positivity
  have fint (n : ℕ) : Integrable (f n) μ := (hi n).div_const _
  have major (n : ℕ) : (∫ ω, f n ω ∂μ) ≤ C*(1/(n:ℝ)^2) := by
    simp only [f, integral_div]
    by_cases hn : n=0
    · simp [hn]
    · have np : (0:ℝ)<n := by exact_mod_cast Nat.pos_of_ne_zero hn
      calc
        _ ≤ (C*(n:ℝ)^2)/(n:ℝ)^4 := div_le_div_of_nonneg_right (moment_bound n) (by positivity)
        _ = _ := by field_simp
  have hs := ((Real.summable_one_div_nat_pow.mpr (by decide : 1<2)).mul_left C).of_nonneg_of_le
    (fun n => integral_nonneg (pos n)) major
  filter_upwards [ae_summable_cost f pos fint hs] with ω sum
  have small := Metric.tendsto_atTop.mp sum.tendsto_atTop_zero
  apply Metric.tendsto_atTop.mpr
  intro ε he
  obtain ⟨N,hN⟩ := small (ε^4) (by positivity)
  refine ⟨N,fun n hn => ?_⟩
  have e := hN n hn
  rw [Real.dist_eq,sub_zero,abs_of_nonneg (pos n ω)] at e
  rw [Real.dist_eq,sub_zero]
  apply lt_of_not_ge
  intro wrong
  have big := pow_le_pow_left₀ he.le wrong 4
  rw [←abs_pow, div_pow,abs_of_nonneg (pos n ω)] at big
  exact (not_lt_of_ge big) e

end Paths

/-- Complete finite-dimensional distributions of an independent fresh record
    stream. Working with finite prefixes also covers a continuous generator
    whose normalized records are rounded before the digital audio step. -/
def tree : ℕ → (List A → ℝ) → ℝ
  | 0, f => f []
  | n+1, f => ∑ a, w a * tree n (fun xs => f (a::xs))

def records {Ω : Type*} (input : ℕ → Ω → A) (n : ℕ) (ω : Ω) : List A :=
  (List.range n).map (fun t => input t ω)

def Fresh {Ω : Type*} [MeasurableSpace Ω] (μ : Measure Ω) (input : ℕ → Ω → A) : Prop :=
  ∀ (n : ℕ) (f : List A → ℝ), Integrable (fun ω => f (records input n ω)) μ ∧
    (∫ ω, f (records input n ω) ∂μ) = tree w n f

def readout (r : S → A → ℝ) (i : S) : List A → ℝ
  | [] => 0
  | a::xs => r i a + readout r (next i a) xs

omit [Fintype A] in
private theorem appended (r : S → A → ℝ) (i : S) (xs : List A) (a : A) :
    readout next r i (xs++[a]) = readout next r i xs + r (xs.foldl next i) a := by
  induction xs generalizing i with
  | nil => simp [readout]
  | cons j xs ih => simp [readout,ih,add_assoc]

omit [Fintype A] in
private theorem accumulated {Ω : Type*} (input : ℕ → Ω → A) (r : S → A → ℝ)
    (i : S) (ω : Ω) (n : ℕ) : readout next r i (records input n ω) =
      ∑ t ∈ range n, r ((records input t ω).foldl next i) (input t ω) := by
  induction n with
  | zero => simp [records,readout]
  | succ n ih =>
    rw [sum_range_succ,←ih]
    simpa [records,List.range_succ] using
      appended next r i (records input n ω) (input n ω)

private theorem moment_tree (d : S → A → ℝ) (k n : ℕ) (i : S) (x : ℝ) :
    tree w n (fun xs => (x+readout next d i xs)^k) = moment next w k d n i x := by
  induction n generalizing i x with
  | zero => simp [tree,moment,readout]
  | succ n ih => simp only [tree,readout,←add_assoc,ih,moment]

private theorem tree_const_add (one : ∑ a, w a = 1) (n : ℕ) (f : List A → ℝ) (c : ℝ) :
    tree w n (fun xs => c+f xs) = c+tree w n f := by
  induction n generalizing f with
  | zero => rfl
  | succ n ih => simp only [tree,ih,mul_add,sum_add_distrib,←sum_mul,one,one_mul]

private theorem move_tree (one : ∑ a, w a = 1) (h : S → ℝ) (n : ℕ) (i : S) :
    tree w n (fun xs => readout next (fun j a => (h (next j a)-h j)^2) i xs) =
      totalMoves next w h n i := by
  induction n generalizing i with
  | zero => rfl
  | succ n ih => simp only [tree,readout,tree_const_add w one,ih,totalMoves]

/-- Along an actual fresh record stream the noise left after Poisson
    compensation averages to zero almost surely. No state mixing is assumed. -/
theorem edge_noise_vanishes {Ω : Type*} [MeasurableSpace Ω] {μ : Measure Ω}
    (input : ℕ → Ω → A) (fresh : Fresh w μ input)
    (hw : ∀ a, 0 ≤ w a) (one : ∑ a, w a = 1) (i : S) [Fintype S]
    (d : S → A → ℝ) (center : ∀ j, ∑ a, w a*d j a = 0) :
    ∀ᵐ ω ∂μ, Tendsto (fun n : ℕ => readout next d i (records input n ω)/n) atTop (𝓝 0) := by
  let B := 1 + ∑ j, ∑ a, |d j a|
  have BP : 0 < B := by dsimp [B]; positivity
  let e := fun j a => d j a/B
  have ec (j : S) : ∑ a, w a*e j a = 0 := by simp only [e,←mul_div_assoc,←sum_div,center,zero_div]
  have unit (j : S) (a : A) : |e j a| ≤ 1 := by
    have h1 : |d j a| ≤ ∑ t, |d j t| := single_le_sum (fun t _ => abs_nonneg (d j t)) (mem_univ a)
    have h2 : (∑ t, |d j t|) ≤ ∑ k, ∑ t, |d k t| :=
      single_le_sum (fun k _ => sum_nonneg (fun t _ => abs_nonneg (d k t))) (mem_univ j)
    rw [show e j a=d j a/B by rfl,abs_div,abs_of_pos BP,div_le_one BP]
    dsimp [B]; linarith
  let M (n : ℕ) (ω : Ω) := readout next e i (records input n ω)
  have law (n : ℕ) := fresh n (fun xs => (0+readout next e i xs)^4)
  have bound (n : ℕ) : (∫ ω, (M n ω)^4 ∂μ) ≤ 4*(n:ℝ)^2 := by
    have h := (law n).2.trans (moment_tree next w e 4 n i 0)
    simpa [M] using h.le.trans (centered_fourth next w hw one e ec unit n i 0)
  have scales (xs : List A) (j : S) : readout next d j xs = B*readout next e j xs := by
    induction xs generalizing j with
    | nil => simp [readout]
    | cons a xs ih => simp only [readout,mul_add,←ih]; dsimp [e]; field_simp
  filter_upwards [ae_sampling_noise M 4 (fun n => by simpa [M] using (law n).1) bound] with ω hm
  have z := hm.const_mul B
  simp only [mul_zero] at z
  simpa only [scales, mul_div_assoc, M] using z

/-- From any reset, a finite-valued harmonic function eventually keeps the same
    value along almost every path. Transient states and multiple closed classes
    are allowed. -/
theorem harmonic_settles {Ω : Type*} [MeasurableSpace Ω] {μ : Measure Ω}
    (input : ℕ → Ω → A) (fresh : Fresh w μ input)
    (hw : ∀ a, 0 ≤ w a) (one : ∑ a, w a = 1) (i : S) [Fintype S]
    (h : S → ℝ) (harmonic : ∀ j, ∑ a, w a*h (next j a) = h j) :
    ∀ᵐ ω ∂μ, ∃ c, ∀ᶠ n in atTop, h ((records input n ω).foldl next i) = c := by
  classical
  let H (n : ℕ) (ω : Ω) := h ((records input n ω).foldl next i)
  let jump (n : ℕ) (ω : Ω) := (H (n+1) ω-H n ω)^2
  let r (j : S) (a : A) := (h (next j a)-h j)^2
  have eqn (n : ℕ) (ω : Ω) : (∑ t ∈ range n, jump t ω) = readout next r i (records input n ω) := by
    rw [accumulated]
    apply sum_congr rfl
    intro t _
    simp [jump,H,records,List.range_succ,r]
  have prefixInt (n : ℕ) : Integrable (fun ω => ∑ t ∈ range n, jump t ω) μ := by
    simpa only [eqn] using (fresh n (readout next r i)).1
  have jint (n : ℕ) : Integrable (jump n) μ := by
    exact ((prefixInt (n+1)).sub (prefixInt n)).congr (Eventually.of_forall (fun ω => by
      simp only [Pi.sub_apply,sum_range_succ,add_sub_cancel_left]))
  let B := ∑ j, (h j)^2
  have hb (j : S) : (h j)^2 ≤ B := single_le_sum (fun k _ => sq_nonneg (h k)) (mem_univ j)
  have hs := summable_of_sum_range_le (fun n => integral_nonneg (fun ω => sq_nonneg (H (n+1) ω-H n ω))) (c:=B-(h i)^2)
    (fun n => by
      rw [←integral_finsetSum (range n) (fun t _ => jint t)]
      simp only [eqn,(fresh n (readout next r i)).2]
      exact (move_tree next w one h n i).le.trans (harmonic_moves next w hw one h harmonic B hb n i))
  let gaps := insert (1:ℝ) (((univ : Finset (S×S)).image (fun z => (h z.1-h z.2)^2)).erase 0)
  have non : gaps.Nonempty := ⟨1,mem_insert_self ..⟩
  let g := gaps.min' non
  have gp : 0 < g := by
    have mem := min'_mem gaps non
    rcases mem_insert.mp mem with e|e
    · rw [show g=1 from e]; norm_num
    · obtain ⟨ne,hm⟩ := mem_erase.mp e
      obtain ⟨z,_,eq⟩ := mem_image.mp hm
      have nonneg : 0 ≤ g := by change 0 ≤ gaps.min' non; rw [←eq]; positivity
      exact lt_of_le_of_ne nonneg (Ne.symm ne)
  filter_upwards [ae_summable_cost jump (fun n ω => sq_nonneg _) jint hs] with ω s
  have small := (Metric.tendsto_atTop.mp s.tendsto_atTop_zero) g gp
  obtain ⟨N,hN⟩ := small
  refine ⟨H N ω,eventually_atTop.mpr ⟨N,?_⟩⟩
  intro n hn
  change H n ω=H N ω
  induction n,hn using Nat.le_induction with
  | base => rfl
  | succ k hk ih =>
    suffices H (k+1) ω = H k ω by exact this.trans ih
    by_contra ne
    have incl : jump k ω ∈ gaps := mem_insert_of_mem (mem_erase.mpr ⟨pow_ne_zero _ (sub_ne_zero.mpr ne),
      mem_image.mpr ⟨(_, _),mem_univ _,rfl⟩⟩)
    have sm := hN k hk
    rw [Real.dist_eq,sub_zero,abs_of_nonneg (sq_nonneg _)] at sm
    exact (not_lt_of_ge (min'_le gaps _ incl)) sm

/-- The expected transition operator for an independent record, including
    coincident destinations produced by different records. -/
def recordStep : (S → ℝ) →ₗ[ℝ] (S → ℝ) where
  toFun v i := ∑ a, w a*v (next i a)
  map_add' f g := by ext i; simp [mul_add,sum_add_distrib]
  map_smul' c f := by ext i; simp [mul_sum,mul_left_comm]

private theorem harmonic_tree (h : S → ℝ) (fixed : ∀ i, ∑ a, w a*h (next i a)=h i)
    (n : ℕ) (i : S) : tree w n (fun xs => h (xs.foldl next i)) = h i := by
  induction n generalizing i with
  | zero => rfl
  | succ n ih => simpa only [tree,List.foldl_cons,ih] using fixed i

omit [Fintype A] in
private theorem power_split (r : S → A → ℝ) (h v : S → ℝ) (i : S) (xs : List A) :
    readout next r i xs = readout next (fun j _ => h j) i xs +
      readout next (fun j a => r j a - h j + v (next j a) - v j) i xs +
      (v i - v (xs.foldl next i)) := by
  induction xs generalizing i with
  | nil => simp [readout]
  | cons a xs ih =>
    simp only [readout,List.foldl_cons]
    rw [ih]
    ring

private theorem boundary_vanishes [Fintype S] (v : S → ℝ) (i : S) (state : ℕ → S) :
    Tendsto (fun n : ℕ => (v i-v (state n))/n) atTop (𝓝 0) := by
  let B := ∑ j, |v j|
  have bd (j : S) : |v j| ≤ B := single_le_sum (fun k _ => abs_nonneg (v k)) (mem_univ j)
  apply tendsto_zero_iff_norm_tendsto_zero.mpr
  apply squeeze_zero (fun n => norm_nonneg _) _ (tendsto_const_div_atTop_nhds_zero_nat (2*B))
  intro n
  rw [Real.norm_eq_abs,abs_div,abs_of_nonneg (Nat.cast_nonneg n (α:=ℝ))]
  apply div_le_div_of_nonneg_right _ (Nat.cast_nonneg n)
  exact (abs_sub ..).trans (by linarith [bd i,bd (state n)])

/-- The time-averaged measured power on a single fresh iid stream converges
    almost surely from every reset. Its random limit has exactly the ensemble
    harmonic mean at that reset. Different reachable classes may have different
    limits. No graph closure, contraction or coalescing histories are assumed. -/
theorem realized_power [Fintype S] {Ω : Type*} [MeasurableSpace Ω] {μ : Measure Ω}
    [IsProbabilityMeasure μ] (input : ℕ → Ω → A) (fresh : Fresh w μ input)
    (hw : ∀ a, 0 ≤ w a) (one : ∑ a, w a = 1) (r : S → A → ℝ) :
    ∃ (h : S → ℝ), recordStep next w h = h ∧ ∀ i : S, ∃ Z : Ω → ℝ,
      (∀ᵐ ω ∂μ, Tendsto (fun n : ℕ => readout next r i (records input n ω)/n) atTop (𝓝 (Z ω))) ∧
      (∀ᵐ ω ∂μ, ∀ᶠ n in atTop, h ((records input n ω).foldl next i) = Z ω) ∧
      (∫ ω, Z ω ∂μ) = h i := by
  classical
  let P := recordStep next w
  let reward := fun i => ∑ a, w a * r i a
  have bounded : MarkovPower.BoundedStep P := by
    intro f b hb i
    calc
      _ ≤ ∑ a, |w a*f (next i a)| := Finset.abs_sum_le_sum_abs _ _
      _ ≤ ∑ a, w a*b := sum_le_sum fun a _ => by
        rw [abs_mul,abs_of_nonneg (hw a)]; exact mul_le_mul_of_nonneg_left (hb _) (hw a)
      _ = b := by rw [←sum_mul,one,one_mul]
  obtain ⟨h,v,fixed,eqn⟩ := MarkovPower.poisson_solution P bounded reward
  have fixed' (i : S) : ∑ a, w a*h (next i a) = h i := congrFun fixed i
  let d := fun i a => r i a - h i + v (next i a) - v i
  have cent (i : S) : ∑ a, w a*d i a = 0 := by
    apply centered_poisson next w one r h v _ i
    intro j
    have e := congrFun eqn j
    change (∑ a, w a*r j a) = h j + (v j-(∑ a, w a*v (next j a))) at e
    linarith
  refine ⟨h,fixed,fun i => ?_⟩
  let H (n : ℕ) (ω : Ω) := h ((records input n ω).foldl next i)
  let Z := fun ω => limUnder atTop (fun n => H n ω)
  have eventual : ∀ᵐ ω ∂μ, ∀ᶠ n in atTop, H n ω = Z ω := by
    filter_upwards [harmonic_settles next w input fresh hw one i h fixed'] with ω e
    obtain ⟨c,hc⟩ := e
    have lim : Tendsto (fun n => H n ω) atTop (𝓝 c) := tendsto_const_nhds.congr' (hc.mono fun _ e => e.symm)
    simpa only [Z,lim.limUnder_eq] using hc
  have limitH : ∀ᵐ ω ∂μ, Tendsto (fun n => H n ω) atTop (𝓝 (Z ω)) := by
    filter_upwards [eventual] with ω e using tendsto_const_nhds.congr' (e.mono fun _ he => he.symm)
  refine ⟨Z,?_,eventual,?_⟩
  · filter_upwards [limitH,edge_noise_vanishes next w input fresh hw one i d cent] with ω hc hz
    have lim := (hc.cesaro.add hz).add (boundary_vanishes v i (fun n => (records input n ω).foldl next i))
    simp only [add_zero] at lim
    apply lim.congr'
    filter_upwards with n
    rw [power_split next r h v,accumulated (r:=fun j _ => h j)]
    simp only [add_div, H]
    ring
  · have bound (n : ℕ) (ω : Ω) : ‖H n ω‖ ≤ ∑ j, |h j| :=
      single_le_sum (fun j _ => abs_nonneg (h j)) (mem_univ _)
    have lim := tendsto_integral_filter_of_norm_le_const
      (μ:=μ) (F:=H) (Eventually.of_forall (fun n => (fresh n (fun xs => h (xs.foldl next i))).1.aestronglyMeasurable))
      ⟨_,Eventually.of_forall (fun n => Eventually.of_forall (bound n))⟩ limitH
    have exactMean (n : ℕ) : (∫ ω, H n ω ∂μ) = h i :=
      (fresh n (fun xs => h (xs.foldl next i))).2.trans (harmonic_tree next w h fixed' n i)
    simp only [exactMean] at lim
    exact (tendsto_nhds_unique lim tendsto_const_nhds)

/-- All wet and dry bands converge together on the same stream. This also
    permits a random boundary state after an initial partial record: on the
    full-measure event the conclusion holds simultaneously from every state. -/
theorem realized_spectrum [Fintype S] {B Ω : Type*} [Fintype B] [MeasurableSpace Ω]
    {μ : Measure Ω} [IsProbabilityMeasure μ] (input : ℕ → Ω → A) (fresh : Fresh w μ input)
    (hw : ∀ a, 0 ≤ w a) (one : ∑ a, w a = 1) (r : B → S → A → ℝ) :
    ∃ (h : B → S → ℝ) (Z : B → S → Ω → ℝ),
      (∀ᵐ ω ∂μ, ∀ b i, Tendsto
        (fun n : ℕ => readout next (r b) i (records input n ω)/n) atTop (𝓝 (Z b i ω))) ∧
      (∀ b i, (∫ ω, Z b i ω ∂μ) = h b i) ∧ ∀ b, recordStep next w (h b) = h b := by
  classical
  choose h harmonic result using fun b => realized_power next w input fresh hw one (r b)
  choose Z average _ mean using result
  exact ⟨h, Z, ae_all_iff.mpr (fun b => ae_all_iff.mpr (average b)), mean, harmonic⟩

/-- The dry analyzer carries fewer samples than one complete record. At fixed
    epoch alignment its limiting power is the same on almost every run and
    equals an explicit two-record expectation, even with overlapping windows. -/
theorem dry_record_pair {Ω : Type*} [MeasurableSpace Ω] {μ : Measure Ω}
    [IsProbabilityMeasure μ] (input : ℕ → Ω → A) (fresh : Fresh w μ input)
    (hw : ∀ a, 0 ≤ w a) (one : ∑ a, w a = 1) (r : A → A → ℝ) (i : A) :
    ∀ᵐ ω ∂μ, Tendsto
      (fun n : ℕ => readout (fun _ a => a) r i (records input n ω)/n) atTop
      (𝓝 (∑ a, w a * ∑ b, w b * r a b)) := by
  let shift : A → A → A := fun _ a => a
  let v := fun a => ∑ b, w b * r a b
  let c := ∑ a, w a * v a
  let d := fun j a => r j a - c + v a - v j
  have cent (j : A) : ∑ a, w a*d j a = 0 := by
    apply centered_poisson shift w one r (fun _ => c) v _ j
    intro a
    change v a = c + v a - c
    ring
  filter_upwards [edge_noise_vanishes shift w input fresh hw one i d cent] with ω noise
  have lim := ((tendsto_const_nhds (x:=c) (f:=atTop (α:=ℕ))).add noise).add
    (boundary_vanishes v i (fun n => (records input n ω).foldl shift i))
  simp only [add_zero] at lim
  apply lim.congr'
  filter_upwards [eventually_ge_atTop 1] with n hn
  rw [power_split shift r (fun _ => c) v, accumulated (r:=fun _ _ => c)]
  simp only [sum_const,card_range,nsmul_eq_mul,add_div]
  have nz : (n:ℝ) ≠ 0 := by exact_mod_cast (by omega : n ≠ 0)
  change c + _ + _ = (n:ℝ)*c/n + _ + _
  rw [mul_div_cancel_left₀ _ nz]

private theorem square_tree (one : ∑ a, w a = 1) (h : S → ℝ)
    (fixed : ∀ i, ∑ a, w a*h (next i a)=h i) (n : ℕ) (i : S) :
    tree w n (fun xs => (h (xs.foldl next i))^2) =
      (h i)^2 + totalMoves next w h n i := by
  induction n generalizing i with
  | zero => simp [tree,totalMoves]
  | succ n ih =>
    simp only [tree,List.foldl_cons,ih,totalMoves]
    have e : (∑ a, w a*((h (next i a))^2+totalMoves next w h n (next i a))) -
        (∑ a, w a*((h (next i a)-h i)^2+totalMoves next w h n (next i a))) =
        (2*h i)*(∑ a, w a*h (next i a)) - (h i)^2*(∑ a, w a) := by
      simp only [mul_sum,←sum_sub_distrib]
      exact sum_congr rfl fun _ _ => by ring
    rw [fixed i,one] at e
    linarith

/-- Squared harmonic changes give the variance of the eventual response.
    This tests only power observable from the reset. -/
theorem limiting_spread [Fintype S] {Ω : Type*} [MeasurableSpace Ω] {μ : Measure Ω}
    [IsProbabilityMeasure μ] (input : ℕ → Ω → A) (fresh : Fresh w μ input)
    (one : ∑ a, w a = 1) (h : S → ℝ) (fixed : recordStep next w h=h)
    (i : S) (Z : Ω → ℝ)
    (settles : ∀ᵐ ω ∂μ, ∀ᶠ n in atTop, h ((records input n ω).foldl next i) = Z ω) :
    Tendsto (fun n => totalMoves next w h n i) atTop (𝓝 (∫ ω, (Z ω-h i)^2 ∂μ)) := by
  let g := fun j => h j-h i
  let G := fun n ω => (g ((records input n ω).foldl next i))^2
  have gf (j : S) : ∑ a, w a*g (next j a)=g j := by
    simp only [g,mul_sub,sum_sub_distrib,←sum_mul,one,one_mul]
    rw [show ∑ a, w a*h (next j a) = h j from congrFun fixed j]
  have same (n : ℕ) (j : S) : totalMoves next w g n j = totalMoves next w h n j := by
    induction n generalizing j with
    | zero => rfl
    | succ n ih => simp only [totalMoves,ih,g,sub_sub_sub_cancel_right]
  have mv (n : ℕ) : (∫ ω, G n ω ∂μ) = totalMoves next w h n i := by
    rw [show (∫ ω, G n ω ∂μ) = _ from (fresh n (fun xs => (g (xs.foldl next i))^2)).2,
      square_tree next w one g gf n i,same]
    simp [g]
  have bound (n : ℕ) (ω : Ω) : ‖G n ω‖ ≤ ∑ j, (g j)^2 := by
    rw [Real.norm_eq_abs,abs_of_nonneg (sq_nonneg _)]
    exact single_le_sum (fun j _ => sq_nonneg (g j)) (mem_univ _)
  have lim : ∀ᵐ ω ∂μ, Tendsto (fun n => G n ω) atTop (𝓝 ((Z ω-h i)^2)) := by
    filter_upwards [settles] with ω const
    apply tendsto_const_nhds.congr'
    filter_upwards [const] with n hn
    simp only [G,g,hn]
  simpa only [mv] using tendsto_integral_filter_of_norm_le_const
    (μ:=μ) (F:=G) (Eventually.of_forall (fun n => (fresh n (fun xs => (g (xs.foldl next i))^2)).1.aestronglyMeasurable))
    ⟨_,Eventually.of_forall (fun n => Eventually.of_forall (bound n))⟩ lim

end NoiseDisplay.RunAverage

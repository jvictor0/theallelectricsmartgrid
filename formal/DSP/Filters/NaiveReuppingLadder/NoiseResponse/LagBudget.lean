import DSP.Filters.NaiveReuppingLadder.NoiseResponse.JointSpectrum
import DSP.Analysis.CoboundaryBlocks

noncomputable section
namespace NoiseDisplay.LaggedSpectrum
open Filter Finset MarkovPower
open scoped Topology

variable {S : Type*} [Fintype S]

/-- The expected signed old-minus-young reward accumulated in n future
    records, starting from a particular pair of complete boundary states. -/
def futureDifference (p : S → S → ℝ) (d : S → ℝ) : ℕ → S → ℝ
  | 0 => 0
  | n+1 => d + transition p (futureDifference p d n)

private theorem future_sum (p : S → S → ℝ) (d : S → ℝ) (n : ℕ) :
    futureDifference p d n = ∑ j ∈ range n, atTime (transition p) j d := by
  induction n with
  | zero => simp [futureDifference]
  | succ n ih =>
    rw [futureDifference, ih, map_sum, sum_range_succ']
    simp only [atTime, add_comm]

/-- The bounds apply to the sum of signed differences, permitting cancellation
    along a trajectory and across fresh future inputs conditional on the pair. -/
theorem future_bracket (p : S → S → ℝ) (hp : ∀ i j, 0 ≤ p i j)
    (d lo hi : S → ℝ) (l0 : ∀ i, lo i ≤ 0) (u0 : ∀ i, 0 ≤ hi i)
    (lower : ∀ i, lo i ≤ d i + transition p lo i)
    (upper : ∀ i, d i + transition p hi i ≤ hi i) (n : ℕ) (i : S) :
    lo i ≤ futureDifference p d n i ∧ futureDifference p d n i ≤ hi i := by
  induction n generalizing i with
  | zero => exact ⟨l0 i, u0 i⟩
  | succ n ih =>
    exact ⟨(lower i).trans (add_le_add le_rfl (sum_le_sum fun j _ =>
        mul_le_mul_of_nonneg_left (ih j).1 (hp i j))),
      (add_le_add le_rfl (sum_le_sum fun j _ =>
        mul_le_mul_of_nonneg_left (ih j).2 (hp i j))).trans (upper i)⟩

/-- A one-record certificate on a reachable lagged pair bounds the physical
    Cesàro target from any audit age. Only the pair distribution at that age
    enters the reported additive error. Individual ages can keep oscillating. -/
theorem long_run_bracket (p : S → S → ℝ) (hp : ∀ i j, 0 ≤ p i j)
    (w : S → ℝ) (hw : ∀ i, 0 ≤ w i) (d lo hi : S → ℝ)
    (l0 : ∀ i, lo i ≤ 0) (u0 : ∀ i, 0 ≤ hi i)
    (lower : ∀ i, lo i ≤ d i + transition p lo i)
    (upper : ∀ i, d i + transition p hi i ≤ hi i)
    (a : ℕ → ℝ) (lag : ∀ j, ∑ i, w i * atTime (transition p) j d i = a (j+1)-a j)
    (target : ℝ) (averages : Tendsto (fun n : ℕ => (∑ j ∈ range n, a j)/(n:ℝ))
      atTop (𝓝 target)) :
    a 0 + ∑ i, w i*lo i ≤ target ∧ target ≤ a 0 + ∑ i, w i*hi i := by
  let L := a 0 + ∑ i, w i*lo i
  let U := a 0 + ∑ i, w i*hi i
  have bracket (n : ℕ) : L ≤ a n ∧ a n ≤ U := by
    have mass : (∑ i, w i * futureDifference p d n i) =
        ∑ j ∈ range n, ∑ i, w i * atTime (transition p) j d i := by
      simp only [future_sum, Finset.sum_apply, mul_sum]
      rw [sum_comm]
    rw [←finite_correction a _ n lag, ←mass]
    exact ⟨add_le_add_right (sum_le_sum fun i _ =>
      mul_le_mul_of_nonneg_left (future_bracket p hp d lo hi l0 u0 lower upper n i).1 (hw i)) _,
      add_le_add_right (sum_le_sum fun i _ =>
      mul_le_mul_of_nonneg_left (future_bracket p hp d lo hi l0 u0 lower upper n i).2 (hw i)) _⟩
  have allMeans : ∀ᶠ n : ℕ in atTop,
      (∑ j ∈ range n, a j)/(n:ℝ) ∈ Set.Icc L U := by
    filter_upwards [eventually_ge_atTop 1] with n hn
    have pos : (0:ℝ) < n := by exact_mod_cast (by omega : 0 < n)
    have hL := sum_le_sum (s:=range n) fun j _ => (bracket j).1
    have hU := sum_le_sum (s:=range n) fun j _ => (bracket j).2
    simp only [sum_const, card_range, nsmul_eq_mul] at hL hU
    exact ⟨(le_div_iff₀ pos).mpr (by linarith), (div_le_iff₀ pos).mpr (by linarith)⟩
  exact isClosed_Icc.mem_of_tendsto averages allMeans

/-- The difference of two successive block means is a triangularly weighted
    sum of one-record signed differences. All signs are kept before averaging. -/
theorem block_increment (a d : ℕ → ℝ) (lag : ∀ j, d j = a (j+1)-a j)
    (length k : ℕ) :
    (∑ r ∈ range length, ∑ j ∈ range length, d (k*length+r+j))/(length:ℝ) =
      DSP.Coboundary.blockAverage a length (k+1) - DSP.Coboundary.blockAverage a length k := by
  have piece (r : ℕ) : ∑ j ∈ range length, d (k*length+r+j) =
      a ((k+1)*length+r) - a (k*length+r) := by
    have h := finite_correction (fun j => a (k*length+r+j)) (fun j => d (k*length+r+j))
      length (fun j => by simpa [Nat.add_assoc] using lag (k*length+r+j))
    simp only [Nat.add_zero] at h
    rw [show (k+1)*length+r = k*length+r+length by rw [Nat.add_mul]; omega]
    linarith
  simp_rw [piece]
  rw [sum_sub_distrib]
  simp [DSP.Coboundary.blockAverage,DSP.Coboundary.average,div_eq_inv_mul,mul_sub]

/-- A block certificate bounds the original long-run target even if the means
    at individual record ages oscillate. Its transition kernel skips a whole
    block; only the signed block increment has to satisfy the inequalities. -/
theorem block_long_run_bracket (p : S → S → ℝ) (hp : ∀ i j, 0 ≤ p i j)
    (w : S → ℝ) (hw : ∀ i, 0 ≤ w i) (reward lo hi : S → ℝ)
    (l0 : ∀ i, lo i ≤ 0) (u0 : ∀ i, 0 ≤ hi i)
    (lower : ∀ i, lo i ≤ reward i + transition p lo i)
    (upper : ∀ i, reward i + transition p hi i ≤ hi i)
    (a d : ℕ → ℝ) (lag : ∀ j, d j = a (j+1)-a j)
    (length : ℕ) (hL : 0 < length)
    (law : ∀ k, ∑ i, w i * atTime (transition p) k reward i =
      (∑ r ∈ range length, ∑ j ∈ range length, d (k*length+r+j))/(length:ℝ))
    (target : ℝ) (lim : Tendsto (DSP.Coboundary.average a) atTop (𝓝 target)) :
    DSP.Coboundary.blockAverage a length 0 + ∑ i, w i*lo i ≤ target ∧
    target ≤ DSP.Coboundary.blockAverage a length 0 + ∑ i, w i*hi i := by
  have grows : Tendsto (fun n : ℕ => n*length) atTop atTop := by
    apply tendsto_atTop.2
    intro b
    filter_upwards [eventually_ge_atTop b] with n hn
    exact hn.trans (Nat.le_mul_of_pos_right n hL)
  have avg := lim.comp grows
  apply long_run_bracket p hp w hw reward lo hi l0 u0 lower upper
    (DSP.Coboundary.blockAverage a length) (fun k => (law k).trans (block_increment a d lag length k)) target
  convert avg using 1
  funext n
  change _ = DSP.Coboundary.average a (n*length)
  rw [← DSP.Coboundary.average_blocks]
  simp [DSP.Coboundary.average, div_eq_inv_mul]

/-- Bound just the wet-power corrections accrued while some designated
    memories still differ. Other memories may continue to differ afterwards. -/
theorem masked_tail (d : ℕ → ℝ) {D q : ℝ} (hD : 0 ≤ D) (hq : 0 ≤ q) (hlt : q < 1)
    (step : ∀ k, |d k| ≤ D*q^k) (age count : ℕ) :
    |∑ j ∈ range count, d (age+j)| ≤ D*q^age/(1-q) := by
  calc
    _ ≤ ∑ j ∈ range count, |d (age+j)| := abs_sum_le_sum_abs _ _
    _ ≤ ∑ j ∈ range count, D*q^(age+j) := sum_le_sum fun j _ => step _
    _ = (D*q^age) * ∑ j ∈ range count, q^j := by simp [mul_sum,pow_add,mul_assoc]
    _ ≤ (D*q^age) * ∑' j : ℕ, q^j := by
      apply mul_le_mul_of_nonneg_left _ (mul_nonneg hD (pow_nonneg hq _))
      exact (summable_geometric_of_lt_one hq hlt).sum_le_tsum _ (fun _ _ => pow_nonneg hq _)
    _ = _ := by rw [tsum_geometric_of_lt_one hq hlt, div_eq_mul_inv]

omit [Fintype S] in
/-- Charge a signed correction only to the histories on which it is accrued.
    The two individual band powers may take different values anywhere in [0,D]. -/
theorem masked_reward_bound (event : Finset S) (w older younger : S → ℝ)
    {D mass : ℝ} (hD : 0 ≤ D) (hw : ∀ i ∈ event, 0 ≤ w i)
    (ho : ∀ i ∈ event, older i ∈ Set.Icc 0 D)
    (hy : ∀ i ∈ event, younger i ∈ Set.Icc 0 D)
    (hm : ∑ i ∈ event, w i ≤ mass) :
    |∑ i ∈ event, w i*(older i-younger i)| ≤ D*mass := by
  calc
    _ ≤ ∑ i ∈ event, |w i*(older i-younger i)| := abs_sum_le_sum_abs _ _
    _ ≤ ∑ i ∈ event, w i*D := by
      apply sum_le_sum
      intro i hi
      rw [abs_mul, abs_of_nonneg (hw i hi)]
      exact mul_le_mul_of_nonneg_left (abs_le.mpr
        ⟨by linarith [(ho i hi).1, (hy i hi).2], by linarith [(ho i hi).2, (hy i hi).1]⟩)
        (hw i hi)
    _ ≤ D*mass := by rw [← sum_mul,mul_comm]; exact mul_le_mul_of_nonneg_left hm hD

end NoiseDisplay.LaggedSpectrum

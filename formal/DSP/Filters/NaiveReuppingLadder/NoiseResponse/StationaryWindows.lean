import Mathlib.Analysis.Subadditive
import Mathlib.Topology.Order.Compact
import Mathlib.MeasureTheory.Integral.Bochner.Basic
import Mathlib.MeasureTheory.Function.LocallyIntegrable
import Mathlib.Tactic

noncomputable section
namespace NoiseDisplay.Windows
open MeasureTheory Filter Finset
open scoped Topology
variable {Ω S : Type*} [TopologicalSpace Ω] [TopologicalSpace S]
variable [CompactSpace Ω] [CompactSpace S] [Nonempty S]
variable [MeasurableSpace Ω] [OpensMeasurableSpace Ω]
variable (T : Ω → Ω) (F : Ω × S → S) (r : Ω × S → ℝ)

def step (z : Ω × S) := (T z.1,F z)
def reward (n : ℕ) (z : Ω × S) := ∑ j ∈ range n,r ((step T F)^[j] z)
def peak (n : ℕ) (x : Ω) := sSup (Set.range (fun s : S => reward T F r n (x,s)))
def upper (μ : Measure Ω) (n : ℕ) := ∫ x, peak T F r n x ∂μ

omit [CompactSpace Ω] [CompactSpace S] [Nonempty S] [MeasurableSpace Ω] [OpensMeasurableSpace Ω] in
theorem reward_cont (ht : Continuous T) (hf : Continuous F) (hr : Continuous r) (n : ℕ) :
    Continuous (reward T F r n) := by
  have hstep : Continuous (step T F) := (ht.comp continuous_fst).prodMk hf
  exact continuous_finsetSum _ (fun j _ => hr.comp (hstep.iterate j))

omit [CompactSpace Ω] [Nonempty S] [MeasurableSpace Ω] [OpensMeasurableSpace Ω] in
theorem peak_cont (ht : Continuous T) (hf : Continuous F) (hr : Continuous r) (n : ℕ) :
    Continuous (peak T F r n) := by
  have h := isCompact_univ.continuous_sSup (f:=fun (x : Ω) (s : S) => reward T F r n (x,s))
    (reward_cont T F r ht hf hr n)
  change Continuous (fun x : Ω => sSup (Set.range (fun s : S => reward T F r n (x,s))))
  simpa only [Set.image_univ] using h

omit [CompactSpace Ω] [Nonempty S] [MeasurableSpace Ω] [OpensMeasurableSpace Ω] in
theorem value_le_peak (ht : Continuous T) (hf : Continuous F) (hr : Continuous r) (n : ℕ) (z : Ω × S) :
    reward T F r n z ≤ peak T F r n z.1 := by
  have hc := (reward_cont T F r ht hf hr n).comp (continuous_const (y:=z.1) |>.prodMk (continuous_id (X:=S)))
  apply le_csSup (isCompact_range hc).bddAbove
  exact ⟨z.2,rfl⟩

omit [TopologicalSpace Ω] [TopologicalSpace S] [CompactSpace Ω] [CompactSpace S] [Nonempty S] [MeasurableSpace Ω] [OpensMeasurableSpace Ω] in
theorem shift_fst (n : ℕ) (z : Ω × S) : ((step T F)^[n] z).1 = T^[n] z.1 := by
  induction n with
  | zero => rfl
  | succ n ih => simp only [Function.iterate_succ_apply',step,ih]

omit [TopologicalSpace Ω] [TopologicalSpace S] [CompactSpace Ω] [CompactSpace S] [Nonempty S] [MeasurableSpace Ω] [OpensMeasurableSpace Ω] in
theorem reward_split (m n : ℕ) (z : Ω × S) :
    reward T F r (m+n) z = reward T F r m z + reward T F r n ((step T F)^[m] z) := by
  simp only [reward,sum_range_add]
  congr 1
  apply sum_congr rfl;intro i _
  rw [Nat.add_comm m i, Function.iterate_add_apply]

omit [CompactSpace Ω] [MeasurableSpace Ω] [OpensMeasurableSpace Ω] in
theorem peak_split (ht : Continuous T) (hf : Continuous F) (hr : Continuous r) (m n : ℕ) (x : Ω) :
    peak T F r (m+n) x ≤ peak T F r m x + peak T F r n (T^[m] x) := by
  apply csSup_le (Set.range_nonempty _)
  rintro v ⟨s,rfl⟩
  change reward T F r (m+n) (x,s) ≤ _
  rw [reward_split T F r m n (x,s)]
  exact add_le_add (value_le_peak T F r ht hf hr m (x,s)) (by
    simpa [shift_fst] using value_le_peak T F r ht hf hr n ((step T F)^[m] (x,s)))

variable (μ : Measure Ω) [IsProbabilityMeasure μ]

private theorem cint {g : Ω → ℝ} (hc : Continuous g) : Integrable g μ :=
  hc.integrable_of_hasCompactSupport (by exact HasCompactSupport.of_compactSpace _)

theorem upper_subadd (ht : Continuous T) (hf : Continuous F) (hr : Continuous r)
    (stationary : ∀ g : Ω → ℝ, Continuous g → ∫ x,g (T x) ∂μ = ∫ x,g x ∂μ) :
    Subadditive (upper T F r μ) := by
  have stationaryN (m : ℕ) (g : Ω → ℝ) (hg : Continuous g) :
      ∫ x,g (T^[m] x) ∂μ = ∫ x,g x ∂μ := by
    induction m with
    | zero => rfl
    | succ m ih => rw [←ih, ← stationary (fun x => g (T^[m] x)) (hg.comp (ht.iterate m))];rfl
  intro m n
  let a := peak T F r m
  let b := peak T F r n
  have ha := peak_cont T F r ht hf hr m
  have hb := peak_cont T F r ht hf hr n
  calc
    upper T F r μ (m+n) ≤ ∫ x,a x+b (T^[m] x) ∂μ :=
      integral_mono (cint μ (peak_cont T F r ht hf hr _)) ((cint μ ha).add (cint μ (hb.comp (ht.iterate m)))) (peak_split T F r ht hf hr m n)
    _ = (∫ x,a x ∂μ)+(∫ x,b (T^[m] x) ∂μ) := integral_add (f:=a) (g:=fun x => b (T^[m] x)) (cint μ ha) (cint μ (hb.comp (ht.iterate m)))
    _ = _ := by rw [stationaryN m b hb];rfl

theorem upper_bdd (ht : Continuous T) (hf : Continuous F) (hr : Continuous r)
    {B : ℝ} (hB : 0 ≤ B) (hmin : ∀ z, -B ≤ r z) :
    BddBelow (Set.range (fun n : ℕ => upper T F r μ n / n)) := by
  refine ⟨-B, ?_⟩
  rintro x ⟨n,rfl⟩
  rcases n.eq_zero_or_pos with h|h
  · simp [h,hB]
  apply (le_div_iff₀ (by exact_mod_cast h : (0:ℝ)<n)).2
  calc
    -B*n = ∫ _x : Ω, -B*n ∂μ := by simp
    _ ≤ upper T F r μ n := by
      apply integral_mono (integrable_const _) (cint μ (peak_cont T F r ht hf hr n))
      intro x
      let s : S := Classical.choice inferInstance
      apply le_trans _ (value_le_peak T F r ht hf hr n (x,s))
      calc
        -B*n = ∑ _j ∈ range n,-B := by simp [mul_comm]
        _ ≤ _ := sum_le_sum fun j _ => hmin _

/-- Both limiting endpoints exist by applying this statement to a band's reward and
    to its negative. Every finite window bounds the corresponding limiting endpoint. -/
theorem convergent_upper (ht : Continuous T) (hf : Continuous F) (hr : Continuous r)
    (stationary : ∀ g : Ω → ℝ, Continuous g → ∫ x,g (T x) ∂μ = ∫ x,g x ∂μ)
    {B : ℝ} (hB : 0 ≤ B) (hmin : ∀ z,-B ≤ r z) :
    ∃ U : ℝ, Tendsto (fun n : ℕ => upper T F r μ n / n) atTop (𝓝 U) ∧
      ∀ m : ℕ, 0<m → U ≤ upper T F r μ m/m := by
  have sub := upper_subadd T F r μ ht hf hr stationary
  have bdd := upper_bdd T F r μ ht hf hr hB hmin
  exact ⟨sub.lim,sub.tendsto_lim bdd,fun _ hm => sub.lim_le_div bdd hm.ne'⟩

omit [Nonempty S] in
/-- Reset may be any continuous chosen starting memory, in particular a fixed constant. -/
theorem reset_upper (ht : Continuous T) (hf : Continuous F) (hr : Continuous r)
    {s : Ω → S} (hs : Continuous s) (n : ℕ) :
    (∫ x, reward T F r n (x,s x) ∂μ) ≤ upper T F r μ n := by
  exact integral_mono (cint μ ((reward_cont T F r ht hf hr n).comp (continuous_id.prodMk hs)))
    (cint μ (peak_cont T F r ht hf hr n)) (fun x => value_le_peak T F r ht hf hr n (x,s x))

end NoiseDisplay.Windows

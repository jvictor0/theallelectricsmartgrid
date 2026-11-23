import Mathlib.Data.Finset.Card
import Mathlib.MeasureTheory.Measure.ProbabilityMeasure
import Mathlib.MeasureTheory.Measure.MeasureSpace
import Mathlib.Tactic

noncomputable section
namespace NoiseDisplay.DigitalNoise
open Finset Set MeasureTheory Filter
open scoped Topology

variable {S : Type*} [Fintype S] [DecidableEq S]
variable (F : ℤ → S → S)

/-- All histories reachable at t from an arbitrary memory n steps earlier,
    under the same two-sided driving path. -/
def reachable (t : ℤ) : ℕ → Finset S
  | 0 => univ
  | n+1 => (reachable (t-1) n).image (F (t-1))

theorem reachable_mono (t : ℤ) : Antitone (reachable F t) := by
  apply antitone_nat_of_succ_le
  intro n
  induction n generalizing t with
  | zero => exact subset_univ _
  | succ n ih => exact image_subset_image (ih (t-1))

omit [Fintype S] [DecidableEq S] in
theorem stabilizes (a : ℕ → Finset S) (h : Antitone a) :
    ∃ n, ∀ m ≥ n, a m = a n := by
  classical
  have ex : ∃ k, ∃ j, (a j).card=k := ⟨_,0,rfl⟩
  obtain ⟨n,hn⟩ := Nat.find_spec ex
  refine ⟨n,fun m hm => Finset.eq_of_subset_of_card_le (h hm) ?_⟩
  rw [hn];exact Nat.find_min' ex ⟨m,rfl⟩

def depth (t : ℤ) : ℕ := by
  classical
  exact Nat.find (stabilizes _ (reachable_mono F t))
def core (t : ℤ) := reachable F t (depth F t)

theorem reaches_core (t : ℤ) {n : ℕ} (h : depth F t ≤ n) : reachable F t n = core F t := by
  classical
  exact Nat.find_spec (stabilizes _ (reachable_mono F t)) n h

theorem core_forward (t : ℤ) : (core F t).image (F t) = core F (t+1) := by
  let n := max (depth F t) (depth F (t+1))
  have a : reachable F t n = core F t := reaches_core F t (le_max_left ..)
  have b : reachable F (t+1) (n+1) = core F (t+1) :=
    reaches_core F (t+1) ((le_max_right ..).trans (Nat.le_succ n))
  rw [←b,←a,reachable]
  simp

theorem card_decreases (t : ℤ) : (core F (t+1)).card ≤ (core F t).card := by
  rw [←core_forward];exact card_image_le

/-- Equal cardinalities make the transition a permutation between consecutive
    cores. Pushing the uniform conditional law forward gives the uniform next law. -/
theorem core_uniform (t : ℤ) (hcard : (core F (t+1)).card=(core F t).card) (v : S → ℝ) :
    (∑ s ∈ core F t,v (F t s))/(core F t).card =
    (∑ s ∈ core F (t+1),v s)/(core F (t+1)).card := by
  have hc : ((core F t).image (F t)).card=(core F t).card := by rw [core_forward,hcard]
  have inj := Finset.card_image_iff.mp hc
  rw [hcard,←core_forward,Finset.sum_image inj]

theorem forward_member (t : ℤ) {s : S} (h : s ∈ core F t) : F t s ∈ core F (t+1) := by
  rw [←core_forward];exact mem_image_of_mem _ h

theorem reachable_nonempty [Nonempty S] (t : ℤ) (n : ℕ) : (reachable F t n).Nonempty := by
  induction n generalizing t with
  | zero => exact univ_nonempty
  | succ n ih => exact (ih (t-1)).image _

def run (s : S) : ℕ → S
  | 0 => s
  | n+1 => F (n:ℤ) (run s n)

theorem run_mem (s : S) (n : ℕ) : run F s n ∈ reachable F n n := by
  induction n with
  | zero => exact mem_univ _
  | succ n ih =>
    simp only [reachable,run,Nat.cast_succ,add_sub_cancel_right]
    exact mem_image_of_mem _ ih

theorem enters_by_depth (s : S) {n : ℕ} (h : depth F n ≤ n) : run F s n ∈ core F n := by
  simpa only [← reaches_core F n h] using run_mem F s n

/-- A stationary sequence of finite (unbounded) backward stabilization times eventually
    permits a forward entry. No mixing, independence or finite mean of the times is used. -/
theorem finite_depth_entry {Ω : Type*} [MeasurableSpace Ω] (μ : Measure Ω) [IsProbabilityMeasure μ]
    (d : ℕ → Ω → ℕ) (hm : ∀ n, Measurable (d n))
    (stationary : ∀ n, μ {x | n < d n x} = μ {x | n < d 0 x}) :
    ∀ᵐ x ∂μ, ∃ n, d n x ≤ n := by
  let tail := fun n => {x | n < d 0 x}
  have ht (n) : MeasurableSet (tail n) := measurableSet_lt measurable_const (hm 0)
  have hanti : Antitone tail := fun _ _ h _ hx => lt_of_le_of_lt h hx
  have empty : (⋂ n,tail n) = ∅ := by
    apply Set.eq_empty_iff_forall_notMem.mpr
    intro x hx
    exact lt_irrefl (d 0 x) (Set.mem_iInter.mp hx (d 0 x))
  have lim := tendsto_measure_iInter_atTop (μ:=μ) (fun n => (ht n).nullMeasurableSet) hanti ⟨0,measure_ne_top _ _⟩
  rw [empty,measure_empty] at lim
  rw [ae_iff]
  apply le_antisymm _ bot_le
  apply ge_of_tendsto lim
  filter_upwards with n
  change _ ≤ μ {x | n < d 0 x}
  rw [←stationary n]
  apply measure_mono
  intro x hx
  change ¬ ∃ n, d n x ≤ n at hx
  exact lt_of_not_ge fun h => hx ⟨n,h⟩

end NoiseDisplay.DigitalNoise

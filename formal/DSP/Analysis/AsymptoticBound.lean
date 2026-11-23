import DSP.Analysis.Asymptotic

/-! A time-dependent upper bound allows arbitrary startup transients. -/
noncomputable section
namespace DSP.Asymptotic
open Filter
open scoped Topology

def Bound (x : ℕ → ℝ) (B : ℝ) : Prop :=
  ∃ v : ℕ → ℝ, Tendsto v atTop (𝓝 B) ∧ ∀ i, x i ≤ v i

namespace Bound
variable {x y z : ℕ → ℝ} {A B r : ℝ}

theorem of_le (h : ∀ i, x i ≤ A) : Bound x A := ⟨_,tendsto_const_nhds,h⟩
theorem of_tendsto (h : Tendsto x atTop (𝓝 A)) : Bound x A := ⟨x,h,fun _ => le_rfl⟩

theorem mono (hy : Bound y B) (h : ∀ i, x i ≤ y i) : Bound x B := by
  obtain ⟨v,hv,hb⟩ := hy
  exact ⟨v,hv,fun i => (h i).trans (hb i)⟩

theorem add (hx : Bound x A) (hy : Bound y B) : Bound (fun i => x i+y i) (A+B) := by
  obtain ⟨v,hv,hb⟩ := hx
  obtain ⟨w,hw,hc⟩ := hy
  exact ⟨_,hv.add hw,fun i => add_le_add (hb i) (hc i)⟩

theorem mul (hx : Bound x A) (hy : Bound y B) (h₁ : ∀ i, 0 ≤ x i) (h₂ : ∀ i, 0 ≤ y i) :
    Bound (fun i => x i*y i) (A*B) := by
  obtain ⟨v,hv,hb⟩ := hx
  obtain ⟨w,hw,hc⟩ := hy
  exact ⟨_,hv.mul hw,fun i => mul_le_mul (hb i) (hc i) (h₂ i) ((h₁ i).trans (hb i))⟩

theorem scale (hx : Bound x B) (hr : 0 ≤ r) : Bound (fun i => r*x i) (r*B) := by
  obtain ⟨v,hv,hb⟩ := hx
  exact ⟨_,hv.const_mul r,fun i => mul_le_mul_of_nonneg_left (hb i) hr⟩

theorem shift (h : Bound x B) : Bound (fun i => x (i+1)) B := by
  obtain ⟨v,hv,hb⟩ := h
  exact ⟨_,hv.comp (tendsto_add_atTop_nat 1),fun i => hb (i+1)⟩

/-- A stable scalar pole forgets the initial value and inherits the steady gain bound. -/
theorem recurrence (hr : 0 ≤ r) (hr1 : r < 1) (hu : Bound y ((1-r)*B))
    (h : ∀ i, x (i+1) ≤ r*x i+y i) : Bound x B := by
  obtain ⟨v,hv,hb⟩ := hu
  let err := majorant r (fun i => v i-(1-r)*B) (x 0-B)
  have he : Tendsto err atTop (𝓝 0) := majorant_tendsto r hr hr1 _ _ (by
    simpa only [sub_self] using hv.sub_const ((1-r)*B))
  refine ⟨fun i => err i+B,by simpa using he.add_const B,?_⟩
  intro n
  induction n with
  | zero => simp [err,majorant]
  | succ n ih =>
    have hm := mul_le_mul_of_nonneg_left ih hr
    change x (n+1) ≤ r*err n+(v n-(1-r)*B)+B
    linarith [h n,hb n]

theorem smooth (a : ℝ) (ha : 0 < a) (ha1 : a ≤ 1) (hy : Bound y B)
    (h : ∀ i, x (i+1) ≤ (1-a)*x i+a*y i) : Bound x B :=
  recurrence (sub_nonneg.mpr ha1) (by linarith) (by simpa using hy.scale ha.le) h

/-- Iteratively improving an asymptotic bound by a factor below one forces
the actual error to zero, even if early transients grow. -/
theorem tendsto_of_feedback (h0 : ∀ i, 0 ≤ x i) (hr : 0 ≤ r) (hr1 : r < 1)
    (hB : 0 ≤ B) (hinit : Bound x B) (hloop : ∀ A, 0 ≤ A → Bound x A → Bound x (r*A)) :
    Tendsto x atTop (𝓝 0) := by
  have h (n : ℕ) : Bound x (r^n*B) := by
    induction n with
    | zero => simpa using hinit
    | succ n ih =>
      simpa only [pow_succ,mul_assoc,mul_left_comm,mul_comm] using
        hloop _ (mul_nonneg (pow_nonneg hr _) hB) ih
  have hz := (tendsto_pow_atTop_nhds_zero_of_lt_one hr hr1).mul_const B
  simp only [zero_mul] at hz
  apply Metric.tendsto_atTop.mpr
  intro ε hε
  obtain ⟨n,hn⟩ := (hz.eventually_lt_const hε).exists
  obtain ⟨v,hv,hb⟩ := h n
  obtain ⟨m,hm⟩ := eventually_atTop.mp (hv.eventually_lt_const hn)
  exact ⟨m,fun i hi => by simpa [Real.dist_eq,abs_of_nonneg (h0 i)] using (hb i).trans_lt (hm i hi)⟩

end Bound
end DSP.Asymptotic

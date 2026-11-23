import Mathlib.Analysis.SpecificLimits.Normed
import Mathlib.Topology.MetricSpace.Lipschitz
import Mathlib.Algebra.BigOperators.Group.Finset.Basic

/-! A posteriori bounds for discrete time averages. A bounded correction
potential telescopes along the actual trajectory. This requires neither a
phase density nor an irrational rotation number. A fixed nonzero residual
bounds averages; convergence is a separate conclusion requiring arbitrarily
small residual certificates for the same proposed mean. -/

noncomputable section
namespace DSP.Coboundary
open Filter Finset
open scoped Topology NNReal
variable {E : Type*} [NormedAddCommGroup E] [NormedSpace ℝ E]

def average (signal : ℕ → E) (N : ℕ) : E := (N:ℝ)⁻¹ • ∑ n ∈ range N, signal n

theorem average_sub_const (signal : ℕ → E) (c : E) (N : ℕ) (hN : 0 < N) :
    average signal N-c = (N:ℝ)⁻¹ • ∑ n ∈ range N, (signal n-c) := by
  have hn : (N:ℝ) ≠ 0 := by positivity
  simp only [average, sum_sub_distrib, sum_const, card_range, smul_sub]
  rw [← Nat.cast_smul_eq_nsmul ℝ N c, smul_smul, inv_mul_cancel₀ hn, one_smul]

omit [NormedSpace ℝ E] in
/-- Exact telescoping identity; the residual includes every modeling and
numerical approximation in the observable or potential. -/
theorem telescope (signal potential : ℕ → E) (c : E) (N : ℕ) :
    ∑ n ∈ range N, (signal n-c) =
      (∑ n ∈ range N, (signal n-c-(potential n-potential (n+1)))) + potential 0-potential N := by
  induction N with
  | zero => simp
  | succ N ih =>
    simp only [sum_range_succ]
    rw [ih]
    abel

theorem finite_bound (signal potential : ℕ → E) (c : E) (error M : ℝ) (N : ℕ)
    (hN : 0 < N) (he : ∀ n < N, ‖signal n-c-(potential n-potential (n+1))‖ ≤ error)
    (h0 : ‖potential 0‖ ≤ M) (hLast : ‖potential N‖ ≤ M) :
    ‖average signal N-c‖ ≤ error+2*M/(N:ℝ) := by
  have hn : (0:ℝ) < N := by positivity
  have hs : ‖∑ n ∈ range N, (signal n-c-(potential n-potential (n+1)))‖ ≤ (N:ℝ)*error := by
    calc
      _ ≤ ∑ n ∈ range N, ‖signal n-c-(potential n-potential (n+1))‖ := norm_sum_le _ _
      _ ≤ ∑ _n ∈ range N, error := sum_le_sum (fun n hn => he n (mem_range.mp hn))
      _ = _ := by simp
  have hp : ‖potential 0-potential N‖ ≤ 2*M := (norm_sub_le _ _).trans (by linarith)
  rw [average_sub_const signal c N hN, telescope, show (∑ n ∈ range N,
    (signal n-c-(potential n-potential (n+1)))) + potential 0-potential N =
      (∑ n ∈ range N, (signal n-c-(potential n-potential (n+1)))) + (potential 0-potential N) by abel,
    norm_smul, Real.norm_of_nonneg (inv_nonneg.mpr hn.le)]
  calc
    _ ≤ (N:ℝ)⁻¹ * ((N:ℝ)*error+2*M) := by
      gcongr
      exact (norm_add_le _ _).trans (add_le_add hs hp)
    _ = _ := by field_simp

/-- No convergence assumption: every sufficiently long average is within any
strict enlargement of the residual bound. -/
theorem eventually_bound (signal potential : ℕ → E) (c : E) (error M : ℝ)
    (he : ∀ n, ‖signal n-c-(potential n-potential (n+1))‖ ≤ error)
    (hp : ∀ n, ‖potential n‖ ≤ M) (tolerance : ℝ) (ht : 0 < tolerance) :
    ∀ᶠ N in atTop, ‖average signal N-c‖ < error+tolerance := by
  have hz : Tendsto (fun N : ℕ => 2*M/(N:ℝ)) atTop (𝓝 0) := by
    simpa only [div_eq_mul_inv, mul_zero] using
      (tendsto_inv_atTop_nhds_zero_nat (𝕜 := ℝ)).const_mul (2*M)
  filter_upwards [hz.eventually (gt_mem_nhds ht), eventually_gt_atTop 0] with N htail hN
  exact (finite_bound signal potential c error M N hN (fun n _ => he n) (hp 0) (hp N)).trans_lt
    (by linarith)

/-- Arbitrarily accurate bounded correction certificates for a common mean
prove that the average converges to it. The potential bound can grow as the
requested residual shrinks: no uniform rate is silently assumed. -/
theorem tendsto_of_certificates (signal : ℕ → E) (c : E)
    (h : ∀ error > 0, ∃ potential : ℕ → E, ∃ M : ℝ,
      (∀ n, ‖potential n‖ ≤ M) ∧
      ∀ n, ‖signal n-c-(potential n-potential (n+1))‖ ≤ error) :
    Tendsto (average signal) atTop (𝓝 c) := by
  rw [Metric.tendsto_nhds]
  intro epsilon hepsilon
  obtain ⟨potential, M, hp, hr⟩ := h (epsilon/2) (by linarith)
  have hh := eventually_bound signal potential c (epsilon/2) M hr hp (epsilon/2) (by linarith)
  simpa only [dist_eq_norm, add_halves] using hh

/-- If increasingly accurate certificates exist, the time average has a
limit even when the proposed means vary with the certificate. This is a
Cauchy criterion, not a conclusion drawn from one small numerical residual. -/
theorem exists_limit_of_certificates [CompleteSpace E] (signal : ℕ → E)
    (h : ∀ error > 0, ∃ c : E, ∃ potential : ℕ → E, ∃ M : ℝ,
      (∀ n, ‖potential n‖ ≤ M) ∧
      ∀ n, ‖signal n-c-(potential n-potential (n+1))‖ ≤ error) :
    ∃ c : E, Tendsto (average signal) atTop (𝓝 c) := by
  apply cauchySeq_tendsto_of_complete
  rw [Metric.cauchySeq_iff]
  intro epsilon hepsilon
  obtain ⟨c, V, M, hp, hr⟩ := h (epsilon/4) (by linarith)
  have hh := eventually_bound signal V c (epsilon/4) M hr hp (epsilon/4) (by linarith)
  obtain ⟨N, hN⟩ := eventually_atTop.mp hh
  refine ⟨N, fun m hm n hn => ?_⟩
  have hm' := hN m hm
  have hn' := hN n hn
  calc
    dist (average signal m) (average signal n) ≤ dist (average signal m) c+dist c (average signal n) := dist_triangle _ _ _
    _ = ‖average signal m-c‖+‖average signal n-c‖ := by rw [dist_comm c]; simp only [dist_eq_norm]
    _ < epsilon := by linarith

theorem limit_error (signal potential : ℕ → E) (c actual : E) (error M : ℝ)
    (he : ∀ n, ‖signal n-c-(potential n-potential (n+1))‖ ≤ error)
    (hp : ∀ n, ‖potential n‖ ≤ M)
    (ht : Tendsto (average signal) atTop (𝓝 actual)) : ‖actual-c‖ ≤ error := by
  have hb : Tendsto (fun N : ℕ => error+2*M/(N:ℝ)) atTop (𝓝 error) := by
    have htail := (tendsto_inv_atTop_nhds_zero_nat (𝕜 := ℝ)).const_mul (2*M)
    simpa only [div_eq_mul_inv, mul_zero, add_zero] using htail.const_add error
  apply le_of_tendsto_of_tendsto ((ht.sub_const c).norm) hb
  filter_upwards [eventually_gt_atTop 0] with N hN
  exact finite_bound signal potential c error M N hN (fun n _ => he n) (hp 0) (hp N)

/-- Uniform domain checks transfer directly to every actual orbit that stays
in the domain; invariance and residual bounds are explicit inputs. -/
theorem orbit_bound {S : Type*} (f : S → S) (domain : Set S)
    (hinv : Set.MapsTo f domain domain) (observe V : S → E) (c : E) (error M : ℝ)
    (he : ∀ x ∈ domain, ‖observe x-c-(V x-V (f x))‖ ≤ error)
    (hp : ∀ x ∈ domain, ‖V x‖ ≤ M) (x : S) (hx : x ∈ domain) (N : ℕ) (hN : 0 < N) :
    ‖average (fun n => observe (f^[n] x)) N-c‖ ≤ error+2*M/(N:ℝ) := by
  apply finite_bound _ (fun n => V (f^[n] x)) c error M N hN
  · intro n _
    simpa only [Function.iterate_succ_apply'] using he (f^[n] x) (hinv.iterate n hx)
  · exact hp x hx
  · exact hp _ (hinv.iterate N hx)

omit [NormedSpace ℝ E] in
/-- Parameter/model perturbations enter the residual explicitly, including
the change in the next point where the potential is evaluated. -/
theorem perturbation_bound {S : Type*} [MetricSpace S]
    (f reference : S → S) (observe referenceObserve V : S → E) (c : E)
    (L : ℝ≥0) (hV : LipschitzWith L V) (x : S) (error observationError mapError : ℝ)
    (he : ‖referenceObserve x-c-(V x-V (reference x))‖ ≤ error)
    (ho : ‖observe x-referenceObserve x‖ ≤ observationError)
    (hf : dist (f x) (reference x) ≤ mapError) :
    ‖observe x-c-(V x-V (f x))‖ ≤ error+observationError+L*mapError := by
  have hv : ‖V (f x)-V (reference x)‖ ≤ L*mapError := by
    rw [← dist_eq_norm]
    exact (hV.dist_le_mul _ _).trans (mul_le_mul_of_nonneg_left hf L.coe_nonneg)
  have hid : observe x-c-(V x-V (f x)) =
      (referenceObserve x-c-(V x-V (reference x)))+(observe x-referenceObserve x)+(V (f x)-V (reference x)) := by abel
  rw [hid]
  exact (norm_add_le _ _).trans (add_le_add ((norm_add_le _ _).trans (add_le_add he ho)) hv)

/-- A single reference certificate can cover a whole parameter region when
map and observable perturbations are uniformly bounded on a common invariant
domain. All errors remain explicit, including the proposed UI approximation. -/
theorem parameter_region_bound {P S : Type*} [MetricSpace S]
    (region : Set P) (domain : Set S) (f : P → S → S) (observe : P → S → E)
    (reference : S → S) (referenceObserve V : S → E) (referenceMean : E)
    (prediction : P → E) (L : ℝ≥0) (hV : LipschitzWith L V)
    (error observationError mapError approximationError M : ℝ)
    (hinv : ∀ p ∈ region, Set.MapsTo (f p) domain domain)
    (he : ∀ x ∈ domain, ‖referenceObserve x-referenceMean-(V x-V (reference x))‖ ≤ error)
    (ho : ∀ p ∈ region, ∀ x ∈ domain, ‖observe p x-referenceObserve x‖ ≤ observationError)
    (hf : ∀ p ∈ region, ∀ x ∈ domain, dist (f p x) (reference x) ≤ mapError)
    (ha : ∀ p ∈ region, ‖referenceMean-prediction p‖ ≤ approximationError)
    (hp : ∀ x ∈ domain, ‖V x‖ ≤ M)
    (p : P) (hregion : p ∈ region) (x : S) (hx : x ∈ domain) (N : ℕ) (hN : 0 < N) :
    ‖average (fun n => observe p ((f p)^[n] x)) N-prediction p‖ ≤
      error+observationError+L*mapError+approximationError+2*M/(N:ℝ) := by
  have hbound := orbit_bound (f p) domain (hinv p hregion) (observe p) V referenceMean
    (error+observationError+L*mapError) M
    (fun y hy => perturbation_bound (f p) reference (observe p) referenceObserve V referenceMean
      L hV y error observationError mapError (he y hy) (ho p hregion y hy) (hf p hregion y hy))
    hp x hx N hN
  have ht := norm_add_le (average (fun n => observe p ((f p)^[n] x)) N-referenceMean)
    (referenceMean-prediction p)
  rw [sub_add_sub_cancel] at ht
  linarith [ha p hregion]

end DSP.Coboundary

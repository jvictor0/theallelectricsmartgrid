import DSP.Analysis.CoboundaryBlocks
import DSP.Analysis.PeriodicOrbit
import DSP.ResponseMeasurement.ErrorBounds

/-! A discrete correction certificate bounds measurements of an actual sample
processor. Only the input word repeats: the output may drift or lock to a
longer period. These finite-window bounds do not assume existence of a time
average. The separate convergence criteria in `DSP.Coboundary` can establish
that stronger claim when arbitrarily accurate certificates are available. -/

noncomputable section
namespace DSP.Step
variable {S I : Type}

/-- Every sample prefix after a block boundary is a prefix of the same input
word, starting at the iterated block state. No periodic output is assumed. -/
theorem states_block_prefix (p : Step S I) (u : ℕ → I) (N : ℕ)
    (hu : ∀ n, u (N+n) = u n) (x : S) (block j : ℕ) :
    states p u x (block*N+j) =
      states p u ((fun y => states p u y N)^[block] x) j := by
  have hp (n k : ℕ) : u (n*N+k) = u k := by
    induction n with
    | zero => simp
    | succ n ih => rw [Nat.succ_mul]; simpa only [Nat.add_assoc, Nat.add_left_comm,
        Nat.add_comm, hu] using ih
  rw [states_add, states_blocks p u N hu]
  simp_rw [hp]

end DSP.Step

namespace DSP.ResponseMeasurement
variable {S I : Type}

/-- Observe after processing input n, matching audio output indexing. -/
def sampleOutput (p : Step S I) (u : ℕ → I) (observe : S → ℝ) (x : S) (n : ℕ) : ℝ :=
  observe (Step.states p u x (n+1))

theorem sampleOutput_block (p : Step S I) (u : ℕ → I) (observe : S → ℝ)
    (length : ℕ) (hu : ∀ n, u (length+n) = u n) (x : S) (block j : ℕ) :
    sampleOutput p u observe x (block*length+j) =
      sampleOutput p u observe ((fun y => Step.states p u y length)^[block] x) j := by
  unfold sampleOutput
  rw [Nat.add_assoc, Step.states_block_prefix p u length hu]

/-- A uniform correction residual on a trapped state domain bounds the
complex fundamental phasor of the actual sample run. -/
theorem coherent_coboundary_bound (p : Step S I) (u : ℕ → I) (observe : S → ℝ)
    (length : ℕ) (hu : ∀ n, u (length+n) = u n) (z : ℂ) (hz : z^length = 1)
    (domain : Set S) (hinv : Set.MapsTo (fun y => Step.states p u y length) domain domain)
    (V : S → ℂ) (mean : ℂ) (error M : ℝ)
    (he : ∀ x ∈ domain,
      ‖coherent z (sampleOutput p u observe x) length-mean-
        (V x-V (Step.states p u x length))‖ ≤ error)
    (hp : ∀ x ∈ domain, ‖V x‖ ≤ M)
    (x : S) (hx : x ∈ domain) (count : ℕ) (hc : 0 < count) :
    ‖coherent z (sampleOutput p u observe x) (count*length)-mean‖ ≤ error+2*M/count := by
  rw [DSP.Coboundary.coherent_blocks z _ length count hz]
  simp_rw [sampleOutput_block p u observe length hu]
  exact DSP.Coboundary.orbit_bound (fun y => Step.states p u y length) domain hinv
    (fun y => coherent z (sampleOutput p u observe y) length) V mean error M he hp x hx count hc

/-- Total power is averaged directly, so this includes every generated
harmonic, intermodulation component and any autonomous oscillation. -/
theorem meanSquare_coboundary_bound (p : Step S I) (u : ℕ → I) (observe : S → ℝ)
    (length : ℕ) (hu : ∀ n, u (length+n) = u n)
    (domain : Set S) (hinv : Set.MapsTo (fun y => Step.states p u y length) domain domain)
    (V : S → ℝ) (mean error M : ℝ)
    (he : ∀ x ∈ domain,
      |meanSquare (sampleOutput p u observe x) length-mean-
        (V x-V (Step.states p u x length))| ≤ error)
    (hp : ∀ x ∈ domain, |V x| ≤ M)
    (x : S) (hx : x ∈ domain) (count : ℕ) (hc : 0 < count) :
    |meanSquare (sampleOutput p u observe x) (count*length)-mean| ≤ error+2*M/count := by
  rw [DSP.Coboundary.meanSquare_blocks _ length count]
  simp_rw [sampleOutput_block p u observe length hu]
  exact DSP.Coboundary.orbit_bound (fun y => Step.states p u y length) domain hinv
    (fun y => meanSquare (sampleOutput p u observe y) length) V mean error M he hp x hx count hc

/-- A phasor error bounds coherent power without incorrectly averaging the
powers of individual blocks, which would lose inter-block phase cancellation. -/
theorem coherentPower_coboundary_bound (p : Step S I) (u : ℕ → I) (observe : S → ℝ)
    (length : ℕ) (hu : ∀ n, u (length+n) = u n) (z : ℂ) (hz : z^length = 1)
    (domain : Set S) (hinv : Set.MapsTo (fun y => Step.states p u y length) domain domain)
    (V : S → ℂ) (mean : ℂ) (error M : ℝ)
    (he : ∀ x ∈ domain,
      ‖coherent z (sampleOutput p u observe x) length-mean-
        (V x-V (Step.states p u x length))‖ ≤ error)
    (hp : ∀ x ∈ domain, ‖V x‖ ≤ M)
    (x : S) (hx : x ∈ domain) (count : ℕ) (hc : 0 < count) :
    |‖coherent z (sampleOutput p u observe x) (count*length)‖^2/2-‖mean‖^2/2| ≤
      (error+2*M/count)*(2*‖mean‖+(error+2*M/count))/2 :=
  coherentPower_error_bound _ mean _
    (coherent_coboundary_bound p u observe length hu z hz domain hinv V mean error M he hp x hx count hc)

end DSP.ResponseMeasurement

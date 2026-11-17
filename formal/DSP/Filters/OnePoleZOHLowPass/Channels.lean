import DSP.Filters.OnePoleZOHLowPass.Transfer
import DSP.Transfer.Channels

/-! Pointwise ZOH lowpasses with independent coefficients, states and inputs.
Transfer correctness is from rest; exact analog sample agreement permits
arbitrary initial states and a different time constant in every channel.
-/

noncomputable section

namespace DSP

deriving instance Functor for OnePoleZOHLowPass
deriving instance Functor for OnePoleZOHLowPass.Snapshot

namespace OnePoleZOHLowPass

open Channels

variable {n : ℕ} {K : Type}

/-- Capturing all channels and reading one is the same as capturing that channel. -/
theorem makeSnapshot_channel (f : OnePoleZOHLowPass (Fin n → K)) (i : Fin n) :
    channel i f.makeSnapshot = (channel i f).makeSnapshot := rfl

variable [Field K]

theorem natural : Natural (n := n) (step (R := Fin n → K)) step := fun _ _ _ => rfl

/-- Reading a channel after any number of steps equals running it alone. -/
theorem states_channel (f : OnePoleZOHLowPass (Fin n → K)) (u : ℕ → Fin n → K)
    (i : Fin n) (k : ℕ) :
    channel i (Step.states step (fun k => ⟨u k⟩) f k) =
      Step.states step (fun k => ⟨u k i⟩) (channel i f) k :=
  Step.states_map _ _ (channel i) (fun _ x => channel i x) (natural i) _ f k

theorem realizes_channels (f : OnePoleZOHLowPass (Fin n → K)) (hy : ∀ i, f.y i = 0) :
    Channels.Realizes step f (·.y) fun i =>
      Bilinear.const (f.alpha i) / (1 - Bilinear.const (1 - f.alpha i) * Bilinear.delay) :=
  Channels.realizes natural (fun i => realizes (channel i f) (hy i)) fun _ _ => rfl

/-- One vector step returns every channel's analog endpoint under its own
held input and time constant, with a shared sampling period. -/
theorem step_matches_interval_channels (T : ℝ) (τ input initial : Fin n → ℝ)
    (hT : 0 < T) (y : ℝ → Fin n → ℝ)
    (hcont : ∀ i, ContinuousOn (fun t => y t i) (Set.Icc 0 T))
    (hinit : ∀ i, y 0 i = initial i)
    (hode : ∀ i t, t ∈ Set.Ioo 0 T →
      HasDerivAt (fun t => y t i) ((input i - y t i) / τ i) t) :
    (step { alpha := fun i => 1 - Real.exp (-T / τ i), y := initial } { x := input }).y =
      y T := by
  funext i
  exact step_matches_interval T (τ i) (input i) (initial i) hT (fun t => y t i)
    (hcont i) (hinit i) (hode i)

/-- Every sample of the actual multichannel state agrees with the analog
solutions, including the initial sample and arbitrary changing held inputs. -/
theorem analog_matches_digital_channels (T : ℝ) (τ : Fin n → ℝ) (hT : 0 < T)
    (u : ℕ → Fin n → ℝ) (initial : Fin n → ℝ) (y : ℝ → Fin n → ℝ)
    (hy : ∀ i, AnalogOnePoleLowPass T (τ i) (fun k => u k i) (initial i) (fun t => y t i))
    (k : ℕ) :
    y (sampleTime T k) = (Step.states step (fun k => ⟨u k⟩)
      { alpha := fun i => 1 - Real.exp (-T / τ i), y := initial } k).y := by
  funext i
  change y (sampleTime T k) i = (channel i (Step.states step (fun k => ⟨u k⟩)
    { alpha := fun i => 1 - Real.exp (-T / τ i), y := initial } k)).y
  rw [states_channel, states_digitalRun]
  exact analog_matches_digital T (τ i) hT (fun k => u k i) (initial i) (fun t => y t i) (hy i) k

end OnePoleZOHLowPass
end DSP

end

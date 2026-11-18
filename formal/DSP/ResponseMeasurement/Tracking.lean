import DSP.ResponseMeasurement
import DSP.Step

/-! Compositional sinusoid settling, including a vanishing input transient. -/
noncomputable section
namespace DSP.ResponseMeasurement
open Filter
open scoped Topology

def Tracks (z : ℂ) (signal : ℕ → ℝ) (amplitude : ℂ) : Prop :=
  Tendsto (fun n => signal n-(amplitude*z^n).re) atTop (𝓝 0)

theorem Tracks.sub {z A B : ℂ} {u v : ℕ → ℝ} (hu : Tracks z u A) (hv : Tracks z v B) :
    Tracks z (fun n => u n-v n) (A-B) := by
  have h := Filter.Tendsto.sub hu hv
  simp only [sub_zero] at h
  apply h.congr
  intro n
  simp [sub_mul,Complex.sub_re]
  ring

def observed {S : Type} (p : Step S (Sample ℝ)) (s : S) (out : S → ℝ) (u : ℕ → ℝ) (n : ℕ) : ℝ :=
  out (Step.states p (fun i => ⟨u i⟩) s (n+1))

def Settles {S : Type} (p : Step S (Sample ℝ)) (s : S) (out : S → ℝ) (z H : ℂ) : Prop :=
  ∀ u A, Tracks z u A → Tracks z (observed p s out u) (H*A)

variable {S T : Type}

theorem Settles.project (p : Step S (Sample ℝ)) (q : Step T (Sample ℝ)) (π : S → T)
    (h : ∀ s x, π (p s x) = q (π s) x) {s : S} {out : T → ℝ} {z H : ℂ}
    (hq : Settles q (π s) out z H) : Settles p s (fun v => out (π v)) z H := by
  intro u A hu
  have hs := Step.states_map p q π (fun _ x => x) h (fun k => ⟨u k⟩) s
  have hr : observed p s (fun v => out (π v)) u = observed q (π s) out u := by
    funext n
    simp only [observed,hs]
  rw [hr]
  exact hq u A hu

theorem Settles.cascade (p : Step S (Sample ℝ)) (q : Step T (Sample ℝ)) (π : S → T) (d : S → ℝ)
    (h : ∀ s x, π (p s x) = q (π s) ⟨d (p s x)⟩) {s : S} {out : T → ℝ} {z H G : ℂ}
    (hd : Settles p s d z G) (hq : Settles q (π s) out z H) :
    Settles p s (fun v => out (π v)) z (H*G) := by
  intro u A hu
  have hs := Step.states_map p q π (fun s x => ⟨d (p s x)⟩) h (fun k => ⟨u k⟩) s
  have hr : observed p s (fun v => out (π v)) u = observed q (π s) out (observed p s d u) := by
    funext n
    simp only [observed,hs]
    rfl
  rw [hr,mul_assoc]
  exact hq _ _ (hd u A hu)

theorem Settles.sub {p : Step S (Sample ℝ)} {s : S} {f h : S → ℝ} {z F G : ℂ}
    (hf : Settles p s f z F) (hh : Settles p s h z G) :
    Settles p s (fun v => f v-h v) z (F-G) := by
  intro u A hu
  change Tracks z (fun n => observed p s f u n-observed p s h u n) ((F-G)*A)
  rw [sub_mul]
  exact (hf u A hu).sub (hh u A hu)

theorem Settles.congr {p : Step S (Sample ℝ)} {s : S} {f h : S → ℝ} {z H : ℂ}
    (heq : ∀ v x, h (p v x) = f (p v x)) (hf : Settles p s f z H) : Settles p s h z H := by
  intro u A hu
  have hr : observed p s h u = observed p s f u := funext (fun n => heq _ _)
  rw [hr]
  exact hf u A hu

theorem Settles.transfer_eq {p : Step S (Sample ℝ)} {s : S} {f : S → ℝ} {z H G : ℂ}
    (hf : Settles p s f z H) (heq : H = G) : Settles p s f z G := heq ▸ hf

end DSP.ResponseMeasurement

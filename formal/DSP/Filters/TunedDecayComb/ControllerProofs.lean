import DSP.Filters.TunedDecayComb.Controller
import DSP.Filters.TunedDecayComb.Linearity
import DSP.Filters.TunedDecayComb.Realization

namespace DSP.TunedDecayComb.Controller

variable {capacity : ℕ}

theorem request_latest {R : Type} (c : Controller capacity R) (a b : Coefficients capacity R) :
    (c.request a).request b = c.request b := rfl

theorem request_preserves_audio {R : Type} (c : Controller capacity R) (a : Coefficients capacity R) :
    (c.request a).state = c.state ∧ (c.request a).current = c.current ∧
    (c.request a).next = c.next ∧ (c.request a).mix = c.mix ∧ (c.request a).active = c.active :=
  ⟨rfl, rfl, rfl, rfl, rfl⟩

theorem begin_active {R : Type} [OfNat R 0] (c : Controller capacity R) (ha : c.active = true) :
    c.begin = c := by simp [begin, ha]

theorem begin_copy {R : Type} [OfNat R 0] (c : Controller capacity R)
    (desired : Coefficients capacity R) (ha : c.active = false) (hp : c.pending = some desired) :
    c.begin.state.first = c.state.first ∧ c.begin.state.second = c.state.first ∧
    c.begin.state.buffer = c.state.buffer ∧ c.begin.next = desired ∧
    c.begin.current = c.current ∧ c.begin.pending = none ∧ c.begin.mix = 0 ∧ c.begin.active = true := by
  simp [begin, ha, hp]

theorem begin_buffer {R : Type} [OfNat R 0] (c : Controller capacity R) :
    c.begin.state.buffer = c.state.buffer := by
  cases ha : c.active <;> cases hp : c.pending <;> simp [begin, ha, hp]

theorem tick_buffer (c : Controller capacity ℝ) (x : Sample ℝ) :
    (c.tick x).state.buffer = c.state.buffer.push (c.tick x).state.y := by
  unfold tick
  dsimp only
  split <;> try split <;> dsimp only
  all_goals rw [step_buffer, begin_buffer]

theorem tick_promotes (c : Controller capacity ℝ) (x : Sample ℝ)
    (ha : c.begin.active = true) (hm : 1 ≤ c.begin.mix + c.begin.increment) :
    (c.tick x).active = false ∧ (c.tick x).mix = 0 ∧
    (c.tick x).current = c.begin.next ∧
    (c.tick x).state.first = (step c.begin.current c.begin.next (some c.begin.mix) c.begin.state x).second := by
  simp [tick, ha, not_lt.mpr hm]

def Valid (c : Controller capacity ℝ) : Prop :=
  0 ≤ c.mix ∧ c.mix < 1 ∧ 0 < c.increment ∧ (c.active = false → c.mix = 0)

theorem begin_valid (c : Controller capacity ℝ) (hc : c.Valid) : c.begin.Valid := by
  cases ha : c.active with
  | true => simpa [begin, ha] using hc
  | false =>
    cases hp : c.pending with
    | none => simpa [begin, ha, hp] using hc
    | some desired => simpa [Valid, begin, ha, hp] using hc.2.2.1

theorem tick_valid (c : Controller capacity ℝ) (x : Sample ℝ) (hc : c.Valid) : (c.tick x).Valid := by
  have hb := c.begin_valid hc
  unfold tick
  dsimp only
  split
  · split
    · refine ⟨add_nonneg hb.1 hb.2.2.1.le, ?_, hb.2.2.1, ?_⟩
      · assumption
      · simp_all
    · exact ⟨le_rfl, by norm_num, hb.2.2.1, fun _ => rfl⟩
  · exact hb

theorem tick_increment (c : Controller capacity ℝ) (x : Sample ℝ) :
    (c.tick x).increment = c.increment := by
  unfold tick
  dsimp only
  split <;> try split <;> dsimp only
  all_goals cases ha : c.active <;> cases hp : c.pending <;> simp [begin, ha, hp]

theorem active_tick_mix (c : Controller capacity ℝ) (x : Sample ℝ)
    (ha : c.active = true) (hn : (c.tick x).active = true) :
    (c.tick x).mix = c.mix + c.increment := by
  simp only [tick, begin_active c ha, ha, if_true] at hn ⊢
  split <;> simp_all

theorem reset_rest (c : Controller capacity ℝ) (desired : Coefficients capacity ℝ) :
    (c.reset desired).state.AtRest ∧ (c.reset desired).mix = 0 ∧
    (c.reset desired).active = false ∧ (c.reset desired).pending = none := by
  simp [reset, State.AtRest, Branch.AtRest, DelayBuffer.history_empty]

private theorem copy_combination (a b : ℝ) (s t v : State capacity ℝ)
    (h : State.Combination a b s t v) :
    State.Combination a b { s with second := s.first } { t with second := t.first }
      { v with second := v.first } := ⟨h.1, h.2.1, h.2.1, h.2.2.2⟩

private theorem promote_combination (a b : ℝ) (s t v : State capacity ℝ)
    (h : State.Combination a b s t v) :
    State.Combination a b { s with first := s.second } { t with first := t.second }
      { v with first := v.second } := ⟨h.1, h.2.2.1, h.2.2.1, h.2.2.2⟩

/-- State copying and promotion preserve audio superposition. Controls and
accepted requests must be common to the three runs and independent of audio. -/
theorem tick_superposition (c : Controller capacity ℝ) (a b x y : ℝ)
    (s t v : State capacity ℝ) (h : State.Combination a b s t v) :
    State.Combination a b (({ c with state := s }).tick ⟨x⟩).state
      (({ c with state := t }).tick ⟨y⟩).state
      (({ c with state := v }).tick ⟨a * x + b * y⟩).state := by
  cases ha : c.active with
  | true =>
    have hstep := step_superposition c.current c.next (some c.mix) a b x y s t v h
    simp only [tick, begin, if_true]
    by_cases hm : c.mix + c.increment < 1
    · simpa only [hm, if_true] using hstep
    · simpa only [hm, if_false] using promote_combination _ _ _ _ _ hstep
  | false =>
    cases hp : c.pending with
    | none =>
      simpa only [tick, begin, ha, Bool.false_eq_true, if_false, hp]
        using step_superposition c.current c.next none a b x y s t v h
    | some desired =>
      have hstep := step_superposition c.current desired (some 0) a b x y
        _ _ _ (copy_combination a b s t v h)
      simp only [tick, begin, Bool.false_eq_true, if_false, if_true]
      by_cases hm : 0 + c.increment < 1
      · simpa only [hm, if_true] using hstep
      · simpa only [hm, if_false] using promote_combination _ _ _ _ _ hstep

/-- An active transition finishes in finitely many calls, even with a pending
request. A later transition may start afterwards; this theorem witnesses the
completion of the current one. -/
theorem transition_finishes (c : ℕ → Controller capacity ℝ) (x : ℕ → ℝ)
    (hv : (c 0).Valid) (_ha : (c 0).active = true)
    (hs : ∀ n, c (n + 1) = (c n).tick ⟨x n⟩) : ∃ n, (c n).active = false := by
  by_contra hn
  have hall : ∀ n, (c n).active = true := by
    intro n
    have h : ¬(c n).active = false := fun h => hn ⟨n, h⟩
    exact Bool.eq_true_of_not_eq_false h
  have hvalid : ∀ n, (c n).Valid := by
    intro n
    induction n with
    | zero => exact hv
    | succ n ih => rw [hs n]; exact tick_valid _ _ ih
  have hinc : ∀ n, (c n).increment = (c 0).increment := by
    intro n
    induction n with
    | zero => rfl
    | succ n ih => rw [hs n, tick_increment, ih]
  have hmix : ∀ n, (c n).mix = (c 0).mix + n * (c 0).increment := by
    intro n
    induction n with
    | zero => simp
    | succ n ih =>
      rw [hs n, active_tick_mix _ _ (hall n) (by rw [← hs n]; exact hall (n + 1)), hinc n, ih]
      push_cast
      ring
  obtain ⟨n, hn⟩ := exists_nat_gt ((1 - (c 0).mix) / (c 0).increment)
  have hmul := (div_lt_iff₀ hv.2.2.1).mp hn
  have hbound := (hvalid n).2.1
  rw [hmix] at hbound
  linarith

end DSP.TunedDecayComb.Controller

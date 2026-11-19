import DSP.DelayBuffer.Basic
import DSP.IIR.Proofs
import Init.Data.Vector.Lemmas

namespace DSP.DelayBuffer

variable {capacity : ℕ} {R : Type}

theorem ofDelay_samples (n : ℕ) (hp : 0 < n) (hb : n ≤ capacity) :
    (ofDelay n hp hb).val + 1 = n := by
  simp only [ofDelay]
  omega

theorem read_map {S : Type} (f : R → S) (s : DelayBuffer capacity R) (age : Fin capacity) :
    (s.map f).read age = f (s.read age) := by
  simp [map, read]

theorem push_map {S : Type} (f : R → S) (s : DelayBuffer capacity R) (x : R) :
    (s.push x).map f = (s.map f).push (f x) := by
  simp [map, push]

theorem readChannels_projection {ι : Type} (s : DelayBuffer capacity (ι → R))
    (ages : ι → Fin capacity) (ch : ι) :
    s.readChannels ages ch = (s.map (fun x => x ch)).read (ages ch) := by
  simp [readChannels, read_map]

theorem address_mod (head age : Fin capacity) :
    (address head age).val = (head.val + capacity - (age.val + 1)) % capacity := by
  unfold address
  split_ifs with h
  · have he : head.val + capacity - (age.val + 1) =
        capacity + (head.val - (age.val + 1)) := by omega
    rw [he, Nat.add_mod]
    simp [Nat.mod_eq_of_lt (show head.val - (age.val + 1) < capacity by omega)]
  · have he : head.val + capacity - (age.val + 1) =
        head.val + (capacity - (age.val + 1)) := by omega
    simpa only [he] using (Nat.mod_eq_of_lt
      (show head.val + capacity - (age.val + 1) < capacity by omega)).symm

theorem advance_mod (head : Fin capacity) :
    (advance head).val = (head.val + 1) % capacity := by
  unfold advance
  split_ifs with h
  · exact (Nat.mod_eq_of_lt h).symm
  · have he : head.val + 1 = capacity := by omega
    simp [he]

/-- The modular lowering uses neither saturating subtraction nor overflowing
addition when capacity is at most half the native unsigned maximum. -/
theorem modulo_intermediates_bounded (head age : Fin capacity) (limit : ℕ)
    (hcapacity : capacity ≤ limit / 2) :
    age.val + 1 ≤ capacity ∧ age.val + 1 ≤ head.val + capacity ∧
      head.val + capacity ≤ limit ∧ head.val + 1 ≤ limit := by
  have := head.isLt
  have := age.isLt
  omega

theorem address_advance_zero (head : Fin capacity) (h : 0 < capacity) :
    address (advance head) ⟨0, h⟩ = head := by
  apply Fin.ext
  unfold advance
  split <;> simp only [address] <;> split_ifs <;> dsimp at * <;> omega

theorem address_advance_succ (head : Fin capacity) (age : Fin capacity)
    (h : 0 < age.val) :
    address (advance head) age = address head ⟨age.val - 1, by omega⟩ := by
  apply Fin.ext
  unfold advance
  split <;> simp only [address] <;> split_ifs <;> dsimp at * <;> omega

theorem address_advance_ne (head age : Fin capacity) (h : 0 < age.val) :
    address (advance head) age ≠ head := by
  intro he
  have hv := congrArg Fin.val he
  unfold advance at hv
  split at hv <;> simp only [address] at hv <;>
    split_ifs at hv <;> dsimp at * <;> omega

theorem read_push_zero (s : DelayBuffer capacity R) (x : R) (h : 0 < capacity) :
    (s.push x).read ⟨0, h⟩ = x := by
  simp [read, push, address_advance_zero]

theorem read_push_succ (s : DelayBuffer capacity R) (x : R) (age : Fin capacity)
    (h : 0 < age.val) :
    (s.push x).read age = s.read ⟨age.val - 1, by omega⟩ := by
  have hne := address_advance_ne s.head age h
  have hv : s.head.val ≠ (address (advance s.head) age).val := by
    intro he
    exact hne (Fin.ext he.symm)
  unfold read push
  rw [Vector.getElem_set_ne _ _ hv]
  simp only [address_advance_succ _ _ h]

/-- The circular implementation simulates the reusable newest-first shift register. -/
theorem history_push (s : DelayBuffer capacity R) (x : R) :
    (s.push x).history = IIR.shift x s.history := by
  cases capacity with
  | zero => exact Fin.elim0 s.head
  | succ n =>
    funext i
    refine Fin.cases ?_ (fun j => ?_) i
    · exact read_push_zero s x (by omega)
    · change (s.push x).read j.succ = s.read j.castSucc
      rw [read_push_succ s x j.succ (by simp)]
      rfl

theorem history_empty [Zero R] (h : 0 < capacity) :
    (empty h : DelayBuffer capacity R).history = fun _ => 0 := by
  funext i
  simp [history, read, empty]

/-- Client-independent correctness: any trajectory that writes u has exactly
u's history, regardless of the number or order of reads between writes. -/
theorem history_trajectory {K : Type} [Ring K] (s : ℕ → DelayBuffer capacity K)
    (u : ℕ → K) (h0 : (s 0).history = fun _ => 0)
    (hs : ∀ n, s (n + 1) = (s n).push (u n)) (n : ℕ) :
    (s n).history = IIR.history u n := by
  induction n with
  | zero => simpa using h0
  | succ n ih => rw [hs, history_push, ih, IIR.history_succ]

theorem read_trajectory {K : Type} [Ring K] (s : ℕ → DelayBuffer capacity K)
    (u : ℕ → K) (h0 : (s 0).history = fun _ => 0)
    (hs : ∀ n, s (n + 1) = (s n).push (u n)) (n : ℕ) (age : Fin capacity) :
    (s n).read age = IIR.delay (age.val + 1) u n :=
  congrFun (history_trajectory s u h0 hs n) age

end DSP.DelayBuffer

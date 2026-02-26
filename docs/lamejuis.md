# LameJuis

**LameJuis** is an esoteric, layered sequencer that turns the six gate bits from the [Theory of Time](theory-of-time.md) into polyphonic pitch. The implementation is in `private/src/LameJuis.hpp` and `private/src/IndexArp.hpp`; the Nonagon wires the six time-loop gates into LameJuis and uses one LameJuis **output** per **trio** (three voices share one output’s pitch logic).

The design is **stateless** in time: at each moment **x** in **I⁶** (the six gate bits), the set of available notes and the choice of note are pure functions of **x**. Modulating the Theory of Time (e.g. phase modulation) therefore modulates this polyphonic process without breaking it.

---

## 1. The map M and the sheaf F^M_x(U)

Let **M : I⁶ → pitch**. Here “pitch” is represented as **volt-per-octave** (or equivalently log₂ of a just-intonation ratio). Composing M with the Theory of Time would give a single melody; we want many interlocking melodies, so we introduce a **lens** and a **sheaf**.

- **Lens U** — A subset of the 6 dimensions, represented as a 6-bit bitstring (`LameJuisInternal::Lens`, extending `BitVector`).  
  - **Read bits** (bits set to 1 in U): dimensions we “read”; two time slices must agree on these to be equivalent under lens U.  
  - **Co-mute bits** (bits set to 0 in U): dimensions we “co-mute” (ignore for equivalence); they can vary.  
  In the UI this is set per trio as “co-mutes”: lens bit *i* = 1 when *i* is **not** co-mute, i.e. when we read dimension *i* (`Output::CoMuteState::GetLens()` sets `lens.Set(i, !m_coMutes[i])`).
- **Equivalence** — For **x**, **y** in I⁶, define **x ~_U y** iff **x** and **y** agree on the read bits. In code: `Lens::Equivalent(a,b)` is `(a.m_bits ^ b.m_bits) & m_bits == 0`.
- **Harmonic Sheaf** — Define **F^M_x(U) = { M(y) | y ~_U x }**. So at time **x**, for a given lens U, we take all time slices equivalent to **x** under U and collect their M-values. This set is chosen statelessly from **x**; if **x** jumps (e.g. from time modulation), the set changes accordingly.

---

## 2. Trios and lens assignment

There are **9 voices** in **3 trios** of 3 voices each. Each trio is assigned **one LameJuis output** (there are 3 outputs, one per accumulator/output). The performer assigns a **lens U** to that output via the co-mute UI: which of the 6 dimensions are “read” vs “co-mute”. At each time **x** in I⁶, the trio must pick a note from **F^M_x(U)**. That choice is made by the **index arp** (see below), then by ordering and probing that set.

---

## 3. Index arp: clock, reset, rhythm, and range

The **index arp** (`IndexArp`, used per voice inside `NonagonIndexArp`) turns the monodromy (state-change count) of a chosen clock loop into a **point in a range** that is then used to pick a note from **F^M_x(U)**.

### 3.1 Clock and reset

- The performer chooses a **clock loop** and optionally a **reset loop** (an ancestor of the clock for the reset to be meaningful; or no reset, i.e. reset index -1).
- When the clock loop’s **gate changes** (`m_gateChanged`), the Nonagon sets  
`m_arpInput.m_totalIndex[trio] = TheoryOfTime::MonodromyNumber(clockSelect[trio], resetSelect[trio])`.  
So **m_totalIndex** is the number of **state changes** (gate flips) of the clock loop since the reset loop was at zero (or since the clock started if there is no reset). See [Theory of Time — Monodromy](theory-of-time.md#monodromy-relative-state-change-count).

### 3.2 Gate sequencer (rhythm)

- Each voice’s arp has a **rhythm** pattern: `m_rhythm[0..m_rhythmLength-1]` with `m_rhythmLength` default 8 (`IndexArp::x_rhythmLength`). Only some steps are “on”; the rest gate the voice off.
- From **m_totalIndex** we derive:
  - **m_rhythmIndex** = `m_totalIndex % m_rhythmLength` — position on the rhythm loop.
  - **m_motiveIndex** = `m_totalIndex / m_rhythmLength` — which “page” or cycle through the rhythm.
- A **trigger** happens only when `m_rhythm[m_rhythmIndex]` is true and the clock has just advanced (we’re in the `m_clock` / `m_triggered` path). Then we compute **m_index**: the **physical index** among the **on** steps (0 to NumNotes()-1), i.e. how many rhythm steps that are on have been passed up to and including the current step.

### 3.3 Point in range

- The arp exposes a **range** per voice: **m_offset**, **m_interval**, **m_pageInterval**, **m_min**, **m_max**, plus **m_invert**, **m_retro**, **m_cycle**.
- **Output** is computed as  
`GetOutput(m_index, m_motiveIndex)` =  
`m_offset + m_index * m_interval + m_motiveIndex * m_pageInterval`,  
then optionally wrapped (cycle) or inverted, then scaled from [0,1] to **[m_min, m_max]**.
- So the **index** (physical step among on steps) and **motive index** (rhythm page) together determine a single float in a range. That float is passed to LameJuis as either a **percentile** (harmonic mode) or a **pitch to quantize** (melodic mode).

- **When the Nonagon updates the index arp** — The Nonagon only updates index-arp inputs and runs the index arp (and LameJuis) when **m_theoryOfTime.m_anyChange** is true — i.e. when at least one time loop’s integer position changed this frame. On those frames it calls `SetIndexArpInputs` then `m_indexArp.Process`. For each trio, **m_totalIndex** is set to the monodromy when the selected clock loop’s gate has just changed (`m_gateChanged`), or to 0 if no clock is selected; **m_clocks[i]** is set to **m_gateChanged** for loop *i*; and **m_read** is set for voices whose lens reads a dimension that just ticked. When there is no Theory of Time change, the arp and LameJuis are not run that frame.

---

## 4. Ordering F^M_x(U) and probing: harmonic vs melodic

Once we have the set **F^M_x(U)** (all M(y) for y ~_U x), we order it and probe it with the index-arp output to get the final note.

- **Harmonic mode** — The set is ordered by **pitch** (volt-per-octave), i.e. by the just-intonation ratios. The list is sorted by `m_pitch` (and then by `m_result` for ties). The index-arp output is used as a **percentile** in [0,1): we map it to an index into this sorted list (`PercentileToIx(percentile)`). So we “pick a point along the sorted list” of pitches. Integer part of the percentile can be used as an octave offset (`result.m_pitch += floor(percentile)`).
- **Melodic mode** — Pitches are **octave-reduced** (`OctaveReduce()`: `m_pitch -= floor(m_pitch)`), then the list is sorted. The index-arp output is interpreted as a **pitch to quantize**: we find the nearest pitch in this (octave-reduced) list (`Quantize(toQuantize)` / `QuantizeModOctave`), including wraparound at 0/1. So we’re “quantizing” a continuous pitch into the scale defined by F^M_x(U) mod octave.

In both cases the result is a single pitch (volt-per-octave) per voice; that pitch is then used by the rest of the synth (e.g. V/O output, possible octave shift from the UI). Whether a **trigger** is emitted (note on) for that pitch is decided by the [Multi-Phasor Gate](multi-phasor-gate.md) (pitch-changed vs sub-trigger, mutes, interrupt).

---

## 5. The map M: logic operations and accumulators

**M(x)** is not a single ratio; it is computed by a **matrix** of **logic operations** feeding **accumulators**, whose outputs are combined additively in volt-per-octave (i.e. multiplicatively as ratios).

### 5.1 Structure

- There are **6 logic operations** and **3 accumulators**. Each operation outputs to **one** of the 3 accumulators (selected by a switch: Down/Middle/Up → target 2/1/0).
- For a given **x** in I⁶ (the 6 gate bits), each operation evaluates to **0 or 1**. The **MatrixEvalResult** for **x** stores, for each accumulator, how many operations target it (`m_total[acc]`) and how many of those are high (`m_high[acc]`). The **pitch** in volt-per-octave is  
**pitch = Σ_acc accumulators[acc].m_intervalValue * m_high[acc]**  
So in ratio space this is a product of simple factors: each accumulator has an **interval** (e.g. octave, fifth, major third) and an **exponent** 0 or 1 (or more generally 0..m_total[acc] when several ops target the same acc). So **M(x)** is a just-intonation ratio expressed as a product of these simple intervals raised to 0/1 (or small integer) exponents.

### 5.2 Logic operations

Each **LogicOperation** (the “simple functions” in the user’s description) does the following:

- **Input**: the 6-bit time slice **x** (and a fixed configuration of the operation).
- **Per-bit treatment** — For each of the 6 dimensions we have a **MatrixSwitch**: **Muted** (ignore), **Normal** (use the bit), **Inverted** (use the bit inverted). So we get an effective 6-bit vector: only “active” (non-muted) bits matter, and some are flipped. This is implemented as `m_active` (which bits are used) and `m_inverted` (which of those are inverted). `GetTotalAndHigh` does `inputVector &= m_active`, `inputVector ^= m_inverted`, then counts **countTotal** = number of active bits and **countHigh** = number of 1s in the result.
- **Operator** — A function of (countTotal, countHigh) → bool:
  - **Or**: high if countHigh > 0.
  - **And**: high if countHigh == countTotal.
  - **Xor**: high if countHigh is odd (parity — a Walsh-like function).
  - **AtLeastTwo**: high if countHigh >= 2.
  - **Majority**: high if 2*countHigh > countTotal.
  - **Off**: always false.
  - **Direct**: the output is **m_direct[countHigh]** — a lookup table indexed by how many of the (active, possibly inverted) bits are 1. So for each **k** in 0..6 we can choose whether the operation outputs true or false when exactly **k** bits are high.
- **Generalized Walsh** — The default for **Direct** is `m_direct[j] = (j % 2 == 1)`, so **only odd** counts pass. That is parity (Xor), i.e. a Walsh function. By changing the **Direct** table, the performer can select which counts (0..6) pass; these behave like **generalized Walsh functions** on the 6-bit input (with the given active/inverted mask).

So **M(x)** is built from 6 such boolean functions; each contributes 0 or 1 to one of 3 accumulators; the accumulators have fixed intervals (octave, fifth, third, etc.); and the final pitch is the sum in V/O of (interval × exponent) per accumulator.

### 5.3 Extra Timbre Modulators

In addition to pitch, the logic matrix provides **extra timbre modulators**. For each of the 3 accumulators, the matrix computes the ratio of operations that evaluated to high versus the total number of operations targeting that accumulator (`m_high[acc] / m_total[acc]`). This yields 3 discontinuous values in [0, 1] per time slice, which the Nonagon exposes as `m_extraTimbre` (after slewing). These can be routed to DSP parameters (like filter cutoff or wavefolder depth) to provide rhythmic modulation that is perfectly synchronized with the pitch sequence.

---

## 6. Statelessness

Because:

- the Theory of Time gates are a pure function of time,
- the monodromy (and hence **m_totalIndex**) is a pure function of time when the clock gate changes,
- the index arp maps that to a point in a range,
- the lens and M define **F^M_x(U)** purely from **x**,
- and the ordering and probing of **F^M_x(U)** are deterministic,

the whole polyphonic note-generation process is a **pure function of time**. Modulating the Theory of Time (e.g. phase modulation, different clock/reset, or different topology) only changes **x** and the index over time; the logic remains consistent.

---

## Related

- [Theory of Time](theory-of-time.md) — supplies the 6 gate bits and monodromy.
- [Glossary](glossary.md) — **LameJuis**, **lens**, **index arp**, **LogicOperation**, **accumulator**, **sheaf**.
- [Documentation index](index/README.md) — Nonagon and trios.


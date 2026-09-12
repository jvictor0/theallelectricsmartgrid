# Phase-Driven AHD Envelopes

`AHD` in `private/src/AHD.hpp` evaluates attack, hold, and decay from absolute modulated global phase. At a trigger, its input captures:

- the global phase at the start of the microblock;
- the relevant source loop's cycle ratio to the global loop;
- the envelope period in samples.

The running envelope computes:

```
samples = abs(globalPhase - startGlobalPhase) * phaseRatio * envelopePeriodSamples
```

It does not retain a source loop index, track topology edits, or reconstruct winding. A multiplier or parent edit during attack, hold, or decay cannot change the captured timing. Retriggering captures the new timing. In the production voice path the source ratio comes from loop 0; the voice gate ratio used to calculate the envelope period is a separate quantity.

The absolute distance preserves existing reverse behavior: moving back toward the trigger retraces the envelope while it is running; moving past the trigger increases distance again. Once decay reaches idle, moving backward does not restart the envelope. Explicit release uses the existing sample-driven decay.

Attack and decay controls remain sample-based increments. Hold is configured in loop periods (`m_holdLoops`) and converted with the captured envelope period during evaluation. Live hold changes therefore remain available without accidentally adopting a new topology's period. Tempo or phase modulation changes global phase motion and consequently envelope progression.

Physical-model presets can have shorter or longer holds after this change. Previously, the slewed control setter converted hold using a temporary input's default 48,000-sample period. It now slews the loop count and uses the period captured at the trigger, like the other envelope sources. With a 3,000-sample captured period, the same hold knob setting produces one sixteenth of the previous hold duration in envelope sample units. Attack and decay mappings are unchanged.

`AHDControl` carries trigger, release, source phase ratio, and envelope period. It no longer relays an elapsed-sample counter from the gate. See [Multi-Phasor Gate](multi-phasor-gate.md) for how the voice period is captured.

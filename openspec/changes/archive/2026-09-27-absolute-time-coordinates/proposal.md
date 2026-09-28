## Why

Before this change, Theory of Time wrapped phase and integer positions early, then reconstructed lost cycles through winding trackers and recursive monodromy. This also restarted fractional-speed sample playback every source loop. Absolute coordinates make playback continuous across loop boundaries and simplify time consumers.

## What Changes

- Store absolute unmodulated and modulated global phases as doubles; derive loop phases and signed 64-bit lattice positions directly from phase and accepted topology.
- **BREAKING**: Replace synonymous direct/indirect, independent/dependent, master/global, and unwound accessors with one consistently named time API; migrate all callers in the same change.
- Replace recursive monodromy with signed loop-cycle positions and optional reset modulo with ancestry inferred from period divisibility, including negative positions and self reset to zero.
- Use the undoubled LCM lattice and editable per-loop rhythms. Default gates alternate full cycles; gate/size/reset edits wait for that loop's next modulated tick.
- Add paired rhythm/reset controller pages and their StateSaver entries. Preserve current values when older patches omit those keys, and include the final registered state when switching scenes.
- Latch LameJuis matrix and co-mute edits on their own input ticks, including repeated gate values. Exclude empty rows from accumulator totals, include those totals in section identity, and initialize the grid with its accepted lens at startup.
- Derive voice cycle ratios from the undoubled LCM of clock/read ratios while preserving the separate half-voice-cycle note-gate cutoff.
- Accept a requested parent and multiplier together only at simultaneous cycle boundaries of the old and requested parents.
- Apply sample-playback speed before wrapping at the playback output, including fractional and negative speeds.
- Preserve PolyXFader's existing periodic waveform shaping, user controls, automatic amplitude weights, and smoothing.
- Capture an AHD's phase-to-global ratio at trigger and track only modulated global phase thereafter; active envelopes do not follow later topology edits.
- Remove timebase-related winding reconstruction from gates, recording, synchronization, playback, and display adapters. Preserve transport, persistence keys, and realtime scheduling.
- Publish accepted timebase periods, rhythms, harmonic sections and chooser settings, and resolved index-arp parameters into UI snapshots. Compose the shared math into raw pitch queries and bounded per-voice sequence caches off the audio thread.

- Replace the Melody role with a trio/voice pitch preview, read-gate rulers, interpolated choice traces, and a continuous modulated-phase playhead.
- Use current note inputs at Deep Vocoder triggers and prevent residual feedback from creating Partial Machine noise from silence.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `phasor-timebase`: Absolute time, consistent API, simultaneous parent boundaries, direct loop-cycle positions, interpolation under changing topology, and published period/rhythm snapshots.
- `source-machine-oscillators`: Absolute-phase sample playback with fractional and reverse speed.
- `polyxfader-lfos`: Absolute phase inputs with existing per-loop periodic waveform semantics.
- `ahd-envelopes`: Trigger-captured ratio and global phase origin, with topology-stable envelope timing.
- `multi-phasor-gate`: Global absolute phase distance and consistent trigger-captured timing.
- `lamejuis-sequencer`: Signed loop-cycle positions, tick-latched configuration, active-row accumulator counts, denominator-aware section changes, correct startup grid mapping, and harmonic/index-arp snapshots sharing the live evaluation math.
- `nonagon-sequencer`: Tick-driven clock/read wiring, undoubled voice cycle ratios, raw UI pitch queries, and bounded sequence caches.
- `controller-midi-io`: Paired rhythm/reset pages selected by the Wrld.Bldr aux grid.

- `grid-visualizers`: Forward Melody roll, candidate/choice mappings, gate rulers, and continuous playhead.
- `phase-vocoder`: Current note inputs at trigger time between FFT analysis hops.
- `partial-machine`: Silence-preserving residual reduction feedback.

## Impact

The headers-mostly C++17 DSP clock and its consumers, standalone doctest coverage, active Markdown documentation, and controller grids. Existing patch keys remain stable and the rhythm pages add StateSaver entries without changing its missing-key policy. There is no new dependency or clock mode. The obsolete LaTeX time description and PDF are removed. Existing half-cycle rhythms now advance once per full loop cycle; this deliberately changes rhythmic timing without changing clock frequency. Unmodulated versus modulated topology-boundary timing remains as it is; this change does not promise continuity across that existing mismatch or user parameter edits.

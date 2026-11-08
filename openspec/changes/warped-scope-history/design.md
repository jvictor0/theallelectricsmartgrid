## Context

TheoryOfTime owns the continuous write-head coordinate F and its latest-forward inverse G. ScopeWriter stores real-time samples and builds existing FIR LODs on the UI thread. ScopeReader currently constructs its axis from real-time markers. Delay has a separate positional recorder and min/max envelope path.

## Goals / Non-Goals

**Goals:** compose reads with G; place cycle starts, ends, and transfer positions in warped coordinates; undo crossed boundaries generically; display the delay's rectified write signal in dB; make new concurrent reads valid C++.

**Non-Goals:** change audible envelope behavior, improve transport resets, add transport generations, redesign LOD filters, preserve peak/min/max envelopes, or add UI locking on the audio thread.

## Decisions

- Expose a capacity-independent inverse read interface. Atomic inverse entries permit live UI reads. A captured view limits reads to the published real sample, current warped cursor, and retained warped span; unavailable endpoints reject the query before interpolation. Audio reads retain their existing circular semantics.
- Scope streams optionally bind to that interface. Their origin and decimation convert absolute real engine timestamps to stored samples. Unbound scopes retain real-time behavior. Existing LOD selection/filtering remains intact.
- Record real and warped positions for starts and ends. On descending movement remove all crossed ends/starts, restoring the preceding interval state. Ignore start/end callbacks during descending movement. An unchanged coordinate still permits existing restart starts. Fractional events interpolate the observed time step.
- Keep marker history on the writer thread. Publish changed marker slots and small time metadata through atomic payloads with a publication revision; UI readers accept a coherent bounded copy without waiting on the audio thread. Level-zero samples use atomic scalar lanes; higher LODs remain UI-owned vectors.
- ScopeReader computes cycle layout in warped coordinates and composes value reads through the captured inverse. Modulation overlays map the same axis points back to real history; real-time random/fallback axes retain their behavior.
- Delay records the absolute value of its actual completed host write in a dedicated scope. Forward full-display-cycle crossings record starts. Delay visualization uses the generic reader and logarithmic amplitude conversion after sampling. Keep the existing relative head phase behavior and signed delay-return spectrum source.

## Risks / Trade-offs

- Live inverse entries can advance during a paint → reject entries newer than the captured real cutoff; mixed-age gaps are acceptable.
- Circular storage and finite marker history can expire → return unavailable instead of reading aliased coordinates.
- Atomic stores add cost → verify delay performance smoke cases and keep high-rate payload operations relaxed and lock-free.
- Existing tests assume independent clocks or old envelope buckets → preserve behavioral assertions where applicable and replace obsolete bucket assertions with scope behavior tests.

## Migration Plan

Implement the inverse view and marker/read infrastructure first, wire consumers, replace the delay envelope path, then run core and visualizer suites and delay perf smoke checks in the existing build directories. Keep the user's existing inverter refactor intact. No data-format migration is needed.

## Open Questions

None blocking. Backward envelope triggers do not create scope starts. Transport and LOD behavior stay as agreed.

# Time-warp delay corrections

The user approved five changes following a code audit and executable reproductions. Preserve time reversals, the forward-only inverse-map writing policy, fractional Mult behavior, and circular physical-buffer semantics.

1. Protect each complete 4096-sample phase-vocoder analysis window in real audio time. The existing 1024-sample warped-coordinate read-head margin cannot guarantee this. At grain launch, after inverse mapping and the optional sample offset, limit only unsafe starts to the latest complete analysis window. Include the two future samples needed by cubic audio interpolation. Existing safe starts retain their timing. Recorded sample-bank playback is unaffected.
2. Use signed integer coordinates and Euclidean modulo for inverse-map scatter and physical audio/inverse/envelope indexing. Negative absolute positions are valid and wrap by the actual physical buffer capacity.
3. Give the Theory of Time modulation LFO a continuous sine-to-triangle Shape response over its full knob range. This is local to the time-warp LFO: retain the existing Shape behavior for voice and quad LFOs.
4. Map time-warp Skew's knob range onto attack fractions 0.1 through 0.9, keeping its midpoint at 0.5. Preserve the existing slow parameter filtering.
5. Use linear interpolation both when inserting real timestamps into the inverse map and when looking up a fractional warped position. Retain cubic interpolation for audio samples. With two-point interpolation the map can be filled from each adjacent ascending pair; no four-point lookahead is needed.

The real-time recording frontier belongs to the live delay writer. The shared GrainManager accepts an optional latest recorded sample time (unbounded for immutable sample banks). QuadGrainManager supplies each channel's frontier after writing that sample. For latest recorded sample T, the latest permitted start is T - (N - 1) - 2; the previous analysis window starts H samples earlier and still uses circular physical addressing.

Regression tests cover negative and multi-wrap coordinates, forward recording across zero, inverse-map overshoot, reversal followed by renewed forward writing, held positions, and sustained sine output under fast warping. Clock tests cover full-range continuous Shape and bounded Skew without changing other PolyXFader uses. Include a real TheoryOfTime-to-delay test at high Mult with reversals. Run the repository suite and focused undefined-behavior sanitizer checks. Measure scatter work under the accepted control limits rather than adding another DSP behavior change.

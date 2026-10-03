# Melody viewport and diminished ruling

**Goal:** Show repeating periods up to 256 ticks and add a three-level pitch grid.

**Design:** Retain the existing period selection and fallback order. Increase the
visualizer limit from 128 to 256. Keep the shared cache's 128-point processing
budget; allow up to two fills per displayed voice, stopping once the viewport is
covered. For scrolling, request a centered cache range so both viewport edges fit
at every position. Keep the reference voice populated when muted.

Pitch ruling stays anchored to integer octaves, with subdivisions every quarter
octave. Draw octaves at 0.40 alpha / 1.25 px, tritones at 0.25 alpha / 1.0 px,
and intervening minor thirds at 0.15 alpha / 1.0 px. This gives the requested
diminished ruling without changing pitch evaluation or musical data.

**Files:** `JUCE/SmartGridOne/Source/SequencerMelodyVisualizerComponent.hpp` and
`private/test/juce-visualizers/melody.cpp`.

**Validation:** Use the native JUCE visualizer suite. Retain existing 128-tick
coverage, exercise 256-tick alignment and cache coverage at positive/negative
boundaries and seeks, preserve fallback order using periods above the new limit,
and check rendered grid intensity and placement across fractional pitch bounds.

- [x] Build and run the unchanged native visualizer suite.
- [x] Update threshold fixtures and rendered-grid expectations; confirm failures.
- [x] Implement the new limit, bounded cache fills, centered scrolling cache,
  and diminished ruling.
- [x] Run the complete native suite and review the final diff.

**Result:** All 34 native tests and 64,645 assertions pass, covering the final
256-tick limit and the grid hierarchy. The updated window fixtures first failed
against the previous 512-tick limit. Independent review reported no findings.
JUCE-rendered previews and iPad feedback confirmed the brighter grid hierarchy.

**iPad feedback:** The initial 3–10% opacity and subpixel strokes were too faint
on the device. Increase all three levels as specified above, retaining their
relative emphasis and using at least one-pixel strokes.

The brighter ruling was approved on the iPad. Reduce the viewport limit from the
initial 512 ticks to 256 ticks for the final landing.

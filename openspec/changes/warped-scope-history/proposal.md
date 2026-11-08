## Why

LFO, envelope, and delay displays currently use different time axes and marker histories. The shared Theory of Time inverse can make these displays show the latest forward observations in warped coordinates, with backwards travel undoing scope boundaries.

## What Changes

- Compose scope and modulation-overlay reads with the shared inverse while retaining the existing LOD filters and transport behavior.
- Record real and warped scope boundaries, undo crossed starts and ends generically, and suppress starts from backwards envelope triggers.
- Expose the inverse through a bounded UI read view with atomic shared payloads and published time limits.
- Replace the delay's positional recorder and min/max envelope buckets with a rectified write-signal scope, displayed in dB on the common axis.

## Capabilities

### New Capabilities
- `warped-scope-history`: Shared warped-coordinate scope reads, publication, and reversible scope markers.

### Modified Capabilities
- `quad-delay`: Use the common scope pipeline for the delay write envelope and head markers.
- `ui-visualization-pipeline`: Publish atomic samples and coherent markers, and compose bound scope axes with the inverse.
- `grid-visualizers`: Read warped scopes through the captured inverse view.

## Impact

TheoryOfTime, TimeWarpInverter, ScopeWriter/ScopeReader, modulation history, QuadDelay, JUCE scope and delay visualizers, core and visualizer tests. No new transport reset policy, DSP envelope undo, or LOD filtering design.

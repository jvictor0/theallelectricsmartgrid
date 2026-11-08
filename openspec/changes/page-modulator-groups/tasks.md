## 1. Metadata and publication

- [x] 1.1 Define page groups, parameter assignments, expanded source identities, and per-source downsampling flags.
- [x] 1.2 Apply selective manager downsampling and host-only modulation noise generation.

## 2. Random generator ownership and UI

- [x] 2.1 Allocate ten random generators per track and eight quad generators with independent group pairs.
- [x] 2.2 Move wavetable morph random ownership into DualWaveShapingVCO and update source wiring and visibility checks.
- [x] 2.3 Extend source history and encoder visualizers to cover all new random identities.

## 3. Validation

- [x] 3.1 Add regression coverage for group/page mappings, lane counts, distinct random pairs, filtering policy, noise cadence, and internal morph sources.
- [x] 3.2 Run relevant core, visualizer, performance, and specification checks; review the complete diff and update current specs.

## Why

New modulation sources reuse other slots' colors, and SHF1 uses cyan while its Nonagon cells use Ocean. The accepted palette separates all fifteen positions and emphasizes contrast within the five functional pairs.

## What Changes

- Apply the accepted palette with Random and Envelope pairs exchanged: slots 2/3 use Berry/Lime, and 4/5 use Gold/Violet.
- Share the Ocean, Indigo, and SeaGreen definitions between Sheafy modulators and their corresponding Nonagon cells.
- Keep associated parameter encoders and modulator visualizers consistent with their source colors; preserve grey Spread and white Noise.
- Document the fixed slot-to-color convention, exact RGB values, pair grouping, and perceptual design rationale.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `grid-visualizers`: Define the approved palette consistently across groups, source views, associated parameter encoders, and Nonagon cells.

## Impact

Modulator skin metadata, shared palette constants, Nonagon cell colors, associated parameter metadata/baselines, catalogue tests, and encoder documentation. Source identities, group bindings, glyphs, DSP, and saved modulation positions retain their existing behavior.

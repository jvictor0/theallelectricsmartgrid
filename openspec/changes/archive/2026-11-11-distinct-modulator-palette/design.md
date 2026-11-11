## Context

The catalogue stores colors on source identities, while fixed groups bind those sources to fifteen local slots. Colors must follow the documented local-slot convention across every group without turning slot numbers into a runtime palette lookup. SHF1 must share Ocean with the first Nonagon equation, rather than cyan.

## Goals / Non-Goals

**Goals:** Apply the accepted perceptual palette with Random and Envelope pairs swapped; share Sheafy/Nonagon colors; make controls belonging to a published source match it; retain grey Spread and white Noise; record the final RGB mapping and rationale.

**Non-Goals:** Changing glyphs, DSP, source identities, modulation group placement, persistence, unrelated page colors, or introducing runtime color optimization.

## Decisions

- Add a small `SmartGridOneModulatorColors.hpp` header containing named constexpr colors. The source catalogue remains explicit; Nonagon can consume the same constants without depending on modulator routing or encoder layout. Do not replace the general-purpose `Color` constants, because their other users have independent color meanings.
- Final slot colors are: 0 `#FF0047`, 1 `#00DBFF`, 2 `#C2257F`, 3 `#76C70E`, 4 `#FFC628`, 5 `#8D5FFF`, 6 `#FF12D2`, 7 `#556A00`, 8 `#19F8BA`, 9 `#383DF9`, 10 `#00FD1C`, 11 `#8F8F8F`, 12 `#B70000`, 13 `#FCC4FF`, 14 `#FDFDFD`.
- Functional pairs are 0/1, 2/3, 4/5, 6/7, and 12/13; Sheafy remains a trio. The offline Oklab search balances minimum palette separation with minimum functional-pair separation and distinct controller hues. The approved pair swap preserves those distances. This is a fixed designed palette, not a claim of a proven universal optimum.
- Keep existing typed color bindings for Voice/Quad LFOs and the modulation/amp envelopes. Use the same typed lookups for uniquely associated VCO, physical excitation, and Sub controls that still use literal category colors. Tempo modulation and shared effect/filter controls retain their own identities.
- Route Nonagon equation colors and the corresponding matrix/RHS/interval cell colors through the shared anchors, including dim variants.
- Update the existing independent RGB baselines and presentation checks. Extend the existing catalogue contract to cover every populated local slot and the cell/source color agreement; validate constructed parameter encoder colors through the existing catalogue baseline.

## Risks / Trade-offs

- Color recognition changes across existing controls → document names, RGB values, source associations, and the stable slot convention.
- Source IDs are not local slot numbers → validate colors through all real group bindings, including sparse Quad/Global groups.
- Floating-point optimization is not runtime behavior → ship only the final 8-bit RGB values. Grey/white remain exact and all thirteen chromatic colors retain distinct Twister codes.

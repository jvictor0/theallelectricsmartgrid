# Scene Manager

The `SceneManager` (`private/src/SceneManager.hpp`) is the central coordinator for scene selection, scene blending, and scene-change signaling.

## Role

`SceneManager` is the source of truth for:

- active left scene (`m_scene1`)
- active right scene (`m_scene2`)
- global blend factor (`m_blendFactor`)
- shift state (`m_shift`)

It allows the rest of the parameter system to read a single blended value from scene-stored arrays:

- `GetSceneValue(values)` computes
  - `values[m_scene1] * (1 - m_blendFactor) + values[m_scene2] * m_blendFactor`

## Change detection

At the start of each sample, `SceneManager::Process()` compares current scene/blend state to previous values and emits flags:

- `m_changed` when blend or scene assignment changed
- `m_changedScene` when scene indices changed, or a change moves from or to a blend endpoint

Consumers (notably encoder-bank processing) use these flags to refresh topology/state only when needed.

## Ownership and saved selectors

`TheNonagonSquiggleBoyInternal` owns `SmartGridOneContext` and passes its pointer to the Nonagon, StateSaver, and SquiggleBoy encoder system. The context supplies the scene manager and parameter event logger before consumers register state or construct encoder cells.

The left/right scene indices and active trio are registered with the global single-scene `StateSaver`. Their controller setters write through cached `State*` handles. The blend factor and shift flag remain runtime fields. Patch JSON now includes `blend`; fader/blend input messages emit typed parameter events. `SmartGridOneContext` owns the scene manager, recorder, and `ParamEventLogger`. The logger is a separate hook for saved-value edits; it does not replace `SceneManager::Process()` or its change flags.

## Related

- [State Saver](state-saver.md)
- [Encoder System](encoder-system.md)
- [Smart Grid Integration](smart-grid.md)

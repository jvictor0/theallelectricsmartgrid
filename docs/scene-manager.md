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

Each frame, `SceneManager::Process()` compares current scene/blend state to previous values and emits flags:

- `m_changed` when blend or scene assignment changed
- `m_changedScene` when scene indices changed

Consumers (notably encoder-bank processing) use these flags to refresh topology/state only when needed.

## Cell registration

`SceneManager` can track registered stateful cells (`StateEncoderCell`) through `RegisterCell(...)`.

- Registration is enabled when external-state mode is active (`m_externalState`).
- This provides a central place for scene-aware state propagation.

## Related

- [State Saver](state-saver.md)
- [Encoder System](encoder-system.md)
- [Smart Grid Integration](smart-grid.md)

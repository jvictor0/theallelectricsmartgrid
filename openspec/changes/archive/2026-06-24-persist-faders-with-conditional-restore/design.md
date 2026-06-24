## Context

Patch persistence currently flows through `StateInterchange`: the message thread parses or dumps JSON, while the audio thread calls `TheNonagonSquiggleBoyInternal::ToJSON` and `FromJSON`. The saved patch contains `nonagon`, `squiggleBoy`, `stateSaver`, and `configGrid`; `squiggleBoy` currently means encoder state only. The sixteen fader values are live input state in `m_squiggleBoyState.m_faders`, updated from analog `ParamSet14` messages and republished through `AnalogUIState`.

Startup currently loads persisted controller configuration before requesting the current patch load: `MainComponent::LoadConfig()` calls `m_nonagon.ConfigFromJSON(nonagonConfig)` and only afterwards calls `m_fileManager.LoadCurrentPatch()`. `NonagonWrapperWrldBldr::ConfigFromJSON()` attempts to reconnect the saved WRLD.BLRD input and output, and `IsOpen()` is true only when both are open. This means the current startup path can decide whether to restore faders before it arms the patch load request.

## Goals / Non-Goals

**Goals:**

- Persist all sixteen SquiggleBoy fader values on every patch save.
- Allow patch load callers to choose whether saved faders overwrite live fader state.
- Use WRLD.BLRD attachment state at the top-level load request: restore when no WRLD.BLRD is open, skip when it is open.
- Force reload-style patch loads to skip fader restoration, preserving current live/controller fader positions.
- Keep old patch files valid and leave faders unchanged when no fader array exists.
- Cover startup ordering so controller config is established before current-patch fader restore policy is selected.

**Non-Goals:**

- Do not change encoder serialization or move encoder state into `StateSaver`.
- Do not persist physical controller state in app configuration.
- Do not infer individual physical fader pickup positions or implement soft takeover.

## Decisions

1. Store faders as a root patch member, e.g. `faders`, owned by `TheNonagonSquiggleBoyInternal`.

   Rationale: `m_squiggleBoyState.m_faders` is owned by the Nonagon/SquiggleBoy integration, not by `SquiggleBoyWithEncoderBank::m_encoders`. Keeping it as a sibling to the current root sections avoids implying it is encoder state and keeps the write path local to the owner that already serializes the complete patch.

   Alternative considered: nest faders under `squiggleBoy`. This would overload a section that existing specs and code treat as encoder-only state.

2. Thread `restoreFaders` through the high-level load path, and store it in `StateInterchange` with the parsed JSON.

   Rationale: the message thread knows whether WRLD.BLRD is attached when `RequestLoad()` is called, and it also knows when the caller is a reload-style patch path that must preserve live faders. The audio thread performs `FromJSON()`, so the bool must travel with the load request.

   Alternative considered: have `TheNonagonSquiggleBoyInternal::FromJSON()` query controller state directly. That would couple realtime patch deserialization to JUCE/controller wrapper state and make tests less direct.

3. Treat `NonagonWrapperWrldBldr::IsOpen()` as the WRLD.BLRD attachment predicate.

   Rationale: existing UI display-mode logic already treats this as the controller-open signal, and it requires both input and output to be open. Patch load should skip restoring faders only when the real controller path is fully attached.

   Alternative considered: skip restore when either input or output is open. That would suppress standalone fader restoration for partial or failed controller setup.

4. Make reload-style patch functionality pass `restoreFaders` as false explicitly.

   Rationale: reload is used to refresh the patch contents while keeping the current performance surface stable. Applying saved faders during reload would be surprising even without a WRLD.BLRD controller attached.

   Alternative considered: use the same WRLD.BLRD predicate for reload. That would let standalone reloads jump fader values, which conflicts with the requested reload behavior.

5. Preserve missing or short fader arrays by leaving current fader values unchanged.

   Rationale: old patches must keep loading, and a malformed/partial fader field should not silently zero live gains or gesture weights.

   Alternative considered: default missing values to zero. That would change old patch behavior and can unexpectedly mute voices.

## Risks / Trade-offs

- A saved patch loaded with WRLD.BLRD attached or through reload functionality will not visually jump the software faders to saved values. -> This is intentional; the current performance surface remains authoritative until it sends analog values.
- If WRLD.BLRD reconnect fails during startup, the app will restore saved faders. -> This follows the requested policy because the controller is not attached according to `IsOpen()`.
- Root patch JSON changes shape. -> The new member is additive, and old patches remain compatible.

## Migration Plan

1. Add fader array serialization and guarded restoration.
2. Add `restoreFaders` to load request plumbing and high-level `FromJSON` calls.
3. Add explicit false restore policy for reload-style patch loads.
4. Add tests for saved fader JSON, restore enabled, restore disabled, reload disabled, old-patch compatibility, and startup policy ordering.
5. No one-time migration is required; old patches load without the `faders` member.

## Open Questions

- None.

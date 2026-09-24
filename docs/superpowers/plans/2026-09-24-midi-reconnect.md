# MIDI Reconnection Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to execute this approved design inline. Review the complete change before completion.

**Goal:** Recover saved MIDI routes after startup endpoint removal or later device reconnection, even when the device identifier is unchanged.

**Architecture:** The message thread periodically checks actual JUCE connection liveness and reopens dead routes using the existing output mutex. Output producers receive atomic availability and refresh requests; their audio-thread control frame owns writer resets. WRLD.BLDR reconnect resends its handshake and current feedback. No sender pause, queue generations, or queue flushing.

**Tech Stack:** C++17 application, pinned JUCE 8.0.15 Apple overlay, CoreMIDI, CMake/doctest.

## Global Constraints

- Use the existing output lock for output replacement and native submission.
- Reconnect and enumerate only on the message thread; never block the audio thread.
- Preserve healthy connections and saved route names while devices are absent.
- Follow repository naming, brace, and comment conventions for application code.
- Do not change the JUCE submodule; extend the existing generated overlay.

### Task 1: Reconnect and refresh safely

**Files:** `JUCE/SmartGridOne/scripts/prepare_apple_audio_module.py`, `JUCE/SmartGridOne/Source/MidiHandlers.hpp`, `JUCE/SmartGridOne/Source/NonagonWrapper.hpp`, `JUCE/SmartGridOne/Source/MainComponent.cpp`, `JUCE/SmartGridOne/Source/MainComponent.h`, `private/test/juce-midi/`.

**Interfaces:** Expose JUCE input/output `isAlive()`. Handler `IsOpen()` uses liveness. `AttemptConnect()` returns whether it opened a live connection. `PrepareProcess()` consumes an atomic refresh request and resets on the producer thread. Wrapper `CheckMidiConnections()` runs from the existing message-thread timer at one-second intervals.

- [x] Add a macOS integration-test target compiling the real handlers and generated JUCE overlay. Use owned CoreMIDI virtual endpoints so no physical controller is required.
- [x] Verify red: an opened output receives endpoint removal, the endpoint reappears with the same unique ID, and `AttemptConnect()` fails to restore delivery. Also show that `Open()` currently resets the producer writer synchronously.
- [x] Expose `bool isAlive() const` on legacy MIDI input/output through the overlay; input forwards through its implementation, output through its UMP connection.
- [x] Change reconnection predicate from object existence to liveness: `if (!m_name.isEmpty() && !IsOpen())`. Return success so the WRLD.BLDR wrapper can send its handshake once after input/output recovery.
- [x] Publish output availability atomically. In `Open()`, request refresh instead of calling `Reset()`. Each producer starts with `if (PrepareProcess())`; that method consumes refresh and calls `Reset()` before generating feedback.
- [x] Guard sender calls with output liveness under the existing mutex, avoiding sends into a dead JUCE connection. Log connection transitions using the persisted app logger.
- [x] Check all configured routes periodically on the message thread. Preserve healthy routes and retry absent devices without per-tick log spam.
- [x] Verify virtual endpoint startup absence, disappearance, same-ID replacement, healthy-route preservation, input recovery, deferred refresh, and WRLD.BLDR handshake/full feedback recovery.
- [x] Run focused integration tests, core MIDI tests, and an iOS Release compile; review concurrency and shutdown lifetime.
- [x] Commit the tested change and report validation and any remaining device verification.

## Evidence and progress

Design approved in conversation: message thread checks/reconnects, uses the existing sender lock, sends handshake, and requests latest feedback on the audio thread.


Validation:

- Before implementation, all three native regression cases failed at the intended behavior: same-ID output delivery, same-ID input delivery, and deferred writer reset.
- After implementation, the combined native/core MIDI target passed **29 cases / 6,186 assertions** using the worktree sources.
- iOS Release compilation passed with `CODE_SIGNING_ALLOWED=NO` and a temporary derived-data directory. Existing missing app-icon asset warnings remain.
- Independent production review found no correctness or concurrency issues.
- The running iPad app was not replaced or restarted. Physical controller verification remains for deployment.
- Final test review added input-only WRLD.BLDR recovery while preserving the healthy output. Removing the handshake refresh caused the new full-feedback assertion to fail; restoring it returned all 29 cases to green. Scoped re-review found no further issues.

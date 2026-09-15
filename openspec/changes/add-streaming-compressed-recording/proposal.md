## Why

The performance recorder expands every mixer input into four PCM channels, including silent tracks and slowly moving pan coordinates that could compress well. It also packs samples on the audio thread and can block that thread when the disk queue fills; a streaming track format can reduce storage while keeping capture bounded and preserving both mastered output formats.

## What Changes

- **BREAKING**: Replace new performance-recording RF64 files with versioned `.sgrec` files: a JSON session header followed by independently decodable compressed blocks, defaulting to one second.
- Describe the recording date, build Git SHA, sample rate, block size, and stable track metadata in the header. Support `mono`, `panned_mono` (`sample_val`, `x`, `y`), `stereo`, and `quad` tracks, all represented as signed 24-bit integers with explicit audio/coordinate scaling.
- Omit silent tracks per block; independently select raw PCM or first-value-plus-minimum-width packed deltas for every stream in each included track.
- Keep the mixer-facing start/stop/toggle and sample-submission shape. Capture panned inputs as panned mono, their separate mono lanes as mono, and the three effect returns as quad.
- Record dedicated final quad and stereo tracks from their respective mastering chains, before the global listening-volume control, preserving today's recording tap independently of listening level.
- Hand complete frames through one preallocated SPSC ring to a writer thread that transposes, quantizes, compresses, and writes. Preserve nonblocking capture, explicit overload errors, partial-block draining, and safe shutdown.
- Add streaming Python inspection and WAV extraction for selected tracks and final outputs; integrate format detection into the existing iPad recording-sync extraction helper.

## Capabilities

### New Capabilities

- `streaming-recording`: Versioned container, track/stream encodings, bounded realtime capture, writer lifecycle, build SHA, and block integrity.
- `recording-extraction`: Streaming inspection, direct PCM24 master and stem WAV export, usable partial exports with explicit failure reporting, and legacy sync compatibility.

### Modified Capabilities

- `mixdown-mastering`: Define separate recording taps for panned inputs, mono lanes, quad returns, and the final mastered quad/stereo outputs without changing live mix processing.

## Impact

- Main integration: `private/src/QuadMixer.hpp`, `private/src/SquiggleBoy.hpp`, recording controls in `private/src/TheNonagonSquiggleBoy.hpp`, and lifecycle ownership in `JUCE/SmartGridOne/Source/NonagonWrapper.hpp`.
- C++ format/encoding helpers and a recorder owning its ring and worker; reuse existing queue, thread-identification, and atomic UI-state patterns. Python owns decoding. The performance recorder replaces its use of `MultichannelWavWriter`/`FileWriter`; sampler-looper WAV persistence continues using the existing facilities.
- Embed the build commit SHA through the maintained macOS/iOS Xcode/JUCE and standalone CMake build paths.
- Add Python tools, shared binary fixtures, codec/lifecycle/mixer/extraction coverage, format documentation, and performance measurements. Update `scripts/sync_ipad.py`, which currently assumes final stereo occupies the last two WAV channels.
- No general-purpose compression-library dependency is required. Existing recordings remain readable through the legacy extraction route.

## Non-goals

- Changing the sampler-looper recording system, live panning law, mastering DSP, device routing, or supported build topology.
- Reconstructing the mastered mix by summing stems, recording every control/patch change, or recording the dedicated hardware sub output in this first version.
- Lossless preservation of the original floating-point samples, additional predictive codecs, or random-access indexing.
- A production C++ decoder, coordinate WAVs, sidecars, spatial rendering, or dirty-state/source-archive provenance support.

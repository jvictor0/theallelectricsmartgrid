// WP-9: sampler-looper recording -> async WAV persistence -> reload round-trip.
//
// What this exercises (discovered end-to-end; see the long comment block below):
//
//   arm:      RecordingManager::StartRecording(Trio) registers each trio voice's
//             RecordingBuffer into its own RecordingBufferWriter. From then on,
//             SquiggleBoyVoice::Processs() pushes that voice's *synth output*
//             (m_output) into the recording buffer every sample. NOTE: the
//             sampler-looper records the trio's INTERNAL synth audio, not the
//             injectable external audio input. (SynthRig::SetAudioInput feeds the
//             vocoder/source path, a different signal entirely.)
//
//   DETERMINISTIC CONTENT: with the default patch the trio-0 voices emit pure
//             silence -- the LameJuis sequencer fires no amp-envelope triggers
//             through the reachable front-door inputs (verified empirically:
//             running the sequencer and pressing matrix pads leaves every voice
//             output at exactly 0.0). So "drive a voice to make sound" is NOT
//             reachable without programming the sequencer/matrix at a depth
//             outside WP-9's scope. To verify CONTENT correctly we therefore do
//             the real front-door arm/capture/stop (real master-loop alignment,
//             real timing), but overwrite the captured RecordingBuffer with a
//             known seeded signal of the same length *before* StopRecording
//             pushes the persist task -- so the bytes that get written, persisted,
//             and reloaded are content we control and can assert exactly. The
//             alignment math, WAV write, I/O thread, and reload all run for real.
//
//   capture:  RecordingBuffer accumulates floats while State==Recording. Start/
//             stop positions are taken from the master loop's unwound phasor
//             (TheoryOfTime::GetUnwoundMasterIndependent): integer master-loops
//             completed + phasor in [0,1). The span (delta) must be in (0,1].
//
//   align:    StopRecording computes "repeats" = the largest internal sub-loop
//             whose relative size (1/externalMultiplier) is <= delta. WriteToFile
//             then emits exactly ONE master loop (masterSamples = round(buffer /
//             delta)) by tiling that sub-loop `repeats` times. With a ~0.9-loop
//             recording the largest qualifying sub-loop is 0.5 -> repeats=2, so
//             the persisted WAV is the full 4.0s master loop (192000 samples),
//             built from two tiles of the first half of the captured audio.
//
//   persist:  RecordingManager::PersistRecordingForVoice -> IoTaskThread
//             PushPersistRecording. The real I/O thread writes an RF64 / 24-bit
//             PCM / mono / host-rate (48000) WAV named "_recording_NNNNN.wav"
//             into a per-voice directory "<root>/<YYYY-MM-DDThh-mm-ss>_voiceN/".
//             Because the voice had no bank yet, createDirectory=true and the
//             persist task also gets a sink (&voiceConfig.m_audioBufferBank), so
//             on completion it loads the freshly written directory back into an
//             AudioBufferBank and hands it to the audio thread via the ack queue.
//
//   reload:   On the next ProcessFrame, IoTaskThread::Acknowledge installs that
//             AudioBufferBank pointer into the voice config (the SampleSource
//             then plays it). RecordingBuffer::Reset is also acked here.
//             PostStopRecording additionally pushes a ReloadDirectory for any
//             voice that already had a bank.
//
//   file:     reload from a fresh rig pointed at the same root is the
//             LoadAudioBufferBankFromDirectory path (FromJSON of a saved patch,
//             or a direct push). The WAV is read back at host rate (no resample
//             since same rate) into a 192000-sample mono buffer.
//
// FRONT-DOOR vs COMPONENT-LEVEL:
//   The sampler-looper's UI lives in SquiggleBoyConfigGrid (RecordingToggleCell
//   at Put(4,0), SourceSelectCell, etc.). That grid is NOT routed through the
//   QuadLaunchpadTwister, so its pads are unreachable from SynthRig's pad verbs
//   (the SynthRig "record toggle" pad x=-1,y=7 drives the MIXER's master
//   recording -- m_mixer.ToggleRecording -- a *different* recording system).
//   We therefore arm/stop the sampler-looper by calling RecordingManager methods
//   directly on the real wired system (component-level for the trigger), but the
//   capture, persistence (real IoTaskThread), reload, and patch save/load all run
//   through the real system exactly as in production. The patch round-trip (test
//   3) is fully front-door via SynthRig::SavePatchJSON / LoadPatchJSON.
//
// RecordingConfig::m_source defaults to Source::Water (trio 0) for every voice
// and is never reassigned anywhere in the codebase, so recording always routes
// trio-0 voices (0,1,2) into their own buffers. We record Trio::Water to match.

#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "../support/SynthRig.hpp"
#include "../support/TempDir.hpp"

#include "WavReader.hpp"

namespace
{

using synthrig::SynthRig;
using synthrig::TempDir;

constexpr size_t kMasterLoopSamples = 192000; // 4.0s * 48000Hz
constexpr int kPumpCap = 6000;                // bounded persist/reload pump cap

// Pump ProcessFrames until `pred()` is true or we hit the cap. Returns the number
// of frames pumped, or -1 on timeout. Each frame runs one full process frame and
// calls IoTaskThread::Acknowledge(), draining any completed I/O tasks.
template <class Pred>
int PumpUntil(SynthRig& rig, Pred&& pred, int cap = kPumpCap)
{
    for (int i = 0; i < cap; ++i)
    {
        if (pred())
        {
            return i;
        }
        rig.RunFrames(1);
    }
    return pred() ? cap : -1;
}

// Find a "_recording_*.wav" under root whose parent directory name contains
// `voiceTag` (e.g. "_voice0"). Pass "" to match any recording wav.
std::filesystem::path FindRecordingWav(const std::filesystem::path& root, const std::string& voiceTag)
{
    std::error_code ec;
    if (!std::filesystem::exists(root, ec))
    {
        return {};
    }
    for (auto it = std::filesystem::recursive_directory_iterator(root, ec);
         !ec && it != std::filesystem::recursive_directory_iterator();
         it.increment(ec))
    {
        if (it->is_regular_file(ec))
        {
            std::string name = it->path().filename().string();
            if (name.rfind("_recording_", 0) == 0 &&
                name.size() > 4 &&
                name.compare(name.size() - 4, 4, ".wav") == 0)
            {
                if (voiceTag.empty() ||
                    it->path().parent_path().filename().string().find(voiceTag) != std::string::npos)
                {
                    return it->path();
                }
            }
        }
    }
    return {};
}

// Count "_recording_*.wav" files under root.
int CountRecordingWavs(const std::filesystem::path& root)
{
    int count = 0;
    std::error_code ec;
    if (!std::filesystem::exists(root, ec))
    {
        return 0;
    }
    for (auto it = std::filesystem::recursive_directory_iterator(root, ec);
         !ec && it != std::filesystem::recursive_directory_iterator();
         it.increment(ec))
    {
        if (it->is_regular_file(ec))
        {
            std::string name = it->path().filename().string();
            if (name.rfind("_recording_", 0) == 0 &&
                name.size() > 4 &&
                name.compare(name.size() - 4, 4, ".wav") == 0)
            {
                ++count;
            }
        }
    }
    return count;
}

// Read a mono WAV back into floats using the repo's own WavReader.
bool ReadWavMono(const std::filesystem::path& path, std::vector<float>& out, WavReader& reader)
{
    if (!reader.LoadFromFile(path.string().c_str()))
    {
        return false;
    }
    out.clear();
    out.reserve(reader.m_numFrames);
    for (size_t f = 0; f < reader.m_numFrames; ++f)
    {
        out.push_back(reader.GetSample(f, 0));
    }
    return true;
}

float Peak(const std::vector<float>& v)
{
    float p = 0.0f;
    for (float x : v)
    {
        p = std::max(p, std::fabs(x));
    }
    return p;
}

// Convenience accessor: the real, fully-wired SquiggleBoy behind the rig.
SquiggleBoy& Boy(SynthRig& rig)
{
    return rig.Internal().m_squiggleBoy;
}

// Warm up + start the sequencer so the master loop / TheoryOfTime advances (the
// recording start/stop positions are read from the master loop phasor).
void WarmUpVoices(SynthRig& rig)
{
    rig.RunFrames(2);
    rig.StartSequencer();
    rig.RunSeconds(0.5);
}

// A deterministic, seeded test signal: a decaying sine. Index-driven so it is
// fully reproducible and independent of any RNG/global state.
float KnownSignalAt(size_t i)
{
    const double t = static_cast<double>(i);
    const double phase = t * (2.0 * 3.14159265358979323846 * 220.0 / 48000.0);
    const double envelope = 0.6 * (0.5 + 0.5 * std::cos(t * 1.0e-4));
    return static_cast<float>(envelope * std::sin(phase));
}

// Overwrite a recording buffer in-place with the known signal (same length).
// Must be called while the buffer is still owned by the audio thread (i.e. before
// StopRecording hands it to the I/O thread) and with no frames running in
// between, so the writer cannot append more samples.
void OverwriteWithKnownSignal(std::vector<float>& buffer)
{
    for (size_t i = 0; i < buffer.size(); ++i)
    {
        buffer[i] = KnownSignalAt(i);
    }
}

// Arm trio Water, capture ~0.9 master loop, inject the known signal into voice
// 0's buffer, and stop (which pushes the persist task). Does NOT pump for
// completion. Returns voice 0's captured-and-injected buffer (for verification).
// Leaves the sequencer running.
std::vector<float> RecordKnownSignalIntoVoice0(SynthRig& rig)
{
    SquiggleBoy& boy = Boy(rig);
    RecordingManager& rm = boy.m_recordingManager;

    WarmUpVoices(rig);
    rm.StartRecording(TheNonagonInternal::Trio::Water);
    rig.RunSeconds(3.6);

    std::vector<float>& buf = boy.m_voices[0].m_recordingBuffer.m_buffer;
    OverwriteWithKnownSignal(buf);
    std::vector<float> captured = buf;

    rm.StopRecording(TheNonagonInternal::Trio::Water);
    return captured;
}

} // namespace

// ---------------------------------------------------------------------------
// Test 1: record -> persist round-trip.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("WP-9: record -> persist writes a well-formed master-loop WAV")
{
    TempDir td;
    DOCTEST_REQUIRE(td.Valid());

    SynthRig rig;
    rig.IoThread().SetSampleDirectoryRootAbsolute(td.Path());
    DOCTEST_REQUIRE(rig.IoThread().HasSampleDirectoryRootAbsolute());

    SquiggleBoy& boy = Boy(rig);
    RecordingManager& rm = boy.m_recordingManager;

    WarmUpVoices(rig);

    // Arm: record trio Water (voices 0,1,2). m_source defaults to Water for all.
    rm.StartRecording(TheNonagonInternal::Trio::Water);
    DOCTEST_CHECK(rm.AnyRecording());
    DOCTEST_CHECK(rm.m_numRecording == 3);
    DOCTEST_CHECK(boy.m_voices[0].m_recordingBuffer.m_state == RecordingBuffer::State::Recording);

    // Capture ~0.9 of a master loop so delta is comfortably in (0,1] and the
    // alignment picks the 0.5 sub-loop (repeats=2 -> full-loop WAV).
    rig.RunSeconds(3.6);

    RecordingBuffer& buf0 = boy.m_voices[0].m_recordingBuffer;
    DOCTEST_CHECK(buf0.m_buffer.size() > 0);

    // Inject a known, deterministic signal into voice 0's captured buffer BEFORE
    // StopRecording hands it to the I/O thread (no frames run in between, so the
    // writer cannot append more). The synth itself is silent (see header note);
    // this is what lets us verify the persisted CONTENT exactly. Voices 1 and 2
    // keep their (silent) captures.
    OverwriteWithKnownSignal(buf0.m_buffer);
    std::vector<float> captured = buf0.m_buffer; // reference copy for verification
    const float capturedPeak = Peak(captured);
    DOCTEST_CHECK(capturedPeak > 0.0f);

    // Stop: this computes repeats and pushes the persist task to the I/O thread.
    rm.StopRecording(TheNonagonInternal::Trio::Water);
    DOCTEST_CHECK_FALSE(rm.AnyRecording());

    const double delta =
        buf0.m_loopPositionRecordingStop.load() - buf0.m_loopPositionRecordingStart.load();
    DOCTEST_CHECK(delta > 0.0);
    DOCTEST_CHECK(delta <= 1.0);
    DOCTEST_CHECK(buf0.m_recordingRepeats.load() >= 1);

    // Bounded pump until voice 0's persisted WAV appears AND the bank sink is
    // installed for voice 0.
    int frames = PumpUntil(rig, [&]() {
        return !FindRecordingWav(td.Path(), "_voice0").empty() &&
               boy.m_state[0].m_voiceConfig.m_audioBufferBank != nullptr;
    });
    DOCTEST_REQUIRE_MESSAGE(frames >= 0,
        "timed out waiting for IoTaskThread to persist + install the recording bank");

    // ---- file exists, named + foldered as expected ----
    std::filesystem::path wav = FindRecordingWav(td.Path(), "_voice0");
    DOCTEST_REQUIRE_FALSE(wav.empty());
    DOCTEST_CHECK(wav.filename().string().rfind("_recording_", 0) == 0);
    // Per-voice directory naming: "<YYYY-MM-DDThh-mm-ss>_voice0".
    DOCTEST_CHECK(wav.parent_path().filename().string().find("_voice0") != std::string::npos);

    // ---- well-formed header (repo's own RF64/24-bit reader) ----
    WavReader reader;
    std::vector<float> wavSamples;
    DOCTEST_REQUIRE(ReadWavMono(wav, wavSamples, reader));
    DOCTEST_CHECK(reader.m_isRf64);
    DOCTEST_CHECK(reader.m_numChannels == 1);
    DOCTEST_CHECK(reader.m_sampleRate == 48000);
    DOCTEST_CHECK(reader.m_bitsPerSample == 24);
    DOCTEST_CHECK(reader.m_format == WavReader::Format::Pcm);

    // ---- duration / sample count matches the master loop ----
    DOCTEST_CHECK(reader.m_numFrames == kMasterLoopSamples);
    DOCTEST_CHECK(wavSamples.size() == kMasterLoopSamples);

    // ---- content not all zeros, correlates with what was recorded ----
    const float wavPeak = Peak(wavSamples);
    DOCTEST_CHECK(wavPeak > 0.0f);
    // The WAV is the captured audio tiled to a full loop; its peak should track
    // the captured peak (within 24-bit quantization + clamp tolerance).
    DOCTEST_CHECK(wavPeak <= capturedPeak + 1e-2f);
    DOCTEST_CHECK(wavPeak > capturedPeak * 0.25f);

    // Exact correlation: re-derive WriteToFile's tiling from the captured buffer
    // and confirm the persisted WAV matches it sample-for-sample (within 24-bit
    // quantization). This proves the persisted content IS the recorded content.
    const double startPos = buf0.m_loopPositionRecordingStart.load();
    const double stopPos = buf0.m_loopPositionRecordingStop.load();
    const int repeats = buf0.m_recordingRepeats.load();
    const size_t bufferSize = captured.size();
    const double span = stopPos - startPos;
    const double sourceSamplesPerMaster = static_cast<double>(bufferSize) / span;
    const size_t masterSamples = static_cast<size_t>(std::lround(sourceSamplesPerMaster));
    DOCTEST_REQUIRE(masterSamples == kMasterLoopSamples);

    const double loopFraction = 1.0 / static_cast<double>(repeats);
    const double outputSamplesPerMaster = static_cast<double>(masterSamples);
    constexpr double kEps = 1.0e-9;

    size_t mismatches = 0;
    double sumAbsErr = 0.0;
    for (size_t i = 0; i < masterSamples; ++i)
    {
        double outputFraction = static_cast<double>(i) / outputSamplesPerMaster;
        double loopPhase = std::fmod(outputFraction, loopFraction);
        if (loopPhase < 0.0)
        {
            loopPhase += loopFraction;
        }
        double sourcePosition =
            std::floor((startPos - loopPhase) / loopFraction) * loopFraction + loopPhase;
        while (sourcePosition < startPos - kEps)
        {
            sourcePosition += loopFraction;
        }
        if (sourcePosition < startPos)
        {
            sourcePosition = startPos;
        }
        double expected = 0.0;
        if (sourcePosition < stopPos)
        {
            double sourceOffset = sourcePosition - startPos;
            size_t srcIndex =
                static_cast<size_t>(std::floor(sourceOffset * sourceSamplesPerMaster));
            srcIndex = std::min(srcIndex, bufferSize - 1);
            expected = static_cast<double>(captured[srcIndex]);
        }
        expected = std::max(-1.0, std::min(1.0, expected));

        double err = std::fabs(static_cast<double>(wavSamples[i]) - expected);
        sumAbsErr += err;
        if (err > 2.0e-4) // ~1.7 LSB at 24-bit
        {
            ++mismatches;
        }
    }
    const double meanAbsErr = sumAbsErr / static_cast<double>(masterSamples);
    DOCTEST_CHECK(meanAbsErr < 5.0e-5);
    DOCTEST_CHECK(mismatches == 0);

    // ---- on-persist reload installed the bank into the voice's SampleSource ----
    AudioBufferBank* bank = boy.m_state[0].m_voiceConfig.m_audioBufferBank;
    DOCTEST_REQUIRE(bank != nullptr);
    DOCTEST_REQUIRE(bank->m_audioBuffers.size() == 1);
    const std::vector<float>& reloaded = bank->m_audioBuffers[0]->m_buffer;
    DOCTEST_CHECK(reloaded.size() == kMasterLoopSamples);
    // The just-persisted-and-reloaded buffer should equal the WAV we read back.
    DOCTEST_REQUIRE(reloaded.size() == wavSamples.size());
    double reloadErr = 0.0;
    for (size_t i = 0; i < reloaded.size(); ++i)
    {
        reloadErr = std::max(reloadErr, static_cast<double>(std::fabs(reloaded[i] - wavSamples[i])));
    }
    DOCTEST_CHECK(reloadErr < 2.0e-4);

    DOCTEST_CHECK_FALSE(rig.SawNaN());
}

// ---------------------------------------------------------------------------
// Test 2: reload on a fresh rig via LoadAudioBufferBankFromDirectory.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("WP-9: a persisted bank reloads on a fresh rig from the same root")
{
    TempDir td;
    DOCTEST_REQUIRE(td.Valid());

    std::string relativeDir;
    std::vector<float> originalReloaded;

    // ---- Producer rig: record + persist, capture the relative directory name.
    {
        SynthRig rig;
        rig.IoThread().SetSampleDirectoryRootAbsolute(td.Path());
        SquiggleBoy& boy = Boy(rig);

        RecordKnownSignalIntoVoice0(rig);

        int frames = PumpUntil(rig, [&]() {
            AudioBufferBank* b = boy.m_state[0].m_voiceConfig.m_audioBufferBank;
            return b != nullptr && !b->m_directoryName.empty() && b->m_audioBuffers.size() == 1;
        });
        DOCTEST_REQUIRE_MESSAGE(frames >= 0, "timed out persisting on producer rig");

        AudioBufferBank* bank = boy.m_state[0].m_voiceConfig.m_audioBufferBank;
        relativeDir = bank->m_directoryName;
        originalReloaded = bank->m_audioBuffers[0]->m_buffer;
        DOCTEST_CHECK_FALSE(relativeDir.empty());
        DOCTEST_CHECK(originalReloaded.size() == kMasterLoopSamples);
    }

    // ---- Consumer rig: fresh system, same root, drive the load path directly.
    {
        SynthRig rig2;
        rig2.IoThread().SetSampleDirectoryRootAbsolute(td.Path());
        SquiggleBoy& boy2 = Boy(rig2);

        AudioBufferBank** sink = &boy2.m_state[0].m_voiceConfig.m_audioBufferBank;
        DOCTEST_REQUIRE(*sink == nullptr);

        bool pushed = rig2.IoThread().PushLoadAudioBufferBankFromDirectory(
            relativeDir.c_str(), sink);
        DOCTEST_REQUIRE(pushed);

        int frames = PumpUntil(rig2, [&]() {
            return *sink != nullptr && (*sink)->m_audioBuffers.size() == 1;
        });
        DOCTEST_REQUIRE_MESSAGE(frames >= 0, "timed out loading bank on fresh rig");

        AudioBufferBank* loaded = *sink;
        DOCTEST_REQUIRE(loaded != nullptr);
        DOCTEST_CHECK(loaded->m_directoryName == relativeDir);
        DOCTEST_REQUIRE(loaded->m_audioBuffers.size() == 1);

        const std::vector<float>& loadedBuf = loaded->m_audioBuffers[0]->m_buffer;
        DOCTEST_CHECK(loadedBuf.size() == kMasterLoopSamples);
        // Same WAV, same host rate -> identical content (no resampling involved).
        DOCTEST_REQUIRE(loadedBuf.size() == originalReloaded.size());
        double maxErr = 0.0;
        for (size_t i = 0; i < loadedBuf.size(); ++i)
        {
            maxErr = std::max(maxErr, static_cast<double>(std::fabs(loadedBuf[i] - originalReloaded[i])));
        }
        DOCTEST_CHECK(maxErr < 1.0e-6);
        DOCTEST_CHECK(Peak(loadedBuf) > 0.0f);
    }
}

// ---------------------------------------------------------------------------
// Test 3: patch interaction -- the sample directory is saved in the patch
// (SquiggleBoyConfigGrid "sampleDirectoryRelative") and reloaded on load.
// Fully front-door via SynthRig::SavePatchJSON / LoadPatchJSON.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("WP-9: a saved patch reloads the recorded sample bank")
{
    TempDir td;
    DOCTEST_REQUIRE(td.Valid());

    JSON patch = JSON::Null();
    std::string relativeDir;

    // ---- Record + persist, then save a patch.
    {
        SynthRig rig;
        rig.IoThread().SetSampleDirectoryRootAbsolute(td.Path());
        SquiggleBoy& boy = Boy(rig);

        RecordKnownSignalIntoVoice0(rig);

        int frames = PumpUntil(rig, [&]() {
            AudioBufferBank* b = boy.m_state[0].m_voiceConfig.m_audioBufferBank;
            return b != nullptr && !b->m_directoryName.empty();
        });
        DOCTEST_REQUIRE_MESSAGE(frames >= 0, "timed out persisting before patch save");

        relativeDir = boy.m_state[0].m_voiceConfig.m_audioBufferBank->m_directoryName;
        DOCTEST_CHECK_FALSE(relativeDir.empty());

        // Sequencer is still running -- stop it so the save isn't racing audio.
        rig.StopSequencer();
        patch = rig.SavePatchJSON();
        DOCTEST_REQUIRE_FALSE(patch.IsNull());
    }

    // ---- Fresh rig, same root: load the patch, expect the bank to be reloaded.
    {
        SynthRig rig2;
        rig2.IoThread().SetSampleDirectoryRootAbsolute(td.Path());
        SquiggleBoy& boy2 = Boy(rig2);

        DOCTEST_REQUIRE(boy2.m_state[0].m_voiceConfig.m_audioBufferBank == nullptr);

        bool loaded = rig2.LoadPatchJSON(patch);
        DOCTEST_REQUIRE(loaded);

        // FromJSON pushed a LoadAudioBufferBankFromDirectory for voice 0; pump
        // until the bank lands.
        int frames = PumpUntil(rig2, [&]() {
            AudioBufferBank* b = boy2.m_state[0].m_voiceConfig.m_audioBufferBank;
            return b != nullptr && b->m_audioBuffers.size() == 1;
        });
        DOCTEST_REQUIRE_MESSAGE(frames >= 0,
            "timed out reloading sample bank from saved patch");

        AudioBufferBank* bank = boy2.m_state[0].m_voiceConfig.m_audioBufferBank;
        DOCTEST_REQUIRE(bank != nullptr);
        DOCTEST_CHECK(bank->m_directoryName == relativeDir);
        DOCTEST_REQUIRE(bank->m_audioBuffers.size() == 1);
        DOCTEST_CHECK(bank->m_audioBuffers[0]->m_buffer.size() == kMasterLoopSamples);
        DOCTEST_CHECK(Peak(bank->m_audioBuffers[0]->m_buffer) > 0.0f);
    }
}

// ---------------------------------------------------------------------------
// Test 4: shutdown safety -- behavior when a rig is destroyed while a persist
// task may still be in flight.
//
// BUG?: There is a genuine shutdown-ordering use-after-free here, faithfully
// reproduced from the real product. In NonagonWrapper (JUCE/SmartGridOne/Source/
// NonagonWrapper.hpp:509-510) the members are declared:
//       IoTaskThread                 m_ioTaskThread;   // declared first
//       TheNonagonSquiggleBoyInternal m_internal;      // declared second
// C++ destroys members in REVERSE declaration order, so m_internal (which owns
// the voices, their RecordingBuffers, and the AudioBufferBank sinks) is destroyed
// FIRST, and the IoTaskThread -- which only joins its worker thread in its own
// destructor -- is destroyed LAST. If a PersistRecording task is still queued or
// actively running on the I/O thread at that moment, IoTaskElement::Process ->
// ProcessPersistRecording dereferences m_recordingBuffer (and the sink pointer)
// into the already-destroyed m_internal => use-after-free / SIGSEGV. SynthRig
// destroys in the same order (m_internal.reset() then m_ioTaskThread.reset()),
// so the hazard reproduces 1:1. Repro: arm + record + StopRecording (pushes the
// persist task), then destroy the rig WITHOUT pumping frames to let the I/O
// thread finish + Acknowledge. Fix: drain/stop the IoTaskThread (or guarantee no
// task references audio-thread-owned objects) BEFORE destroying the audio core --
// e.g. declare/destroy m_ioTaskThread before m_internal, or join+flush the I/O
// thread at the start of teardown.
//
// Because a SIGSEGV is uncatchable and would abort the whole test binary, this
// test exercises the SAFE teardown contract (pump the persist task to completion
// and Acknowledge it before the rig is destroyed) and WARNs about the unsafe
// path rather than detonating it. The drain-then-destroy path must be crash- and
// hang-free.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("WP-9: shutdown after draining an in-flight persist is safe (UAF hazard documented)")
{
    TempDir td;
    DOCTEST_REQUIRE(td.Valid());

    DOCTEST_WARN_MESSAGE(false,
        "BUG?: destroying the rig/NonagonWrapper while a PersistRecording task is "
        "in flight is a use-after-free (m_internal destroyed before IoTaskThread). "
        "This test drains the task first to keep the suite alive; see the comment "
        "block above for the repro and fix.");

    {
        SynthRig rig;
        rig.IoThread().SetSampleDirectoryRootAbsolute(td.Path());
        SquiggleBoy& boy = Boy(rig);

        RecordKnownSignalIntoVoice0(rig); // arms, records, StopRecording (pushes persist)

        // SAFE contract: pump until the I/O thread has finished persisting AND the
        // audio thread has Acknowledged (sink installed, RecordingBuffer Reset), so
        // no in-flight task references anything owned by the soon-to-be-destroyed
        // audio core.
        int frames = PumpUntil(rig, [&]() {
            return boy.m_state[0].m_voiceConfig.m_audioBufferBank != nullptr &&
                   boy.m_voices[0].m_recordingBuffer.m_state == RecordingBuffer::State::Idle;
        });
        DOCTEST_REQUIRE_MESSAGE(frames >= 0, "timed out draining the persist task before shutdown");

        // A couple of extra frames so any follow-on ReloadDirectory acks also drain.
        rig.RunFrames(3);
    } // ~SynthRig -> ~IoTaskThread joins here with nothing in flight; must return.

    DOCTEST_CHECK(true); // reached here == no crash, no hang
}

// ---------------------------------------------------------------------------
// Test 5: no-IO sanity. Two intended-semantics cases:
//   (a) armed but sequencer never started / stopped before a sub-loop completes
//       -> StopRecording sees delta<=0 or no qualifying repeat -> State::Error
//       -> buffer Reset, NO persist task, NO file written.
//   (b) never armed at all -> nothing recorded, nothing written.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("WP-9: arming without a completed loop writes no file (clean state)")
{
    // (a) Arm, but stop almost immediately (span far below the smallest sub-loop).
    {
        TempDir td;
        DOCTEST_REQUIRE(td.Valid());

        SynthRig rig;
        rig.IoThread().SetSampleDirectoryRootAbsolute(td.Path());
        SquiggleBoy& boy = Boy(rig);
        RecordingManager& rm = boy.m_recordingManager;

        WarmUpVoices(rig);
        rm.StartRecording(TheNonagonInternal::Trio::Water);
        DOCTEST_CHECK(rm.AnyRecording());

        // Record a tiny slice -- nowhere near a full sub-loop.
        rig.RunSeconds(0.05);
        rm.StopRecording(TheNonagonInternal::Trio::Water);

        DOCTEST_CHECK_FALSE(rm.AnyRecording());
        // Per RecordingBuffer::StopRecording semantics: a span that doesn't reach
        // the smallest internal sub-loop yields repeats<=0 -> State::Error -> the
        // buffer is Reset and nothing is persisted.
        RecordingBuffer& buf0 = boy.m_voices[0].m_recordingBuffer;
        DOCTEST_CHECK(buf0.m_state == RecordingBuffer::State::Idle); // Reset() ran

        // Give the I/O thread ample frames to (not) produce anything.
        rig.RunFrames(200);
        DOCTEST_CHECK(CountRecordingWavs(td.Path()) == 0);
        DOCTEST_CHECK(boy.m_state[0].m_voiceConfig.m_audioBufferBank == nullptr);
    }

    // (b) Never armed: a fresh rig that just runs produces no recordings.
    {
        TempDir td;
        DOCTEST_REQUIRE(td.Valid());

        SynthRig rig;
        rig.IoThread().SetSampleDirectoryRootAbsolute(td.Path());
        SquiggleBoy& boy = Boy(rig);

        WarmUpVoices(rig);
        rig.RunSeconds(1.0);

        DOCTEST_CHECK_FALSE(boy.m_recordingManager.AnyRecording());
        DOCTEST_CHECK(CountRecordingWavs(td.Path()) == 0);
        DOCTEST_CHECK(boy.m_state[0].m_voiceConfig.m_audioBufferBank == nullptr);
    }
}

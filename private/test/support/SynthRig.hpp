#pragma once

// SynthRig: the whole-system test harness for the JUCE-free smartgrid test
// binary. All system-level tests (scenes / gestures / patches / recording /
// fuzz / stress) drive the synthesizer through this single front door.
//
// THE API BELOW IS FROZEN as of WP-5. Add new convenience helpers if you must,
// but do not change the semantics of the existing verbs/observers.
//
// ---------------------------------------------------------------------------
// What it wraps
// ---------------------------------------------------------------------------
// SynthRig owns:
//   * a fresh TheNonagonSquiggleBoyInternal              (the DSP core)
//   * a fresh TheNonagonSquiggleBoyQuadLaunchpadTwister  (the grid wrapper that
//     owns the four launchpad pad grids + its own MessageInBus, and routes pad
//     presses to the right interior grid by routeId)
//   * one real IoTaskThread (std::thread), wired exactly as NonagonWrapper wires
//     it, with clean shutdown in the destructor.
//
// It drives them single-threaded, sample-by-sample, in the exact same order as
// JUCE/SmartGridOne/Source/NonagonWrapper.hpp::Process / ProcessSample /
// ProcessFrame. See RunSamples() for the annotated drive sequence.
//
// ---------------------------------------------------------------------------
// One system per process? -> NO. Fresh state per rig.
// ---------------------------------------------------------------------------
// WP-0/WP-1 assumed one TheNonagonSquiggleBoyInternal per process because of a
// global grid-id allocator. That allocator is now effectively a no-op
// (SmartGrid::AllocGridId() returns the "unallocated" sentinel and
// FreeGridId() does nothing), so constructing and destroying multiple system
// instances -- sequentially or concurrently -- is safe and leak-free. This was
// verified empirically (5 fresh instances run identically and finite). Every
// SynthRig therefore builds its own pristine system in its constructor; tests
// get clean state simply by creating a new SynthRig (or calling
// ResetToDefaults()). The legacy SystemFixture singleton is left untouched for
// the existing infra_spike / msg_encoder_set unit tests.
//
// ---------------------------------------------------------------------------
// Control-surface map (for later agents)
// ---------------------------------------------------------------------------
// Pad routes (TheNonagonSquiggleBoyQuadLaunchpadTwister::Routes):
//   0 TopLeftGrid, 1 TopRightGrid, 2 BottomLeftGrid, 3 BottomRightGrid,
//   4 Encoder (-> internal encoder bank), 5 Param7.
// Cells of interest (all on the *interior* grids, coordinates as Put() into the
// InteriorGridBase, base grid is 8x8):
//   * Running (toggle):   BottomLeftGrid (route 2), x=-1, y=6
//   * Shift (momentary):  BottomRightGrid (route 3), x=7,  y=9
//   * Scene selectors:    BottomLeftGrid (route 2), x=0..7, y=9
//   * Gesture selectors:  BottomLeftGrid (route 2), x=0..7, y=8
//   * Save patch:         BottomRightGrid (route 3), x=-1, y=0
//   * Load last save:     BottomRightGrid (route 3), x=-1, y=1
//   * Noise mode:         BottomLeftGrid (route 2), x=-1, y=5
//   * Record toggle:      BottomLeftGrid (route 2), x=-1, y=7
//   * Top-grid mode sel:  BottomRightGrid (route 3), x=8, y=4..7 (Matrix/Water/Earth/Fire)
// Encoders / faders (go to TheNonagonSquiggleBoyInternal::Apply directly):
//   * EncoderSet/IncDec/Push/Release: route 4 (Encoder), x/y are the 4x4 encoder
//     bank coordinates.
//   * Blend (scene crossfade): ParamSet14 with x==0  -> SceneManager.m_blendFactor
//   * Faders:                  ParamSet14 with x==1..N -> m_squiggleBoyState.m_faders[x-1]
//
// NOTE on shift+scene semantics (from TheNonagonSquiggleBoyInternal):
//   Pressing a scene pad with shift held => CopyToScene(scene).
//   Without shift: if blend < 0.5 sets the RIGHT scene to `scene`, else the LEFT
//   scene. SelectScene() below picks the realest single-pad path; for precise
//   left/right control use SetLeftScene/SetRightScene (documented harness
//   conveniences) or drive the pads yourself.

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "../support/GlobalEnv.hpp"

#include "AudioInputBuffer.hpp"
#include "JuceSon.hpp"
#include "MessageIn.hpp"
#include "SampleTimer.hpp"
#include "TheNonagonSquiggleBoy.hpp"
#include "TheNonagonSquiggleBoyQuadLaunchpadTwister.hpp"
#include "IOTaskThread.hpp"

namespace synthrig
{

// A captured output sample (mirror of QuadFloatWithStereoAndSub, flattened so it
// is trivially copyable into a ring/vector). Layout note: m_quad[4], m_stereo[2]
// and m_sub are contiguous so the NaN/peak scan can walk all 7 channels.
//
struct OutputSample
{
    float m_quad[4];
    float m_stereo[2];
    float m_sub;
};

class SynthRig
{
public:
    // Route ids (mirror TheNonagonSquiggleBoyQuadLaunchpadTwister::Routes).
    //
    enum Route : int
    {
        RouteTopLeft     = 0,
        RouteTopRight    = 1,
        RouteBottomLeft  = 2,
        RouteBottomRight = 3,
        RouteEncoder     = 4,
        RouteParam7      = 5,
    };

    // ---- Construction ------------------------------------------------------
    //
    // Mirrors NonagonWrapper: builds the internal system + grid wrapper, then
    // wires the IoTaskThread pointers exactly as the JUCE wrapper does. Resets
    // the global SampleTimer/RNG so each rig starts from a deterministic clock.
    //
    SynthRig()
    {
        GlobalEnv::Init();
        GlobalEnv::ResetPerTest();

        m_ioTaskThread = std::make_unique<IoTaskThread>();
        m_internal = std::make_unique<TheNonagonSquiggleBoyInternal>();
        m_quad = std::make_unique<TheNonagonSquiggleBoyQuadLaunchpadTwister>(m_internal.get());

        // Wire the IO task thread exactly like NonagonWrapper's ctor.
        //
        m_internal->m_squiggleBoy.m_ioTaskThread = m_ioTaskThread.get();
        m_internal->m_squiggleBoy.m_recordingManager.m_ioTaskThread = m_ioTaskThread.get();
        m_internal->m_configGrid.m_ioTaskThread = m_ioTaskThread.get();

        m_audioInput.m_numInputs = 0;

        // Synthetic wallclock that advances ~one audio callback per RunSamples.
        //
        m_wallclockUs = 0;
    }

    ~SynthRig()
    {
        // Join the IO worker thread BEFORE tearing down m_internal: an in-flight
        // persist task references m_internal's recording buffers / sample banks,
        // so stopping the thread first avoids a use-after-free during teardown.
        //
        if (m_ioTaskThread)
        {
            m_ioTaskThread->Shutdown();
        }

        m_quad.reset();
        m_internal.reset();
        m_ioTaskThread.reset();
    }

    SynthRig(const SynthRig&) = delete;
    SynthRig& operator=(const SynthRig&) = delete;

    // ---- Time driving ------------------------------------------------------
    //
    // RunSamples(n): advance the system by n audio samples, capturing output.
    //
    // Drive sequence per RunSamples() call -- faithful to NonagonWrapper::Process:
    //   1. SampleTimer::StartFrame(wallclockUs) ONCE at the top (the JUCE host
    //      calls StartFrame once per audio callback; we treat one RunSamples
    //      call as one callback). This advances the frame-zero timestamp used by
    //      GetAbsTimeUs(), so message timestamps resolve sensibly.
    //   2. For each sample:
    //        a. if SampleTimer::IncrementSample() returned true (frame boundary):
    //               quad.ProcessFrame(); internal.ProcessFrame();
    //               ioTaskThread.Acknowledge();
    //           (NonagonWrapper does quad.ProcessFrame + wrldBldr.ProcessFrame +
    //            internal.ProcessFrame + SendMidiOutput + ioTaskThread.Acknowledge;
    //            we drop the MIDI-out send -- it has no audio/state effect -- and
    //            keep the rest in order.)
    //        b. quad.ProcessSample()  -- runs the pad grids (m_gridHolder.Process)
    //           then drains the grid MessageInBus (ProcessMessages) at the current
    //           GetAbsTimeUs(). This is where queued pad presses take effect.
    //        c. output = internal.ProcessSample(audioInput).
    //        d. capture output; sticky NaN/Inf scan over every channel.
    //
    // Messages are drained per-SAMPLE inside quad.ProcessSample (matching the
    // hardware), so a pad/encoder verb queued before RunSamples lands on the very
    // first sample.
    //
    void RunSamples(std::size_t n)
    {
        SampleTimer::StartFrame(m_wallclockUs);

        for (std::size_t i = 0; i < n; ++i)
        {
            if (SampleTimer::IncrementSample())
            {
                m_quad->ProcessFrame();
                m_internal->ProcessFrame();
                m_ioTaskThread->Acknowledge();
            }

            m_quad->ProcessSample();
            QuadFloatWithStereoAndSub out = m_internal->ProcessSample(m_audioInput);

            CaptureAndScan(out);
        }

        // Advance synthetic wallclock by the wall-time these samples represent so
        // GetAbsTimeUs() keeps moving forward across calls.
        //
        m_wallclockUs += n * 1000 * 1000 / SampleTimer::x_sampleRate;
    }

    // RunFrames(n): advance by n full process frames (n * 512 samples).
    //
    void RunFrames(std::size_t n)
    {
        RunSamples(n * SampleTimer::x_samplesPerProcessFrame);
    }

    // RunSeconds(s): advance by s seconds of audio (rounded to whole samples).
    //
    void RunSeconds(double s)
    {
        RunSamples(static_cast<std::size_t>(s * SampleTimer::x_sampleRate + 0.5));
    }

    // ---- Injectable audio input -------------------------------------------
    //
    // By default the audio input is silent (m_numInputs == 0). Tests that want
    // to feed audio (e.g. the vocoder / sampler) can set the input buffer; it is
    // held constant until changed.
    //
    void SetAudioInput(const AudioInputBuffer& in) { m_audioInput = in; }
    void ClearAudioInput()
    {
        m_audioInput = AudioInputBuffer();
        m_audioInput.m_numInputs = 0;
    }

    // ---- Encoder verbs (front door -> internal encoder bank) ---------------
    //
    // x,y are the 4x4 encoder-bank coordinates. SetEncoder uses the deterministic
    // absolute EncoderSet message (no timestamp acceleration).
    //
    void SetEncoder(int x, int y, float v01)
    {
        PushInternal(SmartGrid::MessageIn::EncoderSetMsg(x, y, v01));
    }

    void IncEncoder(int x, int y, int delta)
    {
        PushInternal(SmartGrid::MessageIn(SmartGrid::MessageIn::Mode::EncoderIncDec, x, y, delta));
    }

    void PressEncoder(int x, int y)
    {
        PushInternal(SmartGrid::MessageIn(SmartGrid::MessageIn::Mode::EncoderPush, x, y));
    }

    void ReleaseEncoder(int x, int y)
    {
        PushInternal(SmartGrid::MessageIn(SmartGrid::MessageIn::Mode::EncoderRelease, x, y));
    }

    // ---- Pad verbs (through the grid wrapper's MessageInBus) ---------------
    //
    // routeId is one of Route::RouteTopLeft .. RouteBottomRight. A press carries a
    // non-zero velocity (PadPress); a release is PadRelease. Coordinates are the
    // interior-grid (i,j) coordinates from the control-surface map above.
    //
    void PressPad(int routeId, int x, int y, int velocity = 127)
    {
        PushPad(routeId, SmartGrid::MessageIn::Mode::PadPress, x, y, velocity);
    }

    void ReleasePad(int routeId, int x, int y)
    {
        PushPad(routeId, SmartGrid::MessageIn::Mode::PadRelease, x, y, 0);
    }

    // Press + (after a frame) release, so momentary/toggle cells see a complete
    // gesture. Returns after the release has been processed.
    //
    void TapPad(int routeId, int x, int y, int velocity = 127)
    {
        PressPad(routeId, x, y, velocity);
        RunFrames(1);
        ReleasePad(routeId, x, y);
        RunFrames(1);
    }

    // ---- Named convenience pad wrappers (control-surface map) --------------
    //
    void PressRunning()   { PressPad(RouteBottomLeft, -1, 6); }
    void ReleaseRunning() { ReleasePad(RouteBottomLeft, -1, 6); }

    void PressScenePad(int scene)   { PressPad(RouteBottomLeft, scene, 9); }
    void ReleaseScenePad(int scene) { ReleasePad(RouteBottomLeft, scene, 9); }

    void PressGesturePad(int gesture)   { PressPad(RouteBottomLeft, gesture, 8); }
    void ReleaseGesturePad(int gesture) { ReleasePad(RouteBottomLeft, gesture, 8); }

    void PressShiftPad()   { PressPad(RouteBottomRight, 7, 9); }
    void ReleaseShiftPad() { ReleasePad(RouteBottomRight, 7, 9); }

    void PressSavePad()  { PressPad(RouteBottomRight, -1, 0); }
    void ReleaseSavePad(){ ReleasePad(RouteBottomRight, -1, 0); }

    void PressLoadPad()  { PressPad(RouteBottomRight, -1, 1); }
    void ReleaseLoadPad(){ ReleasePad(RouteBottomRight, -1, 1); }

    // ---- Shift -------------------------------------------------------------
    //
    // The shift cell is a Momentary StateCell wired to SceneManager.m_shift.
    // Hardware reaches it through the BottomRightGrid shift pad; that is the
    // realest path and what WithShift() drives. SetShift() flips the same flag
    // directly for convenience when you don't want to spend frames on pad taps.
    //
    void SetShift(bool on) { m_internal->m_sceneManager.m_shift = on; }
    bool GetShift() const  { return m_internal->m_sceneManager.m_shift; }

    // Run fn() with the shift pad physically held, then release it. The shift pad
    // press is applied (a frame is run) before fn so cells see shift as held.
    //
    template <class Fn>
    void WithShift(Fn&& fn)
    {
        PressShiftPad();
        RunFrames(1);
        fn();
        ReleaseShiftPad();
        RunFrames(1);
    }

    // ---- Scene / blend -----------------------------------------------------
    //
    // SelectScene(i): drive the scene pad i, the realest single-pad path. Because
    // of the blend<0.5 / >=0.5 semantics this sets the right scene when blend is
    // low (the default) and the left scene when blend is high. For deterministic
    // left/right targeting use SetLeftScene/SetRightScene.
    //
    void SelectScene(int scene) { TapPad(RouteBottomLeft, scene, 9); }

    void SetLeftScene(int scene)  { m_internal->SetLeftScene(scene); }
    void SetRightScene(int scene) { m_internal->SetRightScene(scene); }

    // SetBlend(f): scene crossfade in [0,1] via the real ParamSet14 (x==0) path.
    //
    void SetBlend(float f)
    {
        PushInternal(MakeParamSet14(/*x=*/0, f));
    }

    // SetFader(i, f): mixer fader i in [0,1] via ParamSet14 (x == i+1).
    //
    void SetFader(int faderIndex, float f)
    {
        PushInternal(MakeParamSet14(/*x=*/faderIndex + 1, f));
    }

    // ---- Sequencer start/stop ----------------------------------------------
    //
    // The running cell is a Toggle StateCell. StartSequencer/StopSequencer drive
    // the real running pad (BottomLeftGrid x=-1 y=6) when a state change is
    // needed -- a tap toggles it. We read m_running to decide whether a toggle is
    // required, so calling StartSequencer when already running is a no-op.
    //
    void StartSequencer()
    {
        if (!m_internal->m_running)
        {
            TapPad(RouteBottomLeft, -1, 6);
        }
    }

    void StopSequencer()
    {
        if (m_internal->m_running)
        {
            TapPad(RouteBottomLeft, -1, 6);
        }
    }

    bool IsSequencerRunning() const { return m_internal->m_running; }

    // ---- Save / Load patch -------------------------------------------------
    //
    // SavePatch(): drive StateInterchange synchronously and return the patch JSON
    // as a string. Exact sequence (single-threaded):
    //   1. m_stateInterchange.RequestSave()      -- arms the save.
    //   2. RunFrames(1)                          -- ProcessFrame ->
    //      HandleStateInterchange sees IsSaveRequested(), serializes ToJSON(),
    //      and calls AckSaveRequested(json) (sets m_toSave, save pending).
    //   3. Poll IsSavePending(); GetToSave() returns the JSON; AckSaveCompleted()
    //      clears it. (The JUCE host does step 3 on its own thread; here we do it
    //      inline right after the frame.)
    // Returns "" on failure (e.g. a save already in flight).
    //
    std::string SavePatch()
    {
        JSON json = SavePatchJSON();
        if (json.IsNull())
        {
            return std::string();
        }
        char* dumped = json.Dumps(0);
        std::string result = dumped ? std::string(dumped) : std::string();
        if (dumped)
        {
            free(dumped);
        }
        return result;
    }

    // SavePatchJSON(): same as SavePatch() but returns the JSON object directly.
    //
    JSON SavePatchJSON()
    {
        if (!m_internal->m_stateInterchange.RequestSave())
        {
            return JSON::Null();
        }

        // Give HandleStateInterchange a frame to service the request.
        //
        RunFrames(1);

        if (!m_internal->m_stateInterchange.IsSavePending())
        {
            return JSON::Null();
        }

        JSON toSave = m_internal->m_stateInterchange.GetToSave();
        m_internal->m_stateInterchange.AckSaveCompleted();
        return toSave;
    }

    // LoadPatch(jsonString): parse and load a patch. Exact sequence:
    //   1. Parse the string to JSON (jansson under EMBEDDED_BUILD).
    //   2. m_stateInterchange.RequestLoad(json) -- arms the load.
    //   3. RunFrames(1) -- ProcessFrame -> HandleStateInterchange sees
    //      IsLoadRequested(), calls FromJSON(GetToLoad()) then AckLoadCompleted().
    // Returns false if the JSON failed to parse or a load was already in flight.
    //
    bool LoadPatch(const std::string& jsonString)
    {
        json_error_t error;
        JSON json = JSON::Loads(jsonString.c_str(), 0, &error);
        if (json.IsNull())
        {
            return false;
        }
        return LoadPatchJSON(json);
    }

    bool LoadPatchJSON(JSON json)
    {
        if (json.IsNull())
        {
            return false;
        }
        if (!m_internal->m_stateInterchange.RequestLoad(json))
        {
            return false;
        }
        RunFrames(1);
        // After the frame the load should have been acknowledged.
        //
        return !m_internal->m_stateInterchange.IsLoadRequested();
    }

    // ResetToDefaults(): drive a "new patch" request through StateInterchange
    // (RevertToDefault all scenes/tracks). Synchronous, single-threaded.
    //
    void ResetToDefaults()
    {
        if (m_internal->m_stateInterchange.RequestNew())
        {
            RunFrames(1);
        }
    }

    // ---- Recording verbs (stubs for WP-9) ----------------------------------
    //
    // The record toggle cell lives at BottomLeftGrid x=-1 y=7. WP-9 will flesh
    // out recording assertions (sample bank persistence via the IoTaskThread).
    // For now expose the real pad tap + an observable.
    //
    void ToggleRecording() { TapPad(RouteBottomLeft, -1, 7); }
    bool IsRecording() const { return m_internal->m_squiggleBoy.IsRecording(); }
    // TODO(WP-9): SetRecordingDirectory + sample-bank persistence assertions.

    // ---- Observation -------------------------------------------------------

    // UIState() -- the published UI state (encoder bank, scopes, meters, ...).
    //
    TheNonagonSquiggleBoyInternal::UIState& UIState() { return m_internal->m_uiState; }
    const TheNonagonSquiggleBoyInternal::UIState& UIState() const { return m_internal->m_uiState; }

    // EncoderValue(x,y,k): the value the encoder bank published to UIState for
    // cell (x,y), channel/voice k (default 0). Populated during ProcessFrame.
    //
    float EncoderValue(int x, int y, std::size_t k = 0) const
    {
        return m_internal->m_uiState.m_squiggleBoyUIState.m_encoderBankUIState.GetValue(x, y, k);
    }

    bool EncoderConnected(int x, int y) const
    {
        return m_internal->m_uiState.m_squiggleBoyUIState.m_encoderBankUIState.GetConnected(x, y);
    }

    // Output() -- the captured-output ring (one OutputSample per processed
    // sample, oldest-to-newest, capped at kOutputCapacity).
    //
    const std::vector<OutputSample>& Output() const { return m_output; }
    void ClearOutput()
    {
        m_output.clear();
        m_outputPeak = 0.0f;
    }

    // LastOutput() -- the most recent captured sample, or a zeroed sample if none.
    //
    OutputSample LastOutput() const
    {
        return m_output.empty() ? OutputSample{} : m_output.back();
    }

    // Peak magnitude across all captured channels since the last ClearOutput().
    //
    float OutputPeak() const { return m_outputPeak; }

    // SawNaN() -- sticky: true if any captured output sample was non-finite.
    //
    bool SawNaN() const { return m_sawNaN; }
    void ClearNaN() { m_sawNaN = false; }

    // MasterPhasor() -- the master-loop phasor [0,1), advancing when the
    // sequencer runs. Read straight off TheoryOfTime.
    //
    double MasterPhasor() const
    {
        return m_internal->m_nonagon.m_nonagon.m_theoryOfTime
            .m_loops[TheoryOfTimeBase::x_masterLoop].m_phasor[SampleTimer::GetUBlockIndex()];
    }

    // Blend / scene observables.
    //
    float Blend() const { return m_internal->m_sceneManager.m_blendFactor; }
    int LeftScene() const  { return static_cast<int>(m_internal->m_sceneManager.m_scene1); }
    int RightScene() const { return static_cast<int>(m_internal->m_sceneManager.m_scene2); }

    // ---- Direct internal access for targeted probes ------------------------
    //
    TheNonagonSquiggleBoyInternal& Internal() { return *m_internal; }
    const TheNonagonSquiggleBoyInternal& Internal() const { return *m_internal; }
    TheNonagonSquiggleBoyQuadLaunchpadTwister& Quad() { return *m_quad; }
    IoTaskThread& IoThread() { return *m_ioTaskThread; }

private:
    static constexpr std::size_t kOutputCapacity = 1u << 20; // ~1M samples (~22s)

    static SmartGrid::MessageIn MakeParamSet14(int x, float f)
    {
        if (f < 0.0f) f = 0.0f;
        if (f > 1.0f) f = 1.0f;
        return SmartGrid::MessageIn(SmartGrid::MessageIn::Mode::ParamSet14,
                                    x, /*y=*/0,
                                    static_cast<int64_t>(std::lround(f * ((1 << 14) - 1))));
    }

    void PushInternal(SmartGrid::MessageIn msg)
    {
        // Route through the grid wrapper's bus exactly as the hardware does:
        // encoder/param messages carry the Encoder route so the wrapper's Apply
        // forwards them to internal->Apply. Timestamp 0 => visible immediately on
        // the next ProcessMessages drain.
        //
        msg.m_routeId = RouteEncoder;
        msg.m_timestamp = 0;
        m_quad->SendMessage(msg);
    }

    void PushPad(int routeId, SmartGrid::MessageIn::Mode mode, int x, int y, int velocity)
    {
        SmartGrid::MessageIn msg(/*timestamp=*/0, routeId, mode, x, y, velocity);
        m_quad->SendMessage(msg);
    }

    void CaptureAndScan(const QuadFloatWithStereoAndSub& out)
    {
        OutputSample s{};
        for (int c = 0; c < 4; ++c)
        {
            s.m_quad[c] = out.m_output[c];
        }
        s.m_stereo[0] = out.m_stereoOutput[0];
        s.m_stereo[1] = out.m_stereoOutput[1];
        s.m_sub = out.m_sub;

        const float* vals = &s.m_quad[0];
        for (int i = 0; i < 7; ++i) // 4 quad + 2 stereo + 1 sub, contiguous
        {
            float v = vals[i];
            if (!std::isfinite(v))
            {
                m_sawNaN = true;
            }
            else
            {
                float mag = std::fabs(v);
                if (mag > m_outputPeak)
                {
                    m_outputPeak = mag;
                }
            }
        }

        if (m_output.size() >= kOutputCapacity)
        {
            // Keep the ring bounded: drop the oldest half in one shot.
            //
            m_output.erase(m_output.begin(), m_output.begin() + m_output.size() / 2);
        }
        m_output.push_back(s);
    }

    std::unique_ptr<IoTaskThread> m_ioTaskThread;
    std::unique_ptr<TheNonagonSquiggleBoyInternal> m_internal;
    std::unique_ptr<TheNonagonSquiggleBoyQuadLaunchpadTwister> m_quad;

    AudioInputBuffer m_audioInput; // silent by default (m_numInputs == 0)
    std::size_t m_wallclockUs = 0;

    std::vector<OutputSample> m_output;
    bool m_sawNaN = false;
    float m_outputPeak = 0.0f;
};

} // namespace synthrig

#include "doctest.h"
#include "../support/SynthRig.hpp"

namespace
{
using Amp = SquiggleBoyTrackGeneric<1>::AmpSection;
using Encoders = SmartGridOneEncoders;

struct SubAmpRig
{
    TheoryOfTimeBase m_time;
    Amp m_amp;
    Amp::Input m_input;
    double m_phase = 0;

    SubAmpRig()
    {
        m_time.m_sample.m_running = true;
        m_time.m_globalPeriodSamples = SampleTimer::SecondsToSamples(1.0);
        m_time.m_sample.m_modulatedPhaseIncrement = 1.0 / m_time.m_globalPeriodSamples;
        m_input.m_ahdInput.SetTheoryOfTime(&m_time);
        m_input.m_mutingASDInput.m_channels[0].m_gate = true;
        m_amp.m_mutingASD.m_channels[0].m_rawOutput = 1.0;
        m_input.m_vcoTargetFreq = 50.0f / SampleTimer::x_sampleRate;
        m_input.m_gain.Update(1.0f);
        m_input.m_subGain.Update(1.0f);
        m_input.m_ahdInput.m_channels[0].m_amplitudePolarity = false;
        m_input.m_ahdInput.m_channels[0].m_amplitude = 0.0f;
        m_amp.m_subOut = 0.0f;
        m_amp.m_subRunning[0] = true;
        m_amp.m_freqDependentGainTarget = 1.0f;
        m_amp.m_freqDependentGainSlew.m_output = 1.0f;
    }

    void RunSamples(size_t samples)
    {
        for (size_t sample = 0; sample < samples; ++sample)
        {
            m_amp.Process(m_input, 1.0f, 0.25f);
        }
    }

    double RunSine(size_t samples)
    {
        double energy = 0;
        for (size_t sample = 0; sample < samples; ++sample)
        {
            m_phase += 100.0 / SampleTimer::x_sampleRate;
            m_phase -= std::floor(m_phase);
            m_amp.Process(m_input, 1.0f, static_cast<float>(0.25 * std::sin(2 * M_PI * m_phase)));
            energy += m_amp.m_subOut * m_amp.m_subOut;
        }

        return std::sqrt(energy / samples);
    }
};
}

DOCTEST_TEST_CASE("Sub: envelope rise and fall have independent exponential time ranges")
{
    GlobalEnv::ResetPerTest();
    const float x_settings[][2] = {{0.0f, 1.0f}, {1.0f, 0.0f}, {0.5f, 0.5f}};
    for (const auto& setting : x_settings)
    {
        SubAmpRig rig;
        rig.m_input.m_subRiseSeconds.Update(setting[0]);
        rig.m_input.m_subFallSeconds.Update(setting[1]);
        const double riseSeconds = 0.001 * std::pow(1000.0, setting[0]);
        const double fallSeconds = 0.001 * std::pow(5000.0, setting[1]);
        DOCTEST_CHECK(rig.m_input.m_subRiseSeconds.m_expParam == doctest::Approx(riseSeconds));
        DOCTEST_CHECK(rig.m_input.m_subFallSeconds.m_expParam == doctest::Approx(fallSeconds));
        rig.RunSamples(static_cast<size_t>(std::lround(riseSeconds * SampleTimer::x_sampleRate)));
        DOCTEST_CHECK(rig.m_amp.m_subSlew.m_output == doctest::Approx(1.0 - std::exp(-1.0)).epsilon(0.002));
        DOCTEST_CHECK(rig.m_amp.m_output == doctest::Approx(1.0f));
        double envelope = rig.m_amp.m_subSlew.m_output;
        rig.m_amp.m_subRunning[0] = false;
        rig.RunSamples(static_cast<size_t>(std::lround(fallSeconds * SampleTimer::x_sampleRate)));
        DOCTEST_CHECK(rig.m_amp.m_subSlew.m_output == doctest::Approx(envelope * std::exp(-1.0)).epsilon(0.002));
        DOCTEST_CHECK(rig.m_amp.m_output == doctest::Approx(1.0f));
    }
}

DOCTEST_TEST_CASE("Sub: changing slew times retains the envelope and its direction")
{
    GlobalEnv::ResetPerTest();
    SubAmpRig rig;
    rig.m_input.m_subRiseSeconds.Update(1.0f);
    rig.m_input.m_subFallSeconds.Update(1.0f);
    const size_t partial = static_cast<size_t>(SampleTimer::SecondsToSamples(0.1));
    const size_t fastest = static_cast<size_t>(SampleTimer::SecondsToSamples(0.001));
    rig.RunSamples(partial);
    const double rising = rig.m_amp.m_subSlew.m_output;
    const double fallAlpha = rig.m_amp.m_subSlew.m_slewDown.m_alpha;
    rig.m_input.m_subRiseSeconds.Update(0.0f);
    DOCTEST_CHECK(rig.m_amp.m_subSlew.m_output == rising);
    rig.RunSamples(fastest);
    DOCTEST_CHECK(rig.m_amp.m_subSlew.m_slewDown.m_alpha == fallAlpha);
    DOCTEST_CHECK(rig.m_amp.m_subSlew.m_output ==
        doctest::Approx(1.0 - (1.0 - rising) * std::exp(-1.0)));
    rig.m_amp.m_subRunning[0] = false;
    rig.RunSamples(partial);
    const double falling = rig.m_amp.m_subSlew.m_output;
    const double riseAlpha = rig.m_amp.m_subSlew.m_slewUp.m_alpha;
    rig.m_input.m_subFallSeconds.Update(0.0f);
    DOCTEST_CHECK(rig.m_amp.m_subSlew.m_output == falling);
    rig.RunSamples(fastest);
    DOCTEST_CHECK(rig.m_amp.m_subSlew.m_slewUp.m_alpha == riseAlpha);
    DOCTEST_CHECK(rig.m_amp.m_subSlew.m_output == doctest::Approx(falling * std::exp(-1.0)));
}

DOCTEST_TEST_CASE("Sub: drive shapes the enveloped oscillator")
{
    GlobalEnv::ResetPerTest();
    SubAmpRig rig;
    rig.m_input.m_subRiseSeconds.Update(1.0f);
    rig.m_input.m_subDrive.Update(1.0f);
    SubAmpRig lowDrive;
    lowDrive.m_input.m_subRiseSeconds.Update(1.0f);
    const size_t samples = static_cast<size_t>(SampleTimer::SecondsToSamples(0.1));
    const double highRMS = rig.RunSine(samples);
    const double lowRMS = lowDrive.RunSine(samples);
    DOCTEST_CHECK(rig.m_amp.m_subSlew.m_output > 0.09f);
    DOCTEST_CHECK(rig.m_amp.m_subSlew.m_output < 0.1f);
    DOCTEST_CHECK(highRMS > 4 * lowRMS);
    DOCTEST_CHECK(highRMS < 0.2);
    DOCTEST_CHECK(rig.m_amp.m_subSlew.m_output == lowDrive.m_amp.m_subSlew.m_output);
    DOCTEST_CHECK(rig.m_amp.m_output == 1.0f);
}

DOCTEST_TEST_CASE("Sub: volume scales folded audio without changing the envelope or main output")
{
    GlobalEnv::ResetPerTest();
    SubAmpRig rig;
    SubAmpRig reference;
    rig.m_amp.m_subSlew.m_output = 1.0f;
    reference.m_amp.m_subSlew.m_output = 1.0f;
    rig.m_input.m_subDrive.Update(1.0f);
    reference.m_input.m_subDrive.Update(1.0f);
    for (float shape : {0.0f, 0.5f, 1.0f})
    {
        rig.m_input.m_subShape = shape;
        reference.m_input.m_subShape = shape;
        for (float volume : {1.0f, 0.5f, 0.0f})
        {
            rig.m_input.m_subGain.Update(volume);
            const double actual = rig.RunSine(4096);
            const double expected = reference.RunSine(4096) * rig.m_input.m_subGain.m_expParam;
            DOCTEST_CHECK(actual == doctest::Approx(expected).epsilon(0.00001));
            DOCTEST_CHECK(rig.m_amp.m_subSlew.m_output == 1.0);
            DOCTEST_CHECK(rig.m_amp.m_output == 1.0f);
        }
    }

    DOCTEST_CHECK(rig.m_input.m_subDrive.m_expParam == 8.0f);
}

DOCTEST_TEST_CASE("Sub: page controls reach each trio independently and round trip in patches")
{
    synthrig::SynthRig rig;
    auto& synth = rig.Internal().m_squiggleBoy;
    const float x_settings[3][5] =
    {
        {0.25f, 1.0f, 0.0f, 1.0f, 0.15f},
        {0.5f, 0.5f, 0.5f, 0.5f, 0.55f},
        {0.75f, 0.0f, 1.0f, 0.0f, 0.95f}
    };

    for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
    {
        rig.Internal().SetActiveTrio(static_cast<TheNonagonSmartGrid::Trio>(trio));
        synth.m_encoders.SelectBank(Encoders::Bank::Sub);
        for (int x = 0; x < 4; ++x)
        {
            rig.SetEncoder(x, 0, x_settings[trio][x]);
        }

        rig.SetEncoder(1, 1, x_settings[trio][4]);

        // Let the input bus's 20 ms latency elapse before selecting another trio.
        //
        rig.RunFrames(2);
    }

    rig.RunHostFrames(SmartGrid::TickDistributor::x_maxSamplesToDistribute);
    const std::string patch = rig.SavePatch();
    DOCTEST_REQUIRE_FALSE(patch.empty());
    for (bool reload : {false, true})
    {
        if (reload)
        {
            rig.ResetToDefaults();
            DOCTEST_REQUIRE(rig.LoadPatch(patch));
            rig.RunHostFrames(SmartGrid::TickDistributor::x_maxSamplesToDistribute);
            rig.RunFrames(2);
        }

        for (size_t voice = 0; voice < SquiggleBoy::x_numVoices; ++voice)
        {
            const auto& input = synth.m_trackState[voice / Encoders::x_voicesPerTrio].m_ampInput;
            const size_t lane = voice % Encoders::x_voicesPerTrio;
            const float* setting = x_settings[voice / Encoders::x_voicesPerTrio];
            DOCTEST_CHECK(input.m_subGain.m_expParam[lane] ==
                doctest::Approx((std::pow(20.0f, setting[0]) - 1.0f) / 19.0f).epsilon(0.001));
            DOCTEST_CHECK(input.m_subDrive.m_expParam[lane] ==
                doctest::Approx(0.125f * std::pow(64.0f, setting[1])).epsilon(0.001));
            DOCTEST_CHECK(input.m_subShape[lane] == doctest::Approx(setting[4]).epsilon(0.001));
            DOCTEST_CHECK(input.m_subRiseSeconds.m_expParam[lane] ==
                doctest::Approx(0.001f * std::pow(1000.0f, setting[2])).epsilon(0.001));
            DOCTEST_CHECK(input.m_subFallSeconds.m_expParam[lane] ==
                doctest::Approx(0.001f * std::pow(5000.0f, setting[3])).epsilon(0.001));
        }
    }

    DOCTEST_CHECK_FALSE(rig.SawNaN());
}

DOCTEST_TEST_CASE("Sub: analyzer feed separates the shaped sub from main audio and retains its tail")
{
    GlobalEnv::ResetPerTest();
    auto writer = std::make_unique<ScopeWriterGeneric<1, 15>>(1, 4096);
    auto voice = std::make_unique<SquiggleBoyTrackGeneric<1>>();
    voice->SetupAudioScopeWriters(writer.get(), 0);
    SubAmpRig rig;
    rig.m_amp.m_scopeWriter = voice->m_amp.m_scopeWriter;
    rig.m_amp.m_subScopeWriter = voice->m_amp.m_subScopeWriter;
    rig.m_input.m_gain.m_expParam = 0.5f;
    rig.m_amp.m_subSlew.m_output = 1.0f;
    const size_t mainScope = static_cast<size_t>(SmartGridOne::VoiceScopes::PostAmp);
    const size_t subScope = static_cast<size_t>(SmartGridOne::VoiceScopes::Sub);
    rig.RunSamples(1);
    DOCTEST_CHECK(writer->ReadSample(mainScope * 3, 0, 0) == doctest::Approx(1.0f));
    DOCTEST_CHECK(writer->ReadSample(subScope * 3, 0, 0) == doctest::Approx(rig.m_amp.m_subOut / 0.5f));
    DOCTEST_CHECK(writer->ReadSample(subScope * 3, 0, 0) > 0.0f);
    rig.m_amp.m_subRunning[0] = false;
    rig.m_input.m_subFallSeconds.Update(0.0f);
    const size_t idleSample = 48 * SampleTimer::x_oversample;
    const size_t tailSample = static_cast<size_t>(SampleTimer::SecondsToSamples(0.5));
    for (size_t sample = 1; sample <= tailSample; ++sample)
    {
        writer->AdvanceIndex();
        rig.RunSamples(1);
        if (sample == idleSample)
        {
            DOCTEST_CHECK(rig.m_amp.m_ahd.m_channels[0].m_state == AHD::State::Idle);
            DOCTEST_CHECK(writer->ReadSample(subScope * 3, 0, sample) > 0.0f);
        }
    }

    DOCTEST_CHECK(std::abs(writer->ReadSample(subScope * 3, 0, tailSample)) < 0.00001f);
}

DOCTEST_TEST_CASE("Sub: scope starts on each lane's fractional oscillator wrap")
{
    synthrig::SynthRig rig;
    rig.RunSamples(1);
    auto& synth = rig.Internal().m_squiggleBoy;
    auto& track = synth.m_tracks[0];
    auto& input = synth.m_trackState[0];
    const size_t subScope = SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::Sub), 0);
    for (size_t decimation : {1, 4})
    {
        auto writer = std::make_unique<ScopeWriterGeneric<3, 15>>(decimation, 4096);
        track.SetupAudioScopeWriters(writer.get(), 0);
        writer->m_index = 100;
        track.m_sub.m_phase = TrioFloat(63.0f / 64, 61.0f / 64, 55.0f / 64);
        input.m_subInput.m_baseFreq = TrioFloat(1.0f / 16, 1.0f / 8, 1.0f / 4);
        // Capture beyond the fractional start before publishing to the reader.
        //
        for (size_t sample = 0; sample < 8; ++sample)
        {
            track.Processs(input);
            for (size_t lane = 0; lane < 3; ++lane)
            {
                const bool expectedTop = sample == (lane == 2 ? 1 : 0);
                DOCTEST_CHECK(track.m_sub.m_top[lane].m_triggered == expectedTop);
            }

            writer->AdvanceIndex();
        }

        writer->Publish();
        writer->AcceptSamples();
        constexpr double x_wrapSamples[] = {-0.5, -0.25, 0.125};
        for (size_t lane = 0; lane < 3; ++lane)
        {
            const auto& markers = writer->GetMarkers(subScope, lane);
            DOCTEST_CHECK(markers.m_startIndexIndex.load() == 1);
            ScopeReader reader(writer.get(), lane, subScope, 1024, 1);
            DOCTEST_CHECK(reader.m_startIndex == doctest::Approx(100 + x_wrapSamples[lane] / decimation));
        }
    }
}

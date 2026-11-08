#include "doctest.h"
#include "../support/SynthRig.hpp"
#include "../support/HostFrames.hpp"

namespace
{

using Encoders = SmartGridOneEncoders;
using Param = Encoders::Param;
using DelayOLA = QuadGrainManager<QuadDelay::Writer>::SynthesisOLA;

struct SilentAudioBuffer
{
    QuadDouble GetRealTime(const QuadDouble& position)
    {
        return position;
    }

    QuadFloat ReadRealTime(const QuadDouble& position)
    {
        return QuadFloat();
    }
};

}

DOCTEST_TEST_CASE("Delay PM: the last empty slot is bipolar and maps linearly into DSP")
{
    synthrig::SynthRig rig;
    auto& internal = rig.Internal();
    auto& synth = internal.m_squiggleBoy;
    auto& encoders = synth.m_encoders;
    auto* cell = encoders.m_encoderBankBank.GetEncoder(encoders.EncoderIndex(Param::DelayPhaseModulation, 0));
    DOCTEST_REQUIRE(cell);
    auto& bank = encoders.m_encoderBankBank.m_banks[static_cast<size_t>(Encoders::Bank::Delay)];
    DOCTEST_CHECK(bank.GetBase(0, 3) == cell);
    DOCTEST_CHECK(cell->m_bipolar);
    DOCTEST_CHECK(cell->m_switchValues == 0);
    DOCTEST_CHECK(cell->m_defaultValue == 0.5f);
    DOCTEST_CHECK(std::string(cell->m_shortName) == "OFFS");
    for (size_t channel = 0; channel < 4; ++channel)
    {
        DOCTEST_CHECK(cell->GetValue(channel) == 0.0f);
    }

    const float x_values[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    for (size_t pass = 0; pass < std::size(x_values); ++pass)
    {
        cell->SetValue(x_values[pass], true);
        size_t samples = hostframes::SamplesToHostFrameEnd(SmartGrid::TickDistributor::x_maxSamplesToDistribute + 1);
        for (size_t sample = 0; sample < samples; ++sample)
        {
            SampleTimer::IncrementSample();
            encoders.Process();
        }

        synth.SetEncoderParameters(internal.m_squiggleBoyState);
        for (size_t channel = 0; channel < 4; ++channel)
        {
            float expected = (2.0f * x_values[pass] - 1.0f) *
                static_cast<float>(Resynthesizer::GetSynthesisHopSamples() - 2);
            DOCTEST_CHECK(synth.m_delayState.m_grainManagerInput.m_phaseModulation[channel] ==
                doctest::Approx(expected).epsilon(1e-5).scale(1.0));
        }
    }
}

DOCTEST_TEST_CASE("Delay PM: saved settings round trip and older patches start neutral")
{
    GlobalEnv::ResetPerTest();
    SmartGridOneContext context;
    Encoders encoders(&context);
    auto* cell = encoders.m_encoderBankBank.GetEncoder(encoders.EncoderIndex(Param::DelayPhaseModulation, 0));
    JsonArena arena(JsonArena::kDefaultCapacity);
    JSON oldPatch = arena.Object();
    oldPatch.SetNew("DelayWiden", cell->ToJSON(arena));
    encoders.FromJSON(oldPatch);
    for (size_t channel = 0; channel < 4; ++channel)
    {
        DOCTEST_CHECK(cell->GetValue(channel) == 0.0f);
    }

    cell->SetValue(0.25f, true);
    JSON saved = encoders.ToJSON(arena);
    DOCTEST_CHECK(saved.Get("DelayWiden").IsNull());
    DOCTEST_REQUIRE_FALSE(saved.Get("DelayPhaseModulation").IsNull());
    encoders.RevertToDefault(true);
    encoders.FromJSON(saved);
    for (size_t scene = 0; scene < SmartGrid::SceneManager::x_numScenes; ++scene)
    {
        DOCTEST_CHECK(cell->m_targets[scene] == 0.25f);
    }
}

DOCTEST_TEST_CASE("Delay PM: grain playback applies independent offsets every engine sample")
{
    GlobalEnv::ResetPerTest();
    SilentAudioBuffer buffer;
    using Grains = QuadGrainManager<SilentAudioBuffer>;
    auto grains = std::make_unique<Grains>(&buffer);
    constexpr size_t x_hop = Resynthesizer::GetSynthesisHopSamples();
    static_assert(DelayOLA::x_bufferSize == 6 * x_hop);
    static_assert(GrainManagerGeneric<SilentAudioBuffer, 3>::SynthesisOLA::x_latency == 0);
    auto frame = std::make_unique<Grains::SynthesisOLA::Frame>();
    for (size_t i = 0; i < frame->x_tableSize; ++i)
    {
        for (size_t channel = 0; channel < 4; ++channel)
        {
            frame->m_table[i][channel] = static_cast<float>((channel + 1) * i) / frame->x_tableSize;
        }
    }

    grains->m_ola.Write(*frame);
    grains->m_samplesToNextGrain = 4 * x_hop;
    Grains::Input input;
    for (size_t sample = 0; sample < 3 * x_hop; ++sample)
    {
        float sign = sample % 2 == 0 ? -1.0f : 1.0f;
        input.m_phaseModulation = QuadFloat(sign * DelayOLA::x_maxPhaseModulation,
            -sign * DelayOLA::x_maxPhaseModulation, sign * 0.375f, -sign * 12.75f);
        auto output = grains->Process(QuadDouble(), QuadFloat(), input);
        if (sample >= 2 * x_hop)
        {
            for (size_t channel = 0; channel < 4; ++channel)
            {
                double position = static_cast<double>(sample) - x_hop + input.m_phaseModulation[channel];
                double expected = (channel + 1) * position / frame->x_tableSize / 1.5;
                DOCTEST_REQUIRE(output[channel] == doctest::Approx(expected).epsilon(1e-6).scale(1.0));
            }
        }
    }
}

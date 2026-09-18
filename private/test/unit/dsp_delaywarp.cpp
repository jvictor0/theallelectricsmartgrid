#include "doctest.h"
#include "../support/GlobalEnv.hpp"
#include "DelayLine.hpp"
#include "QuadDelay.hpp"
#include "../support/TimeRig.hpp"
#include <array>
#include <cmath>
#include <memory>

namespace
{

constexpr size_t x_testSize = 1 << 16;
using Storage = QuadDelayLineMovableWriter<x_testSize>;

}

DOCTEST_TEST_CASE("DelayWarp: negative audio positions wrap by the physical buffer size")
{
    auto storage = std::make_unique<Storage>();
    auto& line = storage->m_delayLine[0];
    line.m_delayLine[x_testSize - 2] = -2;
    line.m_delayLine[x_testSize - 1] = -1;
    line.m_delayLine[0] = 0;
    line.m_delayLine[1] = 1;
    line.m_delayLine[2] = 2;
    const double positions[] = {-0.25, -65536.25, -196608.25, 65535.75};
    for (double position : positions)
    {
        DOCTEST_CHECK(line.ReadRealTime(position) == doctest::Approx(-0.25));
    }
}

DOCTEST_TEST_CASE("DelayWarp: negative envelope positions wrap by the bucket count")
{
    auto storage = std::make_unique<Storage>();
    auto& line = storage->m_delayLine[0];
    line.m_maxEnvelope[62] = 0.8f;
    line.m_maxEnvelope[63] = 0.4f;
    line.m_minEnvelope[62] = -0.8f;
    line.m_minEnvelope[63] = -0.4f;
    auto envelope = line.GetEnvelopeAtRealTime(-1280);
    DOCTEST_CHECK(envelope.first == doctest::Approx(0.7));
    DOCTEST_CHECK(envelope.second == doctest::Approx(-0.7));
}

DOCTEST_TEST_CASE("DelayWarp: ascending inverse mapping crosses negative time and zero")
{
    auto storage = std::make_unique<Storage>();
    auto& line = storage->m_delayLine[0];
    double realBase = line.m_lastTime;
    for (int n = 0; n < 64; ++n)
    {
        line.Write(0, -32.0 + n);
    }

    DOCTEST_CHECK(line.GetRealTime(-20.5) - realBase == doctest::Approx(11.5).epsilon(1e-7));
    DOCTEST_CHECK(line.GetRealTime(-0.5) - realBase == doctest::Approx(31.5).epsilon(1e-7));
    DOCTEST_CHECK(line.GetRealTime(0.5) - realBase == doctest::Approx(32.5).epsilon(1e-7));
}

DOCTEST_TEST_CASE("DelayWarp: abrupt forward acceleration cannot overshoot recorded timestamps")
{
    auto storage = std::make_unique<Storage>();
    auto& line = storage->m_delayLine[0];
    double realBase = line.m_lastTime;
    for (int n = 0; n < 20; ++n)
    {
        line.Write(0, 10000.0 + n);
    }

    line.Write(0, 20000);
    line.Write(0, 20001);
    DOCTEST_CHECK(line.GetRealTime(12514.25) - realBase == doctest::Approx(19.25).epsilon(1e-7));
    DOCTEST_CHECK(line.GetRealTime(17504.75) - realBase == doctest::Approx(19.75).epsilon(1e-7));
}

DOCTEST_TEST_CASE("DelayWarp: fractional inverse lookup stays between its adjacent timestamps")
{
    auto storage = std::make_unique<Storage>();
    auto& line = storage->m_delayLine[0];
    line.m_writeHeadInverse[x_testSize - 1] = -1000;
    line.m_writeHeadInverse[0] = 100;
    line.m_writeHeadInverse[1] = 101;
    line.m_writeHeadInverse[2] = 9000;
    DOCTEST_CHECK(line.GetRealTime(0.5) == doctest::Approx(100.5));
    DOCTEST_CHECK(line.GetRealTime(-65535.5) == doctest::Approx(100.5));
}

DOCTEST_TEST_CASE("DelayWarp: reversal preserves old history until forward motion replaces it")
{
    auto storage = std::make_unique<Storage>();
    auto& line = storage->m_delayLine[0];
    double realBase = line.m_lastTime;
    for (int position = 0; position <= 100; ++position)
    {
        line.Write(0, position);
    }

    for (int position = 90; position >= 50; --position)
    {
        line.Write(0, position);
    }

    DOCTEST_CHECK(line.m_descending);
    DOCTEST_CHECK(line.GetRealTime(60) - realBase == doctest::Approx(60).epsilon(1e-7));
    for (int position = 51; position <= 70; ++position)
    {
        line.Write(0, position);
    }

    DOCTEST_CHECK_FALSE(line.m_descending);
    DOCTEST_CHECK(line.GetRealTime(60) - realBase == doctest::Approx(151).epsilon(1e-7));
    DOCTEST_CHECK(line.GetRealTime(80) - realBase == doctest::Approx(80).epsilon(1e-7));
}

DOCTEST_TEST_CASE("DelayWarp: held positions resume from the latest recorded timestamp")
{
    auto storage = std::make_unique<Storage>();
    auto& line = storage->m_delayLine[0];
    double realBase = line.m_lastTime;
    const double positions[] = {100, 101, 101, 101, 102, 103, 104};
    for (double position : positions)
    {
        line.Write(0, position);
    }

    DOCTEST_CHECK(line.GetRealTime(101.5) - realBase == doctest::Approx(3.5).epsilon(1e-7));
}

DOCTEST_TEST_CASE("DelayWarp: fast quad grains retain a steady tone with real-time offsets")
{
    GlobalEnv::ResetPerTest();
    auto storage = std::make_unique<Storage>();
    auto grains = std::make_unique<QuadGrainManager<DelayLineMovableWriter<x_testSize>>>(storage.get());
    QuadGrainManager<DelayLineMovableWriter<x_testSize>>::Input input;
    QuadFloat offsets(0.5f, 500.25f, -500.5f, 0.0f);
    std::array<double, 4> blockEnergy{};
    std::array<double, 4> minRms{1, 1, 1, 1};
    std::array<double, 4> maxRms{};
    for (int n = 0; n < 32768; ++n)
    {
        double write = 10000.0 + 5.0 * n;
        float sample = 0.2f * std::sin(2 * M_PI * 750 * n / 48000.0);
        storage->Write(QuadFloat(sample, sample, sample, sample), QuadDouble(write, write, write, write));
        QuadDouble read(write - 4500, write - 4500, write - 4500, write - 30000);
        QuadFloat output = grains->Process(read, offsets, input);
        if (n >= 16384)
        {
            for (size_t channel = 0; channel < 4; ++channel)
            {
                DOCTEST_REQUIRE(std::isfinite(output[channel]));
                blockEnergy[channel] += output[channel] * output[channel];
                if ((n + 1) % 1024 == 0)
                {
                    double rms = std::sqrt(blockEnergy[channel] / 1024);
                    minRms[channel] = std::min(minRms[channel], rms);
                    maxRms[channel] = std::max(maxRms[channel], rms);
                    blockEnergy[channel] = 0;
                }
            }
        }
    }

    for (size_t channel = 0; channel < 4; ++channel)
    {
        DOCTEST_INFO("channel " << channel << " RMS range " << minRms[channel] << " to " << maxRms[channel]);
        DOCTEST_CHECK(minRms[channel] > 0.139);
        DOCTEST_CHECK(maxRms[channel] < 0.144);
    }
}

namespace
{

struct ObservedAudioBuffer
{
    DelayLineMovableWriter<x_testSize>* m_line;
    double m_firstRead = std::numeric_limits<double>::infinity();
    double m_lastRead = -std::numeric_limits<double>::infinity();

    double GetRealTime(double time)
    {
        return m_line->GetRealTime(time);
    }

    float ReadRealTime(double time)
    {
        m_firstRead = std::min(m_firstRead, time);
        m_lastRead = std::max(m_lastRead, time);
        return m_line->ReadRealTime(time);
    }
};

}

DOCTEST_TEST_CASE("DelayWarp: the complete grain and its interpolation support precede the writer")
{
    auto storage = std::make_unique<Storage>();
    auto& line = storage->m_delayLine[0];
    line.m_writeHeadInverse[10] = 99100.25;
    double firstRead = 94879;
    double lastRead = 99998;
    DOCTEST_SUBCASE("An offset toward the writer is bounded after applying the offset")
    {
    }

    DOCTEST_SUBCASE("A safe requested start keeps its exact timing")
    {
        line.m_writeHeadInverse[10] = 90000.25;
        firstRead = 89476.75;
        lastRead = 94595.75;
    }

    ObservedAudioBuffer observed{&line};
    auto grains = std::make_unique<GrainManager<ObservedAudioBuffer>>();
    grains->m_audioBuffer = &observed;
    GrainManager<ObservedAudioBuffer>::Input input;
    grains->Process(10, 500.5, input, 100000);
    DOCTEST_CHECK(observed.m_firstRead == doctest::Approx(firstRead).epsilon(1e-10));
    DOCTEST_CHECK(observed.m_lastRead == doctest::Approx(lastRead).epsilon(1e-10));
    DOCTEST_CHECK(std::floor(observed.m_lastRead) + 2 <= 100000);
}

DOCTEST_TEST_CASE("DelayWarp: real high-Mult clock retains reversals without losing sustained tone")
{
    struct Setting
    {
        float m_shape;
        float m_index;
        float m_skew;
    };

    const Setting settings[] = {{0, 0.5f, 0.5f}, {1, 0.5f, 0.5f}, {0, 1, 0}, {1, 1, 1}};
    for (Setting setting : settings)
    {
        GlobalEnv::ResetPerTest();
        TimeRig rig;
        rig.SetGlobalPeriodSamples(192000);
        rig.SetRunning(true);
        rig.m_input.m_phaseModLFOInput.m_center = 1;
        rig.m_input.m_phaseModLFOInput.m_shape = setting.m_shape;
        rig.m_input.m_phaseModLFOInput.m_attackFrac = setting.m_skew;
        rig.m_input.m_lfoMult.m_expParam = 16;
        rig.m_input.m_modIndex.Update(setting.m_index);
        QuadDelayInputSetter setter;
        QuadDelayInputSetter::Input setterInput;
        setterInput.m_theoryOfTime = rig.Get();
        QuadDelay::Input delayInput;
        delayInput.m_writeHeadPosition = QuadDouble(0, 0, 0, 0);
        for (size_t channel = 0; channel < 4; ++channel)
        {
            setterInput.m_loopSelectorSwitchVal[channel] = 5;
        }

        constexpr size_t x_audioSize = 1 << 20;
        auto storage = std::make_unique<QuadDelayLineMovableWriter<x_audioSize>>();
        auto grains = std::make_unique<QuadGrainManager<DelayLineMovableWriter<x_audioSize>>>(storage.get());
        QuadGrainManager<DelayLineMovableWriter<x_audioSize>>::Input grainInput;
        bool finite = true;
        bool reversed = false;
        bool negativePosition = false;
        double lastHead = 0;
        double maxAdvance = 0;
        double blockEnergy = 0;
        double minRms = 1;
        double maxRms = 0;
        int measuredSamples = 0;
        for (int n = 0; n < 576000; ++n)
        {
            rig.AdvanceSample();
            setter.Process(setterInput, delayInput, nullptr);
            double head = delayInput.m_writeHeadPosition[0];
            if (n > 0)
            {
                reversed = reversed || head < lastHead;
                maxAdvance = std::max(maxAdvance, head - lastHead);
            }

            negativePosition = negativePosition || head < 0;
            lastHead = head;
            float sample = 0.2f * std::sin(2 * M_PI * 750 * n / 48000.0);
            storage->Write(QuadFloat(sample, sample, sample, sample), delayInput.m_writeHeadPosition);
            QuadFloat output = grains->Process(delayInput.m_readHeadPosition, QuadFloat(0, 0, 0, 0), grainInput);
            finite = finite && std::isfinite(output[0]);
            if (n >= 384000)
            {
                ++measuredSamples;
                blockEnergy += output[0] * output[0];
                if (measuredSamples % 1024 == 0)
                {
                    double rms = std::sqrt(blockEnergy / 1024);
                    minRms = std::min(minRms, rms);
                    maxRms = std::max(maxRms, rms);
                    blockEnergy = 0;
                }
            }
        }

        DOCTEST_INFO("shape=" << setting.m_shape << " index=" << setting.m_index << " skew=" << setting.m_skew
            << " RMS=" << minRms << ".." << maxRms << " max advance=" << maxAdvance);
        DOCTEST_CHECK(finite);
        DOCTEST_CHECK(reversed);
        DOCTEST_CHECK(negativePosition);
        DOCTEST_CHECK(minRms > 0.139);
        DOCTEST_CHECK(maxRms < 0.144);
        DOCTEST_MESSAGE("high-Mult shape=" << setting.m_shape << " index=" << setting.m_index << " skew=" << setting.m_skew
            << " RMS=" << minRms << ".." << maxRms << " max advance=" << maxAdvance);
    }
}

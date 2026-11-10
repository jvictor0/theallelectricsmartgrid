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
using Inverter = TimeWarpInverter<x_testSize>;
using Line = DelayLineMovableWriter<x_testSize>;

// Host-rate inverters, recorded once per written frame.
//
using HostLine = DelayLineMovableWriterGeneric<float, 1, x_testSize, 1>;
using HostStorage = DelayLineMovableWriterGeneric<float, 4, x_testSize, 1>;

}

DOCTEST_TEST_CASE("DelayWarp: negative audio positions wrap by the physical buffer size")
{
    auto storage = std::make_unique<Line>();
    auto& line = *storage;
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

DOCTEST_TEST_CASE("DelayWarp: ascending inverse mapping crosses negative time and zero")
{
    auto storage = std::make_unique<Inverter>();
    auto& inverter = *storage;
    constexpr double x_realBase = 17700716.0;
    double realTime = x_realBase;
    for (int n = 0; n < 64; ++n)
    {
        inverter.Write(-32.0 + n, realTime++);
    }

    DOCTEST_CHECK(inverter.GetRealTime(-20.5) - x_realBase == doctest::Approx(11.5).epsilon(1e-7));
    DOCTEST_CHECK(inverter.GetRealTime(-0.5) - x_realBase == doctest::Approx(31.5).epsilon(1e-7));
    DOCTEST_CHECK(inverter.GetRealTime(0.5) - x_realBase == doctest::Approx(32.5).epsilon(1e-7));
}

DOCTEST_TEST_CASE("DelayWarp: abrupt forward acceleration cannot overshoot recorded timestamps")
{
    auto storage = std::make_unique<Inverter>();
    auto& inverter = *storage;
    constexpr double x_realBase = 17700716.0;
    double realTime = x_realBase;
    for (int n = 0; n < 20; ++n)
    {
        inverter.Write(10000.0 + n, realTime++);
    }

    inverter.Write(20000, realTime++);
    inverter.Write(20001, realTime++);
    DOCTEST_CHECK(inverter.GetRealTime(12514.25) - x_realBase == doctest::Approx(19.25).epsilon(1e-7));
    DOCTEST_CHECK(inverter.GetRealTime(17504.75) - x_realBase == doctest::Approx(19.75).epsilon(1e-7));
}

DOCTEST_TEST_CASE("DelayWarp: fractional inverse lookup stays between its adjacent timestamps")
{
    auto storage = std::make_unique<Inverter>();
    auto& inverter = *storage;
    inverter.m_writeHeadInverse[x_testSize - 1] = -1000;
    inverter.m_writeHeadInverse[0] = 100;
    inverter.m_writeHeadInverse[1] = 101;
    inverter.m_fallbackRightTime[0] = 101;
    inverter.m_writeHeadInverse[2] = 9000;
    DOCTEST_CHECK(inverter.GetRealTime(0.5) == doctest::Approx(100.5));
    DOCTEST_CHECK(inverter.GetRealTime(-65535.5) == doctest::Approx(100.5));
}

DOCTEST_TEST_CASE("DelayWarp: reversal preserves old history until forward motion replaces it")
{
    auto storage = std::make_unique<Inverter>();
    auto& inverter = *storage;
    constexpr double x_realBase = 17700716.0;
    double realTime = x_realBase;
    for (int position = 0; position <= 100; ++position)
    {
        inverter.Write(position, realTime++);
    }

    for (int position = 90; position >= 50; --position)
    {
        inverter.Write(position, realTime++);
    }

    DOCTEST_CHECK(inverter.m_descending);
    DOCTEST_CHECK(inverter.GetRealTime(60) - x_realBase == doctest::Approx(60).epsilon(1e-7));
    for (int position = 51; position <= 70; ++position)
    {
        inverter.Write(position, realTime++);
    }

    DOCTEST_CHECK_FALSE(inverter.m_descending);
    DOCTEST_CHECK(inverter.GetRealTime(60) - x_realBase == doctest::Approx(151).epsilon(1e-7));
    DOCTEST_CHECK(inverter.GetRealTime(80) - x_realBase == doctest::Approx(80).epsilon(1e-7));
}

DOCTEST_TEST_CASE("DelayWarp: held positions resume from the latest recorded timestamp")
{
    auto storage = std::make_unique<Inverter>();
    auto& inverter = *storage;
    constexpr double x_realBase = 17700716.0;
    double realTime = x_realBase;
    const double positions[] = {100, 101, 101, 101, 102, 103, 104};
    for (double position : positions)
    {
        inverter.Write(position, realTime++);
    }

    DOCTEST_CHECK(inverter.GetRealTime(101.5) - x_realBase == doctest::Approx(3.5).epsilon(1e-7));
}

DOCTEST_TEST_CASE("DelayWarp: the first forward interval replaces history immediately after reversal")
{
    for (double offset : {0.0, -60.0})
    {
        auto storage = std::make_unique<Inverter>();
        auto& inverter = *storage;
        constexpr double x_realBase = 17700716.0;
        double realTime = x_realBase;
        for (int position = 0; position <= 100; ++position)
        {
            inverter.Write(offset + position, realTime++);
        }

        for (int position = 90; position >= 50; --position)
        {
            inverter.Write(offset + position, realTime++);
        }

        DOCTEST_CHECK(inverter.GetRealTime(offset + 55) - x_realBase == doctest::Approx(55));
        inverter.Write(offset + 60, realTime++);
        DOCTEST_CHECK(inverter.GetRealTime(offset + 55) - x_realBase == doctest::Approx(141.5));
        DOCTEST_CHECK(inverter.GetRealTime(offset + 59) - x_realBase == doctest::Approx(141.9));
        DOCTEST_CHECK(inverter.GetRealTime(offset + 65) - x_realBase == doctest::Approx(65));
        inverter.Write(offset + 70, realTime++);
        DOCTEST_CHECK(inverter.GetRealTime(offset + 65) - x_realBase == doctest::Approx(142.5));
    }
}

DOCTEST_TEST_CASE("DelayWarp: fast quad grains retain a steady tone with real-time offsets")
{
    GlobalEnv::ResetPerTest();
    auto storage = std::make_unique<HostStorage>();
    auto inverter = std::make_unique<HostStorage::Inverter>();
    storage->m_inverter = inverter.get();
    auto grains = std::make_unique<QuadGrainManager<HostStorage>>(storage.get());
    QuadGrainManager<HostStorage>::Input input;
    QuadFloat offsets(0.5f, 500.25f, -500.5f, 0.0f);
    std::array<double, 4> blockEnergy{};
    std::array<double, 4> minRms{1, 1, 1, 1};
    std::array<double, 4> maxRms{};
    std::array<double, 4> toneReal{};
    std::array<double, 4> toneImag{};

    // Move the heads every engine sample, but record once per host frame as
    // QuadDelay does. The warped write head advances five host samples per write.
    //
    const int ratio = static_cast<int>(SampleTimer::x_oversample);
    const int block = 1024 * ratio;
    for (int n = 0; n < 32768 * ratio; ++n)
    {
        double write = 10000.0 + 5.0 * n / ratio;
        double phase = 2 * M_PI * SampleTimer::HzToCycPerSamp(750.0) * n;
        if ((n + 1) % ratio == 0)
        {
            float sample = 0.2f * std::sin(phase);
            inverter->Write(write, static_cast<double>(n / ratio + 1));
            storage->Write(QuadFloat::Splat(sample), static_cast<size_t>(n / ratio + 1));
        }

        QuadDouble read(write - 4500, write - 4500, write - 4500, write - 30000);
        QuadFloat output = grains->Process(read, offsets, input, storage->GetLatestSampleTime());
        if (n >= 16384 * ratio)
        {
            for (size_t channel = 0; channel < 4; ++channel)
            {
                DOCTEST_REQUIRE(std::isfinite(output[channel]));
                blockEnergy[channel] += output[channel] * output[channel];
                toneReal[channel] += output[channel] * std::cos(phase);
                toneImag[channel] += output[channel] * std::sin(phase);
                if ((n + 1) % block == 0)
                {
                    double rms = std::sqrt(blockEnergy[channel] / block);
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
        double toneAmplitude = 2.0 * std::hypot(toneReal[channel], toneImag[channel]) / (16384 * ratio);
        DOCTEST_CHECK(toneAmplitude == doctest::Approx(0.2).epsilon(0.02));
    }
}

namespace
{

struct ObservedAudioBuffer
{
    HostLine* m_line;
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
    auto storage = std::make_unique<HostLine>();
    auto& line = *storage;
    auto inverter = std::make_unique<HostLine::Inverter>();
    line.m_inverter = inverter.get();
    inverter->m_writeHeadInverse[10] = 99100.25;
    double firstRead = 94879;
    double lastRead = 99998;
    DOCTEST_SUBCASE("An offset toward the writer is bounded after applying the offset")
    {
    }

    DOCTEST_SUBCASE("A safe requested start keeps its exact timing")
    {
        inverter->m_writeHeadInverse[10] = 90000.25;
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
        delayInput.m_writeHeadPhase = 0;
        for (size_t channel = 0; channel < 4; ++channel)
        {
            setterInput.m_loopSelectorSwitchVal[channel] = 5;
        }

        // The theory of time records its write head into this inverter.
        //
        auto storage = std::make_unique<QuadDelay::Writer>();
        storage->m_inverter = rig.Get()->m_timeWarpInverter.get();
        auto grains = std::make_unique<QuadGrainManager<QuadDelay::Writer>>(storage.get());
        QuadGrainManager<QuadDelay::Writer>::Input grainInput;
        bool finite = true;
        bool reversed = false;
        bool negativePosition = false;
        double lastHead = 0;
        double maxAdvance = 0;
        double blockEnergy = 0;
        double minRms = 1;
        double maxRms = 0;
        double toneReal = 0;
        double toneImag = 0;
        int measuredSamples = 0;
        for (int n = 0; n < 576000; ++n)
        {
            rig.AdvanceSample();
            setter.Process(setterInput, delayInput);
            double head = delayInput.m_writeHeadPhase;
            if (n > 0)
            {
                reversed = reversed || head < lastHead;
                maxAdvance = std::max(maxAdvance, head - lastHead);
            }

            negativePosition = negativePosition || head < 0;
            lastHead = head;
            double phase = 2 * M_PI * SampleTimer::HzToCycPerSamp(750.0) * n;
            if ((n + 1) % SampleTimer::x_oversample == 0)
            {
                float sample = 0.2f * std::sin(phase);
                storage->Write(QuadFloat::Splat(sample), SampleTimer::GetHostSample());
            }

            QuadFloat output = grains->Process(delayInput.m_readHeadPhase, QuadFloat(0, 0, 0, 0), grainInput, storage->GetLatestSampleTime());
            finite = finite && std::isfinite(output[0]);
            if (n >= 384000)
            {
                ++measuredSamples;
                blockEnergy += output[0] * output[0];
                toneReal += output[0] * std::cos(phase);
                toneImag += output[0] * std::sin(phase);
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
        double toneAmplitude = 2.0 * std::hypot(toneReal, toneImag) / measuredSamples;
        DOCTEST_CHECK(toneAmplitude == doctest::Approx(0.2).epsilon(0.02));
        DOCTEST_MESSAGE("high-Mult shape=" << setting.m_shape << " index=" << setting.m_index << " skew=" << setting.m_skew
            << " RMS=" << minRms << ".." << maxRms << " max advance=" << maxAdvance);
    }
}

DOCTEST_TEST_CASE("DelayWarp: unavailable history and fractional reads use explicit timestamps")
{
    TimeWarpInverter<16> inverse;
    DOCTEST_CHECK_FALSE(std::isfinite(inverse.GetRealTime(-1.0)));
    DOCTEST_CHECK_FALSE(std::isfinite(inverse.GetRealTime(0.0)));
    inverse.Write(0.0, 1000000000.0);
    inverse.Write(1.0, 1000000004.0);
    DOCTEST_CHECK(inverse.GetRealTime(0.0) == 1000000000.0);
    DOCTEST_CHECK(inverse.GetRealTime(0.5) == 1000000002.0);
    inverse.Write(4.0, 1000000016.0);
    DOCTEST_CHECK(inverse.GetRealTime(2.5) == doctest::Approx(1000000010.0).epsilon(1e-12));
    inverse.Write(2.0, 1000000100.0);
    inverse.Write(3.0, 1000000112.0);
    DOCTEST_CHECK(inverse.GetRealTime(2.0) == 1000000100.0);
    inverse.Write(4.0, 1000000120.0);
    DOCTEST_CHECK(inverse.GetRealTime(2.5) == doctest::Approx(1000000106.0).epsilon(1e-12));
}

DOCTEST_TEST_CASE("DelayWarp: the clock inverse and host audio share absolute engine timestamps")
{
    GlobalEnv::ResetPerTest();
    constexpr size_t x_origin = static_cast<size_t>(1) << 34;
    SampleTimer::s_instance->m_sample = x_origin;
    TimeRig clock;
    auto line = std::make_unique<QuadDelay::Writer>();
    line->m_inverter = clock.Get()->m_timeWarpInverter.get();
    for (size_t n = 1; n <= 64; ++n)
    {
        double previousFrontier = line->GetLatestSampleTime();
        clock.AdvanceSample();
        DOCTEST_CHECK(line->GetLatestSampleTime() == previousFrontier);
        if (SampleTimer::IsHostFrame())
        {
            line->Write(QuadFloat::Splat(static_cast<float>(n)), SampleTimer::GetHostSample());
        }
    }

    double realEngine = clock.Get()->m_timeWarpInverter->GetRealTime(10.5 / 256.0);
    DOCTEST_CHECK(realEngine == static_cast<double>(x_origin) + 10.5);
    double hostTime = line->GetRealTime(10.5 / 256.0);
    DOCTEST_CHECK(hostTime == realEngine / SampleTimer::x_oversample);
    DOCTEST_CHECK(line->ReadRealTime(hostTime)[0] == doctest::Approx(10.5).epsilon(1e-7));
    DOCTEST_CHECK(line->GetLatestSampleTime() == static_cast<double>(SampleTimer::GetHostSample()));
}

namespace
{

template <size_t InverseRate>
void CheckColdStartDelay()
{
    using Storage = DelayLineMovableWriterGeneric<float, 1, 1 << 16, InverseRate>;
    auto line = std::make_unique<Storage>();
    auto inverse = std::make_unique<typename Storage::Inverter>();
    line->m_inverter = inverse.get();
    auto grains = std::make_unique<GrainManager<Storage>>(line.get());
    typename GrainManager<Storage>::Input input;
    constexpr size_t x_ratio = SampleTimer::x_oversample;
    constexpr size_t x_duration = SampleTimer::x_sampleRate * 3 / 2;
    double earlyEnergy = 0.0;
    double lateEnergy = 0.0;
    size_t lateSamples = 0;
    for (size_t n = 1; n <= x_duration; ++n)
    {
        const double host = static_cast<double>(n) / x_ratio;
        if constexpr (InverseRate != 1)
        {
            inverse->Write(host, static_cast<double>(n));
        }

        if (n % x_ratio == 0)
        {
            if constexpr (InverseRate == 1)
            {
                inverse->Write(host, host);
            }

            float tone = 0.2f * std::sin(2.0 * M_PI * 750.0 * n / SampleTimer::x_sampleRate);
            line->Write(tone, n / x_ratio);
        }

        float output = grains->Process(host - 48000.0, 0.0f, input,
            line->GetOldestSampleTime(), line->GetLatestSampleTime())[0];
        DOCTEST_REQUIRE(std::isfinite(output));
        if (n < SampleTimer::x_sampleRate / 2)
        {
            earlyEnergy += output * output;
        }
        else if (n > SampleTimer::x_sampleRate * 5 / 4)
        {
            lateEnergy += output * output;
            ++lateSamples;
        }
    }

    DOCTEST_CHECK(earlyEnergy == 0.0);
    DOCTEST_CHECK(std::sqrt(lateEnergy / lateSamples) == doctest::Approx(0.2 / std::sqrt(2.0)).epsilon(0.02));
}

struct GrainHistoryProbe
{
    size_t m_reads = 0;

    QuadDouble GetRealTime(const QuadDouble& time)
    {
        return time;
    }

    QuadFloat ReadRealTime(const QuadDouble& time)
    {
        ++m_reads;
        QuadFloat result;
        for (size_t channel = 0; channel < 4; ++channel)
        {
            DOCTEST_REQUIRE(std::isfinite(time[channel]));
            result[channel] = 0.2f * std::sin(2.0 * M_PI * 750.0 * time[channel] / SampleTimer::x_hostSampleRate);
        }

        return result;
    }
};

}

DOCTEST_TEST_CASE("DelayWarp: cold-start delay is silent until mapped audio exists and then recovers")
{
    GlobalEnv::ResetPerTest();
    CheckColdStartDelay<1>();
    CheckColdStartDelay<SampleTimer::x_oversample>();
}

DOCTEST_TEST_CASE("DelayWarp: missing grain lanes are silent while valid lanes and OLA tails survive")
{
    GrainHistoryProbe audio;
    auto grains = std::make_unique<QuadGrainManager<GrainHistoryProbe>>(&audio);
    QuadGrainManager<GrainHistoryProbe>::Input input;
    constexpr size_t x_frame = QuadGrainManager<GrainHistoryProbe>::Resynth::x_synthesisFrameSize;
    constexpr size_t x_latency = QuadGrainManager<GrainHistoryProbe>::SynthesisOLA::x_latency;
    double missingEnergy = 0;
    double validEnergy = 0;
    double tailEnergy = 0;
    for (size_t n = 0; n < 4 * x_frame + x_latency; ++n)
    {
        QuadDouble times = QuadDouble::Splat(static_cast<double>(n) / SampleTimer::x_oversample);
        if (n >= x_frame)
        {
            times[0] = std::numeric_limits<double>::quiet_NaN();
        }

        if (n >= 3 * x_frame)
        {
            times = QuadDouble::Splat(std::numeric_limits<double>::quiet_NaN());
        }

        QuadFloat output = grains->Process(times, QuadFloat(), input);
        // Observe the same output windows after the OLA's declared latency.
        // Previously scheduled tails must finish before checking exact silence.
        //
        if (2 * x_frame + x_latency <= n && n < 3 * x_frame + x_latency)
        {
            missingEnergy += output[0] * output[0];
            validEnergy += output[1] * output[1];
        }
        else if (3 * x_frame + x_frame / 4 + x_latency <= n)
        {
            tailEnergy += output[1] * output[1];
        }
    }

    DOCTEST_CHECK(missingEnergy == 0.0);
    DOCTEST_CHECK(validEnergy > 1.0);
    DOCTEST_CHECK(tailEnergy > 1.0);
}

DOCTEST_TEST_CASE("DelayWarp: expired audio is rejected at grain launch before circular storage reads")
{
    GrainHistoryProbe audio;
    auto grains = std::make_unique<QuadGrainManager<GrainHistoryProbe>>(&audio);
    QuadGrainManager<GrainHistoryProbe>::Input input;
    grains->Process(QuadDouble::Splat(100.0), QuadFloat(), input, 5000.0, 15000.0);
    DOCTEST_CHECK(audio.m_reads == 0);
}

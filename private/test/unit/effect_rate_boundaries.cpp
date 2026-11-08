// Effect and I/O rate boundaries, written against SampleTimer::x_oversample.
// The core suite runs them at the production ratio;
// smartgrid_passthrough_rate_tests runs the same cases at ratio 1.
//
#include "doctest.h"

#include <cmath>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "../support/GlobalEnv.hpp"
#include "../support/TimeRig.hpp"
#include "DeepVocoder.hpp"
#include "DelayLine.hpp"
#include "Metering.hpp"
#include "PartialMachine.hpp"
#include "QuadDelay.hpp"
#include "QuadReverb.hpp"
#include "TapeHead.hpp"

#ifdef SMARTGRID_TEST_OVERSAMPLE
static_assert(SampleTimer::x_oversample == SMARTGRID_TEST_OVERSAMPLE);
#endif

namespace
{

constexpr size_t x_ratio = SampleTimer::x_oversample;

// Amplitude of the component at cyclesPerSample, by projection. Windows
// holding whole cycles have no leakage.
//
double ToneAmplitude(const std::vector<float>& signal, double cyclesPerSample)
{
    double real = 0.0;
    double imag = 0.0;
    for (size_t i = 0; i < signal.size(); ++i)
    {
        double phase = 2.0 * M_PI * cyclesPerSample * static_cast<double>(i);
        real += signal[i] * std::cos(phase);
        imag += signal[i] * std::sin(phase);
    }

    return 2.0 * std::sqrt(real * real + imag * imag) / static_cast<double>(signal.size());
}

double MeanSquare(const std::vector<float>& signal)
{
    double sum = 0.0;
    for (float value : signal)
    {
        sum += value * value;
    }

    return sum / static_cast<double>(signal.size());
}

// Smallest and largest RMS over consecutive blocks.
//
std::pair<double, double> BlockRmsRange(const std::vector<float>& signal, size_t blockSize)
{
    double minRms = std::numeric_limits<double>::infinity();
    double maxRms = 0.0;
    for (size_t start = 0; start + blockSize <= signal.size(); start += blockSize)
    {
        double sum = 0.0;
        for (size_t i = start; i < start + blockSize; ++i)
        {
            sum += signal[i] * signal[i];
        }

        double rms = std::sqrt(sum / static_cast<double>(blockSize));
        minRms = std::min(minRms, rms);
        maxRms = std::max(maxRms, rms);
    }

    return std::make_pair(minRms, maxRms);
}

// Positions are in host samples; damping is wide open and the LFO slow.
//
QuadDelay::Input MakeDelayInput(double writePosition, double readPosition, QuadFloat input, float feedback)
{
    QuadDelay::Input result;
    result.m_input = input;
    result.m_return = QuadFloat::Splat(0.0f);
    result.m_feedback = QuadFloat::Splat(feedback);
    result.m_rotate = QuadFloat::Splat(0.0f);
    result.m_writeHeadPosition = writePosition;
    result.m_readHeadPosition = QuadDouble::Splat(readPosition);
    result.m_bffBase = QuadFloat::Splat(static_cast<float>(SampleTimer::HzToCycPerSamp(0.5)));
    result.m_bffWidth = QuadFloat::Splat(65536.0f);

    return result;
}

PartialMachine::Input MakePartialMachineInput()
{
    using Parameter = FrequencyDependentParameter::Parameter;
    PartialMachine::Input result;
    result.m_spectralModelInput.m_numAtoms = 64;
    result.m_spectralModelInput.m_slewUpAlpha = Parameter(1.0f);
    result.m_spectralModelInput.m_slewDownAlpha = Parameter(1.0f);
    result.m_spectralModelInput.m_omegaPortamentoAlpha = Parameter(1.0f);
    result.m_spectralModelInput.m_omegaDensity = Parameter(1.0f);
    result.m_spectralModelInput.m_gainThreshold = 1e-4f;
    result.m_synthesisContextInput.m_bwBaseFrequency = Parameter(1.0f / 4096.0f);
    result.m_synthesisContextInput.m_bwWidth = Parameter(4096.0f);
    result.m_synthesisContextInput.m_volume = Parameter(1.0f);
    result.m_synthesisContextInput.m_bassCutoff = Parameter(1.0f / 4096.0f);
    result.m_synthesisContextInput.m_azimuthFactor = Parameter(0.25f);
    result.m_synthesisContextInput.m_reductionFeedback = Parameter(0.0f);
    result.m_synthesisContextInput.m_unison = Parameter(0.0f);
    result.m_synthesisContextInput.m_pitchShiftDepth = Parameter(1.0f);
    result.m_synthesisContextInput.m_pitchShift = Parameter(0.0f);
    result.m_synthesisContextInput.m_azimuthOffset = 0.0;
    return result;
}

// Renders a partial-machine response to a quad input pattern; returns the
// four-channel sum of the last window.
//
std::vector<float> RenderPartialMachine(QuadFloat pattern, double cyclesPerSample, size_t numSamples, size_t windowSize)
{
    GlobalEnv::ResetPerTest();
    auto partialMachine = std::make_unique<PartialMachine>();
    PartialMachine::Input input = MakePartialMachineInput();
    std::vector<float> window;
    for (size_t n = 0; n < numSamples; ++n)
    {
        float tone = 0.5f * static_cast<float>(std::sin(2.0 * M_PI * cyclesPerSample * static_cast<double>(n)));
        QuadFloat output = partialMachine->Process(pattern * tone, input);
        if (n + windowSize >= numSamples)
        {
            window.push_back(output.Sum());
        }
    }

    return window;
}

struct CountingAudioBuffer
{
    size_t m_reads = 0;

    double GetRealTime(double time) const
    {
        return time;
    }

    float ReadRealTime(double time)
    {
        ++m_reads;
        return 0.0f;
    }
};

}

DOCTEST_TEST_CASE("Rate boundaries: reverb keeps its physical delay times, modulation and damping")
{
    GlobalEnv::ResetPerTest();
    auto reverb = std::make_unique<QuadReverb>();
    QuadReverbInputSetter setter;
    QuadReverb::Input input;

    DOCTEST_CHECK(setter.m_delayTime.m_factor == doctest::Approx(60.0 * x_ratio));
    DOCTEST_CHECK(input.m_delayTimeSamples[0] == doctest::Approx(64.0 * x_ratio));
    DOCTEST_CHECK(SampleTimer::CycPerSampToHz(setter.m_dampingBase.m_factor) == doctest::Approx(23.4375).epsilon(1e-6));
    for (size_t channel = 0; channel < 4; ++channel)
    {
        DOCTEST_CHECK(setter.m_delayTime.m_max[channel] == doctest::Approx(61440.0 * x_ratio));
        DOCTEST_CHECK(SampleTimer::CycPerSampToHz(setter.m_dampingBase.m_max[channel]) == doctest::Approx(24000.0).epsilon(1e-6));
    }

    double ringSeconds = static_cast<double>(QuadReverb::x_delayLineSize) * SampleTimer::x_samplePeriod;
    double longestSeconds = (QuadReverb::x_maxDelayHostSamples + QuadReverb::x_modulationHostSamples) * SampleTimer::x_hostSamplePeriod;
    DOCTEST_CHECK(ringSeconds == doctest::Approx(65536.0 / 48000.0));
    DOCTEST_CHECK(longestSeconds < ringSeconds);

    DOCTEST_CHECK(reverb->m_inputFilter.m_filters[7].GetDelaySamples(0) == static_cast<int>(877 * x_ratio));
    DOCTEST_CHECK(reverb->m_postFeedbackFilter.m_apf2.GetDelaySamples(3) == static_cast<int>(293 * x_ratio));
    DOCTEST_CHECK(reverb->m_preFeedbackFilter.GetDelaySamples(0) == static_cast<int>(3 * x_ratio));
    DOCTEST_CHECK(877 * x_ratio < QuadAllPassFilter::x_maxDelaySamples);
}

DOCTEST_TEST_CASE("Rate boundaries: a reverb impulse first arrives after the physical loop delay")
{
    GlobalEnv::ResetPerTest();
    auto reverb = std::make_unique<QuadReverb>();
    QuadReverb::Input input;
    const size_t delaySamples = 1920 * x_ratio;
    input.m_delayTimeSamples = QuadFloat::Splat(static_cast<float>(delaySamples));
    input.m_feedback = QuadFloat::Splat(0.0f);
    input.m_bffBase = QuadFloat::Splat(static_cast<float>(SampleTimer::HzToCycPerSamp(0.5)));
    input.m_bffWidth = QuadFloat::Splat(65536.0f);

    double before = 0.0;
    double after = 0.0;
    for (size_t n = 0; n < delaySamples * 2; ++n)
    {
        input.m_input = n == 0 ? QuadFloat::Splat(0.5f) : QuadFloat::Splat(0.0f);
        QuadFloat output = reverb->Process(input);
        double energy = (output * output).Sum();
        if (n + 2 < delaySamples)
        {
            before += energy;
        }
        else
        {
            after += energy;
        }
    }

    DOCTEST_CHECK(before < 1e-12);
    DOCTEST_CHECK(after > 1e-3);
}

DOCTEST_TEST_CASE("Rate boundaries: grains launch every analysis hop's duration in engine samples")
{
    GlobalEnv::ResetPerTest();
    CountingAudioBuffer buffer;
    auto grains = std::make_unique<GrainManager<CountingAudioBuffer>>();
    grains->m_audioBuffer = &buffer;
    GrainManager<CountingAudioBuffer>::Input input;
    using Resynth = GrainManager<CountingAudioBuffer>::Resynth;
    DOCTEST_CHECK(Resynth::x_synthesisFrameSize == 4096 * x_ratio);
    DOCTEST_CHECK(Resynth::GetSynthesisHopSamples() == 1024 * x_ratio);
    DOCTEST_CHECK(Resynth::GetGrainLaunchSamples() == 1024);

    std::vector<size_t> launches;
    for (size_t n = 0; n < 4096 * x_ratio + 1; ++n)
    {
        size_t reads = buffer.m_reads;
        grains->Process(1000.0 + static_cast<double>(n) / x_ratio, 0.0f, input, 1.0e9);
        if (buffer.m_reads != reads)
        {
            DOCTEST_CHECK(buffer.m_reads - reads == 2 * Resynth::x_tableSize);
            launches.push_back(n);
        }
    }

    std::vector<size_t> expected = {0, 1024 * x_ratio, 2048 * x_ratio, 3072 * x_ratio, 4096 * x_ratio};
    DOCTEST_CHECK(launches == expected);
}

DOCTEST_TEST_CASE("Rate boundaries: grains keep a host-rate tone's pitch, gain and continuity")
{
    GlobalEnv::ResetPerTest();
    using Line = DelayLineMovableWriter<1 << 16>;
    auto line = std::make_unique<Line>();
    auto grains = std::make_unique<GrainManager<Line>>(line.get());
    GrainManager<Line>::Input input;
    const double hostCycles = 32.0 / 4096.0;
    const size_t numSamples = 32768 * x_ratio;
    const size_t windowSize = 8192 * x_ratio;
    std::vector<float> window;
    for (size_t n = 0; n < numSamples; ++n)
    {
        if (n % x_ratio == 0)
        {
            double hostFrame = static_cast<double>(n / x_ratio);
            float sample = 0.2f * static_cast<float>(std::sin(2.0 * M_PI * hostCycles * hostFrame));
            line->Write(sample, 10000.0 + hostFrame);
        }

        double read = 10000.0 + static_cast<double>(n) / x_ratio - 4500.0;
        float output = grains->Process(read, 0.0f, input, line->m_lastTime - 1.0)[0];
        DOCTEST_REQUIRE(std::isfinite(output));
        if (n + windowSize >= numSamples)
        {
            window.push_back(output);
        }
    }

    double amplitude = ToneAmplitude(window, hostCycles / x_ratio);
    std::pair<double, double> rmsRange = BlockRmsRange(window, 1024 * x_ratio);
    double minRms = rmsRange.first;
    double maxRms = rmsRange.second;
    DOCTEST_INFO("amplitude " << amplitude << " RMS range " << minRms << " to " << maxRms);
    DOCTEST_CHECK(amplitude == doctest::Approx(0.2).epsilon(0.03));
    DOCTEST_CHECK(MeanSquare(window) - amplitude * amplitude / 2.0 < 1e-6);
    DOCTEST_CHECK(minRms > 0.139);
    DOCTEST_CHECK(maxRms < 0.144);
}

DOCTEST_TEST_CASE("Rate boundaries: the delay line is written once per host frame")
{
    GlobalEnv::ResetPerTest();
    auto delay = std::make_unique<QuadDelay>();
    double firstTime = delay->m_delayLine.m_lastTime;
    const size_t numSamples = 4096 * x_ratio;
    for (size_t n = 0; n < numSamples; ++n)
    {
        double host = static_cast<double>(n) / x_ratio;
        QuadDelay::Input input = MakeDelayInput(host, host - 2048.0, QuadFloat::Splat(0.0f), 0.0f);
        delay->Process(input);
    }

    DOCTEST_CHECK(delay->m_delayLine.m_lastTime - firstTime == doctest::Approx(4096.0));
}

DOCTEST_TEST_CASE("Rate boundaries: delay tones keep their gain and shift by the requested octave")
{
    const double hostCycles = 18.0 / 4096.0;
    for (bool shifted : {false, true})
    {
        DOCTEST_CAPTURE(shifted);
        GlobalEnv::ResetPerTest();
        auto delay = std::make_unique<QuadDelay>();
        const size_t numSamples = 32768 * x_ratio;
        const size_t windowSize = 4096 * x_ratio;
        std::vector<float> window;
        for (size_t n = 0; n < numSamples; ++n)
        {
            double host = static_cast<double>(n) / x_ratio;
            float tone = 0.01f * static_cast<float>(std::cos(2.0 * M_PI * hostCycles * host));
            QuadDelay::Input input = MakeDelayInput(host, host - 8192.0, QuadFloat(tone, 0.0f, 0.0f, 0.0f), 0.0f);
            auto& resynthesis = input.m_grainManagerInput.m_resynthInput[0];
            resynthesis.m_shift[0] = Q(2, 1);
            resynthesis.m_fade[0] = shifted ? 1.0f : 0.0f;
            float output = delay->Process(input)[0];
            if (n + windowSize >= numSamples)
            {
                window.push_back(output);
            }
        }

        double targetCycles = (shifted ? 2.0 : 1.0) * hostCycles / x_ratio;
        double amplitude = ToneAmplitude(window, targetCycles);
        double meanSquare = MeanSquare(window);
        DOCTEST_INFO("amplitude " << amplitude << " mean square " << meanSquare);
        DOCTEST_CHECK(amplitude > 0.0100);
        DOCTEST_CHECK(amplitude < 0.0112);
        DOCTEST_CHECK(meanSquare > 0.00005);
        DOCTEST_CHECK(meanSquare < 0.000063);
        DOCTEST_CHECK(meanSquare - amplitude * amplitude / 2.0 < 1e-9);
    }
}

DOCTEST_TEST_CASE("Rate boundaries: the delay stays finite through maximum feedback")
{
    GlobalEnv::ResetPerTest();
    auto delay = std::make_unique<QuadDelay>();
    RGen gen;
    QuadFloat output = QuadFloat::Splat(0.0f);
    float peak = 0.0f;
    const size_t numSamples = SampleTimer::x_sampleRate * 2;
    for (size_t n = 0; n < numSamples; ++n)
    {
        double host = static_cast<double>(n) / x_ratio;
        QuadFloat noise = n < SampleTimer::x_sampleRate / 10
            ? QuadFloat(gen.UniGen(), gen.UniGen(), gen.UniGen(), gen.UniGen()) * 2.0f - QuadFloat::Splat(1.0f)
            : QuadFloat::Splat(0.0f);
        QuadDelay::Input input = MakeDelayInput(host, host - 4800.0, noise, 1.25f);
        input.m_return = output;
        output = delay->Process(input);
        for (size_t i = 0; i < 4; ++i)
        {
            DOCTEST_REQUIRE(std::isfinite(output[i]));
            peak = std::max(peak, std::abs(output[i]));
        }
    }

    DOCTEST_INFO("peak " << peak);
    DOCTEST_CHECK(peak < 8.0f);
}

DOCTEST_TEST_CASE("Rate boundaries: delay tape heads count host samples")
{
    GlobalEnv::ResetPerTest();
    TimeRig clock;
    clock.SetGlobalPeriodSamples(static_cast<double>(SampleTimer::x_sampleRate));
    QuadDelayInputSetter setter;
    QuadDelayInputSetter::Input input;
    input.m_theoryOfTime = clock.Get();
    QuadDelay::Input delayInput;
    clock.AdvanceSample();
    setter.Process(input, delayInput, nullptr);
    double start = delayInput.m_writeHeadPosition;
    const size_t numSamples = 4800 * x_ratio;
    for (size_t n = 0; n < numSamples; ++n)
    {
        clock.AdvanceSample();
        setter.Process(input, delayInput, nullptr);
    }

    DOCTEST_CHECK(delayInput.m_writeHeadPosition - start == doctest::Approx(4800.0));
    DOCTEST_CHECK(setter.m_writeTapeHead.m_globalPeriodSamples == doctest::Approx(48000.0));
    DOCTEST_CHECK(TapeHead::ComputeLoopSamples(clock.Get(), TheoryOfTimeBase::x_globalLoop, setter.m_writeTapeHead.m_globalPeriodSamples) == doctest::Approx(48000.0));
}

DOCTEST_TEST_CASE("Rate boundaries: the partial machine analyzes decimated mono input every host hop")
{
    GlobalEnv::ResetPerTest();
    auto partialMachine = std::make_unique<PartialMachine>();
    PartialMachine::Input input = MakePartialMachineInput();
    DOCTEST_CHECK(PartialMachine::SynthesisOLA::x_frameSize == 4096 * x_ratio);
    const size_t numSamples = 4096 * x_ratio + 3;
    for (size_t n = 0; n < numSamples; ++n)
    {
        partialMachine->Process(QuadFloat::Splat(0.0f), input);
    }

    DOCTEST_CHECK(partialMachine->m_index == numSamples / x_ratio);
}

DOCTEST_TEST_CASE("Rate boundaries: partial-machine tones keep pitch, gain and continuity")
{
    GlobalEnv::ResetPerTest();
    const double cyclesPerSample = 18.0 / (4096.0 * x_ratio);
    const size_t numSamples = 12 * 4096 * x_ratio;
    const size_t windowSize = 4096 * x_ratio;
    std::vector<float> window = RenderPartialMachine(QuadFloat::Splat(1.0f), cyclesPerSample, numSamples, windowSize);
    double amplitude = ToneAmplitude(window, cyclesPerSample);
    double meanSquare = MeanSquare(window);
    std::pair<double, double> rmsRange = BlockRmsRange(window, 1024 * x_ratio);
    double minRms = rmsRange.first;
    double maxRms = rmsRange.second;
    DOCTEST_INFO("amplitude " << amplitude << " mean square " << meanSquare << " RMS range " << minRms << " to " << maxRms);
    DOCTEST_CHECK(amplitude > 2.60);
    DOCTEST_CHECK(amplitude < 2.68);
    DOCTEST_CHECK(meanSquare - amplitude * amplitude / 2.0 < 1e-3 * meanSquare);
    DOCTEST_CHECK(maxRms / minRms < 1.01);
}

DOCTEST_TEST_CASE("Rate boundaries: the partial machine sums its input to mono before analysis")
{
    GlobalEnv::ResetPerTest();
    const double cyclesPerSample = 18.0 / (4096.0 * x_ratio);
    const size_t numSamples = 8 * 4096 * x_ratio;
    const size_t windowSize = 4096 * x_ratio;
    std::vector<float> spread = RenderPartialMachine(QuadFloat(2.0f, 2.0f, 0.0f, 0.0f), cyclesPerSample, numSamples, windowSize);
    std::vector<float> single = RenderPartialMachine(QuadFloat(4.0f, 0.0f, 0.0f, 0.0f), cyclesPerSample, numSamples, windowSize);
    std::vector<float> cancelled = RenderPartialMachine(QuadFloat(1.0f, -1.0f, 1.0f, -1.0f), cyclesPerSample, numSamples, windowSize);
    DOCTEST_CHECK(spread == single);
    DOCTEST_CHECK(MeanSquare(spread) > 1.0);
    DOCTEST_CHECK(MeanSquare(cancelled) < 1e-12);
}

DOCTEST_TEST_CASE("Rate boundaries: the deep vocoder analyzes at the host rate and returns engine pitches")
{
    GlobalEnv::ResetPerTest();
    auto vocoder = std::make_unique<DeepVocoder>();
    DeepVocoder::Input input;
    input.m_enabled = true;
    const double hz = 440.0;
    const size_t numSamples = SampleTimer::x_sampleRate / 2;
    for (size_t n = 0; n < numSamples; ++n)
    {
        float tone = 0.5f * static_cast<float>(std::sin(2.0 * M_PI * SampleTimer::HzToCycPerSamp(hz) * static_cast<double>(n)));
        vocoder->Process(tone, input);
    }

    DOCTEST_CHECK(vocoder->m_index == numSamples / x_ratio);
    DOCTEST_REQUIRE(!vocoder->m_spectralModel.m_atoms.Empty());

    DeepVocoder::VoiceInput& voice = input.m_voiceInput[0];
    voice.m_pitchRatioPre.m_expParam = 1.0f;
    voice.m_pitchCenter = static_cast<float>(SampleTimer::HzToCycPerSamp(hz));
    voice.m_pitchRatioPost = 1.0f;
    AHD::AHDControl control;
    control.m_trig = true;
    float omega = vocoder->TransformNote(0, input, &control);
    DOCTEST_CHECK(control.m_trig);
    DOCTEST_CHECK(SampleTimer::CycPerSampToHz(omega) == doctest::Approx(hz).epsilon(0.01));

    auto uiState = std::make_unique<DeepVocoder::UIState>();
    vocoder->PopulateUIState(uiState.get());
    DOCTEST_CHECK(SampleTimer::CycPerHostSampToHz(uiState->GetPitchCenter(0)) == doctest::Approx(hz).epsilon(1e-6));
    DOCTEST_CHECK(SampleTimer::CycPerHostSampToHz(uiState->GetUsedOmega(0)) == doctest::Approx(hz).epsilon(0.01));
}

DOCTEST_TEST_CASE("Rate boundaries: meters hold peaks and smooth levels in host frames")
{
    GlobalEnv::ResetPerTest();
    Meter meter;
    for (size_t n = 0; n < x_ratio; ++n)
    {
        meter.Process(Meter::Vector::Splat(n == 0 ? 1.0f : 0.0f));
    }

    for (size_t n = 0; n < (SampleTimer::x_hostSampleRate - 1) * x_ratio; ++n)
    {
        meter.Process(Meter::Vector::Splat(0.0f));
    }

    DOCTEST_CHECK(meter.m_peak[0] == 1.0f);
    for (size_t n = 0; n < 1000 * x_ratio; ++n)
    {
        meter.Process(Meter::Vector::Splat(0.0f));
    }

    DOCTEST_CHECK(meter.m_peak[0] < 1.0f);

    Meter steady;
    float saturated = 0.0f;
    for (size_t n = 0; n < 1000 * x_ratio; ++n)
    {
        saturated = steady.ProcessAndSaturate(Meter::Vector::Splat(0.5f))[0];
    }

    double hostFrames = 1000.0;
    double expected = saturated * saturated * (1.0 - std::pow(1.0 - 0.0008, hostFrames));
    DOCTEST_CHECK(steady.m_rms[0] == doctest::Approx(expected).epsilon(1e-3));
}

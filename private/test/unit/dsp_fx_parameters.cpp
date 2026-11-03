#include "doctest.h"
#include "../support/GlobalEnv.hpp"
#include "PartialMachine.hpp"
#include "QuadDelay.hpp"
#include "QuadReverb.hpp"
#include <algorithm>
#include <cmath>

// The effect input setters map knobs four lanes at a time. Each lane must
// match the per-channel scalar mapping it replaced, with distinct knobs per
// lane and knobs changing mid-run.
//
namespace
{

float Knob(size_t lane, size_t sample, float offset)
{
    float base = 0.1f + 0.2f * static_cast<float>(lane) + offset;
    float knob = sample < 64 ? base : base + 0.13f;
    return knob - std::floor(knob);
}

QuadFloat QuadKnob(size_t sample, float offset)
{
    QuadFloat knob;
    for (size_t lane = 0; lane < 4; ++lane)
    {
        knob[lane] = Knob(lane, sample, offset);
    }

    return knob;
}

PhaseUtils::ExpParam Widener(size_t lane)
{
    static constexpr double x_widths[4] = {0.978615137775, 1.0, 0.986803398875, 0.989701964951};
    return PhaseUtils::ExpParam(1.0, x_widths[lane]);
}

constexpr float x_tolerance = 0.00001f;

TheoryOfTimeBase::Input FlatClock(double phase)
{
    TheoryOfTimeBase::Input clock;
    clock.m_running = true;
    clock.m_unmodulatedPhase = phase;
    for (size_t i = 0; i < TheoryOfTimeBase::x_globalLoop; ++i)
    {
        clock.m_input[i].m_parentIndex = TheoryOfTimeBase::x_globalLoop;
        clock.m_input[i].m_parentMult = 1;
    }

    return clock;
}

}

DOCTEST_TEST_CASE("QuadReverbInputSetter: four-lane mappings and slews match per-channel scalars")
{
    GlobalEnv::ResetPerTest();
    QuadReverbInputSetter setter;
    QuadReverbInputSetter::Input input;
    QuadReverb::Input output;

    PhaseUtils::ExpParam delayTime[4];
    PhaseUtils::ExpParam dampingBase[4];
    PhaseUtils::ExpParam dampingWidth[4];
    PhaseUtils::ExpParam modFreq[4];
    PhaseUtils::ExpParam wideners[4];
    PhaseUtils::ZeroedExpParam modDepth[4];
    PhaseUtils::ZeroedExpParam feedback[4];
    OPLowPassFilter delayTimeFilter[4];
    OPLowPassFilter modDepthFilter[4];
    for (size_t lane = 0; lane < 4; ++lane)
    {
        delayTime[lane] = PhaseUtils::ExpParam(60.0, 1024.0 * 60.0);
        dampingBase[lane] = PhaseUtils::ExpParam(1.0 / 2048.0, 0.5);
        dampingWidth[lane] = PhaseUtils::ExpParam(1.0, 2048.0);
        modFreq[lane] = PhaseUtils::ExpParam(SampleTimer::HzToCycPerSamp(0.05), SampleTimer::HzToCycPerSamp(1024.0 * 0.05));
        wideners[lane] = Widener(lane);
        delayTimeFilter[lane].SetAlphaFromNatFreq(SampleTimer::HzToCycPerSamp(1.0));
        modDepthFilter[lane].SetAlphaFromNatFreq(SampleTimer::HzToCycPerSamp(1.0));
        feedback[lane].SetBaseByCenter(0.25);
    }

    for (size_t sample = 0; sample < 128; ++sample)
    {
        input.m_reverbTimeKnob = QuadKnob(sample, 0.0f);
        input.m_modFreqKnob = QuadKnob(sample, 0.05f);
        input.m_modDepthKnob = QuadKnob(sample, 0.1f);
        input.m_feedbackKnob = QuadKnob(sample, 0.15f);
        input.m_dampingBaseKnob = QuadKnob(sample, 0.2f);
        input.m_dampingWidthKnob = QuadKnob(sample, 0.25f);
        input.m_lfoPhaseKnob = QuadKnob(sample, 0.3f);
        setter.Process(input, output);
        for (size_t lane = 0; lane < 4; ++lane)
        {
            DOCTEST_CAPTURE(sample);
            DOCTEST_CAPTURE(lane);
            DOCTEST_CHECK(output.m_delayTimeSamples[lane] == doctest::Approx(delayTime[lane].Update(delayTimeFilter[lane].Process(input.m_reverbTimeKnob[lane]))).epsilon(x_tolerance));
            DOCTEST_CHECK(output.m_bffBase[lane] == doctest::Approx(dampingBase[lane].Update(input.m_dampingBaseKnob[lane])).epsilon(x_tolerance));
            DOCTEST_CHECK(output.m_bffWidth[lane] == doctest::Approx(dampingWidth[lane].Update(input.m_dampingWidthKnob[lane])).epsilon(x_tolerance));
            DOCTEST_CHECK(output.m_modDepth[lane] == doctest::Approx(modDepthFilter[lane].Process(modDepth[lane].Update(input.m_modDepthKnob[lane]))).epsilon(x_tolerance));
            DOCTEST_CHECK(output.m_lfoInput.m_freq[lane] == doctest::Approx(modFreq[lane].Update(input.m_modFreqKnob[lane])).epsilon(x_tolerance));
            DOCTEST_CHECK(output.m_widen[lane] == doctest::Approx(wideners[lane].Update(1.0)).epsilon(x_tolerance));
            DOCTEST_CHECK(output.m_feedback[lane] == doctest::Approx(1.25 * feedback[lane].Update(input.m_feedbackKnob[lane])).epsilon(x_tolerance));
            DOCTEST_CHECK(output.m_lfoInput.m_phaseKnob[lane] == input.m_lfoPhaseKnob[lane]);
        }
    }

    // Each lane keeps its own widening range.
    //
    DOCTEST_CHECK(output.m_widen[0] != output.m_widen[1]);
    DOCTEST_CHECK(output.m_widen[2] != output.m_widen[3]);
}

DOCTEST_TEST_CASE("QuadDelayInputSetter: four-lane mappings and slews match per-channel scalars")
{
    GlobalEnv::ResetPerTest();
    TheoryOfTime time;
    TheoryOfTimeBase& timeBase = time;
    TheoryOfTimeBase::Input clock = FlatClock(100.0);
    timeBase.Process(clock);
    time.m_globalPeriodSamples = 64.0;
    QuadDelayInputSetter setter;
    QuadDelayInputSetter::Input input;
    QuadDelay::Input output;
    input.m_theoryOfTime = &time;

    PhaseUtils::ExpParam dampingBase[4];
    PhaseUtils::ExpParam dampingWidth[4];
    PhaseUtils::ExpParam modFreq[4];
    PhaseUtils::ZeroedExpParam modDepth[4];
    PhaseUtils::ZeroedExpParam feedback[4];
    OPLowPassFilter modDepthFilter[4];
    OPLowPassFilter rotateFilter[4];
    PhaseUtils::ExpParam unisonDetune[4];
    PhaseUtils::ZeroedExpParam unisonGain[4];
    PhaseUtils::ExpParam slewUp[4];
    for (size_t lane = 0; lane < 4; ++lane)
    {
        dampingBase[lane] = PhaseUtils::ExpParam(1.0 / 2048.0, 0.5);
        dampingWidth[lane] = PhaseUtils::ExpParam(1.0, 2048.0);
        modFreq[lane] = PhaseUtils::ExpParam(SampleTimer::HzToCycPerSamp(0.05), SampleTimer::HzToCycPerSamp(1024.0 * 0.05));
        modDepthFilter[lane].SetAlphaFromNatFreq(SampleTimer::HzToCycPerSamp(1.0));
        rotateFilter[lane].SetAlphaFromNatFreq(SampleTimer::HzToCycPerSamp(1.0));
        feedback[lane].SetBaseByCenter(0.25);
        unisonDetune[lane] = PhaseUtils::ExpParam(1.03);
        unisonGain[lane] = PhaseUtils::ZeroedExpParam(20);
        slewUp[lane] = PhaseUtils::ExpParam(0.25 / Resynthesizer::GetGrainLaunchSamples(), 0.5);
    }

    for (size_t sample = 0; sample < 128; ++sample)
    {
        SampleTimer::s_instance->m_sample = sample;
        input.m_modFreqKnob = QuadKnob(sample, 0.05f);
        input.m_modDepthKnob = QuadKnob(sample, 0.1f);
        input.m_feedbackKnob = QuadKnob(sample, 0.15f);
        input.m_dampingBaseKnob = QuadKnob(sample, 0.2f);
        input.m_dampingWidthKnob = QuadKnob(sample, 0.25f);
        input.m_lfoPhaseKnob = QuadKnob(sample, 0.3f);
        input.m_resynthUnisonKnob = QuadKnob(sample, 0.35f) * 0.2f;
        input.m_resynthSlewUpKnob = QuadKnob(sample, 0.4f);
        input.m_resynthShiftFadeKnob = QuadKnob(sample, 0.45f);
        input.m_widenKnob = QuadKnob(sample, 0.5f);
        for (size_t lane = 0; lane < 4; ++lane)
        {
            input.m_rotateSwitchVal[lane] = static_cast<int>((lane + sample / 64) % 4);
        }

        setter.Process(input, output, nullptr);
        for (size_t lane = 0; lane < 4; ++lane)
        {
            DOCTEST_CAPTURE(sample);
            DOCTEST_CAPTURE(lane);
            float rotate = static_cast<float>(input.m_rotateSwitchVal[lane]) / 4.0f;
            DOCTEST_CHECK(output.m_rotate[lane] == doctest::Approx(rotateFilter[lane].Process(rotate)).epsilon(x_tolerance));
            DOCTEST_CHECK(output.m_modDepth[lane] == doctest::Approx(modDepthFilter[lane].Process(modDepth[lane].Update(input.m_modDepthKnob[lane]))).epsilon(x_tolerance));
            DOCTEST_CHECK(output.m_lfoInput.m_freq[lane] == doctest::Approx(modFreq[lane].Update(input.m_modFreqKnob[lane])).epsilon(x_tolerance));
            DOCTEST_CHECK(output.m_feedback[lane] == doctest::Approx(1.25 * feedback[lane].Update(input.m_feedbackKnob[lane])).epsilon(x_tolerance));
            DOCTEST_CHECK(output.m_bffBase[lane] == doctest::Approx(dampingBase[lane].Update(input.m_dampingBaseKnob[lane])).epsilon(x_tolerance));
            DOCTEST_CHECK(output.m_bffWidth[lane] == doctest::Approx(dampingWidth[lane].Update(input.m_dampingWidthKnob[lane])).epsilon(x_tolerance));
            DOCTEST_CHECK(output.m_lfoInput.m_phaseKnob[lane] == input.m_lfoPhaseKnob[lane]);

            const Resynthesizer::Input& resynth = output.m_grainManagerInput.m_resynthInput[lane];
            float unisonKnob = input.m_resynthUnisonKnob[lane];
            DOCTEST_CHECK(resynth.m_unisonDetune == doctest::Approx(unisonDetune[lane].Update(unisonKnob)).epsilon(x_tolerance));
            DOCTEST_CHECK(resynth.m_unisonGain == doctest::Approx(unisonGain[lane].Update(std::min<float>(1.0, 10 * unisonKnob))).epsilon(x_tolerance));
            float slewUpKnob = input.m_resynthSlewUpKnob[lane];
            float expectedSlewUp = slewUpKnob < 0.05f
                ? Resynthesizer::x_immediateSlewUp
                : static_cast<float>(slewUp[lane].Update(1.0f - slewUpKnob));
            DOCTEST_CHECK(resynth.m_slewUp == doctest::Approx(expectedSlewUp).epsilon(x_tolerance));
            DOCTEST_CHECK(resynth.m_fade[0] == input.m_resynthShiftFadeKnob[lane]);
        }

        clock.m_unmodulatedPhase += 1.0 / 64.0;
        timeBase.Process(clock);
    }
}

DOCTEST_TEST_CASE("PartialMachine::InputSetter: anchor mappings match per-anchor scalars")
{
    GlobalEnv::ResetPerTest();
    PartialMachine::InputSetter setter;
    PartialMachine::InputSetter::Input knobs;
    PartialMachine::Input input;
    PhaseUtils::ExpParam attack(0.01, 2.0);
    PhaseUtils::ExpParam decay(0.01, 10.0);
    PhaseUtils::ExpParam portamento(0.01, 2.0);
    PhaseUtils::ExpParam linearFrequency(1.0f / 10.0f, 100.0f);
    PhaseUtils::ExpParam density(1.0f / 1200.0f, 1.0f);
    PhaseUtils::ExpParam bwBase(1.0f / 2048.0f, 0.5f);
    PhaseUtils::ExpParam bwWidth(1.0f, 2048.0f);
    PhaseUtils::ExpParam bassCutoff(1.0f / 2048.0f, 0.5f);
    PhaseUtils::ExpParam azimuth(1.0f / 32.0f, 1.0f);
    PhaseUtils::ExpParam pitchShiftDepth(1.0f, 2.0f);
    for (size_t sample = 0; sample < 128; sample += 63)
    {
        knobs.m_attack = QuadKnob(sample, 0.0f);
        knobs.m_decay = QuadKnob(sample, 0.05f);
        knobs.m_portamento = QuadKnob(sample, 0.1f);
        knobs.m_parameterLinearFrequency = QuadKnob(sample, 0.15f);
        knobs.m_density = QuadKnob(sample, 0.2f);
        knobs.m_bwBaseFrequency = QuadKnob(sample, 0.25f);
        knobs.m_bwWidth = QuadKnob(sample, 0.3f);
        knobs.m_bassCutoff = QuadKnob(sample, 0.35f);
        knobs.m_azimuthFactor = QuadKnob(sample, 0.4f);
        knobs.m_reductionFeedback = QuadKnob(sample, 0.45f);
        knobs.m_unison = QuadKnob(sample, 0.5f);
        knobs.m_pitchShiftDepth = QuadKnob(sample, 0.55f);
        knobs.m_pitchShift = QuadKnob(sample, 0.6f);
        setter.SetInput(knobs, input);
        for (int i = 0; i < FrequencyDependentParameter::x_numParameters; ++i)
        {
            DOCTEST_CAPTURE(sample);
            DOCTEST_CAPTURE(i);
            auto alpha = PartialMachine::InputSetter::TimeToAlpha;
            DOCTEST_CHECK(input.m_spectralModelInput.m_parameterInput.m_linearFreqs.m_parameters[i] == doctest::Approx(linearFrequency.Update(knobs.m_parameterLinearFrequency[i])).epsilon(x_tolerance));
            DOCTEST_CHECK(input.m_spectralModelInput.m_slewUpAlpha.m_parameters[i] == doctest::Approx(alpha(attack.Update(knobs.m_attack[i]))).epsilon(x_tolerance));
            DOCTEST_CHECK(input.m_spectralModelInput.m_slewDownAlpha.m_parameters[i] == doctest::Approx(alpha(decay.Update(knobs.m_decay[i]))).epsilon(x_tolerance));
            DOCTEST_CHECK(input.m_spectralModelInput.m_omegaPortamentoAlpha.m_parameters[i] == doctest::Approx(alpha(portamento.Update(knobs.m_portamento[i]))).epsilon(x_tolerance));
            DOCTEST_CHECK(input.m_spectralModelInput.m_omegaDensity.m_parameters[i] == doctest::Approx(density.Update(1.0f - knobs.m_density[i])).epsilon(x_tolerance));
            DOCTEST_CHECK(input.m_synthesisContextInput.m_bwBaseFrequency.m_parameters[i] == doctest::Approx(bwBase.Update(knobs.m_bwBaseFrequency[i])).epsilon(x_tolerance));
            DOCTEST_CHECK(input.m_synthesisContextInput.m_bwWidth.m_parameters[i] == doctest::Approx(bwWidth.Update(knobs.m_bwWidth[i])).epsilon(x_tolerance));
            DOCTEST_CHECK(input.m_synthesisContextInput.m_volume.m_parameters[i] == 1.0f);
            DOCTEST_CHECK(input.m_synthesisContextInput.m_bassCutoff.m_parameters[i] == doctest::Approx(bassCutoff.Update(knobs.m_bassCutoff[i])).epsilon(x_tolerance));
            DOCTEST_CHECK(input.m_synthesisContextInput.m_azimuthFactor.m_parameters[i] == doctest::Approx(azimuth.Update(knobs.m_azimuthFactor[i])).epsilon(x_tolerance));
            DOCTEST_CHECK(input.m_synthesisContextInput.m_reductionFeedback.m_parameters[i] == knobs.m_reductionFeedback[i]);
            DOCTEST_CHECK(input.m_synthesisContextInput.m_unison.m_parameters[i] == knobs.m_unison[i]);
            DOCTEST_CHECK(input.m_synthesisContextInput.m_pitchShiftDepth.m_parameters[i] == doctest::Approx(pitchShiftDepth.Update(knobs.m_pitchShiftDepth[i])).epsilon(x_tolerance));
            DOCTEST_CHECK(input.m_synthesisContextInput.m_pitchShift.m_parameters[i] == doctest::Approx(2.0f * knobs.m_pitchShift[i] - 1.0f).epsilon(x_tolerance));
        }
    }
}

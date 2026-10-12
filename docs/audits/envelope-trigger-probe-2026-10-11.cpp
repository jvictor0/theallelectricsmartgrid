#include "SquiggleBoy.hpp"
#include <cmath>
#include <cstdio>

namespace
{
void FillClock(TheoryOfTimeBase& time)
{
    for (size_t i = 0; i < time.m_samples.size(); ++i)
    {
        double phase = 1.0 + static_cast<double>(i) / 48000.0;
        time.m_samples[i].m_unmodulatedPhase = phase;
        time.m_samples[i].m_modulatedPhase = phase;
    }
}

void ProbePhysicalTrigger()
{
    TheoryOfTimeBase time;
    FillClock(time);
    PhysicalModelingSource source;
    PhysicalModelingSource::Input input;
    input.m_ahdInput.m_theoryOfTime = &time;
    float attack = std::log(0.01f / 0.0001f) / std::log(2.5f / 0.0001f);
    input.SetAHD(attack, 0.0f, 0.5f, 1.0f);
    for (int block = 0; block < 128; ++block)
    {
        source.ProcessUBlock(input);
    }

    AHD::AHDControl control;
    control.m_trig = true;
    input.m_ahdInput.Set(control);
    source.ProcessUBlock(input);

    AHD reference;
    AHD::Input referenceInput = input.m_ahdInput;
    for (size_t i = 0; i < PhysicalModelingSource::x_uBlockSize; ++i)
    {
        referenceInput.m_trig = i == 0;
        referenceInput.m_samplePosition = static_cast<float>(i) / PhysicalModelingSource::x_oversample;
        reference.Process(referenceInput);
    }

    std::puts("physical_model_10ms_attack,last_sample_time_ms,raw_output,latched_start_level");
    std::printf("current,%.9f,%.9f,%.9f\n", 7.75 * 1000.0 / 48000.0,
        source.m_ahd.m_rawOutput, source.m_ahd.m_startOutput);
    std::printf("trigger_once_reference,%.9f,%.9f,%.9f\n", 7.75 * 1000.0 / 48000.0,
        reference.m_rawOutput, reference.m_startOutput);
    std::printf("ratio,%.6f\n", source.m_ahd.m_rawOutput / reference.m_rawOutput);
    std::printf("trigger_still_high_after_block,%d\n", input.m_ahdInput.m_trig);
}

void ProbeBaseRateEnvelopes()
{
    SampleTimer::Init(SampleTimer::x_samplesPerProcessFrame);
    TheoryOfTimeBase time;
    FillClock(time);
    SquiggleBoyVoice::AmpSection amp;
    SquiggleBoyVoice::Input input;
    input.m_ampInput.m_ahdInput.m_theoryOfTime = &time;
    input.m_ampInput.m_modulationAHDInput.m_theoryOfTime = &time;
    input.m_sourceInput.m_physicalModelingInput.m_ahdInput.m_theoryOfTime = &time;
    input.m_ampInput.m_vcoTargetFreq = 55.0f / 48000.0f;
    float attack = std::log(0.01f / 0.001f) / std::log(2.5f / 0.001f);
    input.m_ampInput.SetAHD(attack, 0.0f, 0.0f, 1.0f);
    input.m_ampInput.SetModulationAHD(attack, 0.0f, 0.0f, 1.0f);
    int ampTriggerCalls = 0;
    int modulationTriggerCalls = 0;
    for (size_t i = 0; i < SampleTimer::x_controlFrameRate; ++i)
    {
        SampleTimer::s_instance->m_sample = i;
        input.m_ahdControl.m_trig = i == 0;
        input.SetGates();
        ampTriggerCalls += input.m_ampInput.m_ahdInput.m_trig ? 1 : 0;
        modulationTriggerCalls += input.m_ampInput.m_modulationAHDInput.m_trig ? 1 : 0;
        amp.Process(input.m_ampInput, 0.0f, 0.0f);
    }

    std::puts("base_rate_10ms_attack,trigger_calls,last_sample_time_ms,raw_output,expected_output,latched_start_level");
    std::printf("amp,%d,%.9f,%.9f,%.9f,%.9f\n", ampTriggerCalls,
        7.0 * 1000.0 / 48000.0, amp.m_ahd.m_rawOutput,
        7.0 * input.m_ampInput.m_ahdInput.m_attackIncrement, amp.m_ahd.m_startOutput);
    std::printf("modulation,%d,%.9f,%.9f,%.9f,%.9f\n", modulationTriggerCalls,
        7.0 * 1000.0 / 48000.0, amp.m_modulationAHD.m_rawOutput,
        7.0 * input.m_ampInput.m_modulationAHDInput.m_attackIncrement,
        amp.m_modulationAHD.m_startOutput);

    amp.m_ahd.m_rawOutput = 1.0f;
    amp.m_ahd.m_state = AHD::State::Running;
    amp.m_modulationAHD.m_rawOutput = 1.0f;
    amp.m_modulationAHD.m_state = AHD::State::Running;
    input.m_ahdControl.m_trig = false;
    input.m_ahdControl.m_release = true;
    int calls = 0;
    while ((amp.m_ahd.m_state != AHD::State::Idle || amp.m_modulationAHD.m_state != AHD::State::Idle) && calls < 1000)
    {
        input.SetGates();
        amp.Process(input.m_ampInput, 0.0f, 0.0f);
        ++calls;
        SampleTimer::IncrementSample();
    }

    std::printf("amp_and_modulation_10ms_release,samples=%d,time_ms=%.6f\n", calls, calls * 1000.0 / 48000.0);
}
}

int main()
{
    ProbePhysicalTrigger();
    ProbeBaseRateEnvelopes();
}

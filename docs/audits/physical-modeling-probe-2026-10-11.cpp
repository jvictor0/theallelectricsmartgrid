#include "PhysicalModelingSource.hpp"
#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
constexpr double x_sampleRate = 192000.0;
constexpr double x_pi = 3.14159265358979323846;

void Settle(CombFilterWithOnePole& comb)
{
    comb.Reset();
    comb.m_dampingAlphaSlew.m_filter.m_output = comb.m_dampingAlphaSlew.m_target;
}

void ProbeMorph()
{
    LinearStateVariableFilter filter;
    std::puts("morph,lp_weight,bp_weight,hp_weight");
    for (float morph : {0.0f, 0.125f, 0.25f, 0.375f, 0.5f, 0.500001f, 0.625f, 0.75f, 0.875f, 1.0f})
    {
        filter.m_lowPass = 1.0f;
        filter.m_bandPass = 0.0f;
        filter.m_highPass = 0.0f;
        float lp = filter.GetMorphedOutput(morph);
        filter.m_lowPass = 0.0f;
        filter.m_bandPass = 1.0f;
        float bp = filter.GetMorphedOutput(morph);
        filter.m_bandPass = 0.0f;
        filter.m_highPass = 1.0f;
        float hp = filter.GetMorphedOutput(morph);
        std::printf("%.6f,%.8f,%.8f,%.8f\n", morph, lp, bp, hp);
    }
}

void ProbeIdealDecay()
{
    std::puts("ideal_hz,knob,intended_t60_s,delay_samples,feedback,echo_time_s,echo_db,inferred_t60_s");
    for (float hz : {50.0f, 200.0f})
    {
        for (float knob : {0.0f, 0.25f, 0.75f, 1.0f})
        {
            CombFilterWithOnePole comb(4.0f);
            comb.SetDampingTime(knob, 48000.0f, 4.0f);
            comb.SetParams(hz / x_sampleRate, 1.0f);
            Settle(comb);
            double t60 = comb.m_dampingTime.m_expParam;
            int delay = static_cast<int>(std::lround(comb.m_compensatedDelaySlew.m_target));
            int repeats = static_cast<int>(std::ceil(t60 * x_sampleRate / delay));
            int echoSample = repeats * delay;
            float output = 0.0f;
            for (int i = 0; i <= echoSample; ++i)
            {
                output = comb.Process(i == 0 ? 1.0f : 0.0f);
            }

            double echoDb = 20.0 * std::log10(std::abs(output));
            double inferredT60 = -60.0 * echoSample / x_sampleRate / echoDb;
            std::printf("%.1f,%.2f,%.8f,%d,%.9f,%.6f,%.6f,%.3f\n",
                hz, knob, t60, delay, comb.m_feedbackSlew.m_target,
                echoSample / x_sampleRate, echoDb, inferredT60);
        }
    }
}

double LoopPhaseFrequency(const CombFilterWithOnePole& comb)
{
    double alpha = comb.m_dampingAlphaSlew.m_target;
    double delay = comb.m_compensatedDelaySlew.m_target;
    double targetPhase = comb.m_feedbackSlew.m_target < 0.0f ? x_pi : 2.0 * x_pi;
    double lo = 0.0;
    double hi = x_sampleRate * 0.25;
    for (int i = 0; i < 80; ++i)
    {
        double hz = (lo + hi) * 0.5;
        double omega = 2.0 * x_pi * hz / x_sampleRate;
        double phase = omega * delay + std::atan2(
            (1.0 - alpha) * std::sin(omega),
            1.0 - (1.0 - alpha) * std::cos(omega));
        if (phase < targetPhase)
        {
            lo = hz;
        }
        else
        {
            hi = hz;
        }
    }

    return (lo + hi) * 0.5;
}

void ProbeRealDecay()
{
    std::puts("real_hz,damping_hz,knob,loop_phase_hz,cents_error,tail_1s_ac_rms,tail_6s_ac_rms,change_over_5s_db,estimated_t60_s,tail_dc");
    for (float hz : {55.0f, 110.0f, 440.0f})
    {
        for (float cutoff : {1000.0f, 23952.0f})
        {
            CombFilterWithOnePole comb(4.0f);
            comb.SetDampingTime(1.0f, 48000.0f, 4.0f);
            comb.SetParams(hz / x_sampleRate, OPLowPassFilter::AlphaFromNatFreq(cutoff / x_sampleRate));
            Settle(comb);
            double frequency = LoopPhaseFrequency(comb);
            for (int i = 0; i < static_cast<int>(x_sampleRate); ++i)
            {
                comb.Process(0.01f * std::sin(2.0 * x_pi * frequency * i / x_sampleRate));
            }

            double rms[7] = {};
            double mean[7] = {};
            for (int second = 0; second < 7; ++second)
            {
                double sum = 0.0;
                double squares = 0.0;
                for (int i = 0; i < static_cast<int>(x_sampleRate); ++i)
                {
                    double y = comb.Process(0.0f);
                    sum += y;
                    squares += y * y;
                }

                mean[second] = sum / x_sampleRate;
                rms[second] = std::sqrt(std::max(0.0, squares / x_sampleRate - mean[second] * mean[second]));
            }

            double changeDb = rms[6] > 1e-7 && rms[1] > 1e-7
                ? 20.0 * std::log10(rms[6] / rms[1])
                : std::numeric_limits<double>::quiet_NaN();
            std::printf("%.1f,%.1f,1.00,%.6f,%.3f,%.9f,%.9f,%.6f,%.3f,%.9f\n",
                hz, cutoff, frequency, 1200.0 * std::log2(frequency / hz),
                rms[1], rms[6], changeDb, -300.0 / changeDb, mean[6]);
        }
    }
}

void ProbeNegativeTuning()
{
    std::puts("negative_target_hz,damping_hz,delay_samples,loop_phase_hz,cents_error,measured_tail_hz,loop_phase_hz_with_compensation_at_played_pitch");
    for (float hz : {55.0f, 110.0f})
    {
        for (float cutoff : {200.0f, 1000.0f})
        {
            CombFilterWithOnePole comb(4.0f);
            comb.SetDampingTime(0.0f, 48000.0f, 4.0f);
            comb.SetParams(hz / x_sampleRate, OPLowPassFilter::AlphaFromNatFreq(cutoff / x_sampleRate));
            Settle(comb);
            double frequency = LoopPhaseFrequency(comb);
            double previous = 0.0;
            double firstCrossing = 0.0;
            double lastCrossing = 0.0;
            int crossings = 0;
            for (int i = 0; i < static_cast<int>(x_sampleRate * 2.0); ++i)
            {
                double y = comb.Process(i == 0 ? 1.0f : 0.0f);
                if (i > static_cast<int>(x_sampleRate * 0.25) && previous < 0.0 && y >= 0.0)
                {
                    double crossing = i - y / (y - previous);
                    if (crossings == 0)
                    {
                        firstCrossing = crossing;
                    }

                    lastCrossing = crossing;
                    ++crossings;
                }

                previous = y;
            }

            double measured = (crossings - 1) * x_sampleRate / (lastCrossing - firstCrossing);
            double originalDelay = comb.m_compensatedDelaySlew.m_target;
            comb.m_compensatedDelaySlew.m_target = x_sampleRate / (2.0 * hz)
                - OPLowPassFilter::PhaseDelay(comb.m_dampingAlphaSlew.m_target, hz / x_sampleRate);
            std::printf("%.1f,%.1f,%.6f,%.6f,%.3f,%.6f,%.6f\n", hz, cutoff,
                originalDelay, frequency,
                1200.0 * std::log2(frequency / hz), measured, LoopPhaseFrequency(comb));
        }
    }
}

void ProbeIdleExcitation()
{
    std::puts("idle_amplitude,settled_envelope,excitation_gain,output_rms");
    for (float amplitude : {0.0f, 0.5f, 1.0f})
    {
        PhysicalModelingSource source;
        PhysicalModelingSource::Input input;
        input.m_baseFreq = 55.0f / 48000.0f;
        input.m_combFeedback = 0.5f;
        input.m_mainSVFCutoff.Update(0.5f);
        input.m_sampleRateReducerFreq.Update(1.0f);
        input.SetAHD(0.0f, 0.0f, 0.0f, amplitude);
        double sum = 0.0;
        size_t samples = 0;
        for (int block = 0; block < 12000; ++block)
        {
            source.ProcessUBlock(input);
            if (block >= 6000)
            {
                for (float value : source.m_uBlockOutput)
                {
                    sum += value * value;
                    ++samples;
                }
            }
        }

        std::printf("%.2f,%.9f,%.9f,%.9f\n", amplitude, source.m_ahd.m_output,
            PhaseUtils::ZeroedExpParam::Compute(10.0f, source.m_ahd.m_output),
            std::sqrt(sum / samples));
    }
}

void ProbeSourceTail()
{
    std::puts("source_hz,tail_second,ac_rms,peak,dc,fundamental_fraction_of_ac_power");
    for (float hz : {55.0f, 440.0f})
    {
        PhysicalModelingSource source;
        PhysicalModelingSource::Input input;
        input.m_baseFreq = hz / 48000.0f;
        input.m_combFeedback = 1.0f;
        input.m_mainSVFCutoff.Update(0.5f);
        input.m_combDampenCutoff.m_expParam = 1000.0f / 48000.0f;
        input.m_sampleRateReducerFreq.Update(1.0f);
        input.SetAHD(0.0f, 0.0f, 0.0f, 0.0f);
        for (int block = 0; block < 600; ++block)
        {
            source.ProcessUBlock(input);
        }

        input.SetAHD(0.0f, 0.0f, 0.0f, 1.0f);
        for (int second = 0; second < 7; ++second)
        {
            double sum = 0.0;
            double squares = 0.0;
            double peak = 0.0;
            double cosine = 0.0;
            double sine = 0.0;
            size_t sample = 0;
            for (int block = 0; block < 6000; ++block)
            {
                source.ProcessUBlock(input);
                for (float y : source.m_uBlockOutput)
                {
                    sum += y;
                    squares += y * y;
                    peak = std::max(peak, static_cast<double>(std::abs(y)));
                    double phase = 2.0 * x_pi * hz * sample / x_sampleRate;
                    cosine += y * std::cos(phase);
                    sine += y * std::sin(phase);
                    ++sample;
                }
            }

            double mean = sum / sample;
            double variance = std::max(0.0, squares / sample - mean * mean);
            double fundamentalPower = 2.0 * (cosine * cosine + sine * sine) / (sample * sample);
            std::printf("%.1f,%d,%.9f,%.9f,%.9f,%.6f\n",
                hz, second, std::sqrt(variance), peak, mean, fundamentalPower / variance);
        }
    }
}

void ProbeRelease()
{
    PhysicalModelingSource source;
    PhysicalModelingSource::Input input;
    float decay = std::log(0.01f / 0.0001f) / std::log(2.5f / 0.0001f);
    input.SetAHD(0.0f, 0.0f, decay, 1.0f);
    for (int block = 0; block < 100; ++block)
    {
        source.ProcessUBlock(input);
    }

    source.m_ahd.m_state = AHD::State::Running;
    source.m_ahd.m_rawOutput = 1.0f;
    input.m_ahdInput.m_release = true;
    int blocks = 0;
    while (source.m_ahd.m_state != AHD::State::Idle && blocks < 1000)
    {
        source.ProcessUBlock(input);
        ++blocks;
    }

    std::puts("release_decay_setting_ms,release_completed_by_ms,measurement_block_ms");
    std::printf("%.6f,%.6f,%.6f\n",
        1000.0 / (48000.0 * input.m_ahdInput.m_decayIncrement),
        blocks * PhysicalModelingSource::x_uBlockSize * 1000.0 / x_sampleRate,
        PhysicalModelingSource::x_uBlockSize * 1000.0 / x_sampleRate);
}
}

int main()
{
    ProbeMorph();
    ProbeIdealDecay();
    ProbeRealDecay();
    ProbeNegativeTuning();
    ProbeIdleExcitation();
    ProbeSourceTail();
    ProbeRelease();
}

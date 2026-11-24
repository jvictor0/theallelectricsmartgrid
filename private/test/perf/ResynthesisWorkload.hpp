#pragma once

#include "Bench.hpp"
#include "Resynthesis.hpp"

template<size_t Size>
void MeasureResynthesis(double seconds, std::ostream& report)
{
    using Synth = ResynthesizerGeneric<float, Size>;
    constexpr size_t x_rate = 48000;
    constexpr size_t x_fixtures = 8;
    constexpr size_t x_warmGrains = 24;
    constexpr double x_tau = 6.2831853071795864769;
    const auto grains = static_cast<size_t>(seconds * x_rate / Synth::x_H);
    Require(grains != 0, "Resynthesis duration must include at least one grain");

    std::vector<typename Synth::Buffer> previous(x_fixtures);
    std::vector<typename Synth::Buffer> current(x_fixtures);
    std::array<ResynthesizerInput, Size> controls;
    for (size_t lane = 0; lane < Size; ++lane)
    {
        auto& control = controls[lane];
        control.m_unisonGain = 0.75f;
        control.m_unisonDetune = 1.015625f + 0.00390625f * lane;
        control.m_shift[0] = lane % 2 == 0 ? Q(3, 2) : Q(1, 2);
        control.m_fade[0] = 0.75f;
        control.m_slewUp = 0.0625f;
        const auto signal = [lane](double sample)
        {
            double value = 0.0;
            for (size_t partial = 1; partial <= 12; ++partial)
            {
                const double frequency = (133.7 + 41.0 * lane) * partial + 0.7 * partial * partial;
                value += std::sin(x_tau * frequency * sample / x_rate) / (8.0 * partial);
            }

            return static_cast<float>(value);
        };

        for (size_t fixture = 0; fixture < x_fixtures; ++fixture)
        {
            for (size_t sample = 0; sample < Synth::x_N; ++sample)
            {
                const double time = 701.0 * fixture + sample;
                const float window = Math4096::Hann(sample);
                previous[fixture].m_table[sample][lane] = window * signal(time - Synth::x_H);
                current[fixture].m_table[sample][lane] = window * signal(time);
            }
        }
    }

    auto synth = std::make_unique<Synth>();
    auto frame = std::make_unique<typename Synth::SynthesisFrame>();
    RGen::s_gen.seed(17);
    for (size_t grain = 0; grain < x_warmGrains; ++grain)
    {
        const size_t fixture = grain % x_fixtures;
        synth->Process(previous[fixture], current[fixture], *frame, controls.data());
    }

    std::vector<double> wallMicros(grains);
    double cpuSeconds = 0;
    double checksum = 0;
    for (size_t grain = 0; grain < grains; ++grain)
    {
        const size_t fixture = grain % x_fixtures;
        const auto wallStart = std::chrono::steady_clock::now();
        const double cpuStart = CPUSeconds();
        synth->Process(previous[fixture], current[fixture], *frame, controls.data());
        asm volatile("" : : "g"(frame.get()) : "memory");
        cpuSeconds += CPUSeconds() - cpuStart;
        wallMicros[grain] = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - wallStart).count();
        for (size_t lane = 0; lane < Size; ++lane)
        {
            checksum += std::abs(frame->m_table[Synth::x_synthesisFrameSize / 2 + fixture][lane]);
        }
    }

    Require(std::isfinite(checksum) && checksum > 0, "Invalid resynthesis output");
    std::sort(wallMicros.begin(), wallMicros.end());
    const double audioSeconds = static_cast<double>(grains * Synth::x_H) / x_rate;
    report << "{\"kind\":\"resynthesis\",\"lanes\":" << Size << ",\"grains\":" << grains
           << ",\"cpu_us_per_grain\":" << cpuSeconds * 1e6 / grains
           << ",\"ms_per_audio_second\":" << cpuSeconds * 1e3 / audioSeconds
           << ",\"wall_p50_us\":" << wallMicros[grains / 2]
           << ",\"wall_p95_us\":" << wallMicros[(grains - 1) * 95 / 100]
           << ",\"wall_max_us\":" << wallMicros.back() << ",\"checksum\":" << checksum << "}\n";
}

#include "doctest.h"
#include "TunedDecayComb.hpp"
#include <cmath>
#include <complex>
#include <iostream>
#include <vector>

namespace
{
constexpr double x_pi = 3.14159265358979323846;

std::complex<double> Project(const std::vector<float>& signal, double frequency, double rate, double start, double duration)
{
    std::complex<double> sum{0.0, 0.0};
    int count = static_cast<int>(duration * rate);
    int offset = static_cast<int>(start * rate);
    for (int i = 0; i < count; ++i)
    {
        double window = 0.5 - 0.5 * std::cos(2.0 * x_pi * i / count);
        sum += static_cast<double>(signal[offset + i]) * window *
            std::polar(1.0, -2.0 * x_pi * frequency * (offset + i) / rate);
    }

    return sum / static_cast<double>(count);
}

std::vector<float> Ring(double frequency, double rate, double knob, double damp, double excitationFrequency,
    double excitationDuration, double outputDuration)
{
    TunedDecayComb comb;
    comb.SetParams(frequency / rate, knob, damp, rate);
    comb.Reset();
    for (int i = 0; i < static_cast<int>(excitationDuration * rate); ++i)
    {
        comb.Process(static_cast<float>(0.01 * std::sin(2.0 * x_pi * excitationFrequency * i / rate)));
    }

    std::vector<float> out(static_cast<size_t>(outputDuration * rate));
    for (auto& y : out)
    {
        y = comb.Process(0.0f);
    }

    return out;
}

std::vector<float> Ring(double frequency, double rate, double knob, double damp, double excitationFrequency)
{
    return Ring(frequency, rate, knob, damp, excitationFrequency, 1.0, 6.5);
}

}

// Literal fixtures catch a wrong cutoff mapping, missing unity-peak gain, or
// excess prototype resonance through actual branch audio, independent of UI.
//
DOCTEST_TEST_CASE("TunedDecayComb: prototype has requested cutoff and unity fundamental peak")
{
    constexpr double x_rate = 192000.0;
    const double x_fixtures[][2]{{0.0, 200.0}, {0.25, 565.685424949238},
        {0.5, 1600.0}, {0.75, 4525.48339959390}, {1.0, 12800.0}};
    for (const auto& fixture : x_fixtures)
    {
        TunedDecayComb comb;
        comb.SetParams(100.0 / x_rate, 1.0, fixture[0], x_rate);
        std::vector<double> probes{100.0, fixture[1]};
        if (fixture[0] == 0.0)
        {
            probes.push_back(300.0);
        }

        for (double hz : probes)
        {
            auto branch = comb.m_branch[0];
            branch.m_coeff.m_r = 1.0;
            std::vector<float> out(static_cast<size_t>(0.15 * x_rate));
            for (int i = 0; i < static_cast<int>(out.size()); ++i)
            {
                out[i] = static_cast<float>(branch.Process(
                    std::sin(2.0 * x_pi * hz * i / x_rate), 0.0));
            }

            double gain = 4.0 * std::abs(Project(out, hz, x_rate, 0.05, 0.1));
            DOCTEST_INFO("damp=" << fixture[0] << " Hz=" << hz << " gain=" << gain);
            if (hz == 300.0)
            {
                DOCTEST_CHECK(gain < 0.36);
            }
            else
            {
                DOCTEST_CHECK(std::abs(gain - (hz == 100.0 ? 1.0 : 0.7071067811865476)) <
                    (hz == 100.0 ? 0.001 : 0.002));
            }

            std::cout << "Comb prototype: damp=" << fixture[0] << " Hz=" << hz
                      << " gain=" << gain << '\n';
        }

        auto dc = comb.m_branch[0];
        dc.m_coeff.m_r = 1.0;
        double dcGain = 0.0;
        for (int i = 0; i < 28800; ++i)
        {
            dcGain = dc.Process(1.0, 0.0);
        }

        if (fixture[0] == 0.0)
        {
            DOCTEST_CHECK(std::abs(dcGain - 0.948683298) < 0.00001);
        }

        DOCTEST_CHECK(dcGain <= 1.0 + 1e-6);
        double peak = dcGain;
        std::vector<double> frequencies{100.0, fixture[1], 96000.0};
        for (int i = 0; i <= 60; ++i)
        {
            frequencies.push_back(10.0 * std::pow(9600.0, i / 60.0));
        }

        for (double hz : frequencies)
        {
            auto real = comb.m_branch[0];
            auto imag = real;
            real.m_coeff.m_r = 1.0;
            imag.m_coeff.m_r = 1.0;
            std::complex<double> measured{};
            for (int i = 0; i < 28800; ++i)
            {
                auto input = std::polar(1.0, 2.0 * x_pi * hz * i / x_rate);
                std::complex<double> output{real.Process(input.real(), 0.0), imag.Process(input.imag(), 0.0)};
                if (i >= 9600)
                {
                    measured += output / input / 19200.0;
                }
            }

            double gain = std::abs(measured);
            peak = std::max(peak, gain);
            DOCTEST_INFO("prototype sweep damp=" << fixture[0] << " Hz=" << hz << " gain=" << gain);
            DOCTEST_CHECK(gain <= 1.0 + 1e-6);
        }

        std::cout << "Comb prototype sweep: damp=" << fixture[0] << " DC=" << dcGain
                  << " peak=" << peak << '\n';
    }
}

// Measuring excitation above the cutoff catches an unfiltered direct output
// even when the return loop itself attenuates upper partials. Compare closed
// DAMP 0 against full-open DAMP 1; both settings retain the low-pass.
//
DOCTEST_TEST_CASE("TunedDecayComb: low-pass output attenuates stopband excitation")
{
    constexpr double x_rate = 48000.0;
    for (double knob : {0.45, 0.55})
    {
        double previousDB = 0.0;
        for (double hz : {8000.0, 16000.0})
        {
            double amplitude[2]{};
            for (int damp = 0; damp < 2; ++damp)
            {
                TunedDecayComb comb;
                comb.SetParams(200.0 / x_rate, knob, static_cast<double>(damp), x_rate);
                std::vector<float> out(48000);
                for (int i = 0; i < 48000; ++i)
                {
                    out[i] = comb.Process(static_cast<float>(std::sin(2.0 * x_pi * hz * i / x_rate)));
                }

                amplitude[damp] = std::abs(Project(out, hz, x_rate, 0.8, 0.2));
            }

            double attenuationDB = 20.0 * std::log10(amplitude[0] / amplitude[1]);
            DOCTEST_INFO("knob=" << knob << " Hz=" << hz << " attenuation dB=" << attenuationDB);
            DOCTEST_CHECK(attenuationDB < (hz == 8000.0 ? -25.0 : -45.0));
            DOCTEST_CHECK(attenuationDB < previousDB - 10.0);
            previousDB = attenuationDB;
            std::cout << "Comb stopband: knob=" << knob << " Hz=" << hz
                      << " attenuation dB=" << attenuationDB << '\n';
        }
    }
}

// Detects per-circulation loss, omitted filter-memory loss, mistuned interpolation,
// or lost unity gain/phase compensation at the stationary fundamental.
//
DOCTEST_TEST_CASE("TunedDecayComb: rendered fundamental pitch and five-second decay")
{
    double worstDecayError = 0.0;
    double worstPitchError = 0.0;
    double worstRelativePitchError = 0.0;
    for (double rate : {48000.0, 192000.0})
    {
        for (double knob : {0.0, 1.0})
        {
            for (double frequency : {(knob == 0.0 ? 0.5 : 1.0) * rate / 16383.0,
                27.53, 55.3, 443.17, 5013.7, rate * 0.25})
            {
                for (double damp : {0.0, 0.25, 0.5, 0.75, 1.0})
                {
                    // Sub-3 Hz boundary fixtures need several cycles to reject image
                    // leakage and more excitation to settle than ordinary pitches.
                    //
                    bool slow = frequency < 3.0;
                    double start = slow ? 3.0 : 0.5;
                    double window = slow ? 20.0 / frequency : 1.0;
                    auto out = Ring(frequency, rate, knob, damp, frequency,
                        slow ? 30.0 : 1.0, start + 5.0 + window);
                    auto early = Project(out, frequency, rate, start, window);
                    auto late = Project(out, frequency, rate, start + 5.0, window);
                    double db = 20.0 * std::log10(std::abs(late / early));
                    double pitchError = std::arg(late / early) / (2.0 * x_pi * 5.0);
                    worstPitchError = std::max(worstPitchError, std::abs(pitchError));
                    DOCTEST_INFO("rate=" << rate << " Hz=" << frequency << " knob=" << knob << " damp=" << damp << " dB=" << db << " pitch error Hz=" << pitchError);
                    worstDecayError = std::max(worstDecayError, std::abs(db + 60.0));
                    DOCTEST_CHECK(std::abs(db + 60.0) < 0.15);
                    DOCTEST_CHECK(std::abs(pitchError) < 0.002);

                    // Extra lower-bound fixtures retain phase/decay checks. Their
                    // crossing shape includes residual modes, biasing this estimator.
                    //
                    if (frequency != (knob == 0.0 ? 0.5 : 1.0) * rate / 16383.0)
                    {
                        double firstCrossing = -1.0;
                        double lastCrossing = 0.0;
                        int crossings = 0;
                        for (int i = static_cast<int>(rate);
                            i < static_cast<int>(2.0 * rate); ++i)
                        {
                            if (out[i - 1] <= 0.0f && out[i] > 0.0f)
                            {
                                double crossing = i - 1.0 - out[i - 1] / (out[i] - out[i - 1]);
                                if (firstCrossing < 0.0)
                                {
                                    firstCrossing = crossing;
                                }

                                lastCrossing = crossing;
                                ++crossings;
                            }
                        }

                        double measuredFrequency = (crossings - 1) * rate / (lastCrossing - firstCrossing);
                        worstRelativePitchError = std::max(worstRelativePitchError, std::abs(measuredFrequency - frequency) / frequency);
                        DOCTEST_CHECK(std::abs(measuredFrequency - frequency) / frequency < 0.0002);
                    }

                    auto middle = Project(out, frequency, rate, start + 2.0, window);
                    double middleDB = 20.0 * std::log10(std::abs(middle / early));
                    DOCTEST_CHECK(std::abs(middleDB - 0.4 * db) < 0.15);
                }
            }
        }
    }

    std::cout << "Comb modal measurement: max five-second decay error dB=" << worstDecayError
              << "; max phase pitch error Hz=" << worstPitchError
              << "; max crossing relative pitch error=" << worstRelativePitchError << '\n';
}

DOCTEST_TEST_CASE("TunedDecayComb: center dry and reset silences memory")
{
    TunedDecayComb comb;
    comb.SetParams(443.17 / 48000.0, 0.5, 1.0, 48000.0);
    for (int i = 0; i < 40000; ++i)
    {
        float x = static_cast<float>(std::sin(i * 0.43));
        DOCTEST_CHECK(comb.Process(x) == x);
    }

    comb.SetParams(0.001, 1.0, 0.5, 48000.0);
    for (int i = 0; i < 40000; ++i)
    {
        comb.Process(i == 0 ? 1.0f : 0.0f);
    }

    comb.Reset();
    for (int i = 0; i < 40000; ++i)
    {
        DOCTEST_CHECK(comb.Process(0.0f) == 0.0f);
    }
}

// Third-mode literals were independently derived from the complex loop poles.
// A full-open comparison catches excess dispersion and missing upper-mode loss.
//
DOCTEST_TEST_CASE("TunedDecayComb: low-pass third partial tuning and loss")
{
    const double x_expectedHz[]{658.545543665752, 659.270770269177};
    const double x_expectedCents[]{-3.819368033379, -1.913886817808};
    const double x_expectedT60[]{1.427293671700, 2.210498614261};
    for (int sign = 0; sign < 2; ++sign)
    {
        for (double damp : {1.0 / 3.0, 1.0})
        {
            auto out = Ring(220.0, 192000.0, static_cast<double>(sign), damp, 660.0);
            auto early = Project(out, 660.0, 192000.0, 0.2, 0.1);
            auto late = Project(out, 660.0, 192000.0, 0.4, 0.1);
            double modalHz = 660.0 + std::arg(late / early) / (2.0 * x_pi * 0.2);
            double cents = 1200.0 * std::log2(modalHz / 660.0);
            double t60 = -12.0 / (20.0 * std::log10(std::abs(late / early)));
            DOCTEST_INFO("sign=" << sign << " damp=" << damp << " cents=" << cents << " T60=" << t60);
            if (damp == 1.0)
            {
                DOCTEST_CHECK(std::abs(cents) < 0.2);
                DOCTEST_CHECK(std::abs(t60 - 5.0) < 0.02);
            }
            else
            {
                DOCTEST_CHECK(std::abs(1200.0 * std::log2(modalHz / x_expectedHz[sign])) < 0.2);
                DOCTEST_CHECK(std::abs(cents - x_expectedCents[sign]) < 0.2);
                DOCTEST_CHECK(std::abs(t60 - x_expectedT60[sign]) / x_expectedT60[sign] < 0.01);
            }

            std::cout << "Comb third partial: sign=" << (sign == 0 ? -1 : 1)
                      << " damp=" << damp << " Hz=" << modalHz << " cents=" << cents
                      << " T60=" << t60 << '\n';
        }
    }
}

DOCTEST_TEST_CASE("TunedDecayComb: damping removes upper partials and DC decay stays bounded")
{
    for (double knob : {0.0, 1.0})
    {
        double previous = 1e30;
        for (double damp : {1.0, 0.5, 0.0})
        {
            TunedDecayComb comb;
            comb.SetParams(200.0 / 48000.0, knob, damp, 48000.0);
            comb.Reset();
            std::vector<float> out(48000);
            for (int i = 0; i < 48000; ++i)
            {
                out[i] = comb.Process(i == 0 ? 1.0f : 0.0f);
            }

            double upper = std::abs(Project(out, 600.0, 48000.0, 0.1, 0.2));
            DOCTEST_CHECK(upper < previous);
            previous = upper;
            double earlyDC = std::abs(Project(out, 0.0, 48000.0, 0.1, 0.2));
            double lateDC = std::abs(Project(out, 0.0, 48000.0, 0.7, 0.2));
            DOCTEST_CHECK(lateDC < earlyDC + 1e-9);
        }
    }
}

// Complex sine response detects missing allpass or radial filter terms in UI.
//
DOCTEST_TEST_CASE("TunedDecayComb: UI agrees with rendered complex transfer")
{
    for (double rate : {48000.0, 192000.0})
    {
        for (double knob : {0.15, 0.5, 0.85})
        {
            for (double damp : {0.0, 0.6, 1.0})
            {
                for (double hz : {0.0, 317.3, 930.0, 7312.0})
                {
                    TunedDecayComb real;
                    TunedDecayComb imag;
                    real.SetParams(317.3 / rate, knob, damp, rate);
                    imag.SetParams(317.3 / rate, knob, damp, rate);
                    TunedDecayComb::UIState ui;
                    real.PopulateUIState(&ui);
                    std::complex<double> measured;
                    for (int i = 0; i < static_cast<int>(3.0 * rate); ++i)
                    {
                        auto input = std::polar(1.0, 2.0 * x_pi * hz * i / rate);
                        std::complex<double> output{real.Process(static_cast<float>(input.real())), imag.Process(static_cast<float>(input.imag()))};
                        if (i >= static_cast<int>(2.0 * rate))
                        {
                            measured += output / input / rate;
                        }
                    }

                    auto expectedFloat = ui.TransferFunctionValue(static_cast<float>(hz / rate));
                    std::complex<double> expected{expectedFloat.real(), expectedFloat.imag()};
                    DOCTEST_INFO("rate=" << rate << " knob=" << knob << " damp=" << damp << " frequency=" << hz);
                    DOCTEST_CHECK(std::abs(measured - expected) / std::max(1.0, std::abs(expected)) < 0.001);
                }
            }
        }
    }
}

// Freezing the live fade retains the shared delay and both independent filter
// memories, so audio exercises the weighted numerator as well as both loops.
//
DOCTEST_TEST_CASE("TunedDecayComb: UI agrees with frozen transition audio")
{
    for (double rate : {48000.0, 192000.0})
    {
        for (double targetDamp : {0.0, 1.0})
        {
            for (double hz : {317.3, 930.0, 7312.0})
            {
                TunedDecayComb real;
                TunedDecayComb imag;
                for (auto* comb : {&real, &imag})
                {
                    comb->SetParams(220.0 / rate, 0.15, 1.0 - targetDamp, rate);
                    comb->SetParams(317.3 / rate, 0.85, targetDamp, rate);
                    for (int i = 0; i < static_cast<int>(0.002 * rate); ++i)
                    {
                        comb->Process(0.0f);
                    }

                    comb->m_step = 0.0;
                }

                TunedDecayComb::UIState ui;
                real.PopulateUIState(&ui);
                DOCTEST_REQUIRE(real.m_transition);
                DOCTEST_REQUIRE(real.m_mix > 0.3);
                DOCTEST_REQUIRE(real.m_mix < 0.5);
                std::complex<double> measured;
                for (int i = 0; i < static_cast<int>(3.0 * rate); ++i)
                {
                    auto input = std::polar(1.0, 2.0 * x_pi * hz * i / rate);
                    std::complex<double> output{real.Process(static_cast<float>(input.real())), imag.Process(static_cast<float>(input.imag()))};
                    if (i >= static_cast<int>(2.0 * rate))
                    {
                        measured += output / input / rate;
                    }
                }

                auto expectedFloat = ui.TransferFunctionValue(static_cast<float>(hz / rate));
                std::complex<double> expected{expectedFloat.real(), expectedFloat.imag()};
                DOCTEST_INFO("rate=" << rate << " target damp=" << targetDamp << " Hz=" << hz);
                DOCTEST_CHECK(std::abs(measured - expected) / std::max(1.0, std::abs(expected)) < 0.001);
            }
        }
    }
}

DOCTEST_TEST_CASE("TunedDecayComb: excited modulation crosses delay boundaries and signs smoothly")
{
    for (double rate : {48000.0, 192000.0})
    {
        TunedDecayComb comb;
        comb.SetParams(1.0 / 100.01, 1.0, 0.0, rate);
        double peak = 0.0;
        double maxStep = 0.0;
        double previous = 0.0;
        for (int i = 0; i < static_cast<int>(rate * 2.0); ++i)
        {
            if (i % 32 == 0)
            {
                double pitch = i < rate ? 1.0 / (100.0 + 0.2 * std::sin(i * 0.001)) :
                    (i % 128 == 0 ? 0.0000001 : 0.9);
                comb.SetParams(pitch, i % 512 < 256 ? 1.0 : 0.0,
                    0.5 + 0.5 * std::sin(i * 0.003), rate);
            }

            double y = comb.Process(static_cast<float>(0.01 * std::sin(i * 0.071)));
            DOCTEST_REQUIRE(std::isfinite(y));
            peak = std::max(peak, std::abs(y));
            maxStep = std::max(maxStep, std::abs(y - previous));
            previous = y;
        }

        std::cout << "Comb modulation: rate=" << rate << " peak=" << peak << " max step=" << maxStep << '\n';
        DOCTEST_CHECK(peak < 1.0);
        DOCTEST_CHECK(maxStep < 0.1);
    }
}

// Every impulse sample must scale by the same radial time factor, even the
// nonfundamental transient. This catches unscaled interpolation and SVF memory.
//
DOCTEST_TEST_CASE("TunedDecayComb: exponential knob law scales every impulse mode")
{
    for (double rate : {48000.0, 192000.0})
    {
        for (double sign : {-1.0, 1.0})
        {
            for (double amount : {0.01, 0.1, 0.5, 0.8})
            {
                for (double damp : {0.0, 0.6, 1.0})
                {
                    TunedDecayComb longComb;
                    TunedDecayComb shortComb;
                    longComb.SetParams(5013.7 / rate, 0.5 + 0.5 * sign, damp, rate);
                    shortComb.SetParams(5013.7 / rate, 0.5 + 0.5 * sign * amount, damp, rate);
                    double t60 = 0.001 * std::pow(5000.0, amount);
                    for (int i = 0; i < static_cast<int>(0.05 * rate); ++i)
                    {
                        float input = i == 0 ? 1.0f : 0.0f;
                        double reference = longComb.Process(input);
                        double actual = shortComb.Process(input);
                        double expected = reference * std::exp(-std::log(1000.0) * i / rate * (1.0 / t60 - 1.0 / 5.0));
                        DOCTEST_CHECK(std::abs(actual - expected) < 1e-7);
                    }
                }
            }
        }
    }
}

DOCTEST_TEST_CASE("TunedDecayComb: pitch clamps to representable bounds")
{
    for (double knob : {0.0, 1.0})
    {
        for (bool lower : {false, true})
        {
            TunedDecayComb actual;
            TunedDecayComb reference;
            double bound = lower ? (knob < 0.5 ? 0.5 : 1.0) / 16383.0 : 0.25;
            actual.SetParams(lower ? -1.0 : 10.0, knob, 0.8, 192000.0);
            reference.SetParams(bound, knob, 0.8, 192000.0);
            for (int i = 0; i < 40000; ++i)
            {
                float input = i == 0 ? 1.0f : 0.0f;
                DOCTEST_CHECK(actual.Process(input) == reference.Process(input));
            }
        }
    }
}

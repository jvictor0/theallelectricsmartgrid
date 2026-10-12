#pragma once

#include "TransferFunction.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>

// H0(z) = L(z) / (1 - s z^-N A(z) L(z)).
// L is a two-pole low-pass with a unity peak at the played fundamental.
// Its variable Q tends to Butterworth as DAMP opens; excitation is filtered too.
// Integer delay and allpass A compensate L's phase at the played fundamental.
// Applying H0(z/r) to every memory sets an upper bound on all modal lifetimes.
//
struct TunedDecayComb
{
    static_assert(std::atomic<double>::is_always_lock_free, "Audio/UI snapshots require lock-free doubles");

    static constexpr size_t x_maxDelaySamples = 1 << 14;
    static constexpr double x_pi = 3.14159265358979323846;
    static constexpr double x_sqrt2 = 1.41421356237309504880;

    struct Coefficients
    {
        size_t m_delay = 100;
        double m_a = 0.0;
        double m_b = 1.0;
        double m_g = 0.01;
        double m_k = x_sqrt2;
        double m_gain = 1.0;
        double m_h = 1.0;
        double m_r = 0.9;
        double m_delayGain = 0.0;
        bool m_bypass = true;
        double m_sign = 0.0;
    };

    struct Branch
    {
        Coefficients m_coeff;
        double m_allpass = 0.0;
        double m_ic1 = 0.0;
        double m_ic2 = 0.0;

        double Process(double input, double delayed)
        {
            const auto& c = m_coeff;
            double x = delayed * c.m_delayGain;
            double memory = c.m_r * m_allpass;
            double allpass = c.m_a * x + c.m_b * memory;
            m_allpass = c.m_b * x - c.m_a * memory;
            double excitation = input + c.m_sign * allpass;
            if (c.m_bypass)
            {
                // Clear dormant integrators so bypass cannot accumulate energy
                // that resurfaces when a later branch re-enables the low-pass.
                //
                m_ic1 = 0.0;
                m_ic2 = 0.0;
                return excitation;
            }

            double ic1 = c.m_r * m_ic1;
            double ic2 = c.m_r * m_ic2;
            double v1 = c.m_h * (ic1 + c.m_g * (excitation - ic2));
            double v2 = ic2 + c.m_g * v1;
            m_ic1 = 2.0 * v1 - ic1;
            m_ic2 = 2.0 * v2 - ic2;
            return c.m_gain * v2;
        }
    };

    struct AtomicCoefficients
    {
        std::atomic<size_t> m_delay{100};
        std::atomic<double> m_a{0.0};
        std::atomic<double> m_g{0.01};
        std::atomic<double> m_k{x_sqrt2};
        std::atomic<double> m_gain{1.0};
        std::atomic<double> m_r{0.9};
        std::atomic<double> m_delayGain{0.0};
        std::atomic<bool> m_bypass{true};
        std::atomic<double> m_sign{0.0};

        void Store(const Coefficients& c)
        {
            m_delay.store(c.m_delay);
            m_a.store(c.m_a);
            m_g.store(c.m_g);
            m_k.store(c.m_k);
            m_gain.store(c.m_gain);
            m_r.store(c.m_r);
            m_delayGain.store(c.m_delayGain);
            m_bypass.store(c.m_bypass);
            m_sign.store(c.m_sign);
        }

        std::complex<double> Numerator(double omega) const
        {
            if (m_bypass.load())
            {
                return 1.0;
            }

            auto q = m_r.load() * std::polar(1.0, -omega);
            double g = m_g.load();
            double k = m_k.load();
            double gain = m_gain.load();
            return gain * g * g * (1.0 + q) * (1.0 + q) /
                ((1.0 + k * g + g * g) + 2.0 * (g * g - 1.0) * q +
                 (1.0 - k * g + g * g) * q * q);
        }

        std::complex<double> Loop(double omega) const
        {
            auto z = std::polar(1.0, -omega);
            auto q = m_r.load() * z;
            double a = m_a.load();
            auto allpass = (a + q) / (1.0 + a * q);
            return m_sign.load() * m_delayGain.load() *
                std::polar(1.0, -omega * m_delay.load()) * allpass * Numerator(omega);
        }
    };

    struct UIState : TransferFunction
    {
        AtomicCoefficients m_branch[2];
        std::atomic<double> m_mix{0.0};

        std::complex<float> TransferFunctionValue(float normalizedFreq) const override
        {
            double omega = 2.0 * x_pi * std::clamp(static_cast<double>(normalizedFreq), 0.0, 0.5);
            double mix = m_mix.load();
            auto loop = (1.0 - mix) * m_branch[0].Loop(omega) + mix * m_branch[1].Loop(omega);
            auto numerator = (1.0 - mix) * m_branch[0].Numerator(omega) + mix * m_branch[1].Numerator(omega);
            auto response = numerator / (1.0 - loop);
            return {static_cast<float>(response.real()), static_cast<float>(response.imag())};
        }

        float FrequencyResponse(float normalizedFreq) const override
        {
            return std::abs(TransferFunctionValue(normalizedFreq));
        }
    };

    std::array<double, x_maxDelaySamples> m_delay{};
    size_t m_index = 0;
    Branch m_branch[2];
    Coefficients m_target;
    double m_mix = 0.0;
    double m_step = 1.0;
    bool m_transition = false;
    bool m_pending = false;
    bool m_initialized = false;
    double m_lastFrequency = -1.0;
    double m_lastDecay = -1.0;
    double m_lastDamp = -1.0;
    double m_lastRate = -1.0;

    // Call once per microblock. Frequencies are normalized to the processing rate.
    // Reserve one sample in the allpass after compensating low-pass phase.
    // At the highest negative pitches delta may fall below 1; N stays >= 1.
    //
    void SetParams(double normalizedFrequency, double decayKnob, double dampKnob, double sampleRate)
    {
        sampleRate = std::max(1.0, sampleRate);
        decayKnob = std::clamp(decayKnob, 0.0, 1.0);
        dampKnob = std::clamp(dampKnob, 0.0, 1.0);
        double cycles = decayKnob < 0.5 ? 0.5 : 1.0;
        normalizedFrequency = std::clamp(normalizedFrequency,
            cycles / static_cast<double>(x_maxDelaySamples - 1), 0.25);
        if (normalizedFrequency == m_lastFrequency && decayKnob == m_lastDecay &&
            dampKnob == m_lastDamp && sampleRate == m_lastRate)
        {
            return;
        }

        m_lastFrequency = normalizedFrequency;
        m_lastDecay = decayKnob;
        m_lastDamp = dampKnob;
        m_lastRate = sampleRate;
        double duration = 0.001 * std::pow(5000.0, std::abs(2.0 * decayKnob - 1.0));
        double omega = 2.0 * x_pi * normalizedFrequency;
        Coefficients c;
        c.m_bypass = decayKnob == 0.5;
        double cutoff = std::min(0.49, normalizedFrequency * std::exp2(1.0 + 6.0 * dampKnob));
        double t = std::tan(x_pi * normalizedFrequency);
        double v = std::tan(x_pi * cutoff) / t;
        double shape = std::hypot(1.0, v * v - 1.0);
        double u2 = 1.0 / shape;
        c.m_g = t * std::sqrt(shape);
        c.m_k = std::sqrt(2.0 * (1.0 - u2));
        c.m_gain = std::sqrt((1.0 - u2) * (1.0 + u2));
        c.m_h = 1.0 / (1.0 + c.m_k * c.m_g + c.m_g * c.m_g);
        double filterPhase = c.m_bypass ? 0.0 : -std::atan2(c.m_k * std::sqrt(u2), 1.0 - u2);

        double delay = (2.0 * x_pi * cycles + filterPhase) / omega;
        c.m_delay = static_cast<size_t>(std::max(1.0, std::floor(delay) - 1.0));
        double delta = delay - c.m_delay;
        double td = std::tan(omega * delta * 0.5);
        c.m_a = (t - td) / (t + td);
        c.m_b = std::sqrt(std::max(0.0, 1.0 - c.m_a * c.m_a));
        c.m_r = std::exp(-std::log(1000.0) / (sampleRate * duration));
        c.m_delayGain = std::pow(c.m_r, static_cast<double>(c.m_delay));
        c.m_sign = decayKnob == 0.5 ? 0.0 : (decayKnob < 0.5 ? -1.0 : 1.0);
        m_target = c;
        m_step = 1.0 / std::max(1.0, 0.005 * sampleRate);
        if (!m_initialized)
        {
            m_branch[0].m_coeff = c;
            m_branch[1].m_coeff = c;
            m_initialized = true;
        }
        else
        {
            m_pending = true;
        }
    }

    float Process(float input)
    {
        if (!m_transition && m_pending)
        {
            m_branch[1] = m_branch[0];
            m_branch[1].m_coeff = m_target;
            m_mix = 0.0;
            m_transition = true;
            m_pending = false;
        }

        double output = m_branch[0].Process(input, m_delay[(m_index + x_maxDelaySamples -
            m_branch[0].m_coeff.m_delay) % x_maxDelaySamples]);
        if (m_transition)
        {
            double next = m_branch[1].Process(input, m_delay[(m_index + x_maxDelaySamples -
                m_branch[1].m_coeff.m_delay) % x_maxDelaySamples]);
            output += m_mix * (next - output);
            m_mix = std::min(1.0, m_mix + m_step);
            if (m_mix >= 1.0)
            {
                m_branch[0] = m_branch[1];
                m_mix = 0.0;
                m_transition = false;
            }
        }

        m_delay[m_index] = output;
        m_index = (m_index + 1) % x_maxDelaySamples;
        return static_cast<float>(output);
    }

    void Reset()
    {
        m_delay.fill(0.0);
        m_index = 0;
        for (auto& branch : m_branch)
        {
            branch = Branch{};
            branch.m_coeff = m_target;
        }

        m_mix = 0.0;
        m_transition = false;
        m_pending = false;
    }

    void PopulateUIState(UIState* state) const
    {
        state->m_branch[0].Store(m_branch[0].m_coeff);
        state->m_branch[1].Store(m_branch[1].m_coeff);
        state->m_mix.store(m_mix);
    }
};

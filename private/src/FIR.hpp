#pragma once

#include "Math.hpp"
#include "Slew.hpp"
#include "TransferFunction.hpp"
#include <algorithm>
#include <atomic>
#include <complex>
#include <cstring>

// FIR filter with windowed sinc design
// Supports LP, HP, BP with blend and asymmetry controls
// Taps are slewed for smooth parameter modulation
//
template<size_t Size>
struct FIR
{
    static_assert(Size >= 3, "FIR size must be at least 3");

    static constexpr size_t x_oversample = 4;

    // Target taps (designed from params) and slewed taps (used in processing)
    //
    float m_targetTaps[Size];
    float m_slewedTaps[Size];
    OPLowPassFilter m_tapSlew[Size];

    float m_delayLine[Size];
    size_t m_writeIndex;

    float m_cutoff;
    float m_blend;
    float m_asymmetry;

    struct UIState : TransferFunction
    {
        std::atomic<float> m_taps[Size];

        UIState()
        {
            for (size_t i = 0; i < Size; ++i)
            {
                m_taps[i].store(0.0f);
            }
        }

        std::complex<float> TransferFunctionValue(float normalizedFreq) const override
        {
            float freq = std::max(0.0f, std::min(0.5f, normalizedFreq));
            float omega = 2.0f * static_cast<float>(M_PI) * freq;
            std::complex<float> response(0.0f, 0.0f);

            for (size_t n = 0; n < Size; ++n)
            {
                float phase = -omega * static_cast<float>(n);
                std::complex<float> expTerm(Math::Cos(phase), Math::Sin(phase));
                response += m_taps[n].load() * expTerm;
            }

            return response;
        }

        float FrequencyResponse(float normalizedFreq) const override
        {
            return std::abs(TransferFunctionValue(normalizedFreq));
        }
    };

    FIR()
        : m_targetTaps{}
        , m_slewedTaps{}
        , m_delayLine{}
        , m_writeIndex(0)
        , m_cutoff(0.25f)
        , m_blend(0.0f)
        , m_asymmetry(0.5f)
    {
        // Initialize slew filters for oversampled rate
        //
        for (size_t i = 0; i < Size; ++i)
        {
            m_tapSlew[i].SetAlphaFromNatFreq((1000.0f / 48000.0f) / x_oversample);
            m_tapSlew[i].m_output = 0.0f;
        }

        DesignFilter();

        // Initialize slewed taps to target
        //
        for (size_t i = 0; i < Size; ++i)
        {
            m_slewedTaps[i] = m_targetTaps[i];
            m_tapSlew[i].m_output = m_targetTaps[i];
        }
    }

    // Set parameters and redesign target taps (call once per block)
    // cutoff: normalized frequency (0 to 0.5)
    // blend: 0 = LP, 0.5 = BP, 1 = HP
    // asymmetry: 0.5 = symmetric, <0.5 = tilt toward low freq, >0.5 = tilt toward high freq
    //
    void SetParams(float cutoff, float blend, float asymmetry)
    {
        m_cutoff = std::max(0.001f, std::min(0.499f, cutoff));
        m_blend = std::max(0.0f, std::min(1.0f, blend));
        m_asymmetry = std::max(0.0f, std::min(1.0f, asymmetry));
        DesignFilter();
    }

    void DesignFilter()
    {
        // Design LP first (used by both HP and BP)
        //
        float lpTaps[Size];
        DesignLowPass(lpTaps, m_cutoff);

        // Design HP from LP
        //
        float hpTaps[Size];
        DesignHighPassFromLP(hpTaps, lpTaps);

        // Design BP from LP
        //
        float bpTaps[Size];
        DesignBandPassFromLP(bpTaps, lpTaps, m_cutoff);

        // Blend: 0-0.5 LP->BP, 0.5-1 BP->HP
        //
        if (m_blend <= 0.5f)
        {
            float t = m_blend * 2.0f;
            for (size_t i = 0; i < Size; ++i)
            {
                m_targetTaps[i] = lpTaps[i] * (1.0f - t) + bpTaps[i] * t;
            }
        }
        else
        {
            float t = (m_blend - 0.5f) * 2.0f;
            for (size_t i = 0; i < Size; ++i)
            {
                m_targetTaps[i] = bpTaps[i] * (1.0f - t) + hpTaps[i] * t;
            }
        }

        // Apply asymmetry (spectral tilt)
        //
        ApplyAsymmetry();
    }

    void DesignLowPass(float* taps, float cutoff)
    {
        int center = static_cast<int>(Size) / 2;
        float sum = 0.0f;
        float twoPi = 2.0f * static_cast<float>(M_PI);

        for (size_t i = 0; i < Size; ++i)
        {
            int n = static_cast<int>(i) - center;

            // Sinc function
            //
            float sinc;
            if (n == 0)
            {
                sinc = 2.0f * cutoff;
            }
            else
            {
                float x = twoPi * cutoff * n;
                sinc = Math::Sin(x) / (static_cast<float>(M_PI) * n);
            }

            // Hann window
            //
            float window = 0.5f * (1.0f - Math::Cos(twoPi * i / (Size - 1)));

            taps[i] = sinc * window;
            sum += taps[i];
        }

        // Normalize for unity gain at DC
        //
        if (sum > 0.0f)
        {
            for (size_t i = 0; i < Size; ++i)
            {
                taps[i] /= sum;
            }
        }
    }

    void DesignHighPassFromLP(float* hpTaps, const float* lpTaps)
    {
        // Spectral inversion: subtract LP from (split) impulse
        // For even size, use split impulse at center-1 and center
        // For odd size, use single impulse at center
        //
        int center = static_cast<int>(Size) / 2;

        for (size_t i = 0; i < Size; ++i)
        {
            hpTaps[i] = -lpTaps[i];
        }

        if (Size % 2 == 0)
        {
            hpTaps[center - 1] += 0.5f;
            hpTaps[center] += 0.5f;
        }
        else
        {
            hpTaps[center] += 1.0f;
        }
    }

    void DesignBandPassFromLP(float* bpTaps, const float* lpTaps, float cutoff)
    {
        // Bandpass centered at cutoff
        // BP = LP_high - LP_low where cutoffs bracket the center frequency
        // We already have LP at cutoff; design LP at lower cutoff and subtract
        //
        float lpLow[Size];
        float lpHigh[Size];

        float lowCutoff = std::max(0.001f, cutoff * 0.5f);
        float highCutoff = std::min(0.499f, cutoff * 1.5f);

        DesignLowPass(lpLow, lowCutoff);
        DesignLowPass(lpHigh, highCutoff);

        // BP = LP_high - LP_low
        //
        for (size_t i = 0; i < Size; ++i)
        {
            bpTaps[i] = lpHigh[i] - lpLow[i];
        }

        // Normalize for unity gain at center
        //
        float sum = 0.0f;
        for (size_t i = 0; i < Size; ++i)
        {
            sum += std::fabs(bpTaps[i]);
        }

        if (sum > 0.0f)
        {
            for (size_t i = 0; i < Size; ++i)
            {
                bpTaps[i] /= sum;
            }
        }
    }

    void ApplyAsymmetry()
    {
        // Spectral tilt via exponential weighting
        // asymmetry = 0.5: no change
        // asymmetry < 0.5: emphasize early taps (low freq emphasis)
        // asymmetry > 0.5: emphasize late taps (high freq emphasis)
        //
        if (std::fabs(m_asymmetry - 0.5f) < 0.001f)
        {
            return;
        }

        // Map asymmetry to decay factor
        // 0 -> decay = -4 (heavy early emphasis)
        // 0.5 -> decay = 0 (no change)
        // 1 -> decay = +4 (heavy late emphasis)
        //
        float decay = (m_asymmetry - 0.5f) * 8.0f;

        float sum = 0.0f;
        for (size_t i = 0; i < Size; ++i)
        {
            float t = static_cast<float>(i) / static_cast<float>(Size - 1);
            float weight = std::exp(decay * (t - 0.5f));
            m_targetTaps[i] *= weight;
            sum += std::fabs(m_targetTaps[i]);
        }

        // Renormalize
        //
        if (sum > 0.0f)
        {
            for (size_t i = 0; i < Size; ++i)
            {
                m_targetTaps[i] /= sum;
            }
        }
    }

    // Process slew on taps and convolve (call every sample)
    //
    float Process(float input)
    {
        // Slew taps toward target
        //
        for (size_t i = 0; i < Size; ++i)
        {
            m_slewedTaps[i] = m_tapSlew[i].Process(m_targetTaps[i]);
        }

        // Write to delay line
        //
        m_delayLine[m_writeIndex] = input;

        // Convolve with slewed taps
        //
        float output = 0.0f;
        size_t readIndex = m_writeIndex;

        for (size_t i = 0; i < Size; ++i)
        {
            output += m_delayLine[readIndex] * m_slewedTaps[i];

            if (readIndex == 0)
            {
                readIndex = Size - 1;
            }
            else
            {
                --readIndex;
            }
        }

        // Advance write index
        //
        ++m_writeIndex;
        if (m_writeIndex >= Size)
        {
            m_writeIndex = 0;
        }

        return output;
    }

    void Reset()
    {
        std::memset(m_delayLine, 0, Size * sizeof(float));
        m_writeIndex = 0;

        // Reset slews to current target
        //
        for (size_t i = 0; i < Size; ++i)
        {
            m_slewedTaps[i] = m_targetTaps[i];
            m_tapSlew[i].m_output = m_targetTaps[i];
        }
    }

    // Compute group delay at a given normalized frequency.
    // Group delay = -d(phase)/d(omega)
    // Uses finite difference approximation.
    // Returns delay in samples.
    //
    float ComputeGroupDelay(float normalizedFreq) const
    {
        float freq = std::max(0.001f, std::min(0.498f, normalizedFreq));
        float delta = 0.001f;
        float freqLow = std::max(0.0f, freq - delta);
        float freqHigh = std::min(0.5f, freq + delta);

        // Compute phase at freq - delta and freq + delta
        //
        std::complex<float> hLow(0.0f, 0.0f);
        std::complex<float> hHigh(0.0f, 0.0f);
        float omegaLow = 2.0f * static_cast<float>(M_PI) * freqLow;
        float omegaHigh = 2.0f * static_cast<float>(M_PI) * freqHigh;

        for (size_t n = 0; n < Size; ++n)
        {
            float phaseLow = -omegaLow * static_cast<float>(n);
            float phaseHigh = -omegaHigh * static_cast<float>(n);
            hLow += m_slewedTaps[n] * std::complex<float>(Math::Cos(phaseLow), Math::Sin(phaseLow));
            hHigh += m_slewedTaps[n] * std::complex<float>(Math::Cos(phaseHigh), Math::Sin(phaseHigh));
        }

        float phaseLow = std::arg(hLow);
        float phaseHigh = std::arg(hHigh);

        // Unwrap phase difference
        //
        float phaseDiff = phaseHigh - phaseLow;
        while (phaseDiff > static_cast<float>(M_PI))
        {
            phaseDiff -= 2.0f * static_cast<float>(M_PI);
        }

        while (phaseDiff < -static_cast<float>(M_PI))
        {
            phaseDiff += 2.0f * static_cast<float>(M_PI);
        }

        float omegaDiff = omegaHigh - omegaLow;

        // Group delay = -d(phase)/d(omega)
        //
        float groupDelay = -phaseDiff / omegaDiff;
        return groupDelay;
    }

    // Compute phase at a given normalized frequency.
    // Returns phase in radians.
    //
    float ComputePhase(float normalizedFreq) const
    {
        float freq = std::max(0.0f, std::min(0.5f, normalizedFreq));
        float omega = 2.0f * static_cast<float>(M_PI) * freq;
        std::complex<float> response(0.0f, 0.0f);

        for (size_t n = 0; n < Size; ++n)
        {
            float phase = -omega * static_cast<float>(n);
            std::complex<float> expTerm(Math::Cos(phase), Math::Sin(phase));
            response += m_slewedTaps[n] * expTerm;
        }

        return std::arg(response);
    }

    void PopulateUIState(UIState* uiState) const
    {
        for (size_t i = 0; i < Size; ++i)
        {
            uiState->m_taps[i].store(m_slewedTaps[i]);
        }
    }
};

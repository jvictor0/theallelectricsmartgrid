#pragma once

#include "Filter.hpp"
#include "Math.hpp"
#include "MultiChannelNumber.hpp"
#include "SampleTimer.hpp"
#include "Saturator.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

// One engine sample per lane. Gain is positive and input is in [-1, 1];
// Shape scans seven equally spaced landmarks in [0, 1]. DC state survives
// parameter changes. The blocker can leave a decaying tail after silence.
//
template <size_t Size>
struct WavefolderGeneric
{
    using Vector = MultiChannelNumber<float, Size>;
    using Saturator = TanhSaturatorGeneric<float, Size, false>;

    static constexpr size_t x_numShapes = 7;
    static constexpr float x_dcCutoffHz = 5.0f;
    static constexpr float x_foldPeakInput = 0.5773502691896258f;

    // First positive maxima of the three mixed folds, using the existing
    // rational tanh and sine. Later maxima are 2, 1, 2 at 3*pi/2, pi, 3*pi/2.
    //
    static constexpr float x_mixedPeakInputs[3] = {0.6203883040f, 0.5975659569f, 0.5782429879f};
    static constexpr float x_mixedPeaks[3] = {0.3768772696f, 0.3697895228f, 0.3634337041f};

    Vector m_gain;
    Vector m_normalization[x_numShapes];
    OPHighPassFilterGeneric<float, Size> m_dcBlocker;
    Vector m_output;

    WavefolderGeneric()
        : m_gain(Vector::Splat(0))
        , m_output(Vector::Splat(0))
    {
        m_dcBlocker.SetAlphaFromNatFreq(SampleTimer::HzToCycPerSamp(x_dcCutoffHz));
        SetGain(Vector::Splat(1));
    }

    void SetGain(const Vector& gain)
    {
        if (m_gain == gain)
        {
            return;
        }

        for (size_t i = 0; i < Size; ++i)
        {
            assert(std::isfinite(gain[i]) && gain[i] > 0);
        }

        m_gain = gain;
        Vector tanhPeak = Saturator::Tanh(gain);
        Vector sineAtGain = Math::Sin2pi(gain * static_cast<float>(1.0 / (2.0 * M_PI)));
        Vector sinePeak = Vector::IfLess(gain, Vector::Splat(M_PI / 2), sineAtGain, Vector::Splat(1));
        Vector tanhFoldPeak = Vector::Min(tanhPeak, Vector::Splat(x_foldPeakInput));
        Vector sineFoldPeak = Vector::Min(sinePeak, Vector::Splat(x_foldPeakInput));
        const Vector x_one = Vector::Splat(1);
        m_normalization[0] = x_one / tanhPeak;
        m_normalization[6] = x_one / sinePeak;

        // A fold's full-scale endpoint can be zero. Normalize its reachable
        // interior peak instead, without tracking the instantaneous envelope.
        //
        m_normalization[1] = x_one / (tanhFoldPeak * (x_one - tanhFoldPeak * tanhFoldPeak));
        m_normalization[5] = x_one / (sineFoldPeak * (x_one - sineFoldPeak * sineFoldPeak));
        const Vector x_mixedEndpoints[3] =
        {
            tanhPeak * (x_one - tanhPeak * sineAtGain),
            tanhPeak * (x_one - sineAtGain * sineAtGain),
            tanhPeak - sineAtGain * sineAtGain * sineAtGain
        };
        for (size_t mixed = 0; mixed < 3; ++mixed)
        {
            const float finalPeakInput = static_cast<float>(mixed == 1 ? M_PI : 1.5 * M_PI);
            for (size_t lane = 0; lane < Size; ++lane)
            {
                float peak = std::abs(x_mixedEndpoints[mixed][lane]);
                if (gain[lane] >= x_mixedPeakInputs[mixed])
                {
                    peak = std::max(peak, x_mixedPeaks[mixed]);
                }

                if (gain[lane] >= finalPeakInput)
                {
                    peak = mixed == 1 ? 1.0f : 2.0f;
                }

                m_normalization[mixed + 2][lane] = 1.0f / peak;
            }
        }
    }

    Vector Process(const Vector& input, const Vector& shape)
    {
        Vector driven = input * m_gain;
        Vector t = Saturator::Tanh(driven);
        Vector s = Math::Sin2pi(driven * static_cast<float>(1.0 / (2.0 * M_PI)));

        for (size_t i = 0; i < Size; ++i)
        {
            assert(std::isfinite(input[i]) && std::abs(input[i]) <= 1.0f);
            assert(std::isfinite(shape[i]) && shape[i] >= 0 && shape[i] <= 1);
        }

        Vector tt = t * t;
        Vector ss = s * s;

        const Vector x_shapes[x_numShapes] =
        {
            t * m_normalization[0],
            t * (Vector::Splat(1) - tt) * m_normalization[1],
            (t - tt * s) * m_normalization[2],
            t * (Vector::Splat(1) - ss) * m_normalization[3],
            (t - ss * s) * m_normalization[4],
            s * (Vector::Splat(1) - ss) * m_normalization[5],
            s * m_normalization[6]
        };
        Vector shaped;
        for (size_t i = 0; i < Size; ++i)
        {
            float position = shape[i] * static_cast<float>(x_numShapes - 1);
            size_t first = std::min(static_cast<size_t>(position), x_numShapes - 2);
            float fraction = position - static_cast<float>(first);
            float left = x_shapes[first][i];
            shaped[i] = left + fraction * (x_shapes[first + 1][i] - left);
        }

        m_output = m_dcBlocker.Process(shaped);
        return m_output;
    }
};

using Wavefolder = WavefolderGeneric<1>;
using TrioWavefolder = WavefolderGeneric<3>;

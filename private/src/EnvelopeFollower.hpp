#pragma once

#include "ScopeWriter.hpp"
#include "SampleTimer.hpp"
#include <cmath>

// Slew power at engine rate, then hold its RMS envelope for each host frame.
//
template<size_t Size>
struct EnvelopeFollowerGeneric
{
    using Vector = MultiChannelNumber<float, Size>;
    static constexpr float x_defaultTimeConstantSeconds = 0.01f;
    float m_alpha;
    Vector m_audio;
    Vector m_meanSquare;
    Vector m_envelope;
    ScopeWriterHolderGeneric<Size> m_audioScopeWriter;
    ScopeWriterHolderGeneric<Size> m_envelopeScopeWriter;

    EnvelopeFollowerGeneric()
        : m_audio(Vector::Splat(0))
        , m_meanSquare(Vector::Splat(0))
        , m_envelope(Vector::Splat(0))
    {
        SetTimeConstantSeconds(x_defaultTimeConstantSeconds);
    }

    void SetTimeConstantSeconds(float seconds)
    {
        assert(std::isfinite(seconds) && seconds > 0.0f);
        m_alpha = static_cast<float>(-std::expm1(-SampleTimer::x_samplePeriod / seconds));
    }

    void Process(const Vector& audio)
    {
        m_audio = audio;
        m_meanSquare = Vector::FusedMultiplyAdd(Vector::Splat(m_alpha), audio * audio - m_meanSquare, m_meanSquare);
        if (SampleTimer::IsHostFrame())
        {
            m_envelope = m_meanSquare.Sqrt();
        }

        m_audioScopeWriter.Write(m_audio);
        m_envelopeScopeWriter.Write(m_envelope);
    }
};

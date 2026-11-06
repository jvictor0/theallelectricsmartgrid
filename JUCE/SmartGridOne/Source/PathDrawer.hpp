#pragma once

#include <JuceHeader.h>
#include "ScopeWriter.hpp"
#include "TransferFunction.hpp"

struct PathDrawer
{
    static constexpr size_t x_numPoints = 1024;
    static constexpr float x_minFrequency = 1.0f / (2.0f * WindowedFFT::Transform::x_tableSize);
    static constexpr float x_maxFrequency = 0.5f;

    float m_height;
    float m_width;
    float m_xMin;
    float m_yMin;

    bool m_logX;
    bool m_scaleY;

    float m_bucketExpX[x_numPoints];

    PathDrawer()
        : m_height(0)
        , m_width(0)
        , m_xMin(0)
        , m_yMin(0)
        , m_logX(true)
        , m_scaleY(false)
    {
        ComputeBucketExpX();
    }

    PathDrawer(float height, float width, float xMin, float yMin)
        : m_height(height)
        , m_width(width)
        , m_xMin(xMin)
        , m_yMin(yMin)
        , m_logX(true)
        , m_scaleY(false)
    {
        ComputeBucketExpX();
    }

    void ComputeBucketExpX()
    {
        for (size_t i = 0; i < x_numPoints; ++i)
        {
            m_bucketExpX[i] = x_minFrequency * std::pow(x_maxFrequency / x_minFrequency,
                static_cast<float>(i) / (x_numPoints - 1));
        }
    }

    static float NormalizeDb(float x)
    {
        return std::max(0.0f, (x + 60) / 60);
    }

    static float AmpToDb(float x)
    {
        return 20 * std::log10(std::max(0.00001f, x));
    }

    static float AmpToDbNormalized(float x)
    {
        return NormalizeDb(AmpToDb(x));
    }

    static float LinearToLog(float x)
    {
        return std::log(x / x_minFrequency) / std::log(x_maxFrequency / x_minFrequency);
    }

    template<typename Fn>
    void DrawPath(juce::Graphics& g, juce::Colour colour, Fn fn)
    {
        juce::Path path;

        float yScale = 1;
        float yOffset = 0;
        if (m_scaleY)
        {
            float minY = std::numeric_limits<float>::max();
            float maxY = std::numeric_limits<float>::min();
            for (size_t j = 0; j < x_numPoints; ++j)
            {
                float xIn = m_logX ? m_bucketExpX[j] : static_cast<float>(j) / x_numPoints;
                float y = fn(xIn);
                minY = std::min(minY, y);
                maxY = std::max(maxY, y);
            }

            yScale = 1 / (maxY - minY);
            yOffset = -minY;
        }

        for (size_t j = 0; j < x_numPoints; ++j)
        {
            float xIn = m_logX ? m_bucketExpX[j] : static_cast<float>(j);
            float y = fn(xIn);
            y = yScale * y + yOffset;
            float screenY = m_height * (1 - y);
            float x = static_cast<float>(j) / static_cast<float>(x_numPoints - 1);
            float screenX = m_width * x;
            if (j == 0)
            {
                path.startNewSubPath(m_xMin + screenX, m_yMin + screenY);
            }
            else
            {
                path.lineTo(m_xMin + screenX, m_yMin + screenY);
            }
        }
    
        path.lineTo(m_xMin + m_width, m_yMin + m_height);
        g.setColour(colour);
        g.strokePath(path, juce::PathStrokeType(1.0f));
    }

    template<size_t Size>
    struct WindowedDFTFn
    {
        WindowedFFTGeneric<Size>* m_windowedFFT;
        size_t m_lane;
        float m_freqScale;
        float m_halfBucketRatio;
        float m_halfBucketWidth;

        WindowedDFTFn(WindowedFFTGeneric<Size>* windowedFFT, size_t lane, const PathDrawer& drawer)
            : m_windowedFFT(windowedFFT)
            , m_lane(lane)
            , m_freqScale(drawer.FrequencyAxisScale())
            , m_halfBucketRatio(drawer.m_logX ? LogHalfBucketRatio() : 1.0f)
            , m_halfBucketWidth(drawer.m_logX ? 0.0f : m_freqScale / 2.0f)
        {
        }

        float operator()(float x) const
        {
            float frequency = x * m_freqScale;
            return PathDrawer::NormalizeDb(m_windowedFFT->GetPeakMagDb(
                m_lane,
                frequency / m_halfBucketRatio - m_halfBucketWidth,
                frequency * m_halfBucketRatio + m_halfBucketWidth));
        }
    };

    void DrawWindowedDFT(juce::Graphics& g, juce::Colour colour, WindowedFFT* windowedFFT)
    {
        DrawWindowedDFT(g, colour, windowedFFT, 0);
    }

    template<size_t Size>
    void DrawWindowedDFT(juce::Graphics& g, juce::Colour colour, WindowedFFTGeneric<Size>* windowedFFT, size_t lane)
    {
        WindowedDFTFn<Size> fn(windowedFFT, lane, *this);
        DrawPath(g, colour, fn);
    }

    static float LogHalfBucketRatio()
    {
        return std::pow(x_maxFrequency / x_minFrequency, 0.5f / (x_numPoints - 1));
    }

    float FrequencyAxisScale() const
    {
        // DrawPath supplies frequencies on the log axis and point indices on the linear axis.
        // Convert linear indices to cycles per engine sample, ending at Nyquist.
        //
        return m_logX ? 1.0f : x_maxFrequency / (x_numPoints - 1);
    }

    struct FrequencyResponseFn
    {
        const TransferFunction* m_transferFunction;
        float m_freqScale;

        FrequencyResponseFn(const TransferFunction* transferFunction, float freqScale)
            : m_transferFunction(transferFunction)
            , m_freqScale(freqScale)
        {
        }

        float operator()(float freq) const
        {
            float response = m_transferFunction->FrequencyResponse(freq * m_freqScale);
            return PathDrawer::AmpToDbNormalized(response) / 2.0f;
        }
    };

    void DrawFrequencyResponse(
        juce::Graphics& g,
        juce::Colour colour,
        const TransferFunction& transferFunction,
        float freqScale)
    {
        FrequencyResponseFn fn(&transferFunction, freqScale * FrequencyAxisScale());
        DrawPath(g, colour, fn);
    }

    struct ScopeReaderFn
    {
        ScopeReader* m_scopeReader;
        float m_yScale;
        float m_yOffset;

        ScopeReaderFn(ScopeReader* scopeReader, float yScale, float yOffset)
            : m_scopeReader(scopeReader)
            , m_yScale(yScale)
            , m_yOffset(yOffset)
        {
        }

        float operator()(double x) const
        {
            float y = m_scopeReader->Get(x);
            return y * m_yScale + m_yOffset;
        }
    };

    void DrawScopePath(juce::Graphics& g, juce::Colour colour, ScopeReader& scopeReader, float yScale, float yOffset)
    {
        juce::Path path;
        ScopeReaderFn fn(&scopeReader, yScale, yOffset);
        double transferSample = scopeReader.m_transferXSample;
        size_t numPoints = scopeReader.m_numXSamples;

        for (size_t j = 0; j < numPoints; ++j)
        {
            double sample = static_cast<double>(j);
            float y = fn(sample);
            float screenY = m_height * (1.0f - y);
            float screenX = m_width * static_cast<float>(j) / static_cast<float>(numPoints);
            if (j == 0 || (sample - 1.0 < transferSample && transferSample <= sample))
            {
                path.startNewSubPath(m_xMin + screenX, m_yMin + screenY);
            }
            else
            {
                path.lineTo(m_xMin + screenX, m_yMin + screenY);
            }
        }

        g.setColour(colour);
        g.strokePath(path, juce::PathStrokeType(1.0f));
    }

    void DrawScopeMarker(
        juce::Graphics& g,
        juce::Colour colour,
        ScopeReader& scopeReader,
        float yScale,
        float yOffset)
    {
        double transferSample = std::max(0.0, scopeReader.m_transferXSample - 1.0);
        double lastSample = static_cast<double>(scopeReader.m_numXSamples - 1);
        double clampedSample = std::min(transferSample, lastSample);
        double xFrac = clampedSample / lastSample;
        float screenX = m_xMin + m_width * xFrac;
        float y = ScopeReaderFn(&scopeReader, yScale, yOffset)(clampedSample);
        float screenY = m_yMin + m_height * (1.0f - y);
        float markerRadius = 3.0f;
        g.setColour(colour);
        g.fillEllipse(screenX - markerRadius, screenY - markerRadius, markerRadius * 2.0f, markerRadius * 2.0f);
    }
};

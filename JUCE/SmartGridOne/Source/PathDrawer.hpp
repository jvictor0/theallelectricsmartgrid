#pragma once

#include <JuceHeader.h>

struct PathDrawer
{
    static constexpr size_t x_numPoints = 1024;

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
        float m = static_cast<float>(x_numPoints);
        for (size_t i = 0; i < x_numPoints; ++i)
        {
            m_bucketExpX[i] = std::pow(m - 1, static_cast<float>(i) / m) / (2 * m);
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
        float m = static_cast<float>(x_numPoints);
        return std::log(x * 2.0f * m) / std::log(m - 1.0f);
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
            float x = static_cast<float>(j) / static_cast<float>(x_numPoints);
            float screenX = m_width * x;
            g.setColour(colour);
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

    struct WindowedDFTFn
    {
        WindowedFFT* m_windowedFFT;

        WindowedDFTFn(WindowedFFT* windowedFFT)
            : m_windowedFFT(windowedFFT)
        {
        }

        float operator()(float x) const
        {
            return PathDrawer::NormalizeDb(m_windowedFFT->GetMagDb(x));
        }
    };

    void DrawWindowedDFT(juce::Graphics& g, juce::Colour colour, WindowedFFT* windowedFFT)
    {
        WindowedDFTFn fn(windowedFFT);
        DrawPath(g, colour, fn);
    }
};
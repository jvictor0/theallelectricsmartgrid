#pragma once

#include "SmartGridInclude.hpp"
#include <JuceHeader.h>

struct ScopeComponent : public juce::Component
{
    enum class ScopeType
    {
        Audio,
        Control,
    };

    static constexpr size_t x_voicesPerTrack = 3;
    ScopeReaderFactory m_scopeReaderFactory;
    static constexpr size_t x_numXSamples = 1024;
    std::atomic<size_t>* m_voiceIx;
    size_t m_scopeIx;
    ScopeWriter* m_scopeWriter;

    int* m_voiceOffset;
    ScopeType m_scopeType;

    ScopeComponent(std::atomic<size_t>* voiceIx, size_t scopeIx, ScopeWriter* scopeWriter, int* voiceOffset, ScopeType scopeType)
        : m_scopeReaderFactory(scopeWriter, voiceIx->load(), scopeIx, x_numXSamples, 1)
        , m_voiceIx(voiceIx)
        , m_scopeIx(scopeIx)
        , m_scopeWriter(scopeWriter)
        , m_voiceOffset(voiceOffset)
        , m_scopeType(scopeType)
    {
        setSize(400, 200);
    }

    void paint(juce::Graphics& g) override
    {
        // Fill background
        //
        g.fillAll(juce::Colours::black);
        
        // Get bounds for drawing
        //
        auto bounds = getLocalBounds().toFloat();
        auto width = bounds.getWidth();
        auto height = bounds.getHeight();
        

        int voiceOffset = *m_voiceOffset;
        int numLoops = voiceOffset == -1 ? x_voicesPerTrack : 1;

        size_t voiceIxFixed = m_voiceIx->load() * x_voicesPerTrack;
        
        for (int i = 0; i < numLoops; ++i)
        {
            size_t voiceIx = voiceIxFixed;

            if (voiceOffset != -1)
            {
                voiceIx += voiceOffset;
            }
            else
            {
                voiceIx += i;
            }

            m_scopeReaderFactory.SetVoiceIx(voiceIx);

            // Create scope reader
            //
            ScopeReader scopeReader = m_scopeReaderFactory.Create();
            
            // Create path for oscilloscope curve
            //
            juce::Path scopePath;
            bool firstPoint = true;
            
            // Draw the oscilloscope curve
            //
            for (int x = 0; x < x_numXSamples; ++x)
            {
                float y = scopeReader.Get(x);
                
                // Convert to screen coordinates
                // y is typically in range [-1, 1], convert to [0, height]
                //
                float screenX = bounds.getX() + (static_cast<float>(x) / static_cast<float>(x_numXSamples - 1)) * width;
                float screenY;
                if (m_scopeType == ScopeType::Audio)
                {
                    screenY = bounds.getY() + height * 0.5f - (y * height * 0.4f);
                }
                else
                {
                    screenY = bounds.getY() + height * (1.0 - y * 0.9f - 0.05f);
                }
                
                if (firstPoint)
                {
                    scopePath.startNewSubPath(screenX, screenY);
                    firstPoint = false;
                }
                else
                {
                    scopePath.lineTo(screenX, screenY);
                }
            }
            
            // Draw the curve
            //
            SmartGrid::Color color = TheNonagonSmartGrid::VoiceColor(voiceIx);
            g.setColour(juce::Colour(color.m_red, color.m_green, color.m_blue));
            g.strokePath(scopePath, juce::PathStrokeType(1.0f));
            
            // Draw center line
            //
            g.setColour(juce::Colours::darkgrey);
            g.drawLine(bounds.getX(), bounds.getY() + height * 0.5f, 
                    bounds.getX() + width, bounds.getY() + height * 0.5f, 1.0f);
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        ++(*m_voiceOffset);
        if (*m_voiceOffset == x_voicesPerTrack)
        {
            *m_voiceOffset = -1;
        }
    }
};

struct SoundStageComponent : public juce::Component
{
    static constexpr size_t x_scopeIx = 4;
    static constexpr size_t x_numVoices = 9;
    std::atomic<float>* m_xPos;
    std::atomic<float>* m_yPos;
    std::atomic<float>* m_volume;
    
    SoundStageComponent(std::atomic<float>* xPos, std::atomic<float>* yPos, std::atomic<float>* volume)
        : m_xPos(xPos)
        , m_yPos(yPos)
        , m_volume(volume)
    {
        setSize(400, 200);
    }

    void paint(juce::Graphics& g) override
    {
        // Fill background
        //
        g.fillAll(juce::Colours::black);
        
        // Get bounds for drawing
        //
        auto bounds = getLocalBounds().toFloat();
        auto width = bounds.getWidth();
        auto height = bounds.getHeight();
        auto border = 0.1;

        for (size_t i = 0; i < x_numVoices; ++i)
        {
            auto centerX = width * m_xPos[i].load() * (1 - border) + border * width / 2;
            auto centerY = height * (1 - m_yPos[i].load()) * (1 - border) + border * height / 2;

            float radius = (0.01 + 0.3 * m_volume[i].load()) * std::min(width, height);

            SmartGrid::Color color = TheNonagonSmartGrid::VoiceColor(i);
            g.setColour(juce::Colour(color.m_red, color.m_green, color.m_blue));
            g.fillEllipse(centerX - radius, centerY - radius, radius * 2, radius * 2);
        }
    }
};

struct MelodyRollComponent : public juce::Component
{
    NonagonNoteWriter* m_nonagonNoteWriter;

    MelodyRollComponent(NonagonNoteWriter* nonagonNoteWriter)
        : m_nonagonNoteWriter(nonagonNoteWriter)
    {
        setSize(400, 200);
    }

    void paint(juce::Graphics& g) override
    {
        // Fill background
        //
        g.fillAll(juce::Colours::black);

        // Get bounds for drawing
        //
        auto bounds = getLocalBounds().toFloat();
        auto width = bounds.getWidth();
        auto height = bounds.getHeight();

        float maxPitch;
        float minPitch;
        float curPosition;

        ssize_t startIndex;
        ssize_t endIndex;
    
        while (true)
        {
            startIndex = m_nonagonNoteWriter->GetLastStartIndex();

            maxPitch = m_nonagonNoteWriter->GetMaxPitch();
            minPitch = m_nonagonNoteWriter->GetMinPitch();
            curPosition = m_nonagonNoteWriter->GetCurPosition();
            endIndex = m_nonagonNoteWriter->GetIndex();

            if (startIndex == m_nonagonNoteWriter->GetLastStartIndex())
            {
                break;
            }
        }

        for (ssize_t ix = endIndex - 1; ix >= 0; --ix)
        {
            if (endIndex - ix > NonagonNoteWriter::x_numEvents)
            {
                break;
            }

            NonagonNoteWriter::EventData eventData = m_nonagonNoteWriter->Get(ix);
            if (ix < startIndex && eventData.m_startPosition <= curPosition)
            {
                break;
            }

            float endPosition = eventData.m_endPosition;
            if (endPosition == -1)
            {
                endPosition = curPosition;
            }

            float pitch = eventData.m_voltPerOct;
            float position = eventData.m_startPosition;

            float screenY = maxPitch == minPitch ? height * 0.5f : height * (1 - (pitch - minPitch) / (maxPitch - minPitch));
            float border = 0.05;
            screenY = screenY * (1 - border) + height * border / 2;

            float screenXStart = width * position;
            float screenXEnd = width * endPosition;

            SmartGrid::Color color = TheNonagonSmartGrid::VoiceColor(eventData.m_voiceIx);
            g.setColour(juce::Colour(color.m_red, color.m_green, color.m_blue));
            g.drawLine(screenXStart, screenY, screenXEnd, screenY, 1.0f);
        }
    }
};

struct AnalyserComponent : public juce::Component
{
    static constexpr size_t x_voicesPerTrack = 3;
    WindowedFFT m_windowedFFT[x_voicesPerTrack];
    std::atomic<size_t>* m_voiceIx;

    int* m_voiceOffset;

    AnalyserComponent(WindowedFFT windowedFFT, std::atomic<size_t>* voiceIx, int* voiceOffset)
        : m_voiceIx(voiceIx)
        , m_voiceOffset(voiceOffset)
    {
        for (size_t i = 0; i < x_voicesPerTrack; ++i)
        {
            m_windowedFFT[i] = windowedFFT;
        }

        setSize(400, 200);
    }

    void paint(juce::Graphics& g) override
    {
        // Fill background
        //
        g.fillAll(juce::Colours::black);

        auto bounds = getLocalBounds().toFloat();
        auto width = bounds.getWidth();
        auto height = bounds.getHeight();

        int voiceOffset = *m_voiceOffset;

        size_t baseVoiceIx = m_voiceIx->load() * x_voicesPerTrack;
        size_t numLoops = 1;
        if (voiceOffset == -1)
        {
            numLoops = x_voicesPerTrack;
        }

        for (size_t i = 0; i < numLoops; ++i)
        {
            size_t voiceIx = baseVoiceIx + voiceOffset;
            if (voiceOffset == -1)
            {
                voiceIx = baseVoiceIx + i;
            }

            m_windowedFFT[i].Compute(voiceIx);

            juce::Path path;
            path.startNewSubPath(0, height);
            SmartGrid::Color color = TheNonagonSmartGrid::VoiceColor(voiceIx);

            for (size_t j = 0; j < DiscreteFourierTransform::x_maxComponents; ++j)
            {
                float y = (m_windowedFFT[i].m_filters[j].m_output + 100) / 100;
                float screenY = height * (1 - y);
                float screenX = width * j / DiscreteFourierTransform::x_maxComponents;
                g.setColour(juce::Colour(color.m_red, color.m_green, color.m_blue));
                path.lineTo(screenX, screenY);
            }
        
            path.lineTo(width, height);
            g.setColour(juce::Colour(color.m_red, color.m_green, color.m_blue));
            g.strokePath(path, juce::PathStrokeType(1.0f));
        }
    }
};
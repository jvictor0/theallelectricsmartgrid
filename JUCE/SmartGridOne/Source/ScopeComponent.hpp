#pragma once

#include "SmartGridInclude.hpp"
#include <JuceHeader.h>
#include "PathDrawer.hpp"
#include "SmartGridOneMainVisualizerComponent.hpp"

struct ScopeComponent : public SmartGridOneMainVisualizerComponent
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
    TheNonagonSquiggleBoyInternal::UIState* m_uiState;

    int* m_voiceOffset;
    ScopeType m_scopeType;

    ScopeComponent(
        size_t scopeIx,
        ScopeWriter* scopeWriter,
        int* voiceOffset,
        ScopeType scopeType,
        TheNonagonSquiggleBoyInternal::UIState* uiState)
        : SmartGridOneMainVisualizerComponent()
        , m_scopeReaderFactory(scopeWriter, uiState->m_squiggleBoyUIState.m_activeTrack.load(), scopeIx, x_numXSamples, 1)
        , m_voiceIx(&uiState->m_squiggleBoyUIState.m_activeTrack)
        , m_scopeIx(scopeIx)
        , m_scopeWriter(scopeWriter)
        , m_uiState(uiState)
        , m_voiceOffset(voiceOffset)
        , m_scopeType(scopeType)
    {
    }

    void Draw(juce::Graphics& g, juce::Rectangle<int> boundsRect) override
    {
        // Fill background
        //
        g.fillAll(juce::Colours::black);
        
        // Get bounds for drawing
        //
        auto bounds = boundsRect.toFloat();
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
                if (m_uiState->m_nonagonUIState.m_muted[voiceIx].load())
                {
                    continue;
                }
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

            if (m_scopeType == ScopeType::Control)
            {
                float markerX = bounds.getX() + (static_cast<float>(scopeReader.m_transferXSample) / static_cast<float>(x_numXSamples - 1)) * width;
                float y = scopeReader.Get(scopeReader.m_transferXSample);
                float markerY = bounds.getY() + height * (1.0 - y * 0.9f - 0.05f);
                g.setColour(juce::Colour(color.m_red, color.m_green, color.m_blue));
                float markerRadius = 3.0f;
                g.fillEllipse(markerX - markerRadius, markerY - markerRadius, markerRadius * 2, markerRadius * 2);
            }
            // Draw center line
            //
            g.setColour(juce::Colours::darkgrey);
            g.drawLine(bounds.getX(), bounds.getY() + height * 0.5f, 
                    bounds.getX() + width, bounds.getY() + height * 0.5f, 1.0f);
        }
    }

    void OnClick(const juce::MouseEvent& event) override
    {
        ++(*m_voiceOffset);
        if (*m_voiceOffset == static_cast<int>(x_voicesPerTrack))
        {
            *m_voiceOffset = -1;
        }
    }
};

struct SoundStageComponent : public SmartGridOneMainVisualizerComponent
{
    static constexpr size_t x_scopeIx = 4;
    static constexpr size_t x_numVoices = 9;

    TheNonagonSquiggleBoyInternal::UIState* m_uiState;
    
    SoundStageComponent(TheNonagonSquiggleBoyInternal::UIState* uiState)
        : SmartGridOneMainVisualizerComponent()
        , m_uiState(uiState)
    {
    }

    void Draw(juce::Graphics& g, juce::Rectangle<int> boundsRect) override
    {
        // Fill background
        //
        g.fillAll(juce::Colours::black);
        
        // Get bounds for drawing
        //
        auto bounds = boundsRect.toFloat();
        auto width = bounds.getWidth();
        auto height = bounds.getHeight();
        auto border = 0.15;

        for (size_t i = 0; i < x_numVoices; ++i)
        {
            float xPos = m_uiState->m_squiggleBoyUIState.m_xPos[i].load();
            float yPos = m_uiState->m_squiggleBoyUIState.m_yPos[i].load();
            float volume = m_uiState->m_squiggleBoyUIState.m_voiceMeterReader[i].GetRMSLinear();
            auto centerX = width * xPos * (1 - border) + border * width / 2;
            auto centerY = height * (1 - yPos) * (1 - border) + border * height / 2;

            float radius = (0.01 + 0.5 * volume) * std::min(width, height);

            SmartGrid::Color color = TheNonagonSmartGrid::VoiceColor(i);
            g.setColour(juce::Colour(color.m_red, color.m_green, color.m_blue));
            g.fillEllipse(centerX - radius, centerY - radius, radius * 2, radius * 2);
        }
    }
};

struct MelodyRollComponent : public SmartGridOneMainVisualizerComponent
{
    NonagonNoteWriter* m_nonagonNoteWriter;

    MelodyRollComponent(NonagonNoteWriter* nonagonNoteWriter)
        : SmartGridOneMainVisualizerComponent()
        , m_nonagonNoteWriter(nonagonNoteWriter)
    {
    }

    void Draw(juce::Graphics& g, juce::Rectangle<int> boundsRect) override
    {
        // Fill background
        //
        g.fillAll(juce::Colours::black);

        // Get bounds for drawing
        //
        auto bounds = boundsRect.toFloat();
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

struct AnalyserComponent : public SmartGridOneMainVisualizerComponent
{
    static constexpr size_t x_voicesPerTrack = 3;
    WindowedFFT m_windowedFFT[x_voicesPerTrack];
    std::atomic<size_t>* m_voiceIx;

    int* m_voiceOffset;

    TheNonagonSquiggleBoyInternal::UIState* m_uiState;
    SquiggleBoyWithEncoderBank::UIState::VoiceFilterUIState* m_voiceFilterUIState;

    bool m_logX;

    float m_bucketExpX[DiscreteFourierTransform::x_maxComponents];

    AnalyserComponent(
        WindowedFFT windowedFFT, 
        int* voiceOffset, 
        TheNonagonSquiggleBoyInternal::UIState* uiState)
        : SmartGridOneMainVisualizerComponent()
        , m_voiceIx(&uiState->m_squiggleBoyUIState.m_activeTrack)
        , m_voiceOffset(voiceOffset)
        , m_uiState(uiState)
        , m_voiceFilterUIState(uiState->m_squiggleBoyUIState.m_voiceFilterUIState)
        , m_logX(true)
    {
        for (size_t i = 0; i < x_voicesPerTrack; ++i)
        {
            m_windowedFFT[i] = windowedFFT;
        }

        float m = static_cast<float>(DiscreteFourierTransform::x_maxComponents);
        for (size_t i = 0; i < DiscreteFourierTransform::x_maxComponents; ++i)
        {
            m_bucketExpX[i] = std::pow(m - 1, static_cast<float>(i) / m) / (2 * m);
        }
    }

    void Draw(juce::Graphics& g, juce::Rectangle<int> boundsRect) override
    {
        // Fill background
        //
        g.fillAll(juce::Colours::black);

        auto bounds = boundsRect.toFloat();
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
                if (m_uiState->m_nonagonUIState.m_muted[voiceIx].load())
                {
                    continue;
                }
            }

            m_windowedFFT[i].Compute(voiceIx);

            juce::Path path;
            path.startNewSubPath(0, height);
            SmartGrid::Color color = TheNonagonSmartGrid::VoiceColor(voiceIx);

            for (size_t j = 1; j < DiscreteFourierTransform::x_maxComponents; ++j)
            {
                float freq = m_logX ? m_bucketExpX[j] : static_cast<float>(j) / (2 * DiscreteFourierTransform::x_maxComponents);
                float y = (m_windowedFFT[i].GetMagDb(freq) + 60) / 60;
                float screenY = height * (1 - y);
                float x = static_cast<float>(j) / static_cast<float>(DiscreteFourierTransform::x_maxComponents);
                float screenX = width * x;
                g.setColour(juce::Colour(color.m_red, color.m_green, color.m_blue));
                path.lineTo(screenX, screenY);
            }
        
            path.lineTo(width, height);
            g.setColour(juce::Colour(color.m_red, color.m_green, color.m_blue));
            g.strokePath(path, juce::PathStrokeType(1.0f));

            color = color.Interpolate(SmartGrid::Color::White, 0.5f);
            PathDrawer responseDrawer(height, width, 0, 0);
            responseDrawer.m_logX = m_logX;
            responseDrawer.DrawFrequencyResponse(
                g,
                juce::Colour(color.m_red, color.m_green, color.m_blue),
                m_voiceFilterUIState[voiceIx],
                1.0f / SquiggleBoyVoice::x_oversample);
        }
    }

    void OnClick(const juce::MouseEvent& event) override
    {
        ++(*m_voiceOffset);
        if (*m_voiceOffset == static_cast<int>(x_voicesPerTrack))
        {
            *m_voiceOffset = -1;
        }
    }
};

struct QuadAnalyserComponent : public SmartGridOneMainVisualizerComponent
{
    enum class Type
    {
        Delay,
        Reverb,
        Master
    };

    QuadWindowedFFT m_quadWindowedFFT[3];
    TheNonagonSquiggleBoyInternal::UIState* m_uiState;
    bool m_drawAll;

    Type m_type;

    QuadAnalyserComponent(TheNonagonSquiggleBoyInternal::UIState* uiState, Type type)
        : SmartGridOneMainVisualizerComponent()
        , m_uiState(uiState)
        , m_drawAll(true)
        , m_type(type)
    {
        if (m_type == Type::Master)
        {
            m_quadWindowedFFT[0] = QuadWindowedFFT(&uiState->m_squiggleBoyUIState.m_quadScopeWriter, static_cast<size_t>(SmartGridOne::QuadScopes::Master));
        }
        else
        {
            m_quadWindowedFFT[0] = QuadWindowedFFT(&uiState->m_squiggleBoyUIState.m_quadScopeWriter, static_cast<size_t>(SmartGridOne::QuadScopes::Delay));
            m_quadWindowedFFT[1] = QuadWindowedFFT(&uiState->m_squiggleBoyUIState.m_quadScopeWriter, static_cast<size_t>(SmartGridOne::QuadScopes::Reverb));
            m_quadWindowedFFT[2] = QuadWindowedFFT(&uiState->m_squiggleBoyUIState.m_quadScopeWriter, static_cast<size_t>(SmartGridOne::QuadScopes::Dry));
        }
    }

    struct QuadDFTFn
    {
        QuadWindowedFFT* m_quadWindowedFFT;
        size_t m_speakerIx;

        QuadDFTFn(QuadWindowedFFT* quadWindowedFFT, size_t speakerIx)
            : m_quadWindowedFFT(quadWindowedFFT)
            , m_speakerIx(speakerIx)
        {
        }

        float operator()(float freq) const
        {
            return PathDrawer::NormalizeDb(m_quadWindowedFFT->GetMagDb(m_speakerIx, freq));
        }
    };

    const TransferFunction* GetTransferFunctionForSpeaker(Type type, size_t speakerIx) const
    {
        switch (type)
        {
            case Type::Delay:
                return &m_uiState->m_squiggleBoyUIState.m_delayUIState.m_dampingFilter[speakerIx];
            case Type::Reverb:
                return &m_uiState->m_squiggleBoyUIState.m_reverbUIState.m_dampingFilter[speakerIx];
            case Type::Master:
                return nullptr;
        }

        return nullptr;
    }

    void Draw(juce::Graphics& g, juce::Rectangle<int> boundsRect) override
    {
        g.fillAll(juce::Colours::black);

        if (m_drawAll && m_type != Type::Master)
        {
            DrawOne(g, 2, Type::Master, SmartGrid::Color::Yellow, boundsRect);

            if (m_type == Type::Delay)
            {
                DrawOne(g, 1, Type::Reverb, SmartGrid::Color::Fuscia, boundsRect);
                DrawOne(g, 0, Type::Delay, SmartGrid::Color::Pink, boundsRect);
            }
            else if (m_type == Type::Reverb)
            {
                DrawOne(g, 0, Type::Delay, SmartGrid::Color::Pink, boundsRect);
                DrawOne(g, 1, Type::Reverb, SmartGrid::Color::Fuscia, boundsRect);
            }
        }
        else if (m_type == Type::Master)
        {
            DrawOne(g, 0, Type::Master, SmartGrid::Color::Yellow, boundsRect);
        }
        else if (m_type == Type::Delay)
        {
            DrawOne(g, 0, Type::Delay, SmartGrid::Color::Pink, boundsRect);
        }
        else if (m_type == Type::Reverb)
        {
            DrawOne(g, 1, Type::Reverb, SmartGrid::Color::Fuscia, boundsRect);
        }
    }

    void DrawOne(juce::Graphics& g, size_t fftIx, Type type, SmartGrid::Color color, juce::Rectangle<int> boundsRect)
    {
        auto bounds = boundsRect.toFloat();
        auto width = bounds.getWidth();
        auto height = bounds.getHeight();

        m_quadWindowedFFT[fftIx].Compute();

        for (size_t x = 0; x < 2; ++x)
        {
            for (size_t y = 0; y < 2; ++y)
            {
                size_t speakerIx = y == 0 ? 3 - x : x;
                PathDrawer pathDrawer(height / 2, width / 2, width * x / 2, height * (1 - y) / 2);
                QuadDFTFn fn(&m_quadWindowedFFT[fftIx], speakerIx);
                juce::Colour juceColor(color.m_red, color.m_green, color.m_blue);
                pathDrawer.DrawPath(g, juceColor, fn);

                if (type != Type::Master)
                {
                    const TransferFunction* transferFunction = GetTransferFunctionForSpeaker(type, speakerIx);
                    if (transferFunction)
                    {
                        pathDrawer.DrawFrequencyResponse(g, juceColor, *transferFunction, 1.0f);
                    }
                }
            }
        }
    }

    void OnClick(const juce::MouseEvent& event) override
    {
        m_drawAll = !m_drawAll;
    }
};

struct TheoryOfTimeScopeComponent : public SmartGridOneMainVisualizerComponent
{
    TheNonagonSquiggleBoyInternal::UIState* m_uiState;

    TheoryOfTimeScopeComponent(TheNonagonSquiggleBoyInternal::UIState* uiState)
        : SmartGridOneMainVisualizerComponent()
        , m_uiState(uiState)
    {
    }
    
    struct DrawFn
    {
        ScopeReader m_scopeReader;
        float m_mod;

        DrawFn(ScopeReaderFactory* scopeReaderFactory, float mod)
            : m_scopeReader(scopeReaderFactory->Create())
            , m_mod(mod)
        {
        }

        float operator()(float x) 
        {
            float y = m_scopeReader.Get(x);
            return std::fmod(y, m_mod) / m_mod;
        }
    };

    void Draw(juce::Graphics& g, juce::Rectangle<int> boundsRect) override
    {
        g.fillAll(juce::Colours::black);

        auto bounds = boundsRect.toFloat();
        auto width = bounds.getWidth();
        auto height = bounds.getHeight();

        ScopeReaderFactory scopeReaderFactory(
            &m_uiState->m_squiggleBoyUIState.m_monoScopeWriter, 
            0, 
            static_cast<size_t>(SmartGridOne::MonoScopes::TheoryOfTime), 
            PathDrawer::x_numPoints, 
            1);
        DrawFn drawFn(&scopeReaderFactory, 1.0 / m_uiState->m_nonagonUIState.m_theoryOfTimeUIState.m_timeYModAmount.load());
        PathDrawer pathDrawer(height, width, 0, 0);
        pathDrawer.m_logX = false;

        size_t trioIx = m_uiState->m_squiggleBoyUIState.m_activeTrack.load();
        int yMod = m_uiState->m_nonagonUIState.m_theoryOfTimeUIState.GetTimeYModAmount();
        int loopMultiplier = m_uiState->m_nonagonUIState.GetLoopMultiplier(trioIx);
        float drawPos = drawFn.m_scopeReader.Get(drawFn.m_scopeReader.m_transferXSample);
        int offset = floor(drawPos * yMod);

        for (int i = offset * loopMultiplier / yMod; i <= (offset + 1) * loopMultiplier / yMod; ++i)
        {
            float y = std::fmod(static_cast<float>(i) / loopMultiplier, 1.0 / yMod) * yMod;
            float yScreen = bounds.getY() + height * (1.0 - y);
            SmartGrid::Color color = TheNonagonSmartGrid::VoiceColor(trioIx);
            g.setColour(juce::Colour(color.m_red, color.m_green, color.m_blue));
            g.drawLine(bounds.getX(), yScreen, bounds.getX() + width, yScreen, 1.0f);
        }

        pathDrawer.DrawPath(g, juce::Colours::white, drawFn);        

        float markerX = bounds.getX() + (static_cast<float>(drawFn.m_scopeReader.m_transferXSample) / static_cast<float>(PathDrawer::x_numPoints - 1)) * width;
        float y = drawFn(drawFn.m_scopeReader.m_transferXSample);
        float markerY = bounds.getY() + height * (1.0 - y);
        g.setColour(juce::Colours::white);
        float markerRadius = 3.0f;
        g.fillEllipse(markerX - markerRadius, markerY - markerRadius, markerRadius * 2, markerRadius * 2);
    }
};

// Physical modeling frequency response visualizer
// Draws the combined response for pre-comb SVF and comb+one-pole damping comb.
//
struct PhysicalModelingFrequencyResponseComponent : public SmartGridOneMainVisualizerComponent
{
    static constexpr size_t x_voicesPerTrack = 3;

    TheNonagonSquiggleBoyInternal::UIState* m_uiState;
    std::atomic<size_t>* m_voiceIx;
    int* m_voiceOffset;
    bool m_logX;

    PhysicalModelingFrequencyResponseComponent(
        TheNonagonSquiggleBoyInternal::UIState* uiState,
        int* voiceOffset)
        : SmartGridOneMainVisualizerComponent()
        , m_uiState(uiState)
        , m_voiceIx(&uiState->m_squiggleBoyUIState.m_activeTrack)
        , m_voiceOffset(voiceOffset)
        , m_logX(true)
    {
    }

    void Draw(juce::Graphics& g, juce::Rectangle<int> boundsRect) override
    {
        g.fillAll(juce::Colours::black);

        auto bounds = boundsRect.toFloat();
        auto width = bounds.getWidth();
        auto height = bounds.getHeight();

        int voiceOffset = *m_voiceOffset;
        size_t baseVoiceIx = m_voiceIx->load() * x_voicesPerTrack;
        size_t numLoops = voiceOffset == -1 ? x_voicesPerTrack : 1;

        for (size_t loopIx = 0; loopIx < numLoops; ++loopIx)
        {
            size_t voiceIx = baseVoiceIx + (voiceOffset == -1 ? loopIx : static_cast<size_t>(voiceOffset));
            if (voiceOffset == -1 && m_uiState->m_nonagonUIState.m_muted[voiceIx].load())
            {
                continue;
            }

            auto& sourceUIState = m_uiState->m_squiggleBoyUIState.m_voiceSourceUIState[voiceIx];
            if (sourceUIState.m_sourceMachine.load() != SquiggleBoyVoice::VoiceConfig::SourceMachine::PhysicalModeling)
            {
                continue;
            }

            SmartGrid::Color color = TheNonagonSmartGrid::VoiceColor(voiceIx);
            PathDrawer responseDrawer(height, width, 0, 0);
            responseDrawer.m_logX = m_logX;
            responseDrawer.DrawFrequencyResponse(
                g,
                juce::Colour(color.m_red, color.m_green, color.m_blue),
                sourceUIState.m_physicalModelingUIState,
                1.0f / PhysicalModelingSource::x_oversample);
        }
    }

    void OnClick(const juce::MouseEvent& event) override
    {
        (void)event;
        if (event.mods.isShiftDown())
        {
            m_logX = !m_logX;
        }
        else
        {
            ++(*m_voiceOffset);
            if (*m_voiceOffset == static_cast<int>(x_voicesPerTrack))
            {
                *m_voiceOffset = -1;
            }
        }
    }
};
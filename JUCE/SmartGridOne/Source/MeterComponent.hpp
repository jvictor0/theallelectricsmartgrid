#pragma once

#include "SmartGridInclude.hpp"
#include <JuceHeader.h>
#include "VUBarDrawer.hpp"

struct MeterDrawer
{
    float m_height;
    float m_width;
    float m_xMin;
    float m_yMin;

    void DrawRadialVUMeter(juce::Graphics& g, float rmsdBFS, float peakdBFS, juce::Colour needleColor)
    {
        // Calculate the center and radius for the arc
        //
        float centerX = m_xMin + m_width / 2.0f;
        float centerY = m_yMin + m_height * 0.9f;
        float radius = std::min(m_width / std::sqrt(2.0f), m_height * 0.8f) * 0.9f;
        
        // Define the arc bounds (top 90 degrees, rotated 45 degrees clockwise)
        // Start at -45 degrees, end at +45 degrees (90 degree span)
        //
        float startAngle = juce::MathConstants<float>::pi * 0.75; 
        float endAngle = juce::MathConstants<float>::pi * 0.25f; 
        float totalAngle = endAngle - startAngle;
        
        // Draw arc segments with different colors using Path
        //
        juce::Path arcPath;
        
        // White segment: -30dB to -12dB (18dB range)
        //
        float whiteStartAngle = startAngle + juce::MathConstants<float>::pi;
        float whiteEndAngle = whiteStartAngle - (18.0f / 36.0f) * totalAngle;
        arcPath.clear();
        arcPath.addCentredArc(centerX, centerY, radius, radius, 0, whiteEndAngle, whiteStartAngle, true);
        g.setColour(juce::Colours::white);
        g.strokePath(arcPath, juce::PathStrokeType(2.0f));
        
        // Yellow segment: -12dB to 0dB (12dB range)
        //
        float yellowStartAngle = whiteEndAngle;
        float yellowEndAngle = yellowStartAngle - (12.0f / 36.0f) * totalAngle;
        arcPath.clear();
        arcPath.addCentredArc(centerX, centerY, radius, radius, 0, yellowEndAngle, yellowStartAngle, true);
        g.setColour(juce::Colours::yellow);
        g.strokePath(arcPath, juce::PathStrokeType(2.0f));
        
        // Red segment: 0dB to +6dB (6dB range)
        //
        float redStartAngle = yellowEndAngle;
        float redEndAngle = redStartAngle - (6.0f / 36.0f) * totalAngle;
        arcPath.clear();
        arcPath.addCentredArc(centerX, centerY, radius, radius, 0, redEndAngle, redStartAngle, true);
        g.setColour(juce::Colours::red);
        g.strokePath(arcPath, juce::PathStrokeType(2.0f));
        
        // Draw tick marks every 6dB
        //
        g.setColour(juce::Colours::white);
        for (int db = -30; db <= 6; db += 6)
        {
            if (-12 <= db && db < 0)
            {
                g.setColour(juce::Colours::yellow);
            }
            else if (0 <= db)
            {
                g.setColour(juce::Colours::red);
            }
            else
            {
                g.setColour(juce::Colours::white);
            }

            float normalizedDb = (db + 30.0f) / 36.0f; // Normalize to 0-1 range
            float angle = startAngle + normalizedDb * totalAngle;
            
            // Calculate tick mark positions
            //
            float innerRadius = radius * 0.8f;
            float outerRadius = radius * 0.9f;
            
            float x1 = centerX + std::cos(angle) * innerRadius;
            float y1 = centerY - std::sin(angle) * innerRadius;
            float x2 = centerX + std::cos(angle) * outerRadius;
            float y2 = centerY - std::sin(angle) * outerRadius;
            
            g.drawLine(x1, y1, x2, y2, 1.0f);
        }
        
        // Draw the needle
        //
        float normalizedDb = (rmsdBFS + 30.0f) / 36.0f; // Normalize to 0-1 range
        normalizedDb = juce::jlimit(0.0f, 1.0f, normalizedDb); // Clamp to valid range
        float needleAngle = startAngle + normalizedDb * totalAngle;
        
        float needleLength = radius * 0.7f;
        float needleX = centerX + std::cos(needleAngle) * needleLength;
        float needleY = centerY - std::sin(needleAngle) * needleLength;
        
        g.setColour(needleColor);
        g.drawLine(centerX, centerY, needleX, needleY, 2.0f);

        float normalizedPeakDb = (peakdBFS + 30.0f) / 36.0f;
        normalizedPeakDb = juce::jlimit(0.0f, 1.0f, normalizedPeakDb);
        float peakNeedleAngle = startAngle + normalizedPeakDb * totalAngle;
      
        float peakNeedleTip = radius * 0.9f;
        float peakNeedleTipX = centerX + std::cos(peakNeedleAngle) * peakNeedleTip;
        float peakNeedleTipY = centerY - std::sin(peakNeedleAngle) * peakNeedleTip;

        float peakNeedleTail = radius * 0.8f;
        float peakNeedleTailX = centerX + std::cos(peakNeedleAngle) * peakNeedleTail;
        float peakNeedleTailY = centerY - std::sin(peakNeedleAngle) * peakNeedleTail;

        g.setColour(needleColor);
        g.drawLine(peakNeedleTailX, peakNeedleTailY, peakNeedleTipX, peakNeedleTipY, 2.0f);
    }
};

struct VoiceMeterComponent : public juce::Component
{
    TheNonagonSquiggleBoyInternal::UIState* m_uiState;
    MeterDrawer m_meterDrawer[TheNonagonInternal::x_numVoices];

    VoiceMeterComponent(TheNonagonSquiggleBoyInternal::UIState* uiState)
        : m_uiState(uiState)
    {
        setSize(400, 200);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);

        for (size_t i = 0; i < TheNonagonInternal::x_numVoices; ++i)
        {
            float rmsdBFS = m_uiState->m_squiggleBoyUIState.m_voiceMeterReader[i].GetRMSDbFS();
            float peakdBFS = m_uiState->m_squiggleBoyUIState.m_voiceMeterReader[i].GetPeakDbFS();
            SmartGrid::Color color = TheNonagonSmartGrid::VoiceColor(i);
            m_meterDrawer[i].m_height = getHeight() / TheNonagonInternal::x_voicesPerTrio;
            m_meterDrawer[i].m_width = getWidth() / TheNonagonInternal::x_voicesPerTrio;
            m_meterDrawer[i].m_xMin = (i % TheNonagonInternal::x_voicesPerTrio) * m_meterDrawer[i].m_width;
            m_meterDrawer[i].m_yMin = (i / TheNonagonInternal::x_voicesPerTrio) * m_meterDrawer[i].m_height;
            m_meterDrawer[i].DrawRadialVUMeter(g, rmsdBFS, peakdBFS, juce::Colour(color.m_red, color.m_green, color.m_blue));
        }
    }
};

struct MeteringComponent : public juce::Component
{
    TheNonagonSquiggleBoyInternal::UIState* m_uiState;

    // Total slots: 62 bars
    // Voice: 18 (9 level + 9 reduction)
    // Source: 8 (4 level + 4 reduction)
    // Quad Returns: 16 (2 sends x 4 channels x 2)
    // Stereo Master: 20 (5 groups x 2 channels x 2)
    //
    static constexpr float x_totalSlots = 62.0f;
    static constexpr float x_voiceStart = 0.0f;
    static constexpr float x_voiceSlots = 18.0f;
    static constexpr float x_sourceStart = 18.0f;
    static constexpr float x_sourceSlots = 8.0f;
    static constexpr float x_quadStart = 26.0f;
    static constexpr float x_quadSlots = 16.0f;
    static constexpr float x_stereoStart = 42.0f;
    static constexpr float x_stereoSlots = 20.0f;

    MeteringComponent(TheNonagonSquiggleBoyInternal::UIState* uiState)
        : m_uiState(uiState)
    {
        setSize(400, 50);
    }

    SquiggleBoyWithEncoderBank::UIState* GetSquiggleBoyUIState()
    {
        return &m_uiState->m_squiggleBoyUIState;
    }

    MultibandSaturator<4, 2>::UIState* GetMasteringChainUIState()
    {
        return &m_uiState->m_squiggleBoyUIState.m_stereoMasteringChainUIState;
    }

    SourceMixer::UIState* GetSourceMixerUIState()
    {
        return &m_uiState->m_squiggleBoyUIState.m_sourceMixerUIState;
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);

        VUBarDrawer vuBarDrawer(
            static_cast<float>(getHeight()),
            static_cast<float>(getWidth() * 0.98),
            0.01 * static_cast<float>(getWidth()),
            0.0f);

        DrawVoiceMeters(g, vuBarDrawer);
        DrawSourceMixerMeters(g, vuBarDrawer);
        DrawQuadReturnMeters(g, vuBarDrawer);
        DrawStereoMasterMeters(g, vuBarDrawer);
    }

    void DrawVoiceMeters(juce::Graphics& g, VUBarDrawer& vuBarDrawer)
    {
        // 9 voices, each with level + reduction = 18 bars
        //
        for (size_t i = 0; i < SquiggleBoy::x_numVoices; ++i)
        {
            MeterReader* meterReader = &GetSquiggleBoyUIState()->m_voiceMeterReader[i];
            float rms = meterReader->GetRMSDbFSNormalized();
            float reduction = meterReader->GetReductionDbFSNormalized();

            SmartGrid::Color color = TheNonagonSmartGrid::VoiceColor(i);
            juce::Colour jColor(color.m_red, color.m_green, color.m_blue);

            // Level bar
            //
            float levelX0 = (x_voiceStart + 2 * i) / x_totalSlots;
            float levelX1 = (x_voiceStart + 2 * i + 1) / x_totalSlots;
            vuBarDrawer.DrawBar(g, jColor, levelX0, levelX1, rms);

            // Reduction bar
            //
            float reductionX0 = (x_voiceStart + 2 * i + 1) / x_totalSlots;
            float reductionX1 = (x_voiceStart + 2 * i + 2) / x_totalSlots;
            vuBarDrawer.DrawReduction(g, juce::Colours::red, reductionX0, reductionX1, reduction);
        }
    }

    void DrawSourceMixerMeters(juce::Graphics& g, VUBarDrawer& vuBarDrawer)
    {
        // 4 sources (voice meters 9-12), each with level + reduction = 8 bars
        //
        for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
        {
            MeterReader* meterReader = &GetSquiggleBoyUIState()->m_voiceMeterReader[SquiggleBoy::x_numVoices + i];
            float rms = meterReader->GetRMSDbFSNormalized();
            float reduction = meterReader->GetReductionDbFSNormalized();

            SmartGrid::Color color = SourceMixer::UIState::Color(i);
            juce::Colour jColor(color.m_red, color.m_green, color.m_blue);

            // Level bar
            //
            float levelX0 = (x_sourceStart + 2 * i) / x_totalSlots;
            float levelX1 = (x_sourceStart + 2 * i + 1) / x_totalSlots;
            vuBarDrawer.DrawBar(g, jColor, levelX0, levelX1, rms);

            // Reduction bar
            //
            float reductionX0 = (x_sourceStart + 2 * i + 1) / x_totalSlots;
            float reductionX1 = (x_sourceStart + 2 * i + 2) / x_totalSlots;
            vuBarDrawer.DrawReduction(g, juce::Colours::red, reductionX0, reductionX1, reduction);
        }
    }

    void DrawQuadReturnMeters(juce::Graphics& g, VUBarDrawer& vuBarDrawer)
    {
        // 2 sends (delay/reverb), each with 4 channels + 4 reductions = 16 bars
        //
        for (size_t send = 0; send < QuadMixerInternal::x_numSends; ++send)
        {
            QuadMeterReader* meterReader = &GetSquiggleBoyUIState()->m_returnMeterReader[send];

            // First return pink, second fuschia
            //
            juce::Colour barColor = (send == 0) ? J(SmartGrid::Color::Pink) : J(SmartGrid::Color::Fuscia);
            for (size_t ch = 0; ch < 4; ++ch)
            {
                float rms = meterReader->GetRMSDbFSNormalized(ch);
                float reduction = meterReader->GetReductionDbFSNormalized(ch);

                // Each send takes 8 slots: 4 level + 4 reduction interleaved
                //
                size_t slotBase = send * 8 + ch * 2;

                // Level bar
                //
                float levelX0 = (x_quadStart + slotBase) / x_totalSlots;
                float levelX1 = (x_quadStart + slotBase + 1) / x_totalSlots;
                vuBarDrawer.DrawBar(g, barColor, levelX0, levelX1, rms);

                // Reduction bar
                //
                float reductionX0 = (x_quadStart + slotBase + 1) / x_totalSlots;
                float reductionX1 = (x_quadStart + slotBase + 2) / x_totalSlots;
                vuBarDrawer.DrawReduction(g, juce::Colours::red, reductionX0, reductionX1, reduction);
            }
        }
    }

    void DrawStereoMasterMeters(juce::Graphics& g, VUBarDrawer& vuBarDrawer)
    {
        // 5 stereo groups (4 bands + master), matching MultibandGainReductionComponent layout
        // Each group: 2 channels with level + reduction = 4 bars per group = 20 bars total
        //
        for (size_t i = 0; i < 5; ++i)
        {
            StereoMeterReader* meterReader = i < 4
                ? &GetMasteringChainUIState()->m_meterReader[i]
                : &GetMasteringChainUIState()->m_masterMeterReader;

            for (size_t j = 0; j < 2; ++j)
            {
                float rms = meterReader->GetRMSDbFSNormalized(j);
                float reduction = meterReader->GetReductionDbFSNormalized(j);

                // Layout: [reduction L] [level L] [level R] [reduction R] for each group
                // Matches MultibandGainReductionComponent pattern
                //
                size_t groupBase = i * 4;

                float levelX0 = (x_stereoStart + groupBase + 1 + j) / x_totalSlots;
                float levelX1 = (x_stereoStart + groupBase + 2 + j) / x_totalSlots;
                vuBarDrawer.DrawBar(g, juce::Colours::green, levelX0, levelX1, rms);

                float reductionX0 = (x_stereoStart + groupBase + 0 + 3 * j) / x_totalSlots;
                float reductionX1 = (x_stereoStart + groupBase + 1 + 3 * j) / x_totalSlots;
                vuBarDrawer.DrawReduction(g, juce::Colours::red, reductionX0, reductionX1, reduction);
            }
        }
    }
};
#pragma once

#include "SmartGridInclude.hpp"
#include <JuceHeader.h>
#include "BinaryData.h"
#include "FourteenSegmentDisplayComponent.hpp"
#include <algorithm>
#include <cmath>

// Static cache for modulator glyph images
//
struct ModulatorGlyphImages
{
    static constexpr size_t x_numGlyphs = 7;
    juce::Image m_images[x_numGlyphs];
    bool m_loaded;

    ModulatorGlyphImages()
        : m_loaded(false)
    {
    }

    void LoadImages()
    {
        if (m_loaded)
        {
            return;
        }

        m_images[static_cast<size_t>(SmartGridOne::ModulationGlyphs::LFO)] = 
            juce::ImageFileFormat::loadFrom(BinaryData::mod_badge_LFO_png, BinaryData::mod_badge_LFO_pngSize);
        m_images[static_cast<size_t>(SmartGridOne::ModulationGlyphs::ADSR)] = 
            juce::ImageFileFormat::loadFrom(BinaryData::mod_badge_ADSR_png, BinaryData::mod_badge_ADSR_pngSize);
        m_images[static_cast<size_t>(SmartGridOne::ModulationGlyphs::Sheaf)] = 
            juce::ImageFileFormat::loadFrom(BinaryData::mod_badge_Sheaf_png, BinaryData::mod_badge_Sheaf_pngSize);
        m_images[static_cast<size_t>(SmartGridOne::ModulationGlyphs::SmoothRandom)] = 
            juce::ImageFileFormat::loadFrom(BinaryData::mod_badge_SmoothRandom_png, BinaryData::mod_badge_SmoothRandom_pngSize);
        m_images[static_cast<size_t>(SmartGridOne::ModulationGlyphs::Noise)] = 
            juce::ImageFileFormat::loadFrom(BinaryData::mod_badge_Noise_png, BinaryData::mod_badge_Noise_pngSize);
        m_images[static_cast<size_t>(SmartGridOne::ModulationGlyphs::Quadrature)] = 
            juce::ImageFileFormat::loadFrom(BinaryData::mod_badge_Quadrature_png, BinaryData::mod_badge_Quadrature_pngSize);
        m_images[static_cast<size_t>(SmartGridOne::ModulationGlyphs::Spread)] = 
            juce::ImageFileFormat::loadFrom(BinaryData::mod_badge_Spread_png, BinaryData::mod_badge_Spread_pngSize);

        m_loaded = true;
    }

    juce::Image& GetImage(SmartGridOne::ModulationGlyphs glyph)
    {
        size_t index = static_cast<size_t>(glyph);
        if (index < x_numGlyphs)
        {
            return m_images[index];
        }

        // Return first image as fallback
        //
        return m_images[0];
    }

    bool HasImage(SmartGridOne::ModulationGlyphs glyph)
    {
        return glyph != SmartGridOne::ModulationGlyphs::None && 
               static_cast<size_t>(glyph) < x_numGlyphs;
    }
};

struct EncoderComponent : public juce::Component
{
    static constexpr int x_defaultPadSize = 100;
    static inline ModulatorGlyphImages s_glyphImages;

    EncoderBankUI m_ui;
    int m_x;
    int m_y;
    juce::Point<float> m_lastMousePosition;
    FourteenSegmentDisplayComponent m_segmentDisplay;

    EncoderComponent(EncoderBankUI uiState, int x, int y)
        : m_ui(uiState)
        , m_x(x)
        , m_y(y)
    {
        s_glyphImages.LoadImages();
        setSize(x_defaultPadSize, x_defaultPadSize);
        addAndMakeVisible(m_segmentDisplay);
        m_segmentDisplay.SetOffColor(juce::Colour(40, 40, 40));
        m_segmentDisplay.SetNumChars(4);
    }

    void SetSize(int size)
    {
        setSize(size, size);
    }

    static float ValueToArcAngle(float value)
    {
        return juce::MathConstants<float>::pi * 1.25f + value * juce::MathConstants<float>::pi * 1.5f;
    }

    static void DrawArc(
        juce::Graphics& g,
        float centerX,
        float centerY,
        float radius,
        float startValue,
        float endValue,
        float strokeWidth)
    {
        juce::Path arcPath;
        arcPath.addCentredArc(centerX, centerY, radius, radius, 0.0f, ValueToArcAngle(startValue), ValueToArcAngle(endValue), true);
        g.strokePath(arcPath, juce::PathStrokeType(strokeWidth));
    }

    static void GetSwitchValueRange(int switchVal, int switchValues, float& startValue, float& endValue)
    {
        if (switchValues <= 1)
        {
            startValue = 0.0f;
            endValue = 1.0f;
            return;
        }

        float denominator = static_cast<float>(switchValues - 1);
        startValue = switchVal == 0 ? 0.0f : (static_cast<float>(switchVal) - 0.5f) / denominator;
        endValue = switchVal == switchValues - 1 ? 1.0f : (static_cast<float>(switchVal) + 0.5f) / denominator;
    }

    static void DrawArcWithSwitchGaps(
        juce::Graphics& g,
        float centerX,
        float centerY,
        float radius,
        float startValue,
        float endValue,
        int switchValues,
        float strokeWidth)
    {
        if (switchValues <= 1)
        {
            DrawArc(g, centerX, centerY, radius, startValue, endValue, strokeWidth);
            return;
        }

        static constexpr float x_switchGapRadians = juce::MathConstants<float>::pi / 90.0f;
        for (int switchVal = 0; switchVal < switchValues; ++switchVal)
        {
            float switchStart;
            float switchEnd;
            GetSwitchValueRange(switchVal, switchValues, switchStart, switchEnd);

            float segmentStart = std::max(startValue, switchStart);
            float segmentEnd = std::min(endValue, switchEnd);
            if (segmentEnd <= segmentStart)
            {
                continue;
            }

            float startAngle = ValueToArcAngle(segmentStart);
            float endAngle = ValueToArcAngle(segmentEnd);
            if (switchVal > 0 && segmentStart <= switchStart)
            {
                startAngle += x_switchGapRadians;
            }

            if (switchVal < switchValues - 1 && segmentEnd >= switchEnd)
            {
                endAngle -= x_switchGapRadians;
            }

            if (endAngle <= startAngle)
            {
                continue;
            }

            juce::Path arcPath;
            arcPath.addCentredArc(centerX, centerY, radius, radius, 0.0f, startAngle, endAngle, true);
            g.strokePath(arcPath, juce::PathStrokeType(strokeWidth));
        }
    }

    static void DrawSwitchValueArc(
        juce::Graphics& g,
        float centerX,
        float centerY,
        float radius,
        float value,
        int switchValues,
        float strokeWidth)
    {
        if (switchValues <= 1)
        {
            return;
        }

        int switchVal = static_cast<int>(std::round(value * static_cast<float>(switchValues - 1)));
        switchVal = std::max(0, std::min(switchValues - 1, switchVal));

        float startValue;
        float endValue;
        GetSwitchValueRange(switchVal, switchValues, startValue, endValue);
        DrawArcWithSwitchGaps(g, centerX, centerY, radius, startValue, endValue, switchValues, strokeWidth);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto centerX = bounds.getCentreX();
        auto centerY = bounds.getCentreY();

        if (m_ui.m_uiState->GetConnected(m_x, m_y))
        {
            BitSet16 modulatorsAffecting = m_ui.m_uiState->GetModulatorsAffecting(m_x, m_y);
            BitSet16 gesturesAffecting = m_ui.m_uiState->GetGesturesAffecting(m_x, m_y);

            size_t numModulators = modulatorsAffecting.Count();
            size_t numGestures = gesturesAffecting.Count();
            float innerRadius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * (0.45f - m_ui.m_uiState->GetNumVoices() * 0.05f);

            // Draw modulator badges with glyph images
            //
            size_t badgeIndex = 0;
            for (size_t modIndex = 0; modIndex < 16 && badgeIndex < numModulators; ++modIndex)
            {
                if (!modulatorsAffecting.Get(modIndex))
                {
                    continue;
                }

                float badgeX, badgeY, badgeLength;
                GetBadgePosition(centerX, centerY, innerRadius, badgeIndex, numModulators, true, badgeX, badgeY, badgeLength);

                // Draw glyph image on top with 5% margins and rounded corners
                //
                SmartGridOne::ModulationGlyphs glyph = m_ui.m_uiState->GetModulationGlyph(modIndex);
                if (s_glyphImages.HasImage(glyph))
                {
                    // Draw badge background with gradient, outline, and highlight
                    //
                    SmartGrid::Color color = m_ui.m_uiState->GetModulationGlyphColor(modIndex);
                    DrawBadgeBackground(g, badgeX, badgeY, badgeLength, color);

                    // Create rectangle with 5% margin on each side (10% total reduction)
                    //
                    float margin = badgeLength * 0.05f;
                    juce::Rectangle<float> imageRect(
                        badgeX + margin,
                        badgeY + margin,
                        badgeLength - margin * 2.0f,
                        badgeLength - margin * 2.0f
                    );

                    juce::Image& img = s_glyphImages.GetImage(glyph);
                    g.drawImage(img, imageRect, juce::RectanglePlacement::centred);
                }

                ++badgeIndex;
            }

            // Draw gesture badges with symbols
            //
            size_t gestureBadgeIndex = 0;
            for (size_t gestureIndex = 0; gestureIndex < 16 && gestureBadgeIndex < numGestures; ++gestureIndex)
            {
                if (!gesturesAffecting.Get(gestureIndex))
                {
                    continue;
                }

                float badgeX, badgeY, badgeLength;
                GetBadgePosition(centerX, centerY, innerRadius, gestureBadgeIndex, numGestures, false, badgeX, badgeY, badgeLength);

                // Draw dark grey badge background
                //
                SmartGrid::Color color = GetGestureColor(gestureIndex);
                DrawBadgeBackground(g, badgeX, badgeY, badgeLength, color);

                // Draw gesture symbol as white text
                //
                juce::String symbol = GetGestureSymbol(gestureIndex);
                g.setColour(juce::Colours::white);
                g.setFont(badgeLength * 0.6f);
                g.drawFittedText(
                    symbol,
                    static_cast<int>(badgeX),
                    static_cast<int>(badgeY),
                    static_cast<int>(badgeLength),
                    static_cast<int>(badgeLength),
                    juce::Justification::centred,
                    1
                );

                ++gestureBadgeIndex;
            }

            for (size_t i = 0; i < m_ui.m_uiState->GetNumVoices(); ++i)
            {
                auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * (0.45f - i * 0.05f);
                int switchValues = m_ui.m_uiState->GetSwitchValues(m_x, m_y);

                // Draw the main 270-degree arc from 7:30 to 4:30
                //
                g.setColour(juce::Colours::white);
                DrawArcWithSwitchGaps(g, centerX, centerY, radius, 0.0f, 1.0f, switchValues, 2.0f);
            }

            // Draw the thicker, brighter arc between min and max values
            //
            for (size_t i = 0; i < m_ui.m_uiState->GetNumVoices(); ++i)
            {
                auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * (0.45f - i * 0.05f);
                size_t voice = m_ui.m_uiState->GetNumVoices() * m_ui.m_uiState->GetCurrentTrack() + i;

                // Skip if voice index would be out of bounds
                //
                if (voice >= 16)
                {
                    continue;
                }

                float minValue = m_ui.m_uiState->GetMinValue(m_x, m_y, voice);
                float maxValue = m_ui.m_uiState->GetMaxValue(m_x, m_y, voice);
                int switchValues = m_ui.m_uiState->GetSwitchValues(m_x, m_y);

                // Skip if values are invalid (NaN or infinite)
                //
                if (!std::isfinite(minValue) || !std::isfinite(maxValue))
                {
                    continue;
                }

                auto indicatorColor = m_ui.m_uiState->GetIndicatorColor(voice);
                g.setColour(juce::Colour(indicatorColor.m_red, indicatorColor.m_green, indicatorColor.m_blue));
                DrawArcWithSwitchGaps(g, centerX, centerY, radius, minValue, maxValue, switchValues, 3.0f);
            }

            for (size_t i = 0; i < m_ui.m_uiState->GetNumVoices(); ++i)
            {
                auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * (0.45f - i * 0.05f);

                // Calculate the indicator position
                //
                size_t voice = m_ui.m_uiState->GetNumVoices() * m_ui.m_uiState->GetCurrentTrack() + i;

                // Skip if voice index would be out of bounds
                //
                if (voice >= 16)
                {
                    continue;
                }

                float value = m_ui.m_uiState->GetValue(m_x, m_y, voice);

                // Skip if value is invalid
                //
                if (!std::isfinite(value))
                {
                    continue;
                }

                float angle = value * 270.0f; // Convert value to degrees (0-270)
                float indicatorAngle = juce::MathConstants<float>::pi * 0.75f + (angle * juce::MathConstants<float>::pi / 180.0f); // Start at 7:30 and go clockwise

                // Draw the small indicator circle
                //
                auto indicatorRadius = radius * 0.15f;
                auto indicatorX = centerX + radius * std::cos(indicatorAngle);
                auto indicatorY = centerY + radius * std::sin(indicatorAngle);

                auto indicatorColor = m_ui.m_uiState->GetIndicatorColor(voice);
                g.setColour(juce::Colour(indicatorColor.m_red, indicatorColor.m_green, indicatorColor.m_blue));
                DrawSwitchValueArc(
                    g,
                    centerX,
                    centerY,
                    radius,
                    value,
                    m_ui.m_uiState->GetSwitchValues(m_x, m_y),
                    3.0f);

                g.fillEllipse(indicatorX - indicatorRadius, indicatorY - indicatorRadius, 
                            indicatorRadius * 2, indicatorRadius * 2);
            }
        }

        // Update and position the 14-segment display in the bottom gap
        //
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.45f;
        auto displayHeight = static_cast<int>(radius * 0.3f);
        auto displayWidth = displayHeight * 3;
        auto displayX = static_cast<int>(centerX - displayWidth / 2.0f);
        auto displayY = static_cast<int>(centerY + radius * 0.6f);

        auto squareColor = m_ui.m_uiState->GetBrightnessAdjustedColor(m_x, m_y);
        if (false)
        {
            m_segmentDisplay.SetOnColor(J(squareColor));

            const char* shortName = m_ui.m_uiState->GetShortName(m_x, m_y);
            m_segmentDisplay.SetText(shortName ? juce::String(shortName) : "");
            m_segmentDisplay.setBounds(displayX, displayY, displayWidth, displayHeight);
        }
        else
        {
            auto rectSize = radius * 0.3f;
            auto squareX = centerX - 3 * rectSize / 2;
            auto squareY = centerY + radius * 0.6f;
            juce::Rectangle<float> rect(squareX, squareY, 3 * rectSize, rectSize);
            float cornerSize = rectSize * 0.7f;
            g.setColour(J(squareColor));
            g.fillRoundedRectangle(rect, cornerSize);
            g.setColour(juce::Colours::black);
            g.drawRoundedRectangle(rect, cornerSize, 1.5f);
        }
    }

    virtual void mouseDown(const juce::MouseEvent& event) override
    {
        m_lastMousePosition = event.position;
    }

    virtual void mouseDrag(const juce::MouseEvent& event) override
    {
        size_t timestamp = event.eventTime.getMilliseconds() * 1000;
        float sensitivity = 0.2;
        
        juce::Point<float> currentPosition = event.position;
        float deltaX = currentPosition.x - m_lastMousePosition.x;
        float deltaY = currentPosition.y - m_lastMousePosition.y;
        
        int change = static_cast<int>((deltaX - deltaY) * sensitivity);
        if (change != 0)
        {
            m_ui.m_messageBus->Push(SmartGrid::MessageIn(timestamp, m_ui.m_routeId, SmartGrid::MessageIn::Mode::EncoderIncDec, m_x, m_y, change));
            m_lastMousePosition = currentPosition;
        }
    }

    virtual void mouseDoubleClick(const juce::MouseEvent& event) override
    {
        size_t timestamp = event.eventTime.getMilliseconds() * 1000;
        m_ui.m_messageBus->Push(SmartGrid::MessageIn(timestamp, m_ui.m_routeId, SmartGrid::MessageIn::Mode::EncoderPush, m_x, m_y, 1));
    }

    void DrawBadgeBackground(juce::Graphics& g, float badgeX, float badgeY, float badgeLength, SmartGrid::Color color)
    {
        juce::Rectangle<float> badgeRect(badgeX, badgeY, badgeLength, badgeLength);
        float cornerSize = badgeLength * 0.08f;

        // Convert SmartGrid::Color to JUCE Colour
        //
        juce::Colour juceColor = juce::Colour(
            color.m_red,
            color.m_green,
            color.m_blue
        );

        // Draw the badge background with rounded corners
        //
        g.setColour(juce::Colours::darkgrey);
        g.fillRoundedRectangle(badgeRect, cornerSize);

        // Draw the LED effect - brighter color = brighter LED
        //
        if (color != SmartGrid::Color::Off)
        {
            // Create a radial gradient for LED effect
            //
            float centerX = badgeX + badgeLength * 0.5f;
            float centerY = badgeY + badgeLength * 0.5f;
            juce::ColourGradient gradient(
                juceColor.withAlpha(1.0f),
                centerX,
                centerY,
                juceColor.withAlpha(1.0f),
                centerX,
                centerY,
                true
            );

            g.setGradientFill(gradient);
            g.fillRoundedRectangle(badgeRect, cornerSize);
        }

        // Draw the badge border
        //
        g.setColour(juce::Colours::lightgrey);
        g.drawRoundedRectangle(badgeRect, cornerSize, 1.0f);
    }

    juce::String GetGestureSymbol(size_t gestureIndex)
    {
        if (gestureIndex < 8)
        {
            // Numbers 1-8
            //
            return juce::String(static_cast<int>(gestureIndex + 1));
        }
        else if (gestureIndex < 12)
        {
            // Single arrows: up, right, down, left
            //
            const juce::juce_wchar arrows[] = { 0x2191, 0x2192, 0x2193, 0x2190 };
            return juce::String::charToString(arrows[gestureIndex - 8]);
        }
        else if (gestureIndex < 16)
        {
            // Double arrows: up, right, down, left
            //
            const juce::juce_wchar doubleArrows[] = { 0x21D1, 0x21D2, 0x21D3, 0x21D0 };
            return juce::String::charToString(doubleArrows[gestureIndex - 12]);
        }

        return "";
    }

    SmartGrid::Color GetGestureColor(size_t gestureIndex)
    {
        if (gestureIndex < 8)
        {
            return SmartGrid::Color::Grey;
        }
        else if (gestureIndex < 12)
        {
            return SmartGrid::Color::Orange.AdjustBrightness(0.5);
        }
        else if (gestureIndex < 16)
        {
            return SmartGrid::Color::Yellow.AdjustBrightness(0.5);
        }

        return SmartGrid::Color::Off;
    }

    void GetBadgePosition(
        float centerX, 
        float centerY, 
        float radius,        
        size_t ix, 
        size_t total,
        bool upper,
        float& badgeX, 
        float& badgeY, 
        float& badgeLength)
    {
        if (total <= 8)
        {
            badgeLength = 1.0 / std::sqrt(1 + static_cast<float>(total) * static_cast<float>(total) / 4.0f);
            badgeX = - badgeLength * total / 2.0f + ix * badgeLength;
            badgeY = badgeLength;
        }
        else 
        {
            badgeLength = 1.0 / (2 * std::sqrt(5));
            badgeX = -4.0 / (2 * std::sqrt(5)) + (ix % 8) * badgeLength;
            badgeY = 2.0 / (2 * std::sqrt(5)) - (ix / 8) * badgeLength;
        }

        badgeX = centerX + radius * badgeX;
        badgeLength = radius * badgeLength;            
        badgeY = upper ? centerY - radius * badgeY : centerY + radius * badgeY - badgeLength;        
    }
};

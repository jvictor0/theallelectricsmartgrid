#pragma once

#include <JuceHeader.h>

// 14-segment ASCII character mappings
// Based on LED-Segment-ASCII by David Madison
// https://github.com/dmadison/LED-Segment-ASCII
// MIT License - Copyright (c) 2017 David Madison
//
// Segment layout:
//
//      ___A___
//     |\  |  /|
//     F H J K B
//     |  \|/  |
//      -G1-G2-
//     |  /|\  |
//     E L M N C
//     |/  |  \|
//      ___D___
//
// Bit mapping:
//   0: A (top horizontal)
//   1: B (upper-right vertical)
//   2: C (lower-right vertical)
//   3: D (bottom horizontal)
//   4: E (lower-left vertical)
//   5: F (upper-left vertical)
//   6: G1 (center-left horizontal)
//   7: G2 (center-right horizontal)
//   8: H (upper-left diagonal)
//   9: J (upper center vertical)
//  10: K (upper-right diagonal)
//  11: L (lower-left diagonal)
//  12: M (lower center vertical)
//  13: N (lower-right diagonal)
//  14: DP (decimal point)
//

struct FourteenSegmentDisplayComponent : public juce::Component
{
    enum class Segment
    {
        A = 0,
        B = 1,
        C = 2,
        D = 3,
        E = 4,
        F = 5,
        G1 = 6,
        G2 = 7,
        H = 8,
        J = 9,
        K = 10,
        L = 11,
        M = 12,
        N = 13,
        DP = 14
    };

    static constexpr uint16_t x_asciiTable[96] =
    {
        0x0000, // (space)
        0x4006, // !
        0x0202, // "
        0x12CE, // #
        0x12ED, // $
        0x3FE4, // %
        0x2359, // &
        0x0200, // '
        0x2400, // (
        0x0900, // )
        0x3FC0, // *
        0x12C0, // +
        0x0800, // ,
        0x00C0, // -
        0x4000, // .
        0x0C00, // /
        0x0C3F, // 0
        0x0406, // 1
        0x00DB, // 2
        0x008F, // 3
        0x00E6, // 4
        0x2069, // 5
        0x00FD, // 6
        0x0007, // 7
        0x00FF, // 8
        0x00EF, // 9
        0x1200, // :
        0x0A00, // ;
        0x2440, // <
        0x00C8, // =
        0x0980, // >
        0x5083, // ?
        0x02BB, // @
        0x00F7, // A
        0x128F, // B
        0x0039, // C
        0x120F, // D
        0x0079, // E
        0x0071, // F
        0x00BD, // G
        0x00F6, // H
        0x1209, // I
        0x001E, // J
        0x2470, // K
        0x0038, // L
        0x0536, // M
        0x2136, // N
        0x003F, // O
        0x00F3, // P
        0x203F, // Q
        0x20F3, // R
        0x00ED, // S
        0x1201, // T
        0x003E, // U
        0x0C30, // V
        0x2836, // W
        0x2D00, // X
        0x00EE, // Y
        0x0C09, // Z
        0x0039, // [
        0x2100, // backslash
        0x000F, // ]
        0x2800, // ^
        0x0008, // _
        0x0100, // `
        0x1058, // a
        0x2078, // b
        0x00D8, // c
        0x088E, // d
        0x0858, // e
        0x14C0, // f
        0x048E, // g
        0x1070, // h
        0x1000, // i
        0x0A10, // j
        0x3600, // k
        0x0030, // l
        0x10D4, // m
        0x1050, // n
        0x00DC, // o
        0x0170, // p
        0x0486, // q
        0x0050, // r
        0x2088, // s
        0x0078, // t
        0x001C, // u
        0x0810, // v
        0x2814, // w
        0x2D00, // x
        0x028E, // y
        0x0848, // z
        0x0949, // {
        0x1200, // |
        0x2489, // }
        0x0CC0, // ~
        0x0000, // (del)
    };

    juce::String m_text;
    juce::Colour m_onColor;
    juce::Colour m_offColor;
    float m_segmentThickness;
    float m_segmentGap;
    int m_numChars;

    FourteenSegmentDisplayComponent()
        : m_text("")
        , m_onColor(juce::Colours::red)
        , m_offColor(juce::Colour(40, 10, 10))
        , m_segmentThickness(0.08f)
        , m_segmentGap(0.02f)
        , m_numChars(0)
    {
    }

    void SetNumChars(int numChars)
    {
        m_numChars = numChars;
        repaint();
    }

    void SetText(const juce::String& text)
    {
        m_text = text;
        repaint();
    }

    void SetOnColor(juce::Colour color)
    {
        m_onColor = color;
        repaint();
    }

    void SetOffColor(juce::Colour color)
    {
        m_offColor = color;
        repaint();
    }

    void SetSegmentThickness(float thickness)
    {
        m_segmentThickness = thickness;
        repaint();
    }

    uint16_t GetSegmentMask(char c) const
    {
        if (c < 32 || c > 127)
        {
            return 0x0000;
        }

        return x_asciiTable[c - 32];
    }

    bool IsSegmentOn(uint16_t mask, Segment segment) const
    {
        return (mask & (1 << static_cast<int>(segment))) != 0;
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        int numChars = m_numChars > 0 ? m_numChars : static_cast<int>(m_text.length());

        if (numChars == 0)
        {
            numChars = 1;
        }

        float charWidth = bounds.getWidth() / static_cast<float>(numChars);
        float charHeight = bounds.getHeight();

        for (int i = 0; i < numChars; ++i)
        {
            char c = (i < m_text.length()) ? static_cast<char>(m_text[i]) : ' ';
            uint16_t mask = GetSegmentMask(c);

            juce::Rectangle<float> charBounds(
                bounds.getX() + i * charWidth,
                bounds.getY(),
                charWidth,
                charHeight
            );

            DrawCharacter(g, charBounds, mask);
        }
    }

    void DrawCharacter(juce::Graphics& g, juce::Rectangle<float> bounds, uint16_t mask)
    {
        float padding = bounds.getWidth() * 0.05f;
        bounds = bounds.reduced(padding);

        float w = bounds.getWidth();
        float h = bounds.getHeight();
        float x = bounds.getX();
        float y = bounds.getY();

        float thickness = w * m_segmentThickness;
        float gap = w * m_segmentGap;
        float halfH = h / 2.0f;

        // Horizontal segment positions
        //
        float horzLeft = x + thickness + gap;
        float horzRight = x + w - thickness - gap;
        float horzMid = x + w / 2.0f;
        float horzLen = (horzRight - horzLeft - gap) / 2.0f;

        // Vertical segment positions
        //
        float vertTop = y + thickness + gap;
        float vertMid = y + halfH;
        float vertBottom = y + h - thickness - gap;

        // Draw segment A (top horizontal)
        //
        DrawHorizontalSegment(g, horzLeft, y, horzRight - horzLeft, thickness, IsSegmentOn(mask, Segment::A));

        // Draw segment D (bottom horizontal)
        //
        DrawHorizontalSegment(g, horzLeft, y + h - thickness, horzRight - horzLeft, thickness, IsSegmentOn(mask, Segment::D));

        // Draw segment G1 (center-left horizontal)
        //
        DrawHorizontalSegment(g, horzLeft, vertMid - thickness / 2.0f, horzLen, thickness, IsSegmentOn(mask, Segment::G1));

        // Draw segment G2 (center-right horizontal)
        //
        DrawHorizontalSegment(g, horzMid + gap / 2.0f, vertMid - thickness / 2.0f, horzLen, thickness, IsSegmentOn(mask, Segment::G2));

        // Draw segment F (upper-left vertical)
        //
        DrawVerticalSegment(g, x, vertTop, thickness, vertMid - vertTop - gap, IsSegmentOn(mask, Segment::F));

        // Draw segment E (lower-left vertical)
        //
        DrawVerticalSegment(g, x, vertMid + gap, thickness, vertBottom - vertMid - gap, IsSegmentOn(mask, Segment::E));

        // Draw segment B (upper-right vertical)
        //
        DrawVerticalSegment(g, x + w - thickness, vertTop, thickness, vertMid - vertTop - gap, IsSegmentOn(mask, Segment::B));

        // Draw segment C (lower-right vertical)
        //
        DrawVerticalSegment(g, x + w - thickness, vertMid + gap, thickness, vertBottom - vertMid - gap, IsSegmentOn(mask, Segment::C));

        // Draw segment J (upper center vertical)
        //
        DrawVerticalSegment(g, horzMid - thickness / 2.0f, vertTop, thickness, vertMid - vertTop - gap, IsSegmentOn(mask, Segment::J));

        // Draw segment M (lower center vertical)
        //
        DrawVerticalSegment(g, horzMid - thickness / 2.0f, vertMid + gap, thickness, vertBottom - vertMid - gap, IsSegmentOn(mask, Segment::M));

        // Draw diagonal segments
        //
        float diagInnerX = horzMid - thickness / 2.0f;
        float diagOuterLeft = x + thickness + gap;
        float diagOuterRight = x + w - thickness - gap;

        // Draw segment H (upper-left diagonal)
        //
        DrawDiagonalSegment(g, diagOuterLeft, vertTop, diagInnerX - gap, vertMid - gap, thickness, IsSegmentOn(mask, Segment::H));

        // Draw segment K (upper-right diagonal)
        //
        DrawDiagonalSegment(g, diagOuterRight, vertTop, diagInnerX + thickness + gap, vertMid - gap, thickness, IsSegmentOn(mask, Segment::K));

        // Draw segment L (lower-left diagonal)
        //
        DrawDiagonalSegment(g, diagInnerX - gap, vertMid + gap, diagOuterLeft, vertBottom, thickness, IsSegmentOn(mask, Segment::L));

        // Draw segment N (lower-right diagonal)
        //
        DrawDiagonalSegment(g, diagInnerX + thickness + gap, vertMid + gap, diagOuterRight, vertBottom, thickness, IsSegmentOn(mask, Segment::N));

        // Draw decimal point
        //
        float dpSize = thickness * 1.2f;
        float dpX = x + w + gap;
        float dpY = y + h - dpSize;
        g.setColour(IsSegmentOn(mask, Segment::DP) ? m_onColor : m_offColor);
        g.fillEllipse(dpX, dpY, dpSize, dpSize);
    }

    void DrawHorizontalSegment(juce::Graphics& g, float x, float y, float width, float height, bool on)
    {
        juce::Path path;
        float halfH = height / 2.0f;

        // Hexagonal shape for horizontal segment
        //
        path.startNewSubPath(x + halfH, y);
        path.lineTo(x + width - halfH, y);
        path.lineTo(x + width, y + halfH);
        path.lineTo(x + width - halfH, y + height);
        path.lineTo(x + halfH, y + height);
        path.lineTo(x, y + halfH);
        path.closeSubPath();

        g.setColour(on ? m_onColor : m_offColor);
        g.fillPath(path);
    }

    void DrawVerticalSegment(juce::Graphics& g, float x, float y, float width, float height, bool on)
    {
        juce::Path path;
        float halfW = width / 2.0f;

        // Hexagonal shape for vertical segment
        //
        path.startNewSubPath(x + halfW, y);
        path.lineTo(x + width, y + halfW);
        path.lineTo(x + width, y + height - halfW);
        path.lineTo(x + halfW, y + height);
        path.lineTo(x, y + height - halfW);
        path.lineTo(x, y + halfW);
        path.closeSubPath();

        g.setColour(on ? m_onColor : m_offColor);
        g.fillPath(path);
    }

    void DrawDiagonalSegment(juce::Graphics& g, float x1, float y1, float x2, float y2, float thickness, bool on)
    {
        juce::Path path;

        // Calculate perpendicular offset for thickness
        //
        float dx = x2 - x1;
        float dy = y2 - y1;
        float len = std::sqrt(dx * dx + dy * dy);

        if (len < 0.001f)
        {
            return;
        }

        float nx = -dy / len * thickness / 2.0f;
        float ny = dx / len * thickness / 2.0f;

        // Draw as a parallelogram
        //
        path.startNewSubPath(x1 + nx, y1 + ny);
        path.lineTo(x2 + nx, y2 + ny);
        path.lineTo(x2 - nx, y2 - ny);
        path.lineTo(x1 - nx, y1 - ny);
        path.closeSubPath();

        g.setColour(on ? m_onColor : m_offColor);
        g.fillPath(path);
    }
};

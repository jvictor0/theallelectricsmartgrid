#pragma once

#include <juce_graphics/juce_graphics.h>

enum class VisualizerRegion
{
    Full,
    Top,
    Bottom,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

inline juce::Rectangle<int> GetVisualizerRegionBounds(
    juce::Rectangle<int> bounds, VisualizerRegion region)
{
    int leftWidth = bounds.getWidth() / 2;
    int topHeight = bounds.getHeight() / 2;
    switch (region)
    {
        case VisualizerRegion::Full:
            return bounds;
        case VisualizerRegion::Top:
            return bounds.withHeight(topHeight);
        case VisualizerRegion::Bottom:
            return bounds.withTrimmedTop(topHeight);
        case VisualizerRegion::TopLeft:
            return bounds.withSize(leftWidth, topHeight);
        case VisualizerRegion::TopRight:
            return bounds.withTrimmedLeft(leftWidth).withHeight(topHeight);
        case VisualizerRegion::BottomLeft:
            return bounds.withTrimmedTop(topHeight).withWidth(leftWidth);
        case VisualizerRegion::BottomRight:
            return bounds.withTrimmedLeft(leftWidth).withTrimmedTop(topHeight);
    }

    jassertfalse;
    return {};
}

#include "doctest.h"
#include "VisualizerRegion.hpp"
#include <array>

DOCTEST_TEST_CASE("Visualizer layout: full and vertical halves preserve every pixel")
{
    for (int size : {1, 336, 337})
    {
        juce::Rectangle<int> bounds(17, 29, 2 * size, size);
        auto full = GetVisualizerRegionBounds(bounds, VisualizerRegion::Full);
        auto top = GetVisualizerRegionBounds(bounds, VisualizerRegion::Top);
        auto bottom = GetVisualizerRegionBounds(bounds, VisualizerRegion::Bottom);
        DOCTEST_CHECK(full == bounds);
        DOCTEST_CHECK(top.getX() == bounds.getX());
        DOCTEST_CHECK(top.getY() == bounds.getY());
        DOCTEST_CHECK(top.getWidth() == bounds.getWidth());
        DOCTEST_CHECK(top.getBottom() == bottom.getY());
        DOCTEST_CHECK(bottom.getBottom() == bounds.getBottom());
        DOCTEST_CHECK(bottom.getWidth() == bounds.getWidth());
        DOCTEST_CHECK(top.getIntersection(bottom).isEmpty());
        DOCTEST_CHECK(top.getHeight() + bottom.getHeight() == bounds.getHeight());
        DOCTEST_CHECK(std::abs(top.getHeight() - bottom.getHeight()) <= 1);
        DOCTEST_CHECK_FALSE(top.contains(bottom.getPosition()));
        DOCTEST_CHECK(bottom.contains(bottom.getPosition()));
    }
}

DOCTEST_TEST_CASE("Visualizer layout: quadrants cover a slot without overlap at odd sizes")
{
    constexpr std::array<VisualizerRegion, 4> regions = {
        VisualizerRegion::TopLeft, VisualizerRegion::TopRight,
        VisualizerRegion::BottomLeft, VisualizerRegion::BottomRight};
    for (int size : {1, 336, 337})
    {
        juce::Rectangle<int> bounds(17, 29, size, size + 2);
        std::array<juce::Rectangle<int>, 4> quarters;
        int area = 0;
        for (size_t i = 0; i < regions.size(); ++i)
        {
            quarters[i] = GetVisualizerRegionBounds(bounds, regions[i]);
            area += quarters[i].getWidth() * quarters[i].getHeight();
            DOCTEST_CHECK(quarters[i].getX() >= bounds.getX());
            DOCTEST_CHECK(quarters[i].getY() >= bounds.getY());
            DOCTEST_CHECK(quarters[i].getRight() <= bounds.getRight());
            DOCTEST_CHECK(quarters[i].getBottom() <= bounds.getBottom());
            for (size_t j = 0; j < i; ++j)
            {
                DOCTEST_CHECK(quarters[i].getIntersection(quarters[j]).isEmpty());
            }
        }

        DOCTEST_CHECK(area == bounds.getWidth() * bounds.getHeight());
        DOCTEST_CHECK(quarters[0].getPosition() == bounds.getPosition());
        DOCTEST_CHECK(quarters[0].getRight() == quarters[1].getX());
        DOCTEST_CHECK(quarters[0].getBottom() == quarters[2].getY());
        DOCTEST_CHECK(quarters[1].getBottom() == quarters[3].getY());
        DOCTEST_CHECK(quarters[2].getRight() == quarters[3].getX());
        DOCTEST_CHECK(quarters[3].getBottomRight() == bounds.getBottomRight());
    }
}

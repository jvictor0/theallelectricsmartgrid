#include "doctest.h"
#include <JuceHeader.h>
#include "ScopeComponent.hpp"
#include "QuadDelayEnvelopeVisualizerComponent.hpp"
#include <memory>

DOCTEST_TEST_CASE("Scope: reduced point counts preserve the time axis and end marker")
{
    auto writer = std::make_unique<ScopeWriter>(1, 1);
    writer->RecordStart(0, 0);
    for (size_t sample = 0; sample < 128; ++sample)
    {
        writer->Write(0, 0, static_cast<float>(sample) / 127.0f);
        writer->AdvanceIndex();
    }

    writer->Publish();
    for (size_t points : {128, 256, 1024})
    {
        DOCTEST_CAPTURE(points);
        ScopeReader reader(writer.get(), 0, 0, points, 1);
        PathDrawer drawer(100, 200, 0, 0);
        juce::Image image(juce::Image::ARGB, 204, 104, true);
        juce::Graphics g(image);
        drawer.DrawScopePath(g, juce::Colours::white, reader, 1, 0);
        for (int x : {25, 100, 175})
        {
            int y = 100 - x / 2;
            bool painted = false;
            for (int dy = -2; dy <= 2; ++dy)
            {
                painted |= image.getPixelAt(x, y + dy).getAlpha() > 0;
            }

            DOCTEST_CHECK(painted);
        }

        drawer.DrawScopeMarker(g, juce::Colours::cyan, reader, 1, 0);
        DOCTEST_CHECK(image.getPixelAt(200, 1) == juce::Colours::cyan);
    }
}

DOCTEST_TEST_CASE("Delay waveform: reduced columns retain peaks from every source bin")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& state = ui->m_squiggleBoyUIState.m_delayUIState;
    for (size_t offset = 0; offset < 4; ++offset)
    {
        DOCTEST_CAPTURE(offset);
        auto& snapshot = state.BeginSnapshot();
        snapshot = QuadDelay::UISnapshot();
        for (auto& lane : snapshot.m_delayEnvelopeState)
        {
            for (size_t column : {0, 64, 255})
            {
                lane.m_maxEnvelope[column * 4 + offset] = 1;
                lane.m_minEnvelope[column * 4 + (offset + 1) % 4] = -1;
            }
        }

        state.CommitSnapshot();
        QuadDelayEnvelopeVisualizerComponent visualizer(ui.get());
        juce::Image image(juce::Image::ARGB, 256, 400, true);
        juce::Graphics g(image);
        visualizer.Draw(g, image.getBounds());
        for (int lane = 0; lane < 4; ++lane)
        {
            for (int x : {0, 64, 255})
            {
                DOCTEST_CHECK(image.getPixelAt(x, lane * 100 + 5).getRed() > 100);
                DOCTEST_CHECK(image.getPixelAt(x, lane * 100 + 95).getRed() > 100);
            }

            DOCTEST_CHECK(image.getPixelAt(32, lane * 100 + 5) == juce::Colours::black);
        }
    }
}

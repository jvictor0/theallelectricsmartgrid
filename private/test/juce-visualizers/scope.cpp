#include "doctest.h"
#include <JuceHeader.h>
#include "ScopeComponent.hpp"
#include "QuadDelayEnvelopeVisualizerComponent.hpp"
#include <memory>

namespace
{
struct PeakedResponse : TransferFunction
{
    float m_frequency;

    explicit PeakedResponse(float frequency)
        : m_frequency(frequency)
    {
    }

    float FrequencyResponse(float frequency) const override
    {
        float distance = std::log(frequency / m_frequency) / 0.03f;
        return std::exp(-distance * distance);
    }

    std::complex<float> TransferFunctionValue(float frequency) const override
    {
        return {FrequencyResponse(frequency), 0.0f};
    }
};

template<typename Matches>
float FindPeakX(const juce::Image& image, Matches matches)
{
    for (int y = 0; y < image.getHeight(); ++y)
    {
        int sumX = 0;
        int count = 0;
        for (int x = 0; x < image.getWidth(); ++x)
        {
            if (matches(image.getPixelAt(x, y)))
            {
                sumX += x;
                ++count;
            }
        }

        if (count > 0)
        {
            return static_cast<float>(sumX) / count;
        }
    }

    DOCTEST_FAIL("No curve pixels found");
    return -1.0f;
}
}

DOCTEST_TEST_CASE("Analyser: spectrum and oversampled response share the frequency axis")
{
    bool logX = true;
    DOCTEST_SUBCASE("logarithmic")
    {
        logX = true;
    }

    DOCTEST_SUBCASE("linear")
    {
        logX = false;
    }

    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    ui->m_squiggleBoyUIState.m_activeTrack.store(1);
    auto& filter = ui->m_squiggleBoyUIState.m_voiceFilterUIState[3];
    filter.m_lpLadder.m_alpha.store(1.0f);
    filter.m_lpLadder.m_feedback.store(0.0f);
    filter.m_hp4Pole.m_g.store(0.0f);
    filter.m_hp4Pole.m_k.store(1.0f);
    auto writer = std::make_unique<ScopeWriter>(9, 1);
    int voiceOffset = 0;

    for (int bin : {4, 16, 64, 192})
    {
        DOCTEST_CAPTURE(bin);
        float frequency = static_cast<float>(bin) / BasicWaveTable::x_tableSize;
        for (size_t sample = 0; sample < BasicWaveTable::x_tableSize; ++sample)
        {
            float value = 0.05f * std::sin(2.0f * static_cast<float>(M_PI) * frequency * sample);
            writer->Write(0, 3, value);
            writer->AdvanceIndex();
        }

        writer->Publish();
        AnalyserComponent analyser(WindowedFFT(writer.get(), 0), &voiceOffset, ui.get());
        analyser.m_logX = logX;
        juce::Image spectrum(juce::Image::ARGB, 1024, 400, true);
        juce::Graphics spectrumGraphics(spectrum);
        analyser.Draw(spectrumGraphics, spectrum.getBounds());

        PeakedResponse response(frequency / SquiggleBoyVoice::x_oversample);
        PathDrawer drawer(400, 1024, 0, 0);
        drawer.m_logX = logX;
        juce::Image transfer(juce::Image::ARGB, 1024, 400, true);
        juce::Graphics transferGraphics(transfer);
        drawer.DrawFrequencyResponse(transferGraphics, juce::Colours::white, response,
            1.0f / SquiggleBoyVoice::x_oversample);

        float spectrumX = FindPeakX(spectrum, [](juce::Colour pixel)
        {
            return pixel.getGreen() > 24 && pixel.getGreen() > 2 * pixel.getRed()
                && pixel.getGreen() > 2 * pixel.getBlue();
        });
        float transferX = FindPeakX(transfer, [](juce::Colour pixel)
        {
            return pixel.getAlpha() > 24;
        });
        DOCTEST_CAPTURE(spectrumX);
        DOCTEST_CAPTURE(transferX);
        DOCTEST_CHECK(std::abs(spectrumX - transferX) <= 3.0f);
    }
}

DOCTEST_TEST_CASE("Analyser: sub spectrum follows the trio and ignores the main signal and filter")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& writer = ui->m_squiggleBoyUIState.m_audioScopeWriter;
    const size_t subScope = static_cast<size_t>(SmartGridOne::AudioScopes::Sub);
    const size_t mainScope = static_cast<size_t>(SmartGridOne::AudioScopes::PostAmp);
    for (size_t sample = 0; sample < BasicWaveTable::x_tableSize; ++sample)
    {
        for (size_t trio = 0; trio < SquiggleBoy::x_numTracks; ++trio)
        {
            const float phase = 2.0f * static_cast<float>(M_PI) * sample / BasicWaveTable::x_tableSize;
            writer.Write(subScope, trio * 3, 0.5f * std::sin((8 + 8 * trio) * phase));
            writer.Write(mainScope, trio * 3, 0.9f * std::sin(64 * phase));
        }

        writer.AdvanceIndex();
    }

    writer.Publish();
    int voiceOffset = 0;
    for (size_t trio = 0; trio < SquiggleBoy::x_numTracks; ++trio)
    {
        ui->m_squiggleBoyUIState.m_activeTrack.store(trio);
        AnalyserComponent analyser(WindowedFFT(&writer, subScope), &voiceOffset, ui.get(),
            AnalyserComponent::FilterOverlay::None);
        juce::Image spectrum(juce::Image::ARGB, 512, 200, true);
        juce::Graphics graphics(spectrum);
        analyser.Draw(graphics, spectrum.getBounds());
        const auto& components = analyser.m_windowedFFT[0].m_dft.m_components;
        size_t peak = 0;
        for (size_t bin = 1; bin < DiscreteFourierTransform::x_maxComponents; ++bin)
        {
            if (std::abs(components[bin]) > std::abs(components[peak]))
            {
                peak = bin;
            }
        }

        DOCTEST_CHECK(peak == 8 + 8 * trio);
        auto& filter = ui->m_squiggleBoyUIState.m_voiceFilterUIState[trio * 3];
        filter.m_lpLadder.m_alpha.store(0.01f);
        filter.m_lpLadder.m_feedback.store(3.0f);
        AnalyserComponent changedFilter(WindowedFFT(&writer, subScope), &voiceOffset, ui.get(),
            AnalyserComponent::FilterOverlay::None);
        juce::Image unchangedSpectrum(juce::Image::ARGB, 512, 200, true);
        juce::Graphics changedGraphics(unchangedSpectrum);
        changedFilter.Draw(changedGraphics, unchangedSpectrum.getBounds());
        bool equal = true;
        for (int y = 0; y < spectrum.getHeight(); ++y)
        {
            for (int x = 0; x < spectrum.getWidth(); ++x)
            {
                equal = equal && spectrum.getPixelAt(x, y) == unchangedSpectrum.getPixelAt(x, y);
            }
        }

        DOCTEST_CHECK(equal);
    }
}

DOCTEST_TEST_CASE("Quad analyser: mix draws five separate colored spectra without transfer functions")
{
    using Scope = SmartGridOne::QuadScopes;
    using Type = QuadAnalyserComponent::Type;
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& writer = ui->m_squiggleBoyUIState.m_quadScopeWriter;
    for (size_t sample = 0; sample < BasicWaveTable::x_tableSize; ++sample)
    {
        const float phase = 2.0f * static_cast<float>(M_PI) * sample / BasicWaveTable::x_tableSize;
        for (size_t scope = 0; scope < static_cast<size_t>(Scope::NumScopes); ++scope)
        {
            for (size_t speaker = 0; speaker < 4; ++speaker)
            {
                writer.Write(scope, speaker, 0.5f * std::sin((8 + 16 * scope + 2 * speaker) * phase));
            }
        }

        writer.AdvanceIndex();
    }

    writer.Publish();
    auto analyser = std::make_unique<QuadAnalyserComponent>(ui.get(), Type::Mix);
    analyser->m_drawAll = false;
    juce::Image spectrum(juce::Image::ARGB, 1024, 512, true);
    juce::Graphics graphics(spectrum);
    analyser->Draw(graphics, spectrum.getBounds());
    const Scope x_scopes[] = {Scope::Delay, Scope::Reverb, Scope::PartialMachine, Scope::Dry, Scope::Sub};
    const SmartGrid::Color x_colors[] =
    {
        SmartGrid::Color::Pink, SmartGrid::Color::Fuscia, SmartGrid::Color::Cyan,
        SmartGrid::Color::Yellow, SmartGrid::Color::White
    };
    for (size_t source = 0; source < 5; ++source)
    {
        DOCTEST_CAPTURE(source);
        for (size_t speaker = 0; speaker < 4; ++speaker)
        {
            const auto& components = analyser->m_quadWindowedFFT[source].m_dft[speaker].m_components;
            size_t peak = 0;
            for (size_t bin = 1; bin < DiscreteFourierTransform::x_maxComponents; ++bin)
            {
                if (std::abs(components[bin]) > std::abs(components[peak]))
                {
                    peak = bin;
                }
            }

            DOCTEST_CHECK(peak == 8 + 16 * static_cast<size_t>(x_scopes[source]) + 2 * speaker);
            for (Type type : {Type::Delay, Type::Reverb, Type::PartialMachine})
            {
                DOCTEST_CHECK(analyser->GetTransferFunctionForSpeaker(type, speaker) == nullptr);
            }
        }

        const auto color = x_colors[source];
        const int maxColor = std::max({color.m_red, color.m_green, color.m_blue});
        size_t pixels[4] = {};
        for (int y = 0; y < spectrum.getHeight(); ++y)
        {
            for (int x = 0; x < spectrum.getWidth(); ++x)
            {
                const auto pixel = spectrum.getPixelAt(x, y);
                const int maxPixel = std::max({pixel.getRed(), pixel.getGreen(), pixel.getBlue()});
                if (maxPixel > 32
                    && std::abs(pixel.getRed() * maxColor - color.m_red * maxPixel) <= 3 * maxColor
                    && std::abs(pixel.getGreen() * maxColor - color.m_green * maxPixel) <= 3 * maxColor
                    && std::abs(pixel.getBlue() * maxColor - color.m_blue * maxPixel) <= 3 * maxColor)
                {
                    ++pixels[(y >= spectrum.getHeight() / 2 ? 2 : 0) + (x >= spectrum.getWidth() / 2 ? 1 : 0)];
                }
            }
        }

        for (size_t count : pixels)
        {
            DOCTEST_CHECK(count > 5);
        }
    }
}

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

DOCTEST_TEST_CASE("Delay waveform: full resolution retains peaks in individual source bins")
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
            for (size_t firstBin : {0, 256, 1020})
            {
                lane.m_maxEnvelope[firstBin + offset] = 1;
                lane.m_minEnvelope[firstBin + offset] = -1;
            }
        }

        state.CommitSnapshot();
        QuadDelayEnvelopeVisualizerComponent visualizer(ui.get());
        juce::Image image(juce::Image::ARGB, 1024, 400, true);
        juce::Graphics g(image);
        visualizer.Draw(g, image.getBounds());
        for (int lane = 0; lane < 4; ++lane)
        {
            for (int firstBin : {0, 256, 1020})
            {
                int x = firstBin + static_cast<int>(offset);
                DOCTEST_CHECK(image.getPixelAt(x, lane * 100 + 5).getRed() > 100);
                DOCTEST_CHECK(image.getPixelAt(x, lane * 100 + 95).getRed() > 100);
            }

            DOCTEST_CHECK(image.getPixelAt(32, lane * 100 + 5) == juce::Colours::black);
        }
    }
}

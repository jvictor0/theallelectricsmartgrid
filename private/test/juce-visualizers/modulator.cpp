#include "doctest.h"
#include "EncoderModulatorComponents.hpp"
#include "ModulatorTimeAxisComponent.hpp"
#include <memory>

namespace
{
bool HasColorNear(const juce::Image& image, int x, int y, juce::Colour color)
{
    for (int dx = -1; dx <= 1; ++dx)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            auto pixel = image.getPixelAt(x + dx, y + dy);
            if (pixel.getAlpha() > 128
                && std::abs(pixel.getRed() - color.getRed()) <= 1
                && std::abs(pixel.getGreen() - color.getGreen()) <= 1
                && std::abs(pixel.getBlue() - color.getBlue()) <= 1)
            {
                return true;
            }
        }
    }

    return false;
}
}

DOCTEST_TEST_CASE("LFO cursors: phase axes use real duration for visibility and source selection")
{
    for (size_t periodSamples : {32768, 8192})
    {
        DOCTEST_CAPTURE(periodSamples);
        auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
        auto& state = ui->m_squiggleBoyUIState;
        state.m_firstScopeSample.store(0);
        state.m_activeTrack.store(0);
        TimeWarpInverter<8192> inverse(4096.0);
        size_t voiceScope = SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::LFO1), 0);
        size_t quadScope = static_cast<size_t>(SmartGridOne::QuadScopes::QuadLFO1);
        state.m_voiceScopeWriter.SetTimeWarp(voiceScope, &inverse);
        state.m_voiceScopeWriter.SetPeriodic(voiceScope, 0, {0.0, 1.0});
        state.m_quadScopeWriter.SetTimeWarp(quadScope, &inverse);
        for (size_t voice = 0; voice < 4; ++voice)
        {
            state.m_quadScopeWriter.SetPeriodic(quadScope, voice, {0.0, 1.0});
        }

        for (size_t sample = 0; sample <= periodSamples * 5 / 4; ++sample)
        {
            inverse.Write(static_cast<double>(sample) / periodSamples, static_cast<double>(sample));
            state.m_voiceScopeWriter.Write(voiceScope, TrioFloat::Splat(0.25f));
            state.m_quadScopeWriter.Write(quadScope, QuadFloat::Splat(0.25f));
            state.m_voiceScopeWriter.AdvanceIndex();
            state.m_quadScopeWriter.AdvanceIndex();
        }

        state.m_voiceScopeWriter.Publish();
        state.m_quadScopeWriter.Publish();
        state.m_voiceScopeWriter.AcceptSamples();
        state.m_quadScopeWriter.AcceptSamples();
        bool slow = periodSamples == 32768;
        ScopeReader reader(&state.m_voiceScopeWriter, 0, voiceScope, 256);
        DOCTEST_CHECK(PathDrawer::ShouldDrawScopeMarker(reader) == slow);
        int selectedVoice = 0;
        for (auto mode : {SmartGridOneLayout::ParamMode::Voice, SmartGridOneLayout::ParamMode::Quad})
        {
            DOCTEST_CAPTURE(mode);
            bool voice = mode == SmartGridOneLayout::ParamMode::Voice;
            ModulatorTimeAxisComponent component;
            component.Configure(ui.get(), mode, voice ? static_cast<size_t>(VoiceModulator::LFO1)
                : static_cast<size_t>(QuadModulator::LFO1), &selectedVoice);
            DOCTEST_CHECK(component.GetTimeExtent() > 0.99 * periodSamples);
            DOCTEST_CHECK(component.GetTimeExtent() <= periodSamples);
            juce::Image image(juce::Image::ARGB, 512, 200, true);
            juce::Graphics graphics(image);
            component.Draw(graphics, image.getBounds());
            int markerY = 145;
            DOCTEST_CHECK((image.getPixelAt(126, markerY - 2) != juce::Colours::black) == slow);
        }
    }
}

DOCTEST_TEST_CASE("LFO and envelope paths: unavailable inverse points break the trace")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& writer = ui->m_squiggleBoyUIState.m_voiceScopeWriter;
    ui->m_squiggleBoyUIState.m_firstScopeSample.store(0);
    TimeWarpInverter<8192> inverse(1024.0);
    size_t lfo = SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::LFO1), 0);
    size_t env = SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::AmpEnvelope), 0);
    writer.SetTimeWarp(lfo, &inverse);
    writer.SetTimeWarp(env, &inverse);
    writer.SetPeriodic(lfo, 0, {0.0, 1.0});
    for (size_t sample = 0; sample <= 1280; ++sample)
    {
        inverse.Write(static_cast<double>(sample) / 1024.0, static_cast<double>(sample));
        if (sample % 1024 == 0)
        {
            writer.RecordStart(env, 0);
        }

        writer.Write(lfo, TrioFloat::Splat(0.75f));
        writer.Write(env, TrioFloat::Splat(0.75f));
        writer.AdvanceIndex();
    }

    writer.Publish();
    writer.AcceptSamples();
    for (size_t cell = 450; cell <= 575; ++cell)
    {
        inverse.ClearCell(cell);
    }

    int selected = 0;
    for (auto source : {VoiceModulator::LFO1, VoiceModulator::AmpEnvelope})
    {
        ModulatorTimeAxisComponent component;
        component.Configure(ui.get(), SmartGridOneLayout::ParamMode::Voice, static_cast<size_t>(source), &selected);
        juce::Image image(juce::Image::ARGB, 512, 200, true);
        juce::Graphics graphics(image);
        component.Draw(graphics, image.getBounds());
        DOCTEST_CHECK(image.getPixelAt(200, 55) != juce::Colours::black);
        for (int y = 0; y < 200; ++y)
        {
            if (y != 100)
            {
                DOCTEST_CHECK(image.getPixelAt(256, y) == juce::Colours::black);
            }
        }
    }
}

DOCTEST_TEST_CASE("Sheafy dial: spokes follow the active lens time slice and selected axis")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& state = ui->m_nonagonUIState.m_lameJuisUIState;
    for (size_t slice = 0; slice < 64; ++slice)
    {
        HarmonicSheaf::Section section;
        section.m_total[0] = 2;
        section.m_high[0] = static_cast<uint8_t>((slice & 1) + ((slice >> 1) & 1));
        section.m_total[1] = 1;
        section.m_high[1] = static_cast<uint8_t>((slice >> 2) & 1);
        state.m_harmonicSheafState.m_sectionState.m_sections[slice].store(section);
    }

    state.m_harmonicSheafState.m_voiceChooserState[0].m_lens.store(HarmonicSheaf::Lens(0x3e));
    state.m_currentTimeSlice.store(0);
    SheafyModulatorDialComponent dial(ui.get(), VoiceModulator::Sheaf1);
    const auto color = J(GetModulatorColor(VoiceModulator::Sheaf1));
    juce::Image first(juce::Image::ARGB, 200, 200, true);
    juce::Graphics firstGraphics(first);
    dial.Draw(firstGraphics, first.getBounds());
    DOCTEST_CHECK(HasColorNear(first, 75, 125, color));
    DOCTEST_CHECK(HasColorNear(first, 100, 65, color));
    DOCTEST_CHECK_FALSE(HasColorNear(first, 125, 125, color));

    state.m_currentTimeSlice.store(2);
    juce::Image second(juce::Image::ARGB, 200, 200, true);
    juce::Graphics secondGraphics(second);
    dial.Draw(secondGraphics, second.getBounds());
    DOCTEST_CHECK_FALSE(HasColorNear(second, 75, 125, color));
    DOCTEST_CHECK(HasColorNear(second, 100, 65, color));
    DOCTEST_CHECK(HasColorNear(second, 125, 125, color));

    ui->m_squiggleBoyUIState.m_activeTrack.store(1);
    state.m_harmonicSheafState.m_voiceChooserState[3].m_lens.store(HarmonicSheaf::Lens(0x3f));
    juce::Image fixed(juce::Image::ARGB, 200, 200, true);
    juce::Graphics fixedGraphics(fixed);
    dial.Draw(fixedGraphics, fixed.getBounds());
    DOCTEST_CHECK(HasColorNear(fixed, 100, 65, color));
    DOCTEST_CHECK_FALSE(HasColorNear(fixed, 125, 125, color));

    SheafyModulatorDialComponent other(ui.get(), VoiceModulator::Sheaf2);
    juce::Image axis(juce::Image::ARGB, 200, 200, true);
    juce::Graphics axisGraphics(axis);
    other.Draw(axisGraphics, axis.getBounds());
    DOCTEST_CHECK(HasColorNear(axis, 75, 125, J(GetModulatorColor(VoiceModulator::Sheaf2))));
    DOCTEST_CHECK_FALSE(HasColorNear(axis, 100, 65, J(GetModulatorColor(VoiceModulator::Sheaf2))));
}

DOCTEST_TEST_CASE("Sheafy dial: concentric held voice arcs stay inside encoder rings and retain the bottom gap")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    ui->m_squiggleBoyUIState.m_activeTrack.store(2);
    ui->m_nonagonUIState.m_extraTimbre[6][2].store(0.25f);
    ui->m_nonagonUIState.m_extraTimbre[7][2].store(0.5f);
    ui->m_nonagonUIState.m_extraTimbre[8][2].store(1.0f);
    SheafyModulatorDialComponent dial(ui.get(), VoiceModulator::Sheaf3);
    juce::Image image(juce::Image::ARGB, 200, 200, true);
    juce::Graphics g(image);
    dial.Draw(g, image.getBounds());
    DOCTEST_CHECK(HasColorNear(image, 49, 100, J(TheNonagonSmartGrid::VoiceColor(6))));
    DOCTEST_CHECK_FALSE(HasColorNear(image, 100, 49, J(TheNonagonSmartGrid::VoiceColor(6))));
    DOCTEST_CHECK(HasColorNear(image, 60, 100, J(TheNonagonSmartGrid::VoiceColor(7))));
    DOCTEST_CHECK_FALSE(HasColorNear(image, 140, 100, J(TheNonagonSmartGrid::VoiceColor(7))));
    DOCTEST_CHECK(HasColorNear(image, 129, 100, J(TheNonagonSmartGrid::VoiceColor(8))));
    for (int x = 0; x < 200; ++x)
    {
        for (int y = 0; y < 200; ++y)
        {
            float distance = std::hypot(x + 0.5f - 100, y + 0.5f - 100);
            if (distance > 58 || (x == 100 && y > 104))
            {
                DOCTEST_CHECK(image.getPixelAt(x, y).getAlpha() == 0);
            }
        }
    }
}

DOCTEST_TEST_CASE("Noise visualizer: a fresh colored waveform spans the bounds on every draw")
{
    NoiseModulatorComponent noise(SmartGrid::Color::White);
    noise.m_random.setSeed(42);
    juce::Image first(juce::Image::ARGB, 100, 100, true);
    juce::Image second(juce::Image::ARGB, 100, 100, true);
    juce::Graphics firstGraphics(first);
    juce::Graphics secondGraphics(second);
    noise.Draw(firstGraphics, first.getBounds());
    noise.Draw(secondGraphics, second.getBounds());
    bool changed = false;
    for (int x = 0; x < 100; ++x)
    {
        int pixels = 0;
        for (int y = 0; y < 100; ++y)
        {
            auto pixel = first.getPixelAt(x, y);
            pixels += pixel.getAlpha() > 0;
            if (pixel.getAlpha() == 255)
            {
                DOCTEST_CHECK(pixel == J(SmartGrid::Color::White));
            }

            changed |= pixel != second.getPixelAt(x, y);
        }

        DOCTEST_CHECK(pixels > 0);
    }

    DOCTEST_CHECK(changed);
}

DOCTEST_TEST_CASE("Spread visualizer: voice-colored bars fit inside encoder rings and follow the active trio")
{
    std::atomic<size_t> activeTrio{0};
    for (size_t voices : {3, 4})
    {
        SpreadModulatorComponent spread(voices, voices == 3 ? &activeTrio : nullptr);
        for (size_t trio = 0; trio < 3; ++trio)
        {
            activeTrio.store(trio);
            juce::Image image(juce::Image::ARGB, 200, 200, true);
            juce::Graphics g(image);
            spread.Draw(g, {20, 40, 160, 120});
            for (size_t voice = 0; voice < voices; ++voice)
            {
                int centerX = voices == 3 ? 84 + 16 * voice : 82 + 12 * voice;
                int top = voices == 3 ? 120 - 20 * voice : 120 - static_cast<int>(40 * voice / 3);
                auto color = TheNonagonSmartGrid::VoiceColor(voices == 3 ? trio * 3 + voice : voice);
                DOCTEST_CHECK(image.getPixelAt(centerX, top + 1) == J(color));
                DOCTEST_CHECK(image.getPixelAt(centerX, top - 2).getAlpha() == 0);
                DOCTEST_CHECK(image.getPixelAt(centerX - 5, 122).getAlpha() == 0);
            }

            for (int x = 0; x < image.getWidth(); ++x)
            {
                for (int y = 0; y < image.getHeight(); ++y)
                {
                    if (std::hypot(x + 0.5f - 100, y + 0.5f - 100) > 36)
                    {
                        DOCTEST_CHECK(image.getPixelAt(x, y).getAlpha() == 0);
                    }
                }
            }
        }
    }
}

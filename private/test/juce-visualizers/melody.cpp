#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "SequencerMelodyVisualizerComponent.hpp"

#include <memory>

namespace
{
using Visualizer = SequencerMelodyVisualizerComponent;
using Strategy = HarmonicSheaf::SectionChoiceStrategy;

struct MelodyRig
{
    std::unique_ptr<TheNonagonSquiggleBoyInternal::UIState> m_ui;
    int m_voiceOffset = -1;
    std::atomic<double> m_phase{9.0 / 32.0};
    Visualizer m_visualizer;

    MelodyRig()
        : m_ui(std::make_unique<TheNonagonSquiggleBoyInternal::UIState>())
        , m_visualizer(m_ui.get(), &m_voiceOffset, &m_phase)
    {
        m_ui->m_squiggleBoyUIState.m_activeTrack.store(0);
        auto& state = m_ui->m_nonagonUIState;
        auto& time = state.m_theoryOfTimeUIState;
        for (size_t loop = 0; loop < 6; ++loop)
        {
            time.m_periodTicks[loop].store(int64_t{1} << loop);
            time.m_rhythm[loop].m_size.store(2);
            time.m_rhythm[loop].m_resetLoopIndex.store(-1);
            for (size_t slot = 0; slot < 16; ++slot)
            {
                time.m_rhythm[loop].m_gate[slot].store(slot % 2 == 0);
            }
        }

        auto& harmonic = state.m_lameJuisUIState.m_harmonicSheafState;
        for (size_t slice = 0; slice < 64; ++slice)
        {
            HarmonicSheaf::Section section;
            for (size_t rank = 0; rank < 3; ++rank)
            {
                section.m_total[rank] = 1;
                section.m_high[rank] = (slice >> rank) & 1;
            }

            harmonic.m_sectionState.m_sections[slice].store(section);
        }

        harmonic.m_evaluatorState.m_coefficients[0].store(0.25f);
        harmonic.m_evaluatorState.m_coefficients[1].store(0.5f);
        harmonic.m_evaluatorState.m_coefficients[2].store(2.0f);
        for (size_t trio = 0; trio < 3; ++trio)
        {
            state.m_indexArpUIState.m_clockSelect[trio].store(0);
            state.m_indexArpUIState.m_resetSelect[trio].store(5);
        }

        for (size_t voice = 0; voice < 9; ++voice)
        {
            state.m_muted[voice].store(false);
            auto& arp = state.m_indexArpUIState.m_arpUIState[voice];
            arp.m_min.store(static_cast<float>(voice));
            arp.m_max.store(static_cast<float>(voice) + 0.75f);
            arp.m_rhythmLength.store(1);
            arp.m_rhythm[0].store(true);
            auto& chooser = harmonic.m_voiceChooserState[voice];
            chooser.m_lens.store(HarmonicSheaf::Lens(0x3c));
            chooser.m_baseStrategy.store(Strategy::None);
            chooser.m_strategy.store(Strategy::ClosestModOne);
        }
    }
};

bool HasLineAt(const juce::Image& image, float pitch, int x)
{
    int y = static_cast<int>(std::round(400.0f - pitch * 100.0f));
    for (int column = x; column < x + 12; ++column)
    {
        for (int row = y - 1; row <= y + 1; ++row)
        {
            if (image.getPixelAt(column, row).getAlpha() != 0)
            {
                return true;
            }
        }
    }

    return false;
}

bool HasPixels(const juce::Image& image, juce::Rectangle<int> area)
{
    for (int x = area.getX(); x < area.getRight(); ++x)
    {
        for (int y = area.getY(); y < area.getBottom(); ++y)
        {
            if (image.getPixelAt(x, y).getAlpha() != 0)
            {
                return true;
            }
        }
    }

    return false;
}
}

DOCTEST_TEST_CASE("Melody: pitch range follows only displayed voice bounds without sheaf padding")
{
    MelodyRig rig;
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    DOCTEST_CHECK(frame.m_minPitch == doctest::Approx(0.0f));
    DOCTEST_CHECK(frame.m_maxPitch == doctest::Approx(2.75f));

    rig.m_ui->m_nonagonUIState.m_muted[2].store(true);
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    DOCTEST_CHECK(frame.m_maxPitch == doctest::Approx(1.75f));

    rig.m_voiceOffset = 2;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    DOCTEST_CHECK(frame.m_minPitch == doctest::Approx(2.0f));
    DOCTEST_CHECK(frame.m_maxPitch == doctest::Approx(2.75f));
}

DOCTEST_TEST_CASE("Melody: modulo-one candidates repeat into every visible octave")
{
    MelodyRig rig;
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    frame.m_start = 0;
    frame.m_end = 1;
    frame.m_minPitch = 0.0f;
    frame.m_maxPitch = 4.0f;
    juce::Image image(juce::Image::ARGB, 320, 400, true);
    juce::Graphics graphics(image);
    rig.m_visualizer.DrawSheaf(graphics, frame, image.getBounds().toFloat());
    DOCTEST_CHECK(HasLineAt(image, 0.25f, 20));
    DOCTEST_CHECK(HasLineAt(image, 1.25f, 20));
    DOCTEST_CHECK(HasLineAt(image, 2.25f, 20));
    DOCTEST_CHECK(HasLineAt(image, 3.25f, 20));
    DOCTEST_CHECK_FALSE(HasLineAt(image, 1.125f, 20));
}

DOCTEST_TEST_CASE("Melody: percentile candidates retain their register and add only requested upper octaves")
{
    MelodyRig rig;
    rig.m_voiceOffset = 0;
    auto& state = rig.m_ui->m_nonagonUIState;
    auto& arp = state.m_indexArpUIState.m_arpUIState[0];
    arp.m_max.store(1.5f);
    state.m_lameJuisUIState.m_harmonicSheafState.m_voiceChooserState[0].m_strategy.store(Strategy::Percentile);
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    frame.m_start = 0;
    frame.m_end = 1;
    frame.m_minPitch = 0.0f;
    frame.m_maxPitch = 4.0f;
    juce::Image image(juce::Image::ARGB, 320, 400, true);
    juce::Graphics graphics(image);
    rig.m_visualizer.DrawSheaf(graphics, frame, image.getBounds().toFloat());
    DOCTEST_CHECK_FALSE(HasLineAt(image, 0.25f, 20));
    DOCTEST_CHECK_FALSE(HasLineAt(image, 1.25f, 20));
    DOCTEST_CHECK(HasLineAt(image, 2.25f, 20));
    DOCTEST_CHECK(HasLineAt(image, 3.25f, 20));

    arp.m_max.store(0.75f);
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    frame.m_start = 0;
    frame.m_end = 1;
    frame.m_minPitch = 0.0f;
    frame.m_maxPitch = 4.0f;
    image.clear(image.getBounds());
    rig.m_visualizer.DrawSheaf(graphics, frame, image.getBounds().toFloat());
    DOCTEST_CHECK(HasLineAt(image, 2.25f, 20));
    DOCTEST_CHECK_FALSE(HasLineAt(image, 3.25f, 20));
}

DOCTEST_TEST_CASE("Melody: percentile choice interpolates on each time slice's pitch axis")
{
    MelodyRig rig;
    rig.m_voiceOffset = 0;
    auto& state = rig.m_ui->m_nonagonUIState;
    auto& arp = state.m_indexArpUIState.m_arpUIState[0];
    arp.m_max.store(1.0f);
    arp.m_offset.store(0.125f);
    state.m_lameJuisUIState.m_harmonicSheafState.m_voiceChooserState[0].m_strategy.store(Strategy::Percentile);
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    frame.m_start = 0;
    frame.m_end = 8;
    frame.m_minPitch = 0.0f;
    frame.m_maxPitch = 4.0f;
    juce::Image image(juce::Image::ARGB, 320, 400, true);
    juce::Graphics graphics(image);
    rig.m_visualizer.DrawVoice(graphics, frame, image.getBounds().toFloat(), 0);
    DOCTEST_CHECK(HasLineAt(image, 2.125f, 10));
    DOCTEST_CHECK(HasLineAt(image, 0.125f, 170));
    DOCTEST_CHECK_FALSE(HasLineAt(image, 3.0f, 10));
    DOCTEST_CHECK_FALSE(HasLineAt(image, 1.0f, 170));
    DOCTEST_CHECK_FALSE(HasLineAt(image, 1.125f, 10));
}

DOCTEST_TEST_CASE("Melody: percentile bounds convert to pitch across the displayed time slices")
{
    MelodyRig rig;
    rig.m_voiceOffset = 0;
    auto& state = rig.m_ui->m_nonagonUIState;
    auto& arp = state.m_indexArpUIState.m_arpUIState[0];
    arp.m_min.store(0.125f);
    arp.m_max.store(1.125f);
    state.m_lameJuisUIState.m_harmonicSheafState.m_voiceChooserState[0].m_strategy.store(Strategy::Percentile);
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    DOCTEST_CHECK(frame.m_minPitch == doctest::Approx(0.125f));
    DOCTEST_CHECK(frame.m_maxPitch == doctest::Approx(3.125f));
}

DOCTEST_TEST_CASE("Melody: a collapsed arp range places its line at the vertical center")
{
    MelodyRig rig;
    rig.m_voiceOffset = 0;
    auto& arp = rig.m_ui->m_nonagonUIState.m_indexArpUIState.m_arpUIState[0];
    arp.m_min.store(0.25f);
    arp.m_max.store(0.25f);
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    DOCTEST_CHECK(frame.m_minPitch == doctest::Approx(0.25f));
    DOCTEST_CHECK(frame.m_maxPitch == doctest::Approx(0.25f));
    DOCTEST_CHECK(Visualizer::PitchY(0.25f, frame, juce::Rectangle<float>(0, 0, 320, 400))
        == doctest::Approx(200.0f));
    DOCTEST_CHECK(Visualizer::PitchY(0.24f, frame, juce::Rectangle<float>(0, 0, 320, 400)) > 400.0f);
    DOCTEST_CHECK(Visualizer::PitchY(0.26f, frame, juce::Rectangle<float>(0, 0, 320, 400)) < 0.0f);
}

DOCTEST_TEST_CASE("Melody: percentile interpolation preserves duplicate ranks and signed octave shifts")
{
    MelodyRig rig;
    auto& harmonic = rig.m_ui->m_nonagonUIState.m_lameJuisUIState.m_harmonicSheafState;
    harmonic.m_evaluatorState.m_coefficients[0].store(0.0f);
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    const auto& pitches = frame.m_pitchSlices[60];
    DOCTEST_CHECK(pitches.InterpolatePercentile(0.125f) == doctest::Approx(2.0f));
    DOCTEST_CHECK(pitches.InterpolatePercentile(0.375f) == doctest::Approx(2.25f));
    DOCTEST_CHECK(pitches.InterpolatePercentile(0.875f) == doctest::Approx(2.5f));
    DOCTEST_CHECK(pitches.InterpolatePercentile(1.375f) == doctest::Approx(3.25f));
    DOCTEST_CHECK(pitches.InterpolatePercentile(-0.625f) == doctest::Approx(1.25f));
}

DOCTEST_TEST_CASE("Melody: percentile range includes extrema between bounds at octave jumps")
{
    MelodyRig rig;
    rig.m_voiceOffset = 0;
    auto& state = rig.m_ui->m_nonagonUIState;
    auto& harmonic = state.m_lameJuisUIState.m_harmonicSheafState;
    harmonic.m_evaluatorState.m_coefficients[1].store(4.0f);
    harmonic.m_voiceChooserState[0].m_strategy.store(Strategy::Percentile);
    auto& arp = state.m_indexArpUIState.m_arpUIState[0];
    arp.m_min.store(0.8f);
    arp.m_max.store(1.1f);
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    DOCTEST_CHECK(frame.m_minPitch == doctest::Approx(1.0f));
    DOCTEST_CHECK(frame.m_maxPitch == doctest::Approx(6.25f));
}

DOCTEST_TEST_CASE("Melody: choice eases toward the next step only during the preceding half step")
{
    MelodyRig rig;
    rig.m_voiceOffset = 0;
    auto& arp = rig.m_ui->m_nonagonUIState.m_indexArpUIState.m_arpUIState[0];
    arp.m_min.store(0.0f);
    arp.m_max.store(4.0f);
    arp.m_offset.store(0.125f);
    arp.m_interval.store(0.25f);
    arp.m_rhythmLength.store(4);
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    frame.m_start = 0;
    frame.m_end = 2;
    juce::Image image(juce::Image::ARGB, 320, 400, true);
    juce::Graphics graphics(image);
    rig.m_visualizer.DrawVoice(graphics, frame, image.getBounds().toFloat(), 0);
    DOCTEST_CHECK(HasPixels(image, juce::Rectangle<int>(110, 270, 30, 50)));
    DOCTEST_CHECK_FALSE(HasPixels(image, juce::Rectangle<int>(20, 270, 50, 50)));
    DOCTEST_CHECK_FALSE(HasPixels(image, juce::Rectangle<int>(159, 280, 3, 40)));
}

DOCTEST_TEST_CASE("Melody: choice paths do not connect across a motive boundary")
{
    MelodyRig rig;
    rig.m_voiceOffset = 0;
    auto& arp = rig.m_ui->m_nonagonUIState.m_indexArpUIState.m_arpUIState[0];
    arp.m_min.store(0.0f);
    arp.m_max.store(4.0f);
    arp.m_offset.store(0.125f);
    arp.m_interval.store(0.25f);
    arp.m_rhythmLength.store(4);
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    frame.m_start = 0;
    frame.m_end = 8;
    juce::Image image(juce::Image::ARGB, 320, 400, true);
    juce::Graphics graphics(image);
    rig.m_visualizer.DrawVoice(graphics, frame, image.getBounds().toFloat(), 0);
    DOCTEST_CHECK_FALSE(HasPixels(image, juce::Rectangle<int>(158, 190, 4, 20)));
    DOCTEST_CHECK(HasLineAt(image, 3.5f, 130));
    DOCTEST_CHECK(HasLineAt(image, 0.5f, 170));
}

DOCTEST_TEST_CASE("Melody: slice separators follow read gates and ignore co-muted gate changes")
{
    MelodyRig rig;
    auto& time = rig.m_ui->m_nonagonUIState.m_theoryOfTimeUIState;
    for (size_t loop = 0; loop < 6; ++loop)
    {
        for (size_t slot = 0; slot < 16; ++slot)
        {
            time.m_rhythm[loop].m_gate[slot].store(true);
        }
    }

    time.m_rhythm[0].m_gate[1].store(false);
    time.m_rhythm[2].m_gate[1].store(false);
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    frame.m_start = 0;
    frame.m_end = 8;
    frame.m_minPitch = 0.0f;
    frame.m_maxPitch = 4.0f;
    juce::Image image(juce::Image::ARGB, 320, 400, true);
    juce::Graphics graphics(image);
    rig.m_visualizer.DrawSheaf(graphics, frame, image.getBounds().toFloat());
    DOCTEST_CHECK(HasPixels(image, juce::Rectangle<int>(159, 235, 2, 5)));
    DOCTEST_CHECK_FALSE(HasPixels(image, juce::Rectangle<int>(79, 235, 2, 5)));
    DOCTEST_CHECK_FALSE(HasPixels(image, juce::Rectangle<int>(239, 235, 2, 5)));

    image.clear(image.getBounds());
    frame.m_end = 32;
    rig.m_visualizer.DrawSheaf(graphics, frame, juce::Rectangle<float>(0, 0, 16, 400));
    DOCTEST_CHECK_FALSE(HasPixels(image, juce::Rectangle<int>(0, 235, 16, 5)));
}

DOCTEST_TEST_CASE("Melody: horizontal grid marks whole octaves inside fractional pitch bounds")
{
    MelodyRig rig;
    rig.m_voiceOffset = 0;
    auto& state = rig.m_ui->m_nonagonUIState;
    auto& harmonic = state.m_lameJuisUIState.m_harmonicSheafState;
    for (auto& coefficient : harmonic.m_evaluatorState.m_coefficients)
    {
        coefficient.store(0.0f);
    }

    harmonic.m_voiceChooserState[0].m_strategy.store(Strategy::None);
    auto& arp = state.m_indexArpUIState.m_arpUIState[0];
    arp.m_min.store(0.25f);
    arp.m_max.store(2.25f);
    juce::Image image(juce::Image::ARGB, 340, 500, true);
    juce::Graphics graphics(image);
    rig.m_visualizer.Draw(graphics, image.getBounds());

    // The pitch plot spans y=8..404: whole octaves fall at y=255.5 and y=57.5.
    //
    DOCTEST_CHECK(image.getPixelAt(200, 255) != image.getPixelAt(200, 259));
    DOCTEST_CHECK(image.getPixelAt(200, 57) != image.getPixelAt(200, 61));
    DOCTEST_CHECK(image.getPixelAt(200, 206) == image.getPixelAt(200, 210));
}

DOCTEST_TEST_CASE("Melody: motive identity follows held notes across rests and signed reset boundaries")
{
    MelodyRig rig;
    auto& state = rig.m_ui->m_nonagonUIState;
    auto& arp = state.m_indexArpUIState.m_arpUIState[0];
    arp.m_rhythmLength.store(4);
    for (size_t slot = 0; slot < 4; ++slot)
    {
        arp.m_rhythm[slot].store(slot % 2 == 1);
    }

    state.Snapshot();
    auto MotiveAt = [&](int64_t tick)
    {
        return rig.m_visualizer.GetMotive(state.GetVoicePoint(tick, 0), 0);
    };

    DOCTEST_CHECK(MotiveAt(3) == MotiveAt(4));
    DOCTEST_CHECK(MotiveAt(4) != MotiveAt(5));
    DOCTEST_CHECK(MotiveAt(31) == MotiveAt(32));
    DOCTEST_CHECK(MotiveAt(32) != MotiveAt(33));
    DOCTEST_CHECK(MotiveAt(-1) == MotiveAt(0));
    DOCTEST_CHECK(MotiveAt(0) != MotiveAt(1));

    state.m_indexArpUIState.m_resetSelect[0].store(1);
    arp.m_rhythm[0].store(true);
    state.Snapshot();
    DOCTEST_CHECK(MotiveAt(0) == MotiveAt(1));
    DOCTEST_CHECK(MotiveAt(1) != MotiveAt(2));
}

DOCTEST_TEST_CASE("Melody: playhead follows published continuous phase within ticks and across loop boundaries")
{
    MelodyRig rig;
    rig.m_voiceOffset = 0;
    auto& time = rig.m_ui->m_nonagonUIState.m_theoryOfTimeUIState;
    Visualizer visualizer(rig.m_ui.get(), &rig.m_voiceOffset, &time.m_globalPhase);
    TheoryOfTimeBase clock;
    TheoryOfTimeBase::Input input;
    input.m_running = true;
    input.m_phaseOffset = 0.125;

    struct Example
    {
        double m_phase;
        int m_pixelX;
    };

    const Example examples[] =
    {
        {9.5 / 32.0, 104},
        {9.75 / 32.0, 106},
        {-0.25 / 32.0, 329},
        {31.75 / 32.0, 329},
        {32.25 / 32.0, 10},
        {(4294967296.0 + 9.5) / 32.0, 104}
    };
    for (const auto& example : examples)
    {
        DOCTEST_CAPTURE(example.m_phase);
        input.m_unmodulatedPhase = example.m_phase - input.m_phaseOffset;
        for (size_t sample = 1; sample <= TheoryOfTimeBase::x_microBlockSize; ++sample)
        {
            clock.Process(sample, input);
        }

        clock.RolloverMicroblockBuffer();
        clock.PopulateUIState(time, input);
        juce::Image image(juce::Image::ARGB, 340, 500, true);
        juce::Graphics graphics(image);
        visualizer.Draw(graphics, image.getBounds());

        DOCTEST_REQUIRE(time.GetGlobalPeriodTicks() == 32);
        auto pixel = image.getPixelAt(example.m_pixelX, 100);
        DOCTEST_CHECK(pixel.getFloatRed() > 0.2f);
        DOCTEST_CHECK(pixel.getRed() == pixel.getGreen());
        DOCTEST_CHECK(pixel.getGreen() == pixel.getBlue());
    }
}

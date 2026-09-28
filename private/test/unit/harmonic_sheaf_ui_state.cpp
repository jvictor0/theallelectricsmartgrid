#include "doctest.h"

#include "HarmonicSheaf.hpp"

namespace
{
struct HarmonicSnapshotRig
{
    HarmonicSheaf::Sheaf m_sheaf;
    HarmonicSheaf::Evaluator m_evaluator;
    HarmonicSheaf::UIState m_ui;

    HarmonicSnapshotRig()
    {
        m_evaluator.m_coefficients[0] = 0.5f;
        m_evaluator.m_coefficients[1] = 0.25f;
        for (size_t slice = 0; slice < HarmonicSheaf::x_numBasePoints; ++slice)
        {
            auto& section = m_sheaf.m_sections[slice];
            section.m_high[0] = static_cast<uint8_t>(slice & 1);
            section.m_high[1] = static_cast<uint8_t>((slice >> 1) & 1);
            section.m_total[0] = 1;
            section.m_total[1] = 1;
        }

        for (auto& voice : m_ui.m_voiceChooserState)
        {
            voice.m_lens.store(HarmonicSheaf::Lens(0x3e));
            voice.m_baseStrategy.store(HarmonicSheaf::SectionChoiceStrategy::Lowest);
            voice.m_strategy.store(HarmonicSheaf::SectionChoiceStrategy::Closest);
        }

        Publish();
        m_ui.Snapshot();
    }

    void Publish()
    {
        m_sheaf.PopulateUIState(&m_ui.m_sectionState);
        m_evaluator.PopulateUIState(&m_ui.m_evaluatorState);
    }
};
}

DOCTEST_TEST_CASE("SequencerUI: harmonic snapshot composes base strategy with each voice lens")
{
    HarmonicSnapshotRig rig;
    auto& fixed = rig.m_ui.m_voiceChooserState[3];
    fixed.m_lens.store(HarmonicSheaf::Lens(0x3f));
    fixed.m_baseStrategy.store(HarmonicSheaf::SectionChoiceStrategy::None);
    auto& percentile = rig.m_ui.m_voiceChooserState[8];
    percentile.m_lens.store(HarmonicSheaf::Lens(0x3d));
    percentile.m_baseStrategy.store(HarmonicSheaf::SectionChoiceStrategy::None);
    percentile.m_strategy.store(HarmonicSheaf::SectionChoiceStrategy::Percentile);
    DOCTEST_CHECK(rig.m_ui.Changed());
    rig.m_ui.Snapshot();

    auto composed = rig.m_ui.Choose(0, HarmonicSheaf::BitVector(2), 0.4f);
    DOCTEST_CHECK(composed.m_value == doctest::Approx(0.75f));
    DOCTEST_CHECK(composed.m_section.m_high[0] == 1);
    DOCTEST_CHECK(composed.m_section.m_high[1] == 1);
    DOCTEST_CHECK(rig.m_ui.Choose(3, HarmonicSheaf::BitVector(2), 0.9f).m_value == doctest::Approx(0.25f));
    DOCTEST_CHECK(rig.m_ui.Choose(8, HarmonicSheaf::BitVector(1), 0.75f).m_value == doctest::Approx(0.75f));
    DOCTEST_CHECK_FALSE(rig.m_ui.Changed());
}

DOCTEST_TEST_CASE("SequencerUI: harmonic pitch and timbre stay frozen until the next snapshot")
{
    HarmonicSnapshotRig rig;
    auto before = rig.m_ui.Choose(0, HarmonicSheaf::BitVector(2), 0.4f);
    DOCTEST_REQUIRE(before.m_value == doctest::Approx(0.75f));
    DOCTEST_REQUIRE(before.m_section.Timbre(0) == doctest::Approx(1.0f));

    rig.m_sheaf.m_sections[3].m_total[0] = 2;
    rig.Publish();
    DOCTEST_CHECK(rig.m_ui.Changed());
    DOCTEST_CHECK(rig.m_ui.Choose(0, HarmonicSheaf::BitVector(2), 0.4f).m_section.Timbre(0) == doctest::Approx(1.0f));
    rig.m_ui.Snapshot();
    auto after = rig.m_ui.Choose(0, HarmonicSheaf::BitVector(2), 0.4f);
    DOCTEST_CHECK(after.m_value == doctest::Approx(0.75f));
    DOCTEST_CHECK(after.m_section.Timbre(0) == doctest::Approx(0.5f));
    DOCTEST_CHECK_FALSE(rig.m_ui.Changed());

    rig.m_evaluator.m_coefficients[1] = 1.0f;
    rig.Publish();
    DOCTEST_CHECK(rig.m_ui.Changed());
    DOCTEST_CHECK(rig.m_ui.Choose(0, HarmonicSheaf::BitVector(2), 0.4f).m_value == doctest::Approx(0.75f));
    rig.m_ui.Snapshot();
    DOCTEST_CHECK(rig.m_ui.Choose(0, HarmonicSheaf::BitVector(2), 0.4f).m_value == doctest::Approx(1.5f));
    DOCTEST_CHECK_FALSE(rig.m_ui.Changed());
}

DOCTEST_TEST_CASE("SequencerUI: changing a chooser affects only its voice after refresh")
{
    HarmonicSnapshotRig rig;
    auto& voice = rig.m_ui.m_voiceChooserState[7];
    voice.m_baseStrategy.store(HarmonicSheaf::SectionChoiceStrategy::None);
    DOCTEST_CHECK(rig.m_ui.Changed());
    DOCTEST_CHECK(rig.m_ui.Choose(7, HarmonicSheaf::BitVector(2), 0.4f).m_value == doctest::Approx(0.75f));
    rig.m_ui.Snapshot();
    DOCTEST_CHECK(rig.m_ui.Choose(7, HarmonicSheaf::BitVector(2), 0.4f).m_value == doctest::Approx(0.25f));
    DOCTEST_CHECK(rig.m_ui.Choose(6, HarmonicSheaf::BitVector(2), 0.4f).m_value == doctest::Approx(0.75f));

    voice.m_strategy.store(HarmonicSheaf::SectionChoiceStrategy::Percentile);
    DOCTEST_CHECK(rig.m_ui.Changed());
    rig.m_ui.Snapshot();
    DOCTEST_CHECK(rig.m_ui.Choose(7, HarmonicSheaf::BitVector(2), 0.6f).m_value == doctest::Approx(0.75f));
    voice.m_lens.store(HarmonicSheaf::Lens(0x3f));
    DOCTEST_CHECK(rig.m_ui.Changed());
    rig.m_ui.Snapshot();
    DOCTEST_CHECK(rig.m_ui.Choose(7, HarmonicSheaf::BitVector(2), 0.6f).m_value == doctest::Approx(0.25f));
}

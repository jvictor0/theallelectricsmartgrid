#include "doctest.h"
#include "../support/SynthRig.hpp"

namespace
{
using Encoders = SmartGridOneEncoders;
using Mode = Encoders::BankMode;
using Glyph = SmartGridOne::ModulationGlyphs;
}

DOCTEST_TEST_CASE("Encoder UI weights: publish current direct per-channel depths without source amplitude or transitive routes")
{
    synthrig::SynthRig rig;
    auto& internal = rig.Internal();
    auto& encoders = internal.m_squiggleBoy.m_encoders;
    encoders.SetTrack(2);
    encoders.SelectBank(Encoders::Bank::Source);
    auto& bank = encoders.m_encoderBankBank.m_banks[static_cast<size_t>(encoders.m_selectedBank)];
    auto* quad = encoders.m_encoderBankBank.GetEncoder(encoders.EncoderIndex(Encoders::Param::DelayTime, 0));
    bank.PlaceEncoder(2, 3, quad);
    quad->m_modulators.FillModulators(quad, quad->m_context);
    auto* depth = quad->m_modulators.m_modulators[4].get();
    depth->SetValue(0.75f, true);
    depth->m_modulators.FillModulators(depth, depth->m_context);
    depth->m_modulators.m_modulators[6]->SetValue(1.0f, true);
    auto* sources = quad->GetSharedEncoderState()->m_modulatorValues;
    for (size_t channel = 0; channel < 4; ++channel)
    {
        sources->m_value[6][channel] = 0.125f + 0.25f * channel;
        sources->m_amplitude[4][channel] = 0.01f;
    }

    quad->SetStateRecursive();
    quad->SetModulatorsAffecting();
    quad->Compute();
    internal.PopulateUIState();
    auto& state = rig.UIState().m_squiggleBoyUIState.m_encoderBankUIState.m_states[2][3];
    EncoderUIState::ModulationWeights snapshot;
    DOCTEST_REQUIRE(state.ReadModulationWeights(snapshot));
    DOCTEST_CHECK(snapshot.m_mode == Mode::Quad);
    DOCTEST_CHECK(snapshot.m_numVoices == 4);
    DOCTEST_CHECK(state.m_modulatorsAffecting.load().Get(6));
    for (size_t channel = 0; channel < 4; ++channel)
    {
        float bipolarDepth = -0.75f + 0.5f * channel;
        float expected = SmartGrid::BankedEncoderCell::ModulationDepthFromValue(bipolarDepth);
        DOCTEST_CHECK(snapshot.m_weights[channel][4] == doctest::Approx(expected));
        DOCTEST_CHECK(snapshot.m_weights[channel][6] == 0.0f);
    }

    state.m_weightsRevision.fetch_add(1);
    DOCTEST_CHECK_FALSE(state.ReadModulationWeights(snapshot));
    state.m_weightsRevision.fetch_add(1);
    bank.PlaceEncoder(2, 3, bank.GetBase(0, 0));
    internal.PopulateUIState();
    DOCTEST_REQUIRE(state.ReadModulationWeights(snapshot));
    DOCTEST_CHECK(snapshot.m_mode == Mode::VoiceEarth);
    DOCTEST_CHECK(snapshot.m_numVoices == 3);
    DOCTEST_CHECK(snapshot.m_weights[3][4] == 0.0f);
    bank.PlaceEncoder(2, 3, nullptr);
    internal.PopulateUIState();
    DOCTEST_REQUIRE(state.ReadModulationWeights(snapshot));
    DOCTEST_CHECK(snapshot.m_numVoices == 0);
    for (const auto& voice : snapshot.m_weights)
    {
        for (float weight : voice)
        {
            DOCTEST_CHECK(weight == 0.0f);
        }
    }
}

DOCTEST_TEST_CASE("Encoder UI scope: mixed controls publish independent channels glyphs and colors across trios")
{
    synthrig::SynthRig rig;
    auto& internal = rig.Internal();
    auto& encoders = internal.m_squiggleBoy.m_encoders;
    auto& ui = rig.UIState().m_squiggleBoyUIState.m_encoderBankUIState;
    auto* quad = encoders.m_encoderBankBank.GetEncoder(encoders.EncoderIndex(Encoders::Param::DelayTime, 0));
    auto* global = encoders.m_encoderBankBank.GetEncoder(encoders.EncoderIndex(Encoders::Param::Tempo, 0));
    quad->m_output[3] = 0.84f;
    quad->m_minValue[3] = 0.72f;
    quad->m_maxValue[3] = 0.91f;

    for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
    {
        DOCTEST_CAPTURE(trio);
        encoders.SetTrack(trio);
        encoders.SelectBank(Encoders::Bank::Source);
        auto& bank = encoders.m_encoderBankBank.m_banks[static_cast<size_t>(encoders.m_selectedBank)];
        bank.PlaceEncoder(2, 3, quad);
        bank.PlaceEncoder(3, 3, global);
        internal.PopulateUIState();

        DOCTEST_CHECK(ui.GetMode(0, 0) == Encoders::VoiceModeForTrio(trio));
        DOCTEST_CHECK(ui.GetMode(2, 3) == Mode::Quad);
        DOCTEST_CHECK(ui.GetMode(3, 3) == Mode::Global);
        DOCTEST_CHECK(ui.GetNumVoices(0, 0) == 3);
        DOCTEST_CHECK(ui.GetNumVoices(2, 3) == 4);
        DOCTEST_CHECK(ui.GetNumVoices(3, 3) == 1);
        DOCTEST_CHECK(ui.GetValue(2, 3, 3) == doctest::Approx(0.84f));
        DOCTEST_CHECK(ui.GetMinValue(2, 3, 3) == doctest::Approx(0.72f));
        DOCTEST_CHECK(ui.GetMaxValue(2, 3, 3) == doctest::Approx(0.91f));
        DOCTEST_CHECK(ui.GetModulationGlyph(0, 0, 4) == Glyph::ADSR);
        DOCTEST_CHECK(ui.GetModulationGlyph(2, 3, 4) == Glyph::Quadrature);
        DOCTEST_CHECK(ui.GetModulationGlyph(3, 3, 4) == Glyph::None);
        DOCTEST_CHECK(ui.GetModulationGlyphColor(0, 0, 4) == SmartGrid::Color::Orange);
        DOCTEST_CHECK(ui.GetModulationGlyphColor(2, 3, 4) == SmartGrid::Color::Orange);
        DOCTEST_CHECK(ui.GetModulationGlyphColor(3, 3, 4) == SmartGrid::Color::Off);
        DOCTEST_CHECK(ui.GetMainIndicatorColor(0, 0) ==
            TheNonagonSmartGrid::TrioColor(static_cast<TheNonagonSmartGrid::Trio>(trio)));
        DOCTEST_CHECK(ui.GetMainIndicatorColor(2, 3) == SmartGrid::Color::White);
        DOCTEST_CHECK(ui.GetMainIndicatorColor(3, 3) == SmartGrid::Color::White);
        for (size_t channel = 0; channel < 3; ++channel)
        {
            DOCTEST_CHECK(ui.GetIndicatorColor(0, 0, channel) == TheNonagonSmartGrid::VoiceColor(trio * 3 + channel));
        }

        for (size_t channel = 0; channel < 4; ++channel)
        {
            DOCTEST_CHECK(ui.GetIndicatorColor(2, 3, channel) == SmartGrid::Color::White);
        }

        DOCTEST_CHECK(ui.GetIndicatorColor(3, 3, 0) == SmartGrid::Color::White);
        DOCTEST_CHECK(ui.GetIndicatorColor(3, 3, 1) == SmartGrid::Color::Off);
    }

    auto& bank = encoders.m_encoderBankBank.m_banks[static_cast<size_t>(encoders.m_selectedBank)];
    bank.PlaceEncoder(2, 3, bank.GetBase(0, 0));
    internal.PopulateUIState();
    DOCTEST_CHECK(ui.GetNumVoices(2, 3) == 3);
    DOCTEST_CHECK(ui.GetIndicatorColor(2, 3, 3) == SmartGrid::Color::Off);

    bank.PlaceEncoder(2, 3, nullptr);
    internal.PopulateUIState();
    DOCTEST_CHECK_FALSE(ui.GetConnected(2, 3));
    DOCTEST_CHECK(ui.GetMode(2, 3) == Mode::NumModes);
    DOCTEST_CHECK(ui.GetNumVoices(2, 3) == 0);
    DOCTEST_CHECK(ui.GetModulationGlyph(2, 3, 4) == Glyph::None);
    DOCTEST_CHECK(ui.GetModulationGlyphColor(2, 3, 4) == SmartGrid::Color::Off);
    DOCTEST_CHECK(ui.GetMainIndicatorColor(2, 3) == SmartGrid::Color::Off);
}

DOCTEST_TEST_CASE("Encoder UI scope: modulation and gesture descendants retain the parameter scope in a voice bank")
{
    synthrig::SynthRig rig;
    auto& internal = rig.Internal();
    auto& encoders = internal.m_squiggleBoy.m_encoders;
    auto& ui = rig.UIState().m_squiggleBoyUIState.m_encoderBankUIState;
    encoders.SetTrack(1);
    encoders.SelectBank(Encoders::Bank::Source);
    auto& bank = encoders.m_encoderBankBank.m_banks[static_cast<size_t>(encoders.m_selectedBank)];
    auto* quad = encoders.m_encoderBankBank.GetEncoder(encoders.EncoderIndex(Encoders::Param::DelayTime, 0));
    bank.PlaceEncoder(2, 3, quad);
    bank.MakeSelection(2, 3, quad);
    internal.PopulateUIState();
    DOCTEST_CHECK(ui.GetMode(3, 3) == Mode::Quad);
    DOCTEST_CHECK(ui.GetMode(0, 1) == Mode::Quad);
    DOCTEST_CHECK(ui.GetNumVoices(0, 1) == 4);
    DOCTEST_CHECK(ui.GetModulationGlyph(0, 1, 4) == Glyph::Quadrature);
    DOCTEST_CHECK(std::string(ui.GetShortName(0, 1)) == "DLFO");
    DOCTEST_CHECK(ui.GetMainIndicatorColor(0, 1) == SmartGrid::Color::White);

    auto* depth = quad->GetModulator(4);
    bank.MakeSelection(0, 1, depth);
    internal.PopulateUIState();
    DOCTEST_CHECK(ui.GetMode(3, 3) == Mode::Quad);
    DOCTEST_CHECK(ui.GetMode(0, 1) == Mode::Quad);
    DOCTEST_CHECK(ui.GetNumVoices(0, 1) == 4);
    DOCTEST_CHECK(ui.GetModulationGlyph(0, 1, 4) == Glyph::Quadrature);

    quad->m_modulators.AddGesture(quad, 2);
    bank.MakeSelection(0, 0, quad->m_modulators.m_gestures[2].get());
    internal.PopulateUIState();
    DOCTEST_CHECK(ui.GetEncoderType(3, 3) == SmartGrid::EncoderType::GestureParam);
    DOCTEST_CHECK(ui.GetMode(3, 3) == Mode::Quad);
    DOCTEST_CHECK(ui.GetNumVoices(3, 3) == 4);
    DOCTEST_CHECK(ui.GetMode(0, 1) == Mode::Quad);

    bank.Deselect();
    internal.PopulateUIState();
    DOCTEST_CHECK(ui.GetMode(0, 0) == Mode::VoiceFire);
    DOCTEST_CHECK(ui.GetNumVoices(0, 0) == 3);
    DOCTEST_CHECK(ui.GetMode(2, 3) == Mode::Quad);
    DOCTEST_CHECK(ui.GetNumVoices(2, 3) == 4);
}

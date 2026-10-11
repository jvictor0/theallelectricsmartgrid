#include "doctest.h"
#include "../support/GlobalEnv.hpp"
#include "SmartGridOneEncoders.hpp"
#include "../support/SynthRig.hpp"

namespace
{
using Encoders = SmartGridOneEncoders;
using Glyph = SmartGridOne::ModulationGlyphs;
struct ExpectedSkin
{
    Glyph m_glyph;
    SmartGrid::Color m_color;
};

const ExpectedSkin x_voice[] =
{
    {Glyph::SmoothRandom, {255, 30, 18}}, {Glyph::SmoothRandom, {134, 63, 249}},
    {Glyph::SmoothRandom, {255, 248, 63}}, {Glyph::SmoothRandom, {255, 71, 250}},
    {Glyph::ADSR, {255, 108, 29}}, {Glyph::ADSR, {0, 247, 167}},
    {Glyph::LFO, {255, 50, 120}}, {Glyph::LFO, {127, 84, 139}},
    {Glyph::Sheaf, {0, 127, 127}}, {Glyph::Sheaf, {28, 30, 124}},
    {Glyph::Sheaf, {4, 123, 29}}, {Glyph::Spread, {143, 143, 143}},
    {Glyph::None, {0, 0, 0}}, {Glyph::None, {0, 0, 0}}, {Glyph::Noise, {253, 253, 253}}
};
const ExpectedSkin x_quad[] =
{
    {Glyph::SmoothRandom, {255, 30, 18}}, {Glyph::SmoothRandom, {134, 63, 249}},
    {Glyph::SmoothRandom, {255, 248, 63}}, {Glyph::SmoothRandom, {255, 71, 250}},
    {Glyph::Quadrature, {255, 108, 29}}, {Glyph::Quadrature, {0, 247, 167}},
    {Glyph::LFO, {255, 50, 120}}, {Glyph::LFO, {127, 84, 139}},
    {Glyph::None, {0, 0, 0}}, {Glyph::None, {0, 0, 0}}, {Glyph::None, {0, 0, 0}},
    {Glyph::Spread, {143, 143, 143}}, {Glyph::None, {0, 0, 0}},
    {Glyph::None, {0, 0, 0}}, {Glyph::Noise, {253, 253, 253}}
};
const ExpectedSkin x_global[] =
{
    {Glyph::None, {0, 0, 0}}, {Glyph::None, {0, 0, 0}},
    {Glyph::SmoothRandom, {255, 248, 63}}, {Glyph::SmoothRandom, {255, 71, 250}},
    {Glyph::None, {0, 0, 0}}, {Glyph::None, {0, 0, 0}}, {Glyph::None, {0, 0, 0}},
    {Glyph::None, {0, 0, 0}}, {Glyph::None, {0, 0, 0}}, {Glyph::None, {0, 0, 0}},
    {Glyph::None, {0, 0, 0}}, {Glyph::None, {0, 0, 0}}, {Glyph::None, {0, 0, 0}},
    {Glyph::None, {0, 0, 0}}, {Glyph::Noise, {253, 253, 253}}
};
}

DOCTEST_TEST_CASE("modulator catalogue: every mode presents preserved sources and empty slots")
{
    const ExpectedSkin* expected[] = {x_voice, x_voice, x_voice, x_quad, x_global};
    for (size_t mode = 0; mode < 5; ++mode)
    {
        for (size_t index = 0; index < 15; ++index)
        {
            DOCTEST_CAPTURE(mode);
            DOCTEST_CAPTURE(index);
            auto skin = Encoders::GetModulatorSkin(index, static_cast<Encoders::BankMode>(mode));
            DOCTEST_CHECK(skin.m_glyph == expected[mode][index].m_glyph);
            DOCTEST_CHECK(skin.m_color == expected[mode][index].m_color);
        }
    }
}

DOCTEST_TEST_CASE("modulator catalogue: legacy global empty depths survive patch load and UI processing")
{
    for (size_t index : {6, 7, 11})
    {
        GlobalEnv::ResetPerTest();
        SmartGridOneContext context;
        Encoders encoders(&context);
        encoders.SelectBank(Encoders::Bank::Mastering);
        auto& bank = encoders.m_encoderBankBank.m_banks[static_cast<size_t>(Encoders::Bank::Mastering)];
        auto* cell = bank.GetBase(3, 0);
        DOCTEST_REQUIRE(cell != nullptr);
        cell->SetValue(0.2f, true);
        cell->FillModulators(&context);
        cell->m_modulators.m_modulators[index].get()->SetValue(1.0f, true);
        JsonArena arena(JsonArena::kDefaultCapacity);
        JSON saved = encoders.ToJSON(arena);
        encoders.RevertToDefault(true);
        encoders.FromJSON(saved);
        DOCTEST_REQUIRE(cell->m_modulators.m_modulators[index].get() != nullptr);
        DOCTEST_CHECK(cell->m_modulators.m_modulators[index].get()->m_values[0] == 1.0f);
        bank.MakeSelection(3, 0, cell);
        auto* depth = cell->m_modulators.m_modulators[index].get();
        DOCTEST_CHECK(bank.GetVisible(index % 4, index / 4) == depth);
        DOCTEST_CHECK(depth->m_ownerBank == &bank);
        auto& sources = encoders.GetModulatorValues(Encoders::BankMode::Global);
        sources.m_value[index][0] = 0.8f;
        cell->SetStateRecursive();
        cell->SetModulatorsAffecting();
        cell->Compute();
        encoders.Process();
        EncoderBankUIState ui;
        encoders.PopulateUIState(&ui);
        DOCTEST_CHECK(ui.GetConnected(index % 4, index / 4));
        DOCTEST_CHECK(ui.GetModulationGlyph(index) == Glyph::None);
        DOCTEST_CHECK(ui.GetModulationGlyphColor(index) == SmartGrid::Color(0, 0, 0));
        DOCTEST_CHECK(std::isfinite(cell->GetValueNoSlew(0)));
        DOCTEST_CHECK(cell->GetValueNoSlew(0) == doctest::Approx(0.8f));
        DOCTEST_CHECK(bank.GetVisible(index % 4, index / 4) == depth);
        auto skin = Encoders::GetModulatorSkin(index, Encoders::BankMode::Global);
        DOCTEST_CHECK(skin.m_color == SmartGrid::Color(0, 0, 0));
        DOCTEST_CHECK(skin.m_glyph == Glyph::None);
    }
}

DOCTEST_TEST_CASE("modulator catalogue: DSP sources retain numeric destinations and amplitudes")
{
    synthrig::SynthRig rig;
    auto& synth = rig.Internal().m_squiggleBoy;
    SquiggleBoyWithEncoderBank::Input input;
    for (size_t voice = 0; voice < 9; ++voice)
    {
        auto& source = synth.m_voices[voice];
        const float offset = 0.01f * static_cast<float>(voice);
        source.m_amp.m_modulationAHD.m_rawOutput = 0.11f + offset;
        source.m_amp.m_ahd.m_rawOutput = 0.22f + offset;
        source.m_amp.m_modulationAHD.m_amplitude = 0.33f + offset;
        source.m_amp.m_ahd.m_amplitude = 0.44f + offset;
        for (size_t lfo = 0; lfo < 2; ++lfo)
        {
            source.m_squiggleLFO[lfo].m_output = 0.51f + 0.1f * lfo + offset;
            source.m_squiggleLFO[lfo].m_polyXFader.m_amplitude = 0.71f + 0.1f * lfo + offset;
        }

        for (size_t sheaf = 0; sheaf < 3; ++sheaf)
        {
            input.m_sheafyModulators[voice][sheaf] = 0.12f + 0.1f * sheaf + offset;
        }

        for (size_t random = 0; random < 4; ++random)
        {
            synth.m_gangedRandomLFO[random][voice / 3].m_voices[voice % 3].m_output = 0.13f + 0.1f * random + offset;
        }
    }

    synth.SetVoiceModulators(input);
    for (size_t voice = 0; voice < 9; ++voice)
    {
        const auto& values = synth.m_encoders.GetModulatorValues(Encoders::VoiceModeForTrio(voice / 3));
        const size_t channel = voice % 3;
        const float offset = 0.01f * static_cast<float>(voice);
        DOCTEST_CHECK(values.m_value[4][channel] == doctest::Approx(0.11f + offset));
        DOCTEST_CHECK(values.m_value[5][channel] == doctest::Approx(0.22f + offset));
        DOCTEST_CHECK(values.m_amplitude[4][channel] == doctest::Approx(0.33f + offset));
        DOCTEST_CHECK(values.m_amplitude[5][channel] == doctest::Approx(0.44f + offset));
        for (size_t lfo = 0; lfo < 2; ++lfo)
        {
            DOCTEST_CHECK(values.m_value[6 + lfo][channel] == doctest::Approx(0.51f + 0.1f * lfo + offset));
            DOCTEST_CHECK(values.m_amplitude[6 + lfo][channel] == doctest::Approx(0.71f + 0.1f * lfo + offset));
        }

        for (size_t sheaf = 0; sheaf < 3; ++sheaf)
        {
            DOCTEST_CHECK(values.m_value[8 + sheaf][channel] == doctest::Approx(0.12f + 0.1f * sheaf + offset));
        }

        for (size_t random = 0; random < 4; ++random)
        {
            DOCTEST_CHECK(values.m_value[random][channel] == doctest::Approx(0.13f + 0.1f * random + offset));
        }

        DOCTEST_CHECK(values.m_value[11][channel] == doctest::Approx(0.5f * channel));
    }

    for (size_t channel = 0; channel < 4; ++channel)
    {
        synth.m_delay.m_lfo.m_output[channel] = -0.8f + 0.2f * channel;
        synth.m_reverb.m_lfo.m_output[channel] = 0.2f + 0.2f * channel;
        for (size_t lfo = 0; lfo < 2; ++lfo)
        {
            synth.m_quadSquiggleLFO[lfo][channel].m_output = 0.15f + 0.1f * channel + 0.2f * lfo;
            synth.m_quadSquiggleLFO[lfo][channel].m_polyXFader.m_amplitude = 0.25f + 0.1f * channel + 0.2f * lfo;
        }
    }

    synth.SetQuadModulators(input);
    const auto& quad = synth.m_encoders.GetModulatorValues(Encoders::BankMode::Quad);
    for (size_t channel = 0; channel < 4; ++channel)
    {
        DOCTEST_CHECK(quad.m_value[4][channel] == doctest::Approx(0.1f + 0.1f * channel));
        DOCTEST_CHECK(quad.m_value[5][channel] == doctest::Approx(0.6f + 0.1f * channel));
        for (size_t lfo = 0; lfo < 2; ++lfo)
        {
            DOCTEST_CHECK(quad.m_value[6 + lfo][channel] == doctest::Approx(0.15f + 0.1f * channel + 0.2f * lfo));
            DOCTEST_CHECK(quad.m_amplitude[6 + lfo][channel] == doctest::Approx(0.25f + 0.1f * channel + 0.2f * lfo));
        }

        DOCTEST_CHECK(quad.m_value[11][channel] == doctest::Approx(static_cast<float>(channel) / 3));
    }

    synth.m_globalGangedRandomLFO[0].m_voices[0].m_output = 0.23f;
    synth.m_globalGangedRandomLFO[1].m_voices[0].m_output = 0.67f;
    synth.SetGlobalModulators(input);
    const auto& global = synth.m_encoders.GetModulatorValues(Encoders::BankMode::Global);
    DOCTEST_CHECK(global.m_value[2][0] == doctest::Approx(0.23f));
    DOCTEST_CHECK(global.m_value[3][0] == doctest::Approx(0.67f));
}

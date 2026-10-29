#include "doctest.h"
#include "../support/SynthRig.hpp"

namespace
{
using Encoders = SmartGridOneEncoders;
using Bank = Encoders::Bank;
using Param = Encoders::Param;

SmartGrid::EncoderBankInternal& GetBank(Encoders& encoders, Bank bank, size_t trio)
{
    return encoders.m_encoderBankBank.m_banks[static_cast<size_t>(Encoders::BankForTrio(bank, trio))];
}

void AddGesture(SmartGrid::BankedEncoderCell* cell, int gesture)
{
    cell->m_modulators.AddGesture(cell, gesture);
    cell->m_modulators.m_gestures[gesture]->SetActive(true, 0);
    cell->m_modulators.m_gestures[gesture]->SetValue(0.8f, true);
    cell->SetModulatorsAffectingRecursive();
}
}

DOCTEST_TEST_CASE("FX Matrix: voice edits follow the trio while shared controls keep their value and UI scope")
{
    synthrig::SynthRig rig;
    auto& internal = rig.Internal();
    auto& encoders = internal.m_squiggleBoy.m_encoders;
    auto& ui = rig.UIState().m_squiggleBoyUIState.m_encoderBankUIState;
    auto* delay = GetBank(encoders, Bank::Delay, 0).GetBase(3, 3);

    for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
    {
        encoders.SetTrack(trio);
        encoders.SelectBank(Bank::FXMatrix);
        auto& matrix = GetBank(encoders, Bank::FXMatrix, trio);
        DOCTEST_REQUIRE(matrix.GetBase(0, 1) == delay);
        DOCTEST_CHECK(matrix.GetBase(0, 0) == encoders.m_encoderBankBank.GetEncoder(
            encoders.EncoderIndex(Param::AmpGain, trio)));
        encoders.Apply(SmartGrid::MessageIn::EncoderSetMsg(0, 0, 0.2f * static_cast<float>(trio + 1)));
        encoders.Apply(SmartGrid::MessageIn::EncoderSetMsg(0, 1, 0.7f));
        delay->m_output[3] = 0.84f;
        internal.PopulateUIState();
        DOCTEST_CHECK(ui.GetMode(0, 0) == Encoders::VoiceModeForTrio(trio));
        DOCTEST_CHECK(ui.GetNumVoices(0, 0) == 3);
        DOCTEST_CHECK(ui.GetMode(0, 1) == Encoders::BankMode::Quad);
        DOCTEST_CHECK(ui.GetNumVoices(0, 1) == 4);
        DOCTEST_CHECK(ui.GetValue(0, 1, 3) == doctest::Approx(0.84f));
        DOCTEST_CHECK(ui.GetModulationGlyph(0, 0, 4) == SmartGridOne::ModulationGlyphs::ADSR);
        DOCTEST_CHECK(ui.GetModulationGlyph(0, 1, 4) == SmartGridOne::ModulationGlyphs::Quadrature);
        DOCTEST_CHECK(ui.GetMainIndicatorColor(0, 0) ==
            TheNonagonSmartGrid::TrioColor(static_cast<TheNonagonSmartGrid::Trio>(trio)));
        DOCTEST_CHECK(ui.GetMainIndicatorColor(0, 1) == SmartGrid::Color::White);
    }

    encoders.SelectBank(Bank::Delay);
    DOCTEST_CHECK(delay->m_values[0] == doctest::Approx(0.7f).epsilon(0.0001f));
    encoders.Apply(SmartGrid::MessageIn::EncoderSetMsg(3, 3, 0.4f));
    for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
    {
        encoders.SetTrack(trio);
        encoders.SelectBank(Bank::FXMatrix);
        auto& matrix = GetBank(encoders, Bank::FXMatrix, trio);
        DOCTEST_CHECK(matrix.GetBase(0, 1)->m_values[0] == doctest::Approx(0.4f).epsilon(0.0001f));
        DOCTEST_CHECK(matrix.GetBase(0, 0)->m_values[0] ==
            doctest::Approx(0.2f * static_cast<float>(trio + 1)).epsilon(0.0001f));
    }
}

DOCTEST_TEST_CASE("FX Matrix: nested gestures reload and reset consistently on every shared page")
{
    GlobalEnv::ResetPerTest();
    SmartGridOneContext context;
    Encoders encoders(&context);
    auto& delayBank = GetBank(encoders, Bank::Delay, 0);
    auto* delay = delayBank.GetBase(3, 3);
    delay->SetValue(0.35f, true);
    delayBank.MakeSelection(3, 3, delay);
    delayBank.Apply(SmartGrid::MessageIn::EncoderSetMsg(0, 1, 0.75f));
    auto* depth = delay->m_modulators.m_modulators[4].get();
    DOCTEST_REQUIRE(depth);
    AddGesture(depth, 2);
    delayBank.Deselect();

    JsonArena arena(JsonArena::kDefaultCapacity);
    JSON saved = encoders.ToJSON(arena);
    DOCTEST_CHECK_FALSE(saved.Get("DelayReturn").IsNull());
    DOCTEST_CHECK(saved.Get("DelayReturnWater").IsNull());
    DOCTEST_CHECK(saved.Get("DelayReturnFire").IsNull());
    DOCTEST_CHECK(saved.Get("DelayReturnEarth").IsNull());

    for (bool reload : {false, true})
    {
        if (reload)
        {
            encoders.RevertToDefault(true);
            encoders.FromJSON(saved);
        }

        DOCTEST_CHECK(delayBank.GetGesturesAffecting().Get(2));
        DOCTEST_CHECK(encoders.IsGestureAffecting(2));
        DOCTEST_CHECK(encoders.GetGestureColor(2) == SmartGrid::Color::Pink);
        for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
        {
            auto& matrix = GetBank(encoders, Bank::FXMatrix, trio);
            auto* alias = matrix.GetBase(0, 1);
            DOCTEST_REQUIRE(alias == delay);
            DOCTEST_CHECK(matrix.GetGesturesAffecting().Get(2));
            DOCTEST_CHECK(alias->m_values[0] == doctest::Approx(0.35f));
            DOCTEST_REQUIRE(alias->m_modulators.m_modulators[4]);
            auto* loadedDepth = alias->m_modulators.m_modulators[4].get();
            DOCTEST_CHECK(loadedDepth->m_values[0] == doctest::Approx(0.75f).epsilon(0.0001f));
            DOCTEST_REQUIRE(loadedDepth->m_modulators.m_gestures[2]);
            DOCTEST_CHECK(loadedDepth->m_modulators.m_gestures[2]->m_isActive[0]);
        }

        encoders.SetTrack(1);
        encoders.ResetBank(Bank::FXMatrix);
        DOCTEST_CHECK(delay->m_values[0] == delay->m_defaultValue);
        DOCTEST_CHECK_FALSE(delayBank.GetGesturesAffecting().Get(2));
        for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
        {
            DOCTEST_CHECK_FALSE(GetBank(encoders, Bank::FXMatrix, trio).GetGesturesAffecting().Get(2));
        }

        DOCTEST_CHECK_FALSE(encoders.IsGestureAffecting(2));
        DOCTEST_CHECK(encoders.GetGestureColor(2) == SmartGrid::Color::Off);
    }
}

DOCTEST_TEST_CASE("FX Matrix: gesture colors come from encoders across pages and trios")
{
    GlobalEnv::ResetPerTest();
    SmartGridOneContext context;
    Encoders encoders(&context);
    auto& matrix = GetBank(encoders, Bank::FXMatrix, 1);
    AddGesture(matrix.GetBase(2, 0), 3);
    DOCTEST_CHECK(encoders.GetGestureColor(3) == SmartGrid::Color::Purple);
    DOCTEST_CHECK(matrix.GetGesturesAffecting().Get(3));
    DOCTEST_CHECK_FALSE(GetBank(encoders, Bank::FXMatrix, 0).GetGesturesAffecting().Get(3));
    DOCTEST_CHECK_FALSE(GetBank(encoders, Bank::FXMatrix, 2).GetGesturesAffecting().Get(3));
    AddGesture(matrix.GetBase(2, 1), 3);
    DOCTEST_CHECK(encoders.GetGestureColor(3) == SmartGrid::Color::Purple);
    DOCTEST_CHECK(GetBank(encoders, Bank::Delay, 0).GetGesturesAffecting().Get(3));
    AddGesture(matrix.GetBase(0, 2), 3);
    DOCTEST_CHECK(encoders.GetGestureColor(3) == SmartGrid::Color::White);
    encoders.ClearGesture(3);
    DOCTEST_CHECK(encoders.GetGestureColor(3) == SmartGrid::Color::Off);
    DOCTEST_CHECK_FALSE(encoders.IsGestureAffecting(3));
    for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
    {
        DOCTEST_CHECK_FALSE(GetBank(encoders, Bank::FXMatrix, trio).GetGesturesAffecting().Get(3));
    }

    auto* hidden = encoders.m_encoderBankBank.GetEncoder(encoders.EncoderIndex(Param::SampleStart, 0));
    AddGesture(hidden, 4);
    DOCTEST_CHECK_FALSE(GetBank(encoders, Bank::Source, 0).GetGesturesAffecting().Get(4));
    DOCTEST_CHECK(encoders.IsGestureAffecting(4));
    DOCTEST_CHECK(encoders.GetGestureColor(4) == hidden->m_color);
    encoders.UpdateEncodersForMachine(VoiceMachine::SourceMachine::Sample, VoiceMachine::FilterMachine::Ladder4Pole, 0);
    DOCTEST_CHECK(GetBank(encoders, Bank::Source, 0).GetGesturesAffecting().Get(4));
    DOCTEST_CHECK(encoders.GetGestureColor(4) == hidden->m_color);
}

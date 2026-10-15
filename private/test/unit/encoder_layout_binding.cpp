#include "doctest.h"

#include "../support/GlobalEnv.hpp"
#include "SmartGridOneEncoders.hpp"

namespace
{
using Encoders = SmartGridOneEncoders;
using Source = VoiceMachine::SourceMachine;
using Filter = VoiceMachine::FilterMachine;
}

DOCTEST_TEST_CASE("encoder layout binding: changed pages close modulation while unchanged pages preserve it")
{
    GlobalEnv::ResetPerTest();
    SmartGridOneContext context;
    Encoders encoders(&context);
    encoders.UpdateEncodersForMachine(Source::DualWaveShapingVCO, Filter::Ladder4Pole);
    auto& source = encoders.m_encoderBankBank.m_banks[0];
    auto* carrier = source.GetBase(0, 0);
    source.MakeSelection(0, 0, carrier);
    encoders.UpdateEncodersForMachine(Source::DualWaveShapingVCO, Filter::SVF2Pole);
    DOCTEST_CHECK(source.m_selected == carrier);
    encoders.UpdateEncodersForMachine(Source::Sample, Filter::SVF2Pole);
    DOCTEST_CHECK(source.m_selected == nullptr);
    DOCTEST_CHECK(source.GetVisible(0, 0) == source.GetBase(0, 0));
    DOCTEST_CHECK(source.GetBase(0, 0) == encoders.m_encoderBankBank.GetEncoder(
        encoders.EncoderIndex(Encoders::Param::SampleLoopIndex, 0)));
}

DOCTEST_TEST_CASE("encoder layout binding: routes preserve owned hidden cells nested state and shared selection")
{
    GlobalEnv::ResetPerTest();
    SmartGridOneContext context;
    Encoders encoders(&context);
    SmartGrid::BankedEncoderCell* carriers[3];
    SmartGrid::BankedEncoderCell* nested[3];
    for (size_t trio = 0; trio < 3; ++trio)
    {
        auto* cell = encoders.m_encoderBankBank.GetEncoder(encoders.EncoderIndex(Encoders::Param::Harmonics1, trio));
        carriers[trio] = cell;
        cell->SetValue(0.2f * static_cast<float>(trio + 1), true);
        cell->FillModulators(&context);
        auto* modulator = cell->GetModulator(0);
        modulator->SetValue(0.6f, true);
        modulator->FillModulators(&context);
        nested[trio] = modulator->GetModulator(1);
        nested[trio]->SetValue(0.7f, true);
        cell->m_modulators.AddGesture(cell, 2);
        cell->m_modulators.m_gestures[2]->SetActive(true, 0);
        cell->m_modulators.m_gestures[2]->SetValue(0.8f, true);
        cell->SetModulatorsAffecting();
    }

    encoders.SelectBank(Encoders::Bank::Delay);
    auto& delay = encoders.m_encoderBankBank.m_banks[12];
    auto* delayCell = delay.GetBase(0, 0);
    delay.MakeSelection(0, 0, delayCell);
    for (size_t trio = 0; trio < 3; ++trio)
    {
        encoders.SetTrack(trio);
        for (auto machine : {Source::Thru, Source::PhysicalModeling, Source::Sample, Source::DualWaveShapingVCO})
        {
            encoders.UpdateEncodersForMachine(machine, Filter::Ladder4Pole);
            auto* cell = carriers[trio];
            DOCTEST_CHECK(cell == encoders.m_encoderBankBank.GetEncoder(encoders.EncoderIndex(Encoders::Param::Harmonics1, trio)));
            DOCTEST_CHECK(cell->m_values[0] == doctest::Approx(0.2f * static_cast<float>(trio + 1)));
            DOCTEST_CHECK(cell->m_modulators.m_modulators[0]->m_modulators.m_modulators[1].get() == nested[trio]);
            DOCTEST_CHECK(nested[trio]->m_values[0] == doctest::Approx(0.7f));
            DOCTEST_CHECK(cell->m_modulators.m_gestures[2]->m_values[0] == doctest::Approx(0.8f));
            DOCTEST_CHECK(cell->m_ownerBank == &encoders.m_encoderBankBank.m_banks[trio]);
            DOCTEST_CHECK(nested[trio]->m_ownerBank == cell->m_ownerBank);
            cell->SetModulatorsAffecting();
            DOCTEST_CHECK(encoders.m_selectedBank == Encoders::Bank::Delay);
            DOCTEST_CHECK(delay.m_selected == delayCell);
        }
    }
}

DOCTEST_TEST_CASE("encoder layout binding: initially hidden cells keep ownership processing and reset scope")
{
    GlobalEnv::ResetPerTest();
    SmartGridOneContext context;
    Encoders encoders(&context);
    for (size_t trio = 0; trio < 3; ++trio)
    {
        auto* hidden = encoders.m_encoderBankBank.GetEncoder(encoders.EncoderIndex(Encoders::Param::SampleStart, trio));
        DOCTEST_REQUIRE(hidden->m_ownerBank == &encoders.m_encoderBankBank.m_banks[trio]);
        for (int x = 0; x < 4; ++x)
        {
            for (int y = 0; y < 4; ++y)
            {
                DOCTEST_CHECK(hidden->m_ownerBank->GetVisible(x, y) != hidden);
            }
        }

        hidden->SetValue(0.37f, true);
        hidden->SetForceUpdateRecursive();
    }

    encoders.Process();
    for (size_t trio = 0; trio < 3; ++trio)
    {
        auto* hidden = encoders.m_encoderBankBank.GetEncoder(encoders.EncoderIndex(Encoders::Param::SampleStart, trio));
        DOCTEST_CHECK(encoders.GetValueNoSlew(Encoders::Param::SampleStart, static_cast<int>(trio * 3)) == doctest::Approx(0.37f));
        hidden->FillModulators(&context);
        auto* child = hidden->m_modulators.m_modulators[0].get();
        DOCTEST_REQUIRE(child->m_ownerBank == hidden->m_ownerBank);
        child->SetModulatorsAffectingRecursive();
        encoders.SetTrack(trio);
        encoders.ResetBank(Encoders::Bank::Source);
        DOCTEST_CHECK(hidden->m_values[0] == doctest::Approx(0.37f));
        encoders.UpdateEncodersForMachine(Source::Sample, Filter::Ladder4Pole);
        DOCTEST_CHECK(encoders.m_encoderBankBank.m_banks[trio].GetBase(1, 0) == hidden);
        encoders.ResetBank(Encoders::Bank::Source);
        DOCTEST_CHECK(hidden->m_values[0] == hidden->m_defaultValue);
    }
}

DOCTEST_TEST_CASE("encoder layout binding: moved controls survive machine changes and patch reloads")
{
    GlobalEnv::ResetPerTest();
    SmartGridOneContext context;
    Encoders encoders(&context);
    using Param = Encoders::Param;
    using Bank = Encoders::Bank;
    struct MovedControl
    {
        Param m_param;
        Bank m_bank;
        int m_x;
        int m_y;
    };

    const MovedControl x_controls[] =
    {
        {Param::FilterDrive, Bank::Shape, 2, 2},
        {Param::SampleRateReduction, Bank::Shape, 3, 2},
        {Param::SubOscillator, Bank::AmpAndRouting, 3, 1},
        {Param::AmpAHDAttack, Bank::AmpAndRouting, 0, 2},
        {Param::AmpAHDHold, Bank::AmpAndRouting, 1, 2},
        {Param::AmpAHDDecay, Bank::AmpAndRouting, 2, 2},
        {Param::AmpAmplitude, Bank::AmpAndRouting, 3, 2},
        {Param::AmpGain, Bank::AmpAndRouting, 0, 3},
        {Param::DelaySend, Bank::AmpAndRouting, 1, 3},
        {Param::ReverbSend, Bank::AmpAndRouting, 2, 3},
        {Param::PartialMachineSend, Bank::AmpAndRouting, 3, 3}
    };

    for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
    {
        for (const auto& control : x_controls)
        {
            auto* cell = encoders.m_encoderBankBank.GetEncoder(encoders.EncoderIndex(control.m_param, trio));
            cell->SetValue(0.2f * static_cast<float>(trio + 1), true);
            cell->FillModulators(&context);
            cell->GetModulator(0)->SetValue(0.6f, true);
            cell->m_modulators.AddGesture(cell, 2);
            cell->m_modulators.m_gestures[2]->SetActive(true, 0);
            cell->m_modulators.m_gestures[2]->SetValue(0.8f, true);
            cell->SetModulatorsAffecting();
        }
    }

    JsonArena arena(JsonArena::kDefaultCapacity);
    JSON saved = encoders.ToJSON(arena);
    for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
    {
        encoders.SetTrack(trio);
        for (const auto& control : x_controls)
        {
            auto* cell = encoders.m_encoderBankBank.GetEncoder(encoders.EncoderIndex(control.m_param, trio));
            encoders.SelectBank(control.m_bank);
            auto& bank = encoders.m_encoderBankBank.m_banks[static_cast<size_t>(encoders.m_selectedBank)];
            bank.MakeSelection(control.m_x, control.m_y, cell);
            for (auto source : {Source::Thru, Source::PhysicalModeling, Source::Sample, Source::DualWaveShapingVCO})
            {
                for (auto filter : {Filter::SVF2Pole, Filter::Ladder4Pole})
                {
                    encoders.UpdateEncodersForMachine(source, filter);
                    DOCTEST_CHECK(bank.GetBase(control.m_x, control.m_y) == cell);
                    DOCTEST_CHECK(bank.m_selected == cell);
                    DOCTEST_CHECK(cell->m_ownerBank == &bank);
                    DOCTEST_CHECK(cell->GetModulator(0)->m_ownerBank == &bank);
                    DOCTEST_CHECK(cell->m_values[0] == doctest::Approx(0.2f * static_cast<float>(trio + 1)));
                    DOCTEST_CHECK(cell->GetModulator(0)->m_values[0] == doctest::Approx(0.6f));
                    DOCTEST_CHECK(cell->m_modulators.m_gestures[2]->m_values[0] == doctest::Approx(0.8f));
                }
            }
        }
    }

    encoders.RevertToDefault(true);
    encoders.FromJSON(saved);
    for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
    {
        for (const auto& control : x_controls)
        {
            auto* cell = encoders.m_encoderBankBank.GetEncoder(encoders.EncoderIndex(control.m_param, trio));
            DOCTEST_CHECK(cell->m_values[0] == doctest::Approx(0.2f * static_cast<float>(trio + 1)));
            DOCTEST_REQUIRE(cell->m_modulators.m_modulators[0]);
            DOCTEST_CHECK(cell->GetModulator(0)->m_values[0] == doctest::Approx(0.6f));
            DOCTEST_REQUIRE(cell->m_modulators.m_gestures[2]);
            DOCTEST_CHECK(cell->m_modulators.m_gestures[2]->m_isActive[0]);
            DOCTEST_CHECK(cell->m_modulators.m_gestures[2]->m_values[0] == doctest::Approx(0.8f));
            auto bank = Encoders::BankForTrio(control.m_bank, trio);
            DOCTEST_CHECK(cell->m_ownerBank == &encoders.m_encoderBankBank.m_banks[static_cast<size_t>(bank)]);
        }
    }
}

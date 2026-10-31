#include "doctest.h"
#include "../support/SynthRig.hpp"
#include <utility>
#include <vector>

namespace
{
using Matrix = ModulationMatrixUIState;
using Encoders = SmartGridOneEncoders;
using Param = Encoders::Param;
using Cell = SmartGrid::BankedEncoderCell;

Cell& GetCell(Encoders& encoders, Param param, size_t trio)
{
    return *encoders.m_encoderBankBank.GetEncoder(Matrix::GetParamId(param, trio));
}

template<typename Modulator>
Cell& AddDepth(Cell& parent, Modulator modulator, float value)
{
    parent.FillModulators(parent.m_context);
    auto& depth = *parent.m_modulators.m_modulators[static_cast<size_t>(modulator)];
    depth.SetValue(value, true);
    depth.SetModulatorsAffectingRecursive();
    return depth;
}

std::vector<std::pair<Param, size_t>> GetTargets(const Matrix& matrix, size_t modulatorId)
{
    std::vector<std::pair<Param, size_t>> targets;
    matrix.ForEachParamModulatedBy(modulatorId, [&](Param param, size_t trio)
    {
        targets.emplace_back(param, trio);
    });
    return targets;
}
}

DOCTEST_TEST_CASE("modulation matrix: identities match encoder instances and start empty")
{
    GlobalEnv::ResetPerTest();
    SmartGridOneContext context;
    Encoders encoders(&context);
    Matrix matrix;
    for (size_t param = 0; param < Encoders::x_numParams; ++param)
    {
        auto parameter = static_cast<Param>(param);
        for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
        {
            size_t id = Matrix::GetParamId(parameter, trio);
            auto& cell = GetCell(encoders, parameter, trio);
            DOCTEST_CHECK(id == static_cast<size_t>(cell.m_index));
            DOCTEST_CHECK(Matrix::GetParam(id) == parameter);
            DOCTEST_CHECK(static_cast<size_t>(Matrix::GetMode(id)) == cell.GetSharedEncoderState()->m_modeIx);
            DOCTEST_CHECK(Matrix::GetTrio(id) ==
                (Encoders::Layout::GetParamMode(parameter) == Encoders::ParamMode::Voice ? trio : 0));
        }
    }

    DOCTEST_CHECK(Matrix::GetParamId(Param::LPCutoff, 0) != Matrix::GetParamId(Param::LPCutoff, 1));
    DOCTEST_CHECK(Matrix::GetParamId(Param::DelayReturn, 0) == Matrix::GetParamId(Param::DelayReturn, 2));
    DOCTEST_CHECK(Matrix::GetParamId(Param::Tempo, 0) == Matrix::GetParamId(Param::Tempo, 2));
    DOCTEST_CHECK(Matrix::GetModulatorId(VoiceModulator::LFO1, 0) != Matrix::GetModulatorId(VoiceModulator::LFO1, 1));
    DOCTEST_CHECK(Matrix::GetModulatorId(VoiceModulator::LFO1, 0) != Matrix::GetModulatorId(QuadModulator::LFO1));
    DOCTEST_CHECK(Matrix::GetModulatorId(VoiceModulator::Random1, 2) != Matrix::GetModulatorId(GlobalModulator::Random1));
    encoders.PopulateModulationMatrixUIState(&matrix);
    for (size_t modulator = 0; modulator < Matrix::x_numModulators; ++modulator)
    {
        DOCTEST_CHECK(GetTargets(matrix, modulator).empty());
    }
}

DOCTEST_TEST_CASE("modulation matrix: recursive depths and gestures match the base parameter badges")
{
    GlobalEnv::ResetPerTest();
    SmartGridOneContext context;
    Encoders encoders(&context);
    Matrix matrix;
    EncoderBankUIState badges;
    auto& carrier = GetCell(encoders, Param::LPCutoff, 0);
    auto& lfo1 = AddDepth(carrier, VoiceModulator::LFO1, 0.5f);
    auto& nested = AddDepth(lfo1, VoiceModulator::Random1, 0.25f);
    auto& lfo2 = AddDepth(carrier, VoiceModulator::LFO2, 0.5f);
    encoders.SelectBank(Encoders::Bank::Shape);

    auto publishAndCheckBadges = [&]()
    {
        encoders.PopulateUIState(&badges);
        encoders.PopulateModulationMatrixUIState(&matrix);
        auto bits = badges.GetModulatorsAffecting(2, 1);
        for (size_t index = 0; index < Matrix::x_numModulatorsPerMode; ++index)
        {
            DOCTEST_CHECK(matrix.IsModulatedBy(Param::LPCutoff, 0,
                Matrix::GetModulatorId(Encoders::BankMode::VoiceWater, index)) == bits.Get(static_cast<int>(index)));
        }
    };

    publishAndCheckBadges();
    DOCTEST_CHECK(matrix.IsModulatedBy(Param::LPCutoff, 0, Matrix::GetModulatorId(VoiceModulator::LFO1, 0)));
    DOCTEST_CHECK(matrix.IsModulatedBy(Param::LPCutoff, 0, Matrix::GetModulatorId(VoiceModulator::Random1, 0)));
    DOCTEST_CHECK_FALSE(matrix.IsModulatedBy(Param::LPCutoff, 0, Matrix::GetModulatorId(VoiceModulator::LFO2, 0)));

    lfo2.m_modulators.AddGesture(&lfo2, 2);
    lfo2.m_modulators.m_gestures[2]->SetActive(true, 0);
    lfo2.SetModulatorsAffectingRecursive();
    publishAndCheckBadges();
    DOCTEST_CHECK(matrix.IsModulatedBy(Param::LPCutoff, 0, Matrix::GetModulatorId(VoiceModulator::LFO2, 0)));

    nested.SetValue(0.5f, true);
    nested.SetModulatorsAffectingRecursive();
    encoders.ClearGesture(2);
    publishAndCheckBadges();
    DOCTEST_CHECK_FALSE(matrix.IsModulatedBy(Param::LPCutoff, 0, Matrix::GetModulatorId(VoiceModulator::LFO1, 0)));
    DOCTEST_CHECK_FALSE(matrix.IsModulatedBy(Param::LPCutoff, 0, Matrix::GetModulatorId(VoiceModulator::Random1, 0)));
    DOCTEST_CHECK_FALSE(matrix.IsModulatedBy(Param::LPCutoff, 0, Matrix::GetModulatorId(VoiceModulator::LFO2, 0)));
}

DOCTEST_TEST_CASE("modulation matrix: reverse lookup includes hidden parameters and separates every scope")
{
    GlobalEnv::ResetPerTest();
    SmartGridOneContext context;
    Encoders encoders(&context);
    Matrix matrix;
    AddDepth(GetCell(encoders, Param::SampleStart, 1), VoiceModulator::LFO1, 0.75f);
    AddDepth(GetCell(encoders, Param::SampleStart, 2), VoiceModulator::LFO1, 0.25f);
    AddDepth(GetCell(encoders, Param::LPCutoff, 0), VoiceModulator::LFO1, 0.75f);
    AddDepth(GetCell(encoders, Param::DelayReturn, 0), QuadModulator::LFO1, 0.75f);
    AddDepth(GetCell(encoders, Param::Tempo, 0), GlobalModulator::Random1, 0.75f);
    encoders.SelectBank(Encoders::Bank::DeepVocoder);
    encoders.PopulateModulationMatrixUIState(&matrix);

    const std::vector<std::pair<Param, size_t>> fireTargets{{Param::SampleStart, 1}};
    const std::vector<std::pair<Param, size_t>> earthTargets{{Param::SampleStart, 2}};
    const std::vector<std::pair<Param, size_t>> waterTargets{{Param::LPCutoff, 0}};
    const std::vector<std::pair<Param, size_t>> quadTargets{{Param::DelayReturn, 0}};
    const std::vector<std::pair<Param, size_t>> globalTargets{{Param::Tempo, 0}};
    DOCTEST_CHECK(GetTargets(matrix, Matrix::GetModulatorId(VoiceModulator::LFO1, 1)) == fireTargets);
    DOCTEST_CHECK(GetTargets(matrix, Matrix::GetModulatorId(VoiceModulator::LFO1, 2)) == earthTargets);
    DOCTEST_CHECK(GetTargets(matrix, Matrix::GetModulatorId(VoiceModulator::LFO1, 0)) == waterTargets);
    DOCTEST_CHECK(GetTargets(matrix, Matrix::GetModulatorId(QuadModulator::LFO1)) == quadTargets);
    DOCTEST_CHECK(GetTargets(matrix, Matrix::GetModulatorId(GlobalModulator::Random1)) == globalTargets);

    encoders.SetTrack(1);
    encoders.UpdateEncodersForMachine(VoiceMachine::SourceMachine::Sample, VoiceMachine::FilterMachine::SVF2Pole);
    encoders.SelectBank(Encoders::Bank::Source);
    encoders.PopulateModulationMatrixUIState(&matrix);
    DOCTEST_CHECK(GetTargets(matrix, Matrix::GetModulatorId(VoiceModulator::LFO1, 1)) == fireTargets);
    encoders.ResetBank(Encoders::Bank::Source);
    encoders.PopulateModulationMatrixUIState(&matrix);
    DOCTEST_CHECK(GetTargets(matrix, Matrix::GetModulatorId(VoiceModulator::LFO1, 1)).empty());
    DOCTEST_CHECK(GetTargets(matrix, Matrix::GetModulatorId(VoiceModulator::LFO1, 2)) == earthTargets);
    DOCTEST_CHECK(GetTargets(matrix, Matrix::GetModulatorId(QuadModulator::LFO1)) == quadTargets);
}

DOCTEST_TEST_CASE("modulation matrix: scene changes resets and patch reloads replace published flags")
{
    GlobalEnv::ResetPerTest();
    SmartGridOneContext context;
    Encoders encoders(&context);
    Matrix matrix;
    auto& depth = AddDepth(GetCell(encoders, Param::LPCutoff, 1), VoiceModulator::LFO1, 0.5f);
    depth.SetValue(0.75f, false);
    depth.SetModulatorsAffectingRecursive();
    size_t modulatorId = Matrix::GetModulatorId(VoiceModulator::LFO1, 1);
    encoders.PopulateModulationMatrixUIState(&matrix);
    DOCTEST_CHECK(matrix.IsModulatedBy(Param::LPCutoff, 1, modulatorId));

    for (float blend : {1.0f, 0.5f, 0.0f})
    {
        context.m_sceneManager.SetBlendFactor(blend);
        context.m_sceneManager.Process();
        encoders.Process();
        encoders.PopulateModulationMatrixUIState(&matrix);
        DOCTEST_CHECK(matrix.IsModulatedBy(Param::LPCutoff, 1, modulatorId) == (blend < 1.0f));
    }

    JsonArena arena(JsonArena::kDefaultCapacity);
    JSON saved = encoders.ToJSON(arena);
    encoders.RevertToDefault(true);
    encoders.PopulateModulationMatrixUIState(&matrix);
    DOCTEST_CHECK(GetTargets(matrix, modulatorId).empty());
    encoders.FromJSON(saved);
    encoders.PopulateModulationMatrixUIState(&matrix);
    DOCTEST_CHECK(matrix.IsModulatedBy(Param::LPCutoff, 1, modulatorId));
    DOCTEST_CHECK_FALSE(matrix.IsModulatedBy(Param::LPCutoff, 0, modulatorId));
}

DOCTEST_TEST_CASE("modulation matrix: normal UI publication includes encoder edits")
{
    synthrig::SynthRig rig;
    auto& internal = rig.Internal();
    auto& encoders = internal.m_squiggleBoy.m_encoders;
    auto& matrix = rig.UIState().m_squiggleBoyUIState.m_modulationMatrixUIState;
    encoders.SetTrack(2);
    encoders.SelectBank(Encoders::Bank::Shape);
    auto& bank = encoders.m_encoderBankBank.m_banks[static_cast<size_t>(encoders.m_selectedBank)];
    bank.MakeSelection(2, 1, bank.GetBase(2, 1));
    encoders.Apply(SmartGrid::MessageIn::EncoderSetMsg(2, 1, 0.75f));
    internal.PopulateUIState();
    DOCTEST_CHECK(matrix.IsModulatedBy(Param::LPCutoff, 2, Matrix::GetModulatorId(VoiceModulator::LFO1, 2)));
    DOCTEST_CHECK_FALSE(matrix.IsModulatedBy(Param::LPCutoff, 1, Matrix::GetModulatorId(VoiceModulator::LFO1, 2)));
    encoders.Apply(SmartGrid::MessageIn::EncoderSetMsg(2, 1, 0.5f));
    internal.PopulateUIState();
    // The 14-bit absolute-set format cannot represent exactly 0.5. Its small
    // positive depth still affects the parameter, just as the badge reports.
    //
    DOCTEST_CHECK(matrix.IsModulatedBy(Param::LPCutoff, 2, Matrix::GetModulatorId(VoiceModulator::LFO1, 2)));
    bank.m_context->m_sceneManager.m_shift = true;
    encoders.Apply(SmartGrid::MessageIn(SmartGrid::MessageIn::Mode::EncoderPush, 2, 1));
    bank.m_context->m_sceneManager.m_shift = false;
    internal.PopulateUIState();
    DOCTEST_CHECK_FALSE(matrix.IsModulatedBy(Param::LPCutoff, 2, Matrix::GetModulatorId(VoiceModulator::LFO1, 2)));
}

#include "doctest.h"
#include "EncoderBankBank.hpp"
#include "support/GlobalEnv.hpp"
#include <algorithm>
#include <cmath>

namespace
{
using Cell = SmartGrid::BankedEncoderCell;

struct ClampRig
{
    SmartGridOneContext m_context;
    EncoderBankBank m_bank;

    explicit ClampRig(bool bipolar)
        : m_bank(1, 1, 1, 1, 1, &m_context)
    {
        m_bank.InitMode(0, 4);
        m_bank.InitBank(0, 0, SmartGrid::Color::Red);
        m_bank.CreateEncoder(0, 0, 0, 0.4f, "Parameter", "P", SmartGrid::Color::Red, 0, bipolar);
        m_bank.PlaceEncoder(0, 0, 0, 0);
        for (size_t i = 0; i < Cell::x_numModulators; ++i)
        {
            Sources().SetModulatorSkin(i, SmartGrid::Color::Red, nullptr);
        }

        AddDepths(Root());
        SetSettled(Root(), 0.4f);
    }

    Cell& Root()
    {
        return *m_bank.GetEncoder(0);
    }

    Cell::ModulatorValues& Sources()
    {
        return m_bank.GetModulatorValues(0);
    }

    static void SetSettled(Cell& cell, float value)
    {
        for (size_t scene = 0; scene < SmartGrid::SceneManager::x_numScenes; ++scene)
        {
            cell.m_values[scene] = value;
            cell.m_targets[scene] = value;
        }

        cell.m_tickDistributor = SmartGrid::TickDistributor();
        cell.m_samplesForceUpdate = 0;
        cell.SetState();
        cell.m_postGestureValue = cell.m_bankedValue;
        cell.m_output = QuadFloat::Splat(value);
        cell.m_outputPrev = cell.m_output;
    }

    void AddDepths(Cell& cell)
    {
        cell.FillModulators(&m_context);
        for (auto& amount : cell.m_modulators.m_modulators)
        {
            SetSettled(*amount, 0.5f);
        }
    }

    void Process(QuadFloat value, bool editing)
    {
        SampleTimer::s_instance->m_sample += SampleTimer::x_oversample;
        Sources().m_hostValue[0] = value;
        Sources().m_hostValue[1] = QuadFloat::Splat(value[0]);
        if (editing)
        {
            Root().m_samplesForceUpdate = 2;
        }

        m_context.m_sceneManager.Process();
        m_bank.ProcessHostFrame();
    }
};

}

DOCTEST_TEST_CASE("encoder output clamps: manager preserves downsampler excursions")
{
    GlobalEnv::ResetPerTest();
    ModulatorManager manager(2, 1);
    manager.Bind(0, 0, 0);
    manager.Bind(0, 1, 1);
    manager.GetSource(1).m_requiresDownsampling = false;
    const QuadFloat bypass(-0.2f, 0.3f, 0.8f, 1.2f);
    manager.Set(1, bypass);
    ModulatorManager::Decimator reference;
    float peak = 0;
    for (size_t n = 0; n < 1024; ++n)
    {
        SampleTimer::IncrementSample();
        manager.Set(0, QuadFloat::Splat(1));
        QuadFloat expected = reference.Process(QuadFloat::Splat(1));
        manager.ProcessSample();
        if (SampleTimer::IsHostFrame())
        {
            manager.Gather();
            auto value = manager.m_groups[0].m_values.m_hostValue[0];
            DOCTEST_CHECK(value == expected);
            peak = std::max(peak, value[0]);
            DOCTEST_CHECK(manager.m_groups[0].m_values.m_hostValue[1] == bypass);
        }
    }

    if constexpr (SampleTimer::x_oversample > 1)
    {
        DOCTEST_CHECK(peak > 1.001f);
    }
}

DOCTEST_TEST_CASE("encoder output clamps: source excursions contribute when the parameter has headroom")
{
    for (float position : {0.25f, 0.75f})
    {
        GlobalEnv::ResetPerTest();
        ClampRig rig(false);
        ClampRig::SetSettled(*rig.Root().GetModulator(0), position);
        rig.Root().SetModulatorsAffecting();
        QuadFloat input(-0.15f, 1.15f, 0.25f, 0.75f);
        for (size_t frame = 0; frame < 8; ++frame)
        {
            rig.Process(input, false);
        }

        float depth = Cell::ModulationDepthFromValue(position * 2 - 1);
        for (size_t lane = 0; lane < 4; ++lane)
        {
            float expected = 0.4f * (1 - std::abs(depth)) + depth * input[lane] + std::max(0.0f, -depth);
            DOCTEST_CHECK(expected > 0);
            DOCTEST_CHECK(expected < 1);
            DOCTEST_CHECK(rig.Root().m_output[lane] == doctest::Approx(expected).epsilon(2e-6));
        }
    }
}

DOCTEST_TEST_CASE("encoder output clamps: roots clamp before interpolation across all compute paths")
{
    for (bool bipolar : {false, true})
    {
        for (int path = 0; path < 3; ++path)
        {
            GlobalEnv::ResetPerTest();
            ClampRig rig(bipolar);
            Cell& amount = *rig.Root().GetModulator(0);
            ClampRig::SetSettled(amount, 1);
            if (path != 0)
            {
                rig.AddDepths(amount);
                ClampRig::SetSettled(*amount.GetModulator(1), 1);
            }

            rig.Root().SetModulatorsAffecting();
            for (size_t frame = 0; frame < 12; ++frame)
            {
                QuadFloat before = rig.Root().m_output;
                float high = frame % 2 ? 1.25f : 1.5f;
                rig.Process(QuadFloat(high, -0.25f, high, -0.5f), path == 2);
                if (frame < 4)
                {
                    continue;
                }

                auto expectedPath = path == 0 ? EncoderMatrix::Path::Simple
                    : path == 1 ? EncoderMatrix::Path::Graph : EncoderMatrix::Path::Editing;
                DOCTEST_REQUIRE(rig.m_bank.m_matrix.m_roots[0].m_path == expectedPath);
                for (size_t lane = 0; lane < 4; ++lane)
                {
                    float value = rig.Root().m_output[lane];
                    DOCTEST_CHECK(value >= 0);
                    DOCTEST_CHECK(value <= 1);
                    DOCTEST_CHECK(rig.Root().m_outputPrev[lane] == before[lane]);
                    for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
                    {
                        float expected = value + (before[lane] - value)
                            * Cell::GetPreviousOutputWeight(sample, SampleTimer::x_oversample);
                        expected = bipolar ? expected * 2 - 1 : expected;
                        float cached = rig.m_bank.m_valueCaches[sample][0][lane];
                        DOCTEST_CHECK(cached == doctest::Approx(expected).epsilon(2e-6));
                        DOCTEST_CHECK(cached >= (bipolar ? -1 : 0));
                        DOCTEST_CHECK(cached <= 1);
                    }
                }

                DOCTEST_CHECK(rig.Root().m_output[0] == 1);
                DOCTEST_CHECK(rig.Root().m_output[1] == 0);

                if (path == 1)
                {
                    auto& matrix = rig.m_bank.m_matrix;
                    size_t child = matrix.FindNode(&amount, matrix.m_roots[0]);
                    DOCTEST_REQUIRE(child != EncoderMatrix::x_leaf);
                    DOCTEST_CHECK(matrix.m_nodes[child].m_output[0] > 1);
                }
                else if (path == 2)
                {
                    DOCTEST_CHECK(amount.m_output[0] > 1);
                }
            }

            // A stationary root keeps bounded output even when no route changes.
            //
            rig.Process(QuadFloat(1.25f, -0.25f, 1.25f, -0.5f), false);
            for (const auto& cache : rig.m_bank.m_valueCaches)
            {
                DOCTEST_CHECK(cache[0][0] == 1);
            }
        }
    }
}

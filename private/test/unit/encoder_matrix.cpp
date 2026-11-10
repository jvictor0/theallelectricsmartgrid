#include "doctest.h"
#include "EncoderBankBank.hpp"
#include "../support/GlobalEnv.hpp"
#include <random>

namespace
{

using Cell = SmartGrid::BankedEncoderCell;

struct MatrixRig
{
    bool m_legacyRefresh = false;

    static void ForceLegacyRefresh(Cell& cell)
    {
        cell.m_samplesForceUpdate = SmartGrid::TickDistributor::x_maxSamplesToDistribute;
        for (size_t i = 0; i < cell.m_modulators.m_numActiveModulators; ++i)
        {
            ForceLegacyRefresh(*cell.m_modulators.GetModulator(i));
        }

        for (size_t i = 0; i < cell.m_modulators.m_numActiveGestures; ++i)
        {
            ForceLegacyRefresh(*cell.m_modulators.GetGesture(i));
        }
    }

    SmartGridOneContext m_context;
    EncoderBankBank m_bank;

    MatrixRig()
        : m_bank(1, 1, 1, 15, 1, &m_context)
    {
        m_bank.InitMode(0, 4);
        m_bank.InitBank(0, 0, SmartGrid::Color::Red);
        m_bank.CreateEncoder(0, 0, 0, 0.3f, "Param", "P", SmartGrid::Color::Red, 0, false);
        m_bank.PlaceEncoder(0, 0, 0, 0);
        for (size_t i = 0; i < Cell::x_numModulators; ++i)
        {
            Sources().SetModulatorSkin(i, SmartGrid::Color::Red, nullptr);
        }
    }

    Cell& Root()
    {
        return *m_bank.GetEncoder(0);
    }

    Cell::ModulatorValues& Sources()
    {
        return m_bank.GetModulatorValues(0);
    }

    Cell::GestureValues& Gestures()
    {
        return m_bank.GetGestureValues();
    }

    static void SetSettled(Cell& cell, float left, float right)
    {
        for (size_t scene = 0; scene < SmartGrid::SceneManager::x_numScenes; ++scene)
        {
            cell.m_values[scene] = scene % 2 ? right : left;
            cell.m_targets[scene] = cell.m_values[scene];
        }

        cell.m_tickDistributor = SmartGrid::TickDistributor();
        cell.m_samplesForceUpdate = 0;
        cell.SetState();
        cell.m_postGestureValue = cell.m_bankedValue;
        cell.m_output = QuadFloat::Splat(cell.m_bankedValue);
    }

    void Configure()
    {
        Root().FillModulators(&m_context);
        SetSettled(Root(), 0.2f, 0.8f);
        for (size_t i = 0; i < Cell::x_numModulators; ++i)
        {
            SetSettled(*Root().m_modulators.m_modulators[i], 0.5f, 0.5f);
        }

        SetSettled(*Root().m_modulators.m_modulators[2], 0.15f, 0.5f);
        SetSettled(*Root().m_modulators.m_modulators[7], 0.5f, 0.9f);
        Root().SetModulatorsAffecting();
    }

    void ConfigureNested()
    {
        Configure();
        Cell& amount = *Root().m_modulators.m_modulators[2];
        amount.FillModulators(&m_context);
        for (size_t i = 0; i < Cell::x_numModulators; ++i)
        {
            SetSettled(*amount.m_modulators.m_modulators[i], 0.5f, 0.5f);
        }

        Cell& nested = *amount.m_modulators.m_modulators[4];
        nested.FillModulators(&m_context);
        for (size_t i = 0; i < Cell::x_numModulators; ++i)
        {
            SetSettled(*nested.m_modulators.m_modulators[i], 0.5f, 0.5f);
        }

        SetSettled(*nested.m_modulators.m_modulators[1], 0.95f, 0.6f);
        SetSettled(nested, 0.1f, 0.8f);
        SetSettled(*amount.m_modulators.m_modulators[9], 0.9f, 0.05f);
        SetSettled(amount, 0.15f, 0.5f);
        SetSettled(Root(), 0.2f, 0.8f);
        Root().SetModulatorsAffecting();
    }

    void AddGesture(Cell& cell, size_t index, float left, float right, bool leftActive, bool rightActive)
    {
        cell.m_modulators.AddGesture(&cell, index);
        Cell& gesture = *cell.m_modulators.m_gestures[index];
        SetSettled(gesture, left, right);
        for (size_t scene = 0; scene < SmartGrid::SceneManager::x_numScenes; ++scene)
        {
            gesture.m_isActive[scene] = scene % 2 ? rightActive : leftActive;
        }
    }

    void ConfigureGestures()
    {
        ConfigureNested();
        AddGesture(Root(), 1, 0.9f, 0.1f, true, false);
        AddGesture(Root(), 5, 0.6f, 0.8f, true, true);
        AddGesture(*Root().m_modulators.m_modulators[2], 7, 0.1f, 0.9f, false, true);
        AddGesture(*Root().m_modulators.m_modulators[7], 3, 0.05f, 0.75f, true, true);
        Root().SetModulatorsAffecting();
    }

    void Process(bool reference)
    {
        m_context.m_sceneManager.Process();
        if (!reference)
        {
            m_bank.ProcessHostFrame();
            return;
        }

        Sources().ComputeChanged();
        Gestures().ComputeChanged();
        auto& scene = m_context.m_sceneManager;
        bool blendChanged = scene.m_changed && !scene.m_changedScene;
        if (blendChanged)
        {
            m_bank.SetBlendChanged(true);
        }
        else
        {
            if (scene.m_changed)
            {
                m_bank.HandleChangedSceneManager();
            }

            if (scene.m_changedScene)
            {
                m_bank.HandleChangedSceneManagerScene();
            }
        }

        if (m_legacyRefresh && scene.m_changed && !blendChanged)
        {
            ForceLegacyRefresh(Root());
        }

        Root().Compute(true);
        for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
        {
            m_bank.m_valueCaches[sample][0] = Root().GetInterpolatedValue(SampleTimer::GetHostSample(),
                Cell::GetPreviousOutputWeight(sample, SampleTimer::x_oversample));
        }

        m_bank.SetBlendChanged(false);
    }
};

void CheckOutputs(MatrixRig& actual, MatrixRig& reference)
{
    DOCTEST_CHECK(actual.Root().m_brightness == doctest::Approx(reference.Root().m_brightness).epsilon(1e-6));
    DOCTEST_CHECK(actual.Root().m_gestureWeightSum == doctest::Approx(reference.Root().m_gestureWeightSum).epsilon(1e-6));
    for (size_t lane = 0; lane < 4; ++lane)
    {
        DOCTEST_REQUIRE(actual.Root().m_output[lane] == doctest::Approx(reference.Root().m_output[lane]).epsilon(1e-6));
        DOCTEST_CHECK(actual.Root().m_minValue[lane] == doctest::Approx(reference.Root().m_minValue[lane]).epsilon(1e-6));
        DOCTEST_CHECK(actual.Root().m_maxValue[lane] == doctest::Approx(reference.Root().m_maxValue[lane]).epsilon(1e-6));
        for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
        {
            DOCTEST_REQUIRE(actual.m_bank.m_valueCaches[sample][0][lane]
                == doctest::Approx(reference.m_bank.m_valueCaches[sample][0][lane]).epsilon(1e-6));
        }
    }
}

}

DOCTEST_TEST_CASE("encoder matrix: short settled refresh matches the legacy full window through live ramps")
{
    GlobalEnv::Init();
    GlobalEnv::ResetPerTest();
    MatrixRig actual;
    MatrixRig reference;
    actual.ConfigureGestures();
    reference.ConfigureGestures();
    reference.m_legacyRefresh = true;
    std::mt19937 random(1741);
    std::uniform_real_distribution<float> value(0.0f, 1.0f);
    size_t preparedFrames = 0;
    for (size_t frame = 0; frame < 5000; ++frame)
    {
        float blend = frame % 240 == 0 ? 0.0f : frame % 240 == 1 ? 1.0f
            : 0.2f + 0.6f * static_cast<float>(frame % 96) / 95.0f;
        for (auto* rig : {&actual, &reference})
        {
            rig->m_context.m_sceneManager.m_blendFactor = blend;
            if (frame == 480 || frame == 520 || frame == 960 || frame == 1200 || frame == 1600)
            {
                Cell* edited = frame < 1200 ? rig->Root().m_modulators.m_modulators[2].get()
                    : frame == 1200 ? &rig->Root() : rig->Root().m_modulators.m_gestures[1].get();
                edited->SetValue(frame == 520 ? 0.35f : frame == 960 ? 0.85f : 0.7f, true);
                edited->SetForceUpdateRecursive();
                edited->SetModulatorsAffectingRecursive();
            }
        }

        for (size_t source = 0; source < Cell::x_numModulators; ++source)
        {
            for (size_t lane = 0; lane < 4; ++lane)
            {
                float sample = value(random);
                float amplitude = value(random);
                actual.Sources().m_hostValue[source][lane] = reference.Sources().m_hostValue[source][lane] = sample;
                actual.Sources().m_amplitude[source][lane] = reference.Sources().m_amplitude[source][lane] = amplitude;
            }
        }

        for (size_t gesture = 0; gesture < Cell::x_numGestureParams; ++gesture)
        {
            float weight = value(random);
            actual.Gestures().m_gestureWeights[gesture] = reference.Gestures().m_gestureWeights[gesture] = weight;
        }

        for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
        {
            SampleTimer::IncrementSample();
        }

        actual.Process(false);
        reference.Process(true);
        CheckOutputs(actual, reference);
        for (size_t scene = 0; scene < SmartGrid::SceneManager::x_numScenes; ++scene)
        {
            DOCTEST_CHECK(actual.Root().m_values[scene] == reference.Root().m_values[scene]);
            DOCTEST_CHECK(actual.Root().m_modulators.m_modulators[2]->m_values[scene]
                == reference.Root().m_modulators.m_modulators[2]->m_values[scene]);
        }

        DOCTEST_CHECK(actual.Root().m_tickDistributor.m_samplesSinceLastChange
            == reference.Root().m_tickDistributor.m_samplesSinceLastChange);
        DOCTEST_CHECK(actual.Root().m_modulators.m_modulators[2]->m_tickDistributor.m_samplesSinceLastChange
            == reference.Root().m_modulators.m_modulators[2]->m_tickDistributor.m_samplesSinceLastChange);
        if (frame < 480)
        {
            preparedFrames += actual.m_bank.m_matrix.m_roots[0].m_path == EncoderMatrix::Path::Graph;
        }
    }

    DOCTEST_CHECK(preparedFrames > 400);
}

DOCTEST_TEST_CASE("encoder matrix: flat routes match recursive evaluation through sweeps and target changes")
{
    GlobalEnv::Init();
    GlobalEnv::ResetPerTest();
    MatrixRig actual;
    MatrixRig reference;
    actual.Configure();
    reference.Configure();
    std::mt19937 random(901);
    std::uniform_real_distribution<float> value(0.0f, 1.0f);
    for (size_t frame = 0; frame < 12000; ++frame)
    {
        float blend = frame < 1000 ? 0.0f : frame < 2000 ? 0.5f
            : 0.1f + 0.8f * static_cast<float>(frame % 173) / 172.0f;
        actual.m_context.m_sceneManager.m_blendFactor = blend;
        reference.m_context.m_sceneManager.m_blendFactor = blend;
        for (size_t source = 0; source < Cell::x_numModulators; ++source)
        {
            for (size_t lane = 0; lane < 4; ++lane)
            {
                float sample = value(random);
                float amplitude = value(random);
                actual.Sources().m_hostValue[source][lane] = reference.Sources().m_hostValue[source][lane] = sample;
                actual.Sources().m_amplitude[source][lane] = reference.Sources().m_amplitude[source][lane] = amplitude;
            }
        }

        if (frame == 2500 || frame == 6000)
        {
            for (auto* rig : {&actual, &reference})
            {
                auto* amount = rig->Root().m_modulators.m_modulators[2].get();
                amount->SetValue(frame == 2500 ? 0.95f : 0.5f, true);
                amount->SetForceUpdateRecursive();
                amount->SetModulatorsAffectingRecursive();
            }
        }

        for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
        {
            SampleTimer::IncrementSample();
        }

        actual.Process(false);
        reference.Process(true);
        CheckOutputs(actual, reference);
    }
}

DOCTEST_TEST_CASE("encoder matrix: nested signed amounts match recursive evaluation independently in every lane")
{
    GlobalEnv::Init();
    GlobalEnv::ResetPerTest();
    MatrixRig actual;
    MatrixRig reference;
    actual.ConfigureNested();
    reference.ConfigureNested();
    std::mt19937 random(523);
    std::uniform_real_distribution<float> value(0.0f, 1.0f);
    for (size_t frame = 0; frame < 10000; ++frame)
    {
        float blend = frame < 100 ? 0.0f : 0.1f + 0.8f * static_cast<float>(frame % 173) / 172.0f;
        actual.m_context.m_sceneManager.m_blendFactor = blend;
        reference.m_context.m_sceneManager.m_blendFactor = blend;
        for (size_t source = 0; source < Cell::x_numModulators; ++source)
        {
            for (size_t lane = 0; lane < 4; ++lane)
            {
                float sample = value(random);
                float amplitude = value(random);
                actual.Sources().m_hostValue[source][lane] = reference.Sources().m_hostValue[source][lane] = sample;
                actual.Sources().m_amplitude[source][lane] = reference.Sources().m_amplitude[source][lane] = amplitude;
            }
        }

        for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
        {
            SampleTimer::IncrementSample();
        }

        actual.Process(false);
        reference.Process(true);
        CheckOutputs(actual, reference);
    }
}

DOCTEST_TEST_CASE("encoder matrix: packed depth batches and tails preserve recursive outputs")
{
    GlobalEnv::Init();
    std::mt19937 random(291);
    std::uniform_real_distribution<float> value(0.0f, 1.0f);
    for (size_t count = 0; count <= Cell::x_numModulators; ++count)
    {
        DOCTEST_CAPTURE(count);
        GlobalEnv::ResetPerTest();
        MatrixRig actual;
        MatrixRig reference;
        actual.Configure();
        reference.Configure();
        for (size_t source = 0; source < Cell::x_numModulators; ++source)
        {
            float left = source < count ? value(random) : 0.5f;
            float right = source < count ? value(random) : 0.5f;
            for (auto* rig : {&actual, &reference})
            {
                MatrixRig::SetSettled(*rig->Root().m_modulators.m_modulators[source], left, right);
            }
        }

        actual.Root().SetModulatorsAffecting();
        reference.Root().SetModulatorsAffecting();
        for (size_t frame = 0; frame < 400; ++frame)
        {
            DOCTEST_CAPTURE(frame);
            float blend = frame < 64 ? 0.0f : static_cast<float>(frame % 129) / 128.0f;
            actual.m_context.m_sceneManager.m_blendFactor = blend;
            reference.m_context.m_sceneManager.m_blendFactor = blend;
            for (size_t source = 0; source < Cell::x_numModulators; ++source)
            {
                for (size_t lane = 0; lane < 4; ++lane)
                {
                    float sample = value(random);
                    actual.Sources().m_hostValue[source][lane] = reference.Sources().m_hostValue[source][lane] = sample;
                    if (frame % 31 == 0)
                    {
                        float amplitude = value(random);
                        actual.Sources().m_amplitude[source][lane] = reference.Sources().m_amplitude[source][lane] = amplitude;
                    }
                }
            }

            if (frame == 200)
            {
                actual.m_bank.m_matrix.m_rebuild = true;
            }

            for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
            {
                SampleTimer::IncrementSample();
            }

            actual.Process(false);
            reference.Process(true);
            CheckOutputs(actual, reference);
        }
    }
}

DOCTEST_TEST_CASE("encoder matrix: gestures edits scenes and UI preserve recursive behavior")
{
    GlobalEnv::Init();
    GlobalEnv::ResetPerTest();
    MatrixRig actual;
    MatrixRig reference;
    actual.ConfigureGestures();
    reference.ConfigureGestures();
    std::mt19937 random(717);
    std::uniform_real_distribution<float> value(0.0f, 1.0f);
    size_t preparedFrames = 0;
    for (size_t frame = 0; frame < 35000 + SmartGrid::TickDistributor::x_maxSamplesToDistribute; ++frame)
    {
        DOCTEST_CAPTURE(frame);
        float blend = frame < 100 ? 0.0f : 0.1f + 0.8f * static_cast<float>(frame % 173) / 172.0f;
        for (auto* rig : {&actual, &reference})
        {
            rig->m_context.m_sceneManager.m_blendFactor = blend;
            if (frame == 5000 || frame == 10000)
            {
                rig->Gestures().m_selectedGestures.Set(1, true);
                rig->Root().Increment(frame == 5000 ? 0.13f : -0.17f);
            }

            if (frame == 15000)
            {
                rig->Root().m_modulators.m_modulators[2]->Increment(-0.12f);
            }

            if (frame == 20000)
            {
                rig->Root().CopyToScene(3);
                rig->m_context.m_sceneManager.m_scene2 = 3;
            }

            if (frame == 25000)
            {
                rig->Root().ZeroModulators(true);
                rig->Root().SetModulatorsAffecting();
            }

            if (frame == 30000)
            {
                rig->Root().FillModulators(&rig->m_context);
                rig->Root().m_modulators.m_modulators[12]->Increment(0.3f);
            }
        }

        for (size_t source = 0; source < Cell::x_numModulators; ++source)
        {
            for (size_t lane = 0; lane < 4; ++lane)
            {
                float sample = value(random);
                float amplitude = value(random);
                actual.Sources().m_hostValue[source][lane] = reference.Sources().m_hostValue[source][lane] = sample;
                actual.Sources().m_amplitude[source][lane] = reference.Sources().m_amplitude[source][lane] = amplitude;
            }
        }

        for (size_t gesture = 0; gesture < Cell::x_numGestureParams; ++gesture)
        {
            float weight = frame % 100 < 50 ? value(random) : 0.0f;
            actual.Gestures().m_gestureWeights[gesture] = reference.Gestures().m_gestureWeights[gesture] = weight;
        }

        for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
        {
            SampleTimer::IncrementSample();
        }

        actual.Process(false);
        reference.Process(true);
        CheckOutputs(actual, reference);
        preparedFrames += actual.m_bank.m_matrix.m_roots[0].m_path == EncoderMatrix::Path::Graph;
        if (frame % 101 == 0)
        {
            EncoderBankUIState actualUI;
            EncoderBankUIState referenceUI;
            actual.m_bank.m_banks[0].PopulateUIState(&actualUI);
            reference.m_bank.m_banks[0].PopulateUIState(&referenceUI);
            for (size_t source = 0; source < Cell::x_numModulators; ++source)
            {
                for (size_t lane = 0; lane < 4; ++lane)
                {
                    DOCTEST_CHECK(actualUI.m_states[0][0].m_modulatorWeights[lane][source].load()
                        == doctest::Approx(referenceUI.m_states[0][0].m_modulatorWeights[lane][source].load()).epsilon(1e-6));
                }
            }

            CheckOutputs(actual, reference);
        }

        if (frame == 8000 || frame == 18000)
        {
            actual.m_bank.m_matrix.m_rebuild = true;
        }
    }

    DOCTEST_CHECK(preparedFrames > 5000);
}

DOCTEST_TEST_CASE("encoder matrix: sparse input changes rebuilds and patch replacement retain cached dependencies")
{
    GlobalEnv::Init();
    GlobalEnv::ResetPerTest();
    MatrixRig actual;
    MatrixRig reference;
    actual.ConfigureGestures();
    reference.ConfigureGestures();
    JsonArena saved(JsonArena::kDefaultCapacity);
    JSON patch = reference.Root().ToJSON(saved);
    for (auto* rig : {&actual, &reference})
    {
        rig->m_context.m_sceneManager.m_blendFactor = 0.35f;
        rig->Gestures().m_gestureWeights[1] = 0.7f;
        rig->Gestures().m_gestureWeights[3] = 0.1f;
        rig->Gestures().m_gestureWeights[5] = 0.4f;
        rig->Gestures().m_gestureWeights[7] = 0.9f;
    }

    std::mt19937 random(991);
    std::uniform_real_distribution<float> value(0.0f, 1.0f);
    size_t nodeCapacity = 0;
    size_t routeCapacity = 0;
    size_t gestureCapacity = 0;
    for (size_t frame = 0; frame < 12000; ++frame)
    {
        DOCTEST_CAPTURE(frame);
        size_t source = (frame / 13) % Cell::x_numModulators;
        if (frame % 13 == 0)
        {
            float sample = value(random);
            actual.Sources().m_hostValue[source] = reference.Sources().m_hostValue[source] = QuadFloat::Splat(sample);
        }

        if (frame % 11 == 0)
        {
            float amplitude = value(random);
            actual.Sources().m_amplitude[source] = reference.Sources().m_amplitude[source] = QuadFloat::Splat(amplitude);
        }

        if (frame % 17 == 0)
        {
            float weight = value(random);
            actual.Gestures().m_gestureWeights[7] = reference.Gestures().m_gestureWeights[7] = weight;
        }

        if (frame == 5000)
        {
            actual.Root().FromJSON(patch);
            reference.Root().FromJSON(patch);
        }

        if (frame == 9000)
        {
            actual.Root().RevertToDefault(true);
            reference.Root().RevertToDefault(true);
        }

        if (frame == 4000 || frame == 4500)
        {
            actual.m_bank.m_matrix.m_rebuild = true;
        }

        for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
        {
            SampleTimer::IncrementSample();
        }

        actual.Process(false);
        reference.Process(true);
        CheckOutputs(actual, reference);
        DOCTEST_CHECK(actual.Root().m_outputHostSample == reference.Root().m_outputHostSample);
        if (frame == 3500)
        {
            DOCTEST_REQUIRE(actual.m_bank.m_matrix.m_roots[0].m_path == EncoderMatrix::Path::Graph);
            nodeCapacity = actual.m_bank.m_matrix.m_nodes.capacity();
            routeCapacity = actual.m_bank.m_matrix.m_nestedRoutes.capacity();
            gestureCapacity = actual.m_bank.m_matrix.m_gestures.capacity();
        }

        if (frame > 3500 && frame < 5000)
        {
            DOCTEST_CHECK(actual.m_bank.m_matrix.m_nodes.capacity() == nodeCapacity);
            DOCTEST_CHECK(actual.m_bank.m_matrix.m_nestedRoutes.capacity() == routeCapacity);
            DOCTEST_CHECK(actual.m_bank.m_matrix.m_gestures.capacity() == gestureCapacity);
        }
    }
}

DOCTEST_TEST_CASE("encoder matrix: identical patch reload preserves full modulation from the first sample")
{
    GlobalEnv::Init();
    GlobalEnv::ResetPerTest();
    MatrixRig rig;
    rig.Root().FillModulators(&rig.m_context);
    rig.Root().m_modulators.m_modulators[0]->Increment(0.5f);
    rig.Sources().m_hostValue[0] = QuadFloat::Splat(1.0f);
    auto process = [&]()
    {
        for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
        {
            SampleTimer::IncrementSample();
        }

        rig.Process(false);
    };
    for (size_t frame = 0; frame < SmartGrid::TickDistributor::x_maxSamplesToDistribute + 2; ++frame)
    {
        process();
    }

    DOCTEST_REQUIRE(rig.Root().m_output[0] == doctest::Approx(1.0f));
    JsonArena saved(JsonArena::kDefaultCapacity);
    JSON patch = rig.Root().ToJSON(saved);
    rig.Root().FromJSON(patch);
    double maximumError = 0;
    for (size_t frame = 0; frame < SmartGrid::TickDistributor::x_maxSamplesToDistribute + 2; ++frame)
    {
        process();
        for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
        {
            for (size_t lane = 0; lane < 4; ++lane)
            {
                maximumError = std::max(maximumError,
                    std::abs(static_cast<double>(rig.m_bank.m_valueCaches[sample][0][lane]) - 1));
            }
        }
    }

    DOCTEST_CHECK(maximumError < 1e-6);
}

DOCTEST_TEST_CASE("encoder matrix: reload preserves nested modulation gestures and an in-flight ramp")
{
    GlobalEnv::Init();
    GlobalEnv::ResetPerTest();
    for (float blend : {0.0f, 0.5f, 1.0f})
    {
        MatrixRig actual;
        MatrixRig untouched;
        for (auto* rig : {&actual, &untouched})
        {
            rig->ConfigureGestures();
            rig->m_context.m_sceneManager.m_blendFactor = blend;
            for (size_t i = 0; i < Cell::x_numModulators; ++i)
            {
                rig->Sources().m_hostValue[i] = QuadFloat(0.1f, 0.4f, 0.7f, 0.9f);
            }

            for (size_t i = 0; i < Cell::x_numGestureParams; ++i)
            {
                rig->Gestures().m_gestureWeights[i] = 0.7f;
            }
        }

        auto process = [&]()
        {
            for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
            {
                SampleTimer::IncrementSample();
            }

            actual.Process(false);
            untouched.Process(false);
        };
        for (size_t frame = 0; frame < SmartGrid::TickDistributor::x_maxSamplesToDistribute + 2; ++frame)
        {
            process();
        }

        DOCTEST_REQUIRE(actual.m_bank.m_matrix.m_roots[0].m_path == EncoderMatrix::Path::Graph);
        Cell* depth = actual.Root().m_modulators.m_modulators[2].get();
        Cell* nested = depth->m_modulators.m_modulators[4].get();
        Cell* gesture = depth->m_modulators.m_gestures[7].get();
        JsonArena saved(JsonArena::kDefaultCapacity);
        actual.Root().FromJSON(actual.Root().ToJSON(saved));
        DOCTEST_CHECK(depth->m_tickDistributor.m_samplesSinceLastChange ==
            untouched.Root().m_modulators.m_modulators[2]->m_tickDistributor.m_samplesSinceLastChange);
        DOCTEST_CHECK(depth->m_tickDistributor.m_samplesToDistribute ==
            untouched.Root().m_modulators.m_modulators[2]->m_tickDistributor.m_samplesToDistribute);
        DOCTEST_CHECK(actual.Root().m_modulators.m_modulators[2].get() == depth);
        DOCTEST_CHECK(depth->m_modulators.m_modulators[4].get() == nested);
        DOCTEST_CHECK(depth->m_modulators.m_gestures[7].get() == gesture);
        DOCTEST_CHECK(gesture->m_tickDistributor.m_samplesSinceLastChange ==
            untouched.Root().m_modulators.m_modulators[2]->m_modulators.m_gestures[7]->m_tickDistributor.m_samplesSinceLastChange);
        DOCTEST_CHECK(gesture->m_targets[0] ==
            untouched.Root().m_modulators.m_modulators[2]->m_modulators.m_gestures[7]->m_targets[0]);
        for (size_t frame = 0; frame < 2500; ++frame)
        {
            if (frame == 8)
            {
                actual.Root().m_modulators.m_modulators[2]->Increment(0.1f);
                untouched.Root().m_modulators.m_modulators[2]->Increment(0.1f);
            }

            if (frame == 20)
            {
                JsonArena moving(JsonArena::kDefaultCapacity);
                float targets[SmartGrid::SceneManager::x_numScenes];
                std::copy(std::begin(depth->m_targets), std::end(depth->m_targets), targets);
                double state[SmartGrid::SceneManager::x_numScenes];
                std::copy(std::begin(depth->m_smoothedValues), std::end(depth->m_smoothedValues), state);
                auto movingScenes = depth->m_tickDistributor.m_movingScenes;
                actual.Root().FromJSON(actual.Root().ToJSON(moving));
                for (size_t scene = 0; scene < SmartGrid::SceneManager::x_numScenes; ++scene)
                {
                    DOCTEST_CHECK(depth->m_targets[scene] == targets[scene]);
                    DOCTEST_CHECK(depth->m_smoothedValues[scene] == state[scene]);
                }

                DOCTEST_CHECK(depth->m_tickDistributor.m_movingScenes == movingScenes);
            }

            process();
            DOCTEST_INFO("blend=" << blend << " frame=" << frame);
            CheckOutputs(actual, untouched);
        }
    }
}

DOCTEST_TEST_CASE("encoder matrix: changed patch reuses matching paths and replaces absent slots")
{
    GlobalEnv::Init();
    GlobalEnv::ResetPerTest();
    MatrixRig actual;
    MatrixRig patch;
    for (auto* rig : {&actual, &patch})
    {
        rig->Root().FillModulators(&rig->m_context);
        rig->Root().m_modulators.m_modulators[0]->FillModulators(&rig->m_context);
    }

    Cell& depth = *actual.Root().m_modulators.m_modulators[0];
    MatrixRig::SetSettled(depth, 0.9f, 0.9f);
    MatrixRig::SetSettled(*depth.m_modulators.m_modulators[5], 0.8f, 0.8f);
    MatrixRig::SetSettled(*actual.Root().m_modulators.m_modulators[2], 0.8f, 0.8f);
    actual.AddGesture(depth, 3, 0.8f, 0.8f, true, true);
    actual.AddGesture(depth, 5, 0.7f, 0.7f, true, true);
    Cell* oldGesture = depth.m_modulators.m_gestures[3].get();

    Cell& newDepth = *patch.Root().m_modulators.m_modulators[0];
    MatrixRig::SetSettled(newDepth, 0.6f, 0.6f);
    MatrixRig::SetSettled(*newDepth.m_modulators.m_modulators[8], 0.9f, 0.9f);
    MatrixRig::SetSettled(*patch.Root().m_modulators.m_modulators[1], 0.95f, 0.95f);
    patch.AddGesture(newDepth, 3, 0.2f, 0.2f, true, true);
    patch.AddGesture(newDepth, 7, 0.4f, 0.4f, true, true);
    JsonArena saved(JsonArena::kDefaultCapacity);
    patch.Root().GarbageCollectModulators();
    actual.Root().FromJSON(patch.Root().ToJSON(saved));

    DOCTEST_REQUIRE(actual.Root().m_modulators.m_modulators[0].get() == &depth);
    DOCTEST_CHECK(depth.m_values[0] == doctest::Approx(0.9f));
    DOCTEST_CHECK(depth.m_targets[0] == doctest::Approx(0.6f));
    DOCTEST_CHECK_FALSE(actual.Root().m_modulators.m_modulators[2].get());
    DOCTEST_REQUIRE(actual.Root().m_modulators.m_modulators[1].get());
    DOCTEST_CHECK(actual.Root().m_modulators.m_modulators[1]->m_values[0] == 0.5f);
    DOCTEST_CHECK_FALSE(depth.m_modulators.m_modulators[5].get());
    DOCTEST_REQUIRE(depth.m_modulators.m_modulators[8].get());
    DOCTEST_CHECK(depth.m_modulators.m_modulators[8]->m_values[0] == 0.5f);
    DOCTEST_REQUIRE(depth.m_modulators.m_gestures[3].get() == oldGesture);
    DOCTEST_CHECK(oldGesture->m_values[0] == doctest::Approx(0.8f));
    DOCTEST_CHECK(oldGesture->m_targets[0] == doctest::Approx(0.2f));
    DOCTEST_CHECK_FALSE(depth.m_modulators.m_gestures[5].get());
    DOCTEST_REQUIRE(depth.m_modulators.m_gestures[7].get());
    DOCTEST_CHECK(depth.m_modulators.m_gestures[7]->m_values[0] == 0.5f);
    DOCTEST_CHECK(actual.Root().m_modulators.m_numActiveModulators == 2);
    DOCTEST_CHECK(depth.m_modulators.m_numActiveModulators == 1);
    DOCTEST_CHECK(depth.m_modulators.m_numActiveGestures == 2);
    DOCTEST_CHECK(depth.m_modulators.GetGestureIndex(0) == 3);
    DOCTEST_CHECK(depth.m_modulators.GetGestureIndex(1) == 7);

    for (size_t frame = 0; frame < SmartGrid::TickDistributor::x_maxSamplesToDistribute + 2; ++frame)
    {
        for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
        {
            SampleTimer::IncrementSample();
        }

        actual.Process(false);
    }

    DOCTEST_CHECK(depth.m_values[0] == doctest::Approx(0.6f));
    DOCTEST_CHECK(oldGesture->m_values[0] == doctest::Approx(0.2f));
    DOCTEST_CHECK(depth.m_modulators.m_modulators[8]->m_values[0] == doctest::Approx(0.9f));
    DOCTEST_CHECK(depth.m_modulators.m_gestures[7]->m_values[0] == doctest::Approx(0.4f));

    MatrixRig empty;
    JsonArena cleared(JsonArena::kDefaultCapacity);
    actual.Root().FromJSON(empty.Root().ToJSON(cleared));
    DOCTEST_CHECK(actual.Root().m_modulators.m_numActiveModulators == 0);
    DOCTEST_CHECK(actual.Root().m_modulators.m_numActiveGestures == 0);
}

namespace
{
Cell* CorrespondingCell(Cell* cell, Cell* root, Cell* reference)
{
    if (cell == root)
    {
        return reference;
    }

    Cell* parent = CorrespondingCell(cell->m_parent, root, reference);
    return cell->m_type == SmartGrid::EncoderType::ModulatorAmount
        ? parent->m_modulators.m_modulators[cell->m_index].get()
        : parent->m_modulators.m_gestures[cell->m_index].get();
}
}

DOCTEST_TEST_CASE("encoder matrix: visible value readers follow nested graphs editing and rebuilds without materializing cells")
{
    GlobalEnv::ResetPerTest();
    MatrixRig actual;
    MatrixRig reference;
    actual.ConfigureGestures();
    reference.ConfigureGestures();
    std::array<EncoderMatrix::ValueReader, 5> readers;
    size_t generation = 0;
    size_t graphFrames = 0;
    size_t editingFrames = 0;
    size_t staleCopies = 0;
    auto& matrix = actual.m_bank.m_matrix;
    Cell* cells[] = {
        &actual.Root(),
        actual.Root().m_modulators.m_modulators[2].get(),
        actual.Root().m_modulators.m_modulators[2]->m_modulators.m_modulators[4].get(),
        actual.Root().m_modulators.m_modulators[7].get(),
        actual.Root().m_modulators.m_gestures[1].get()
    };
    for (size_t frame = 0; frame < 6000 + SmartGrid::TickDistributor::x_maxSamplesToDistribute; ++frame)
    {
        for (auto* rig : {&actual, &reference})
        {
            rig->m_context.m_sceneManager.m_blendFactor = static_cast<float>(frame % 173) / 172.0f;
            if (frame == 1000)
            {
                rig->Root().m_modulators.m_modulators[2]->Increment(0.1f);
            }

            if (frame == 5000)
            {
                rig->m_context.m_sceneManager.m_scene2 = 3;
            }

            for (size_t source = 0; source < Cell::x_numModulators; ++source)
            {
                for (size_t voice = 0; voice < 4; ++voice)
                {
                    rig->Sources().m_hostValue[source][voice] = static_cast<float>((frame + source * 7 + voice * 13) % 97) / 96.0f;
                }
            }

            for (size_t gesture = 0; gesture < Cell::x_numGestureParams; ++gesture)
            {
                rig->Gestures().m_gestureWeights[gesture] = static_cast<float>((frame + gesture * 11) % 101) / 100.0f;
            }
        }

        if (frame == 4000)
        {
            matrix.m_rebuild = true;
        }

        SampleTimer::s_instance->m_sample += SampleTimer::x_oversample;
        actual.Process(false);
        reference.Process(true);
        if (generation != matrix.m_generation)
        {
            generation = matrix.m_generation;
            for (size_t i = 0; i < readers.size(); ++i)
            {
                readers[i] = matrix.BindValue(cells[i]);
            }
        }

        bool prepared = actual.Root().m_preparedValues;
        for (size_t i = 0; i < readers.size(); ++i)
        {
            Cell* expected = CorrespondingCell(cells[i], &actual.Root(), &reference.Root());
            QuadFloat before = cells[i]->m_output;
            QuadFloat value = readers[i].Read(actual.m_context.m_sceneManager);
            for (size_t voice = 0; voice < 4; ++voice)
            {
                DOCTEST_REQUIRE(value[voice] == doctest::Approx(expected->m_output[voice]).epsilon(0.000001));
                DOCTEST_REQUIRE(cells[i]->m_output[voice] == before[voice]);
            }

            staleCopies += value[0] != before[0];
        }

        DOCTEST_REQUIRE(actual.Root().m_preparedValues == prepared);
        graphFrames += matrix.m_roots[0].m_path == EncoderMatrix::Path::Graph;
        editingFrames += matrix.m_roots[0].m_path == EncoderMatrix::Path::Editing;
    }

    DOCTEST_CHECK(graphFrames > 1000);
    DOCTEST_CHECK(editingFrames > 0);
    DOCTEST_CHECK(staleCopies > 1000);
}

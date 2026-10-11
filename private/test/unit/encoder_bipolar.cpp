#include "doctest.h"

#include <algorithm>
#include <array>
#include <map>
#include <random>
#include <vector>

#include "EncoderBankBank.hpp"
#include "StateSaver.hpp"
#include "../support/GlobalEnv.hpp"

namespace
{

using Cell = SmartGrid::BankedEncoderCell;

struct EncoderRig
{
    SmartGridOneContext m_context;
    EncoderBankBank m_banks;

    EncoderRig(bool bipolar, float defaultValue)
        : m_banks(2, 2, 2, &m_context)
    {
        GlobalEnv::Init();
        m_banks.InitMode(0, 2);
        m_banks.InitMode(1, 2);
        m_banks.InitBank(1, 1, SmartGrid::Color::Blue);
        m_banks.CreateEncoder(1, 1, defaultValue, "OtherTrio", "OTHER", SmartGrid::Color::Blue, 0, bipolar);
        m_banks.PlaceEncoder(1, 1, 0, 0);
        m_banks.InitBank(0, 0, SmartGrid::Color::Red);
        m_banks.CreateEncoder(0, 0, defaultValue, "Carrier", "CAR", SmartGrid::Color::Red, 0, bipolar);
        m_banks.PlaceEncoder(0, 0, 0, 0);
        for (size_t i = 0; i < Cell::x_numModulators; ++i)
        {
            Sources().SetModulatorSkin(i, SmartGrid::Color::Red, nullptr);
        }
    }

    Cell& Carrier()
    {
        return *m_banks.GetEncoder(0);
    }

    Cell& OtherTrio()
    {
        return *m_banks.GetEncoder(1);
    }

    Cell::ModulatorValues& Sources()
    {
        return m_banks.m_bankModes[0].m_modulatorValues;
    }

    Cell& Depth(Cell& parent, size_t source)
    {
        parent.m_modulators.FillModulators(&parent, &m_context);
        return *parent.m_modulators.m_modulators[source];
    }

    void Press(int x, int y, bool shift)
    {
        m_context.m_sceneManager.m_shift = shift;
        m_banks.m_banks[0].Apply(SmartGrid::MessageIn(SmartGrid::MessageIn::Mode::EncoderPush, x, y));
        m_context.m_sceneManager.m_shift = false;
    }

    void Compute()
    {
        Carrier().SetStateRecursive();
        Carrier().SetModulatorsAffecting();
        Carrier().Compute();
        OtherTrio().SetStateRecursive();
        OtherTrio().SetModulatorsAffecting();
        OtherTrio().Compute();
    }
};

JSON StoredValues(JsonArena& arena, float position)
{
    JSON root = arena.Object();
    JSON values = arena.Array();
    for (size_t scene = 0; scene < SmartGrid::SceneManager::x_numScenes; ++scene)
    {
        values.AppendNew(arena.Real(position));
    }

    root.SetNew("values", values);
    return root;
}

}

DOCTEST_TEST_CASE("encoder_bipolar: new depths are centered and collectible")
{
    EncoderRig rig(false, 0.3f);
    Cell& depth = rig.Depth(rig.Carrier(), 0);
    DOCTEST_CHECK(depth.m_values[0] == 0.5f);
    DOCTEST_CHECK(depth.m_output[0] == 0.5f);
    DOCTEST_CHECK(depth.m_defaultValue == 0.5f);
    DOCTEST_CHECK(depth.CanBeGarbageCollected());
    depth.SetValue(0.0f, true);
    DOCTEST_CHECK_FALSE(depth.CanBeGarbageCollected());
}

DOCTEST_TEST_CASE("encoder_bipolar: visible neutral depths animate without adding modulation badges")
{
    GlobalEnv::ResetPerTest();
    EncoderRig rig(false, 0.3f);
    auto& bank = rig.m_banks.m_banks[0];
    bank.MakeSelection(0, 0, &rig.Carrier());
    EncoderBankUIState ui;

    for (float source : {0.2f, 0.8f})
    {
        rig.Sources().m_value[0][0] = source;
        rig.Sources().m_value[0][1] = 1.0f - source;
        rig.m_banks.Process();
        bank.PopulateUIState(&ui);

        DOCTEST_CHECK(ui.GetModulatorsAffecting(3, 3).IsZero());
        DOCTEST_CHECK(ui.GetModulatorsAffecting(0, 0).IsZero());
        DOCTEST_CHECK(ui.GetBrightness(0, 0) == doctest::Approx(source));
        DOCTEST_CHECK(ui.GetValue(0, 0, 0) == 0.5f);
        DOCTEST_CHECK(ui.GetValue(3, 3, 0) == doctest::Approx(0.3f));
    }
}

DOCTEST_TEST_CASE("encoder_bipolar: publishing live depth brightness does not recompute parameter values")
{
    GlobalEnv::ResetPerTest();
    EncoderRig rig(false, 0.3f);
    auto& bank = rig.m_banks.m_banks[0];
    bank.MakeSelection(0, 0, &rig.Carrier());
    rig.Sources().m_value[0][0] = 0.2f;
    bank.Apply(SmartGrid::MessageIn::EncoderSetMsg(0, 0, 1.0f));
    rig.m_banks.Process();
    DOCTEST_REQUIRE(rig.Carrier().GetValueNoSlew(0) == doctest::Approx(0.2f));

    rig.Sources().m_value[0][0] = 0.8f;
    rig.Sources().ComputeChanged();
    auto& depth = *rig.Carrier().m_modulators.m_modulators[0];
    DOCTEST_CHECK(depth.GetBrightness() == doctest::Approx(0.8f));
    EncoderBankUIState ui;
    bank.PopulateUIState(&ui);
    DOCTEST_CHECK(ui.GetBrightness(0, 0) == doctest::Approx(0.8f));
    DOCTEST_CHECK(ui.GetBrightness(3, 3) == 0.0f);
    DOCTEST_CHECK(ui.GetValue(3, 3, 0) == doctest::Approx(0.2f));
    DOCTEST_CHECK(rig.Carrier().GetValueNoSlew(0) == doctest::Approx(0.2f));
}

DOCTEST_TEST_CASE("encoder_bipolar: first depth turn activates modulation and returning to zero clears it")
{
    GlobalEnv::ResetPerTest();
    EncoderRig rig(false, 0.3f);
    auto& bank = rig.m_banks.m_banks[0];
    bank.MakeSelection(0, 0, &rig.Carrier());
    rig.Sources().m_value[14][0] = 0.8f;
    rig.Sources().m_value[14][1] = 0.2f;

    bank.Apply(SmartGrid::MessageIn(1000000, -1, SmartGrid::MessageIn::Mode::EncoderIncDec, 2, 3, 500));
    rig.m_banks.Process();
    DOCTEST_CHECK(rig.Carrier().GetValueNoSlew(0) == doctest::Approx(0.8f));
    DOCTEST_CHECK(rig.Carrier().GetValueNoSlew(1) == doctest::Approx(0.2f));
    EncoderBankUIState ui;
    bank.PopulateUIState(&ui);
    DOCTEST_CHECK(ui.GetModulatorsAffecting(3, 3).m_bits == (1 << 14));
    DOCTEST_CHECK(ui.GetValue(2, 3, 0) == 1.0f);

    bank.Apply(SmartGrid::MessageIn(2000000, -1, SmartGrid::MessageIn::Mode::EncoderIncDec, 2, 3, -500));
    rig.m_banks.Process();
    DOCTEST_CHECK(rig.Carrier().GetValueNoSlew(0) == doctest::Approx(0.3f));
    DOCTEST_CHECK(rig.Carrier().GetValueNoSlew(1) == doctest::Approx(0.3f));
    bank.PopulateUIState(&ui);
    DOCTEST_CHECK(ui.GetModulatorsAffecting(3, 3).IsZero());
    DOCTEST_CHECK(ui.GetValue(2, 3, 0) == 0.5f);
    DOCTEST_CHECK(ui.GetMinValue(2, 3, 0) == 0.5f);
    DOCTEST_CHECK(ui.GetMaxValue(2, 3, 0) == 0.5f);
}

DOCTEST_TEST_CASE("encoder_bipolar: nested absolute depth edits update ancestor dependencies")
{
    GlobalEnv::ResetPerTest();
    EncoderRig rig(false, 0.3f);
    auto& bank = rig.m_banks.m_banks[0];
    bank.MakeSelection(0, 0, &rig.Carrier());
    auto& depth = *rig.Carrier().m_modulators.m_modulators[0];
    bank.MakeSelection(0, 0, &depth);
    rig.Sources().m_value[0][0] = 0.8f;
    rig.Sources().m_value[1][0] = 1.0f;
    bank.Apply(SmartGrid::MessageIn::EncoderSetMsg(1, 0, 1.0f));
    rig.m_banks.Process();
    DOCTEST_CHECK(rig.Carrier().GetValueNoSlew(0) == doctest::Approx(0.8f));
    DOCTEST_CHECK(rig.Carrier().m_modulatorsAffecting.m_bits == 3);

    rig.Sources().m_value[1][0] = 0.5f;
    rig.m_banks.Process();
    DOCTEST_CHECK(rig.Carrier().GetValueNoSlew(0) == doctest::Approx(0.3f));
}

DOCTEST_TEST_CASE("encoder_bipolar: shift-clicking a visible depth clears its route and displayed value")
{
    GlobalEnv::ResetPerTest();
    EncoderRig rig(false, 0.3f);
    auto& bank = rig.m_banks.m_banks[0];
    rig.Press(0, 0, false);
    rig.Sources().m_value[0][0] = 0.8f;
    bank.Apply(SmartGrid::MessageIn::EncoderSetMsg(0, 0, 1.0f));
    rig.m_banks.Process();
    DOCTEST_REQUIRE(rig.Carrier().GetValueNoSlew(0) == doctest::Approx(0.8f));

    rig.Press(0, 0, true);
    rig.m_banks.Process();
    EncoderBankUIState ui;
    bank.PopulateUIState(&ui);
    DOCTEST_CHECK(ui.GetModulatorsAffecting(3, 3).IsZero());
    DOCTEST_CHECK(ui.GetValue(3, 3, 0) == doctest::Approx(0.3f));
    DOCTEST_CHECK(ui.GetValue(0, 0, 0) == 0.5f);
}

DOCTEST_TEST_CASE("encoder_bipolar: shift-clicking the selected parent keeps its modulation page usable")
{
    for (bool nested : {false, true})
    {
        GlobalEnv::ResetPerTest();
        EncoderRig rig(false, 0.3f);
        auto& bank = rig.m_banks.m_banks[0];
        bank.Apply(SmartGrid::MessageIn::EncoderSetMsg(0, 0, 1.0f));
        rig.Press(0, 0, false);
        bank.Apply(SmartGrid::MessageIn::EncoderSetMsg(0, 0, 1.0f));
        if (nested)
        {
            rig.Press(0, 0, false);
            bank.Apply(SmartGrid::MessageIn::EncoderSetMsg(1, 0, 1.0f));
        }

        Cell* selected = bank.m_selected;
        rig.Press(3, 3, true);
        rig.m_banks.Process();
        EncoderBankUIState ui;
        bank.PopulateUIState(&ui);
        DOCTEST_REQUIRE(bank.m_selected == selected);
        DOCTEST_CHECK(ui.GetValue(3, 3, 0) == doctest::Approx(nested ? 0.5f : 0.3f));
        DOCTEST_CHECK(ui.GetModulatorsAffecting(3, 3).IsZero());
        for (int index = 0; index < 15; ++index)
        {
            DOCTEST_REQUIRE(selected->m_modulators.m_modulators[index]);
            DOCTEST_CHECK(bank.GetVisible(index % 4, index / 4) == selected->m_modulators.m_modulators[index].get());
            DOCTEST_CHECK(ui.GetConnected(index % 4, index / 4));
            DOCTEST_CHECK(ui.GetValue(index % 4, index / 4, 0) == 0.5f);
        }

        bank.Apply(SmartGrid::MessageIn::EncoderSetMsg(2, 3, 1.0f));
        rig.m_banks.Process();
        bank.PopulateUIState(&ui);
        DOCTEST_CHECK(ui.GetValue(2, 3, 0) == 1.0f);
        DOCTEST_CHECK(ui.GetModulatorsAffecting(3, 3).Get(14));
    }
}

DOCTEST_TEST_CASE("encoder_bipolar: seeded edits navigation scenes and shift resets preserve live controls and expected values")
{
    for (unsigned seed : {0u, 1u, 2u, 3u})
    {
        GlobalEnv::ResetPerTest();
        EncoderRig rig(false, 0.3f);
        auto& bank = rig.m_banks.m_banks[0];
        std::mt19937 random(seed);
        using Path = std::vector<int>;
        std::map<Path, std::array<float, 2>> expected;
        auto values = [&](const Path& path) -> std::array<float, 2>&
        {
            float initial = path.empty() ? 0.3f : 0.5f;
            return expected.try_emplace(path, std::array<float, 2>{initial, initial}).first->second;
        };
        Path selection;
        bool selected = false;
        int scene = 0;
        int actionCounts[7]{};

        // Disable source amplitudes so every displayed value has a simple,
        // independent oracle while modulation routes and ownership remain real.
        //
        for (int index = 0; index < 15; ++index)
        {
            for (int voice = 0; voice < 2; ++voice)
            {
                rig.Sources().m_amplitude[index][voice] = 0.0f;
            }
        }

        for (int step = 0; step < 512; ++step)
        {
            int action = static_cast<int>(random() % 7);
            ++actionCounts[action];
            DOCTEST_INFO("seed=" << seed << " step=" << step << " action=" << action);
            int index = selected ? static_cast<int>(random() % 16) : 0;
            if (action == 4)
            {
                index %= 15;
            }
            else if (action == 5 && selected)
            {
                index = 15;
            }

            Path target = selection;
            if (selected && index < 15)
            {
                target.push_back(index);
            }

            auto& targetValues = values(target);
            if (action == 0)
            {
                if (!selected)
                {
                    rig.Press(0, 0, false);
                    selected = true;
                }
                else if (selection.size() < 2)
                {
                    index %= 15;
                    rig.Press(index % 4, index / 4, false);
                    selection.push_back(index);
                }
                else
                {
                    rig.Press(3, 3, false);
                    selected = false;
                    selection.clear();
                }
            }
            else if (action == 1 && selected)
            {
                rig.Press(3, 3, false);
                selected = false;
                selection.clear();
            }
            else if (action == 2)
            {
                const int x_amounts[] = {0, 4096, 12288, 16383};
                int amount = x_amounts[random() % 4];
                bank.Apply(SmartGrid::MessageIn(SmartGrid::MessageIn::Mode::EncoderSet, index % 4, index / 4, amount));
                targetValues[scene] = static_cast<float>(amount) / 16383.0f;
            }
            else if (action == 3)
            {
                int delta = random() % 2 ? 20 : -20;
                bank.Apply(SmartGrid::MessageIn(static_cast<size_t>(step + 1) * 1000000, -1,
                    SmartGrid::MessageIn::Mode::EncoderIncDec, index % 4, index / 4, delta));
                targetValues[scene] = std::clamp(targetValues[scene] + static_cast<float>(delta) / 1000.0f, 0.0f, 1.0f);
            }
            else if (action == 4 || action == 5)
            {
                rig.Press(index % 4, index / 4, true);
                for (auto& entry : expected)
                {
                    if (entry.first.size() >= target.size() && std::equal(target.begin(), target.end(), entry.first.begin()))
                    {
                        entry.second[scene] = entry.first.empty() ? 0.3f : 0.5f;
                    }
                }
            }
            else if (action == 6)
            {
                scene = 1 - scene;
                rig.m_context.m_sceneManager.m_blendFactor = static_cast<float>(scene);
            }

            rig.m_context.m_sceneManager.Process();
            rig.m_banks.Process();
            Cell* owner = &rig.Carrier();
            for (int part : selection)
            {
                DOCTEST_REQUIRE(owner->m_modulators.m_modulators[part]);
                owner = owner->m_modulators.m_modulators[part].get();
            }

            DOCTEST_REQUIRE(bank.m_selected == (selected ? owner : nullptr));
            EncoderBankUIState ui;
            bank.PopulateUIState(&ui);
            for (int slot = 0; slot < 16; ++slot)
            {
                int x = slot % 4;
                int y = slot / 4;
                if (!selected && slot != 0)
                {
                    DOCTEST_REQUIRE(bank.GetVisible(x, y) == nullptr);
                    DOCTEST_CHECK_FALSE(ui.GetConnected(x, y));
                    continue;
                }

                Path path = selection;
                Cell* cell = owner;
                if (selected && slot < 15)
                {
                    DOCTEST_REQUIRE(owner->m_modulators.m_modulators[slot]);
                    cell = owner->m_modulators.m_modulators[slot].get();
                    path.push_back(slot);
                }

                DOCTEST_REQUIRE(bank.GetVisible(x, y) == cell);
                DOCTEST_CHECK(ui.GetConnected(x, y));
                for (int storedScene = 0; storedScene < 2; ++storedScene)
                {
                    DOCTEST_CHECK(cell->m_values[storedScene] == doctest::Approx(values(path)[storedScene]));
                }

                for (int voice = 0; voice < 2; ++voice)
                {
                    DOCTEST_CHECK(ui.GetValue(x, y, voice) == doctest::Approx(values(path)[scene]));
                }
            }

            DOCTEST_CHECK(rig.OtherTrio().GetValueNoSlew(0) == doctest::Approx(0.3f));
        }

        for (int count : actionCounts)
        {
            DOCTEST_CHECK(count > 0);
        }
    }
}

DOCTEST_TEST_CASE("encoder_bipolar: depth crossfades toward normal and inverted sources")
{
    EncoderRig rig(false, 0.3f);
    Cell& depth = rig.Depth(rig.Carrier(), 0);
    rig.Sources().m_value[0][0] = 0.8f;
    const float positions[] = {0.5f, 1.0f, 0.0f, 0.75f, 0.25f};
    const float outputs[] = {0.3f, 0.8f, 0.2f, 0.425f, 0.275f};
    for (size_t i = 0; i < 5; ++i)
    {
        depth.SetValue(positions[i], true);
        rig.Compute();
        DOCTEST_CHECK(rig.Carrier().m_output[0] == doctest::Approx(outputs[i]));
    }

    DOCTEST_CHECK(rig.Carrier().m_minValue[0] == doctest::Approx(0.225f));
    DOCTEST_CHECK(rig.Carrier().m_maxValue[0] == doctest::Approx(0.475f));
}

DOCTEST_TEST_CASE("encoder_bipolar: mixed signs normalize absolute weights per voice")
{
    EncoderRig rig(false, 0.3f);
    rig.Depth(rig.Carrier(), 0).SetValue(1.0f, true);
    rig.Depth(rig.Carrier(), 1).SetValue(0.0f, true);
    rig.Sources().m_value[0][0] = 0.8f;
    rig.Sources().m_value[1][0] = 0.2f;
    rig.Sources().m_amplitude[0][1] = 0.0f;
    rig.Sources().m_amplitude[1][1] = 0.25f;
    rig.Sources().m_value[1][1] = 0.8f;
    rig.Compute();
    DOCTEST_CHECK(rig.Carrier().m_output[0] == doctest::Approx(0.8f));
    DOCTEST_CHECK(rig.Carrier().m_minValue[0] == 0.0f);
    DOCTEST_CHECK(rig.Carrier().m_maxValue[0] == 1.0f);
    DOCTEST_CHECK(rig.Carrier().m_output[1] == doctest::Approx(0.275f));
}

DOCTEST_TEST_CASE("encoder_bipolar: recursive depth stays normalized until its parent reads it")
{
    EncoderRig rig(false, 0.3f);
    Cell& depth = rig.Depth(rig.Carrier(), 0);
    rig.Depth(depth, 1).SetValue(1.0f, true);
    rig.Sources().m_value[0][0] = 0.8f;
    rig.Sources().m_value[1][0] = 0.25f;
    rig.Compute();
    DOCTEST_CHECK(depth.m_output[0] == doctest::Approx(0.25f));
    DOCTEST_CHECK(rig.Carrier().m_output[0] == doctest::Approx(0.275f));
    DOCTEST_CHECK_FALSE(depth.CanBeGarbageCollected());
}

DOCTEST_TEST_CASE("encoder_bipolar: amplitude alone invalidates the modulation output")
{
    EncoderRig rig(false, 0.3f);
    rig.Depth(rig.Carrier(), 0).SetValue(1.0f, true);
    rig.Sources().m_value[0][0] = 0.8f;
    rig.Sources().ComputeChanged();
    rig.Compute();
    DOCTEST_CHECK(rig.Carrier().m_output[0] == doctest::Approx(0.8f));
    rig.Sources().m_amplitude[0][0] = 0.0f;
    rig.Sources().ComputeChanged();
    rig.Carrier().Compute();
    DOCTEST_CHECK(rig.Carrier().m_output[0] == doctest::Approx(0.3f));
}

DOCTEST_TEST_CASE("encoder_bipolar: unversioned JSON stores signed knob positions")
{
    EncoderRig rig(false, 0.3f);
    Cell& depth = rig.Depth(rig.Carrier(), 0);
    JsonArena arena(1024 * 1024);
    const float stored[] = {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f};
    for (float value : stored)
    {
        depth.StateEncoderCell::FromJSON(StoredValues(arena, value));
        DOCTEST_CHECK(depth.m_bankedValue == doctest::Approx((value + 1.0f) * 0.5f));
        JSON saved = depth.ToJSON(arena);
        DOCTEST_CHECK(saved.Get("values").Get("values").GetAt(0).NumberValue() == doctest::Approx(value));
        DOCTEST_CHECK(saved.Get("version").IsNull());
        DOCTEST_CHECK(saved.Get("bipolar").IsNull());
    }

    rig.Carrier().StateEncoderCell::FromJSON(StoredValues(arena, 0.3f));
    DOCTEST_CHECK(rig.Carrier().m_bankedValue == doctest::Approx(0.3f));
}

DOCTEST_TEST_CASE("encoder_bipolar: DSP getters convert once while UI and slew stay normalized")
{
    EncoderRig rig(true, 0.0f);
    DOCTEST_CHECK(rig.Carrier().m_values[0] == 0.5f);
    rig.Carrier().SetValue(0.25f, true);
    rig.Carrier().InitSlewState(0.25f);
    rig.Compute();
    DOCTEST_CHECK(rig.Carrier().GetValue() == -0.5f);
    DOCTEST_CHECK(rig.Carrier().m_bankedValue == 0.25f);
    DOCTEST_CHECK(rig.m_banks.GetValueByEncoderIndex(0, 0) == -0.5f);
    DOCTEST_CHECK(rig.m_banks.GetValueNoSlewByEncoderIndex(0, 0) == -0.5f);
    DOCTEST_CHECK(rig.m_banks.GetValue(0, 0, 0, 0) == -0.5f);
    DOCTEST_CHECK(rig.m_banks.GetValueNoSlew(0, 0, 0, 0) == -0.5f);
    DOCTEST_CHECK(rig.Carrier().m_slew[0].m_output == 0.25f);

    EncoderBankUIState ui;
    rig.m_banks.m_banks[0].PopulateUIState(&ui);
    DOCTEST_CHECK(ui.GetValue(0, 0, 0) == 0.25f);
    DOCTEST_CHECK(ui.GetBipolar(0, 0));
    rig.m_banks.m_banks[0].PlaceEncoder(0, 0, nullptr);
    rig.m_banks.m_banks[0].PopulateUIState(&ui);
    DOCTEST_CHECK_FALSE(ui.GetBipolar(0, 0));
}

DOCTEST_TEST_CASE("encoder_bipolar: centered scenes preserve negative routes and reset to neutral")
{
    EncoderRig rig(false, 0.3f);
    Cell& depth = rig.Depth(rig.Carrier(), 0);
    depth.SetValue(0.5f, true);
    depth.m_values[0] = 0.0f;
    depth.m_values[1] = 1.0f;
    depth.m_values[2] = 0.25f;
    rig.m_context.m_sceneManager.m_blendFactor = 0.5f;
    rig.Sources().m_value[0][0] = 0.8f;
    rig.Compute();
    DOCTEST_CHECK(rig.Carrier().m_output[0] == doctest::Approx(0.3f));
    DOCTEST_CHECK_FALSE(depth.CanBeGarbageCollected());
    DOCTEST_CHECK(rig.Carrier().m_modulatorsAffecting.Get(0));
    DOCTEST_CHECK_FALSE(rig.OtherTrio().m_modulatorsAffecting.Get(0));
    rig.m_context.m_sceneManager.m_blendFactor = 0.75f;
    rig.Compute();
    DOCTEST_CHECK(rig.Carrier().m_output[0] == doctest::Approx(0.425f));
    rig.Carrier().HandleShiftPress();
    rig.Carrier().Compute();
    DOCTEST_CHECK(rig.Carrier().m_output[0] == doctest::Approx(0.3f));
    DOCTEST_CHECK(rig.Carrier().m_modulatorsAffecting.IsZero());
    DOCTEST_CHECK(rig.Depth(rig.Carrier(), 0).m_values[0] == 0.5f);
    DOCTEST_CHECK(rig.Depth(rig.Carrier(), 0).m_values[2] == 0.25f);
}

DOCTEST_TEST_CASE("encoder_bipolar: shift reset preserves hidden gesture values")
{
    EncoderRig rig(false, 0.3f);
    Cell& carrier = rig.Carrier();
    carrier.m_modulators.AddGesture(&carrier, 0);
    Cell& gesture = *carrier.m_modulators.m_gestures[0];
    gesture.m_isActive[0] = true;
    gesture.m_isActive[2] = true;
    gesture.m_values[0] = 0.75f;
    gesture.m_values[2] = 0.25f;

    carrier.HandleShiftPress();

    DOCTEST_REQUIRE(carrier.m_modulators.m_gestures[0]);
    DOCTEST_CHECK(carrier.m_modulators.m_gestures[0]->m_isActive[2]);
    DOCTEST_CHECK(carrier.m_modulators.m_gestures[0]->m_values[2] == 0.25f);
}

DOCTEST_TEST_CASE("encoder_bipolar: recursive patches preserve signed gesture targets without reapplying the curve")
{
    EncoderRig rig(false, 0.3f);
    Cell& depth = rig.Depth(rig.Carrier(), 0);
    depth.SetValue(0.25f, true);
    depth.m_modulators.AddGesture(&depth, 0);
    Cell& gesture = *depth.m_modulators.m_gestures[0];
    gesture.SetActive(true);
    gesture.SetValue(0.75f, true);
    rig.Depth(depth, 1).SetValue(0.75f, true);
    rig.Sources().m_value[0][0] = 0.8f;
    rig.Sources().m_value[1][0] = 0.25f;
    rig.Sources().m_gestureWeights[0] = 1.0f;
    rig.Compute();
    const float expected = rig.Carrier().m_output[0];

    for (size_t iteration = 0; iteration < 5; ++iteration)
    {
        JsonArena arena(1024 * 1024);
        JSON saved = rig.Carrier().ToJSON(arena);
        JSON savedDepth = saved.Get("modulators").GetAt(0);
        DOCTEST_CHECK(savedDepth.Get("values").Get("values").GetAt(0).NumberValue() == -0.5);
        JSON savedGesture = savedDepth.Get("gestures").GetAt(0);
        DOCTEST_CHECK(savedGesture.Get("values").Get("values").GetAt(0).NumberValue() == 0.5);
        rig.Carrier().FromJSON(saved);
        rig.Compute();
        DOCTEST_CHECK(rig.Carrier().m_output[0] == doctest::Approx(expected));
    }
}

DOCTEST_TEST_CASE("encoder_bipolar: small signed depths meet continuously at zero")
{
    EncoderRig rig(false, 0.3f);
    Cell& depth = rig.Depth(rig.Carrier(), 0);
    rig.Sources().m_value[0][0] = 1.0f;
    depth.SetValue(0.50001f, true);
    rig.Compute();
    const float positive = rig.Carrier().m_output[0];
    depth.SetValue(0.49999f, true);
    rig.Compute();
    const float negative = rig.Carrier().m_output[0];
    DOCTEST_CHECK(positive > 0.3f);
    DOCTEST_CHECK(negative < 0.3f);
    DOCTEST_CHECK(positive - negative < 0.00001f);
}

DOCTEST_TEST_CASE("encoder_bipolar: gesture sweeps a bipolar parameter across zero")
{
    EncoderRig rig(true, -0.5f);
    Cell& carrier = rig.Carrier();
    carrier.m_modulators.AddGesture(&carrier, 0);
    Cell& gesture = *carrier.m_modulators.m_gestures[0];
    gesture.SetActive(true);
    gesture.SetValue(0.75f, true);
    rig.Compute();
    DOCTEST_REQUIRE(gesture.m_bipolar);

    const float weights[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 0.0f};
    const float expected[] = {-0.5f, -0.25f, 0.0f, 0.25f, 0.5f, -0.5f};
    for (size_t i = 0; i < 6; ++i)
    {
        rig.Sources().m_gestureWeights[0] = weights[i];
        rig.Sources().ComputeChanged();
        carrier.Compute();
        for (size_t voice = 0; voice < 2; ++voice)
        {
            DOCTEST_CHECK(carrier.GetValueNoSlew(voice) == doctest::Approx(expected[i]));
        }

        DOCTEST_CHECK(rig.OtherTrio().GetValueNoSlew(0) == -0.5f);
    }
}

DOCTEST_TEST_CASE("encoder_bipolar: reset without gestures restores the signed default and slew")
{
    EncoderRig rig(true, -0.5f);
    Cell& carrier = rig.Carrier();
    carrier.SetValue(0.875f, true);
    carrier.m_values[2] = 0.625f;
    carrier.InitSlewState(0.875f);
    rig.Compute();
    DOCTEST_REQUIRE(carrier.GetValueNoSlew(0) == 0.75f);
    DOCTEST_REQUIRE(carrier.GetSlewedValue(0) == 0.75f);

    rig.m_banks.ResetGrid(0);
    carrier.Compute();
    DOCTEST_CHECK(carrier.GetValue() == -0.5f);
    DOCTEST_CHECK(carrier.m_values[2] == 0.625f);
    for (size_t voice = 0; voice < 2; ++voice)
    {
        DOCTEST_CHECK(carrier.GetValueNoSlew(voice) == -0.5f);
        DOCTEST_CHECK(carrier.GetSlewedValue(voice) == -0.5f);
        DOCTEST_CHECK(carrier.m_slew[voice].m_output == 0.25f);
    }
}

DOCTEST_TEST_CASE("encoder_bipolar: all-scene reset restores hidden scenes across independent cells")
{
    EncoderRig rig(true, -0.5f);
    Cell& carrier = rig.Carrier();
    for (Cell* cell : {&carrier, &rig.OtherTrio()})
    {
        for (size_t scene = 0; scene < SmartGrid::SceneManager::x_numScenes; ++scene)
        {
            cell->m_values[scene] = 0.875f;
        }
    }

    rig.m_banks.RevertToDefault(true);

    for (Cell* cell : {&carrier, &rig.OtherTrio()})
    {
        for (size_t scene = 0; scene < SmartGrid::SceneManager::x_numScenes; ++scene)
        {
            DOCTEST_CHECK(cell->m_values[scene] == 0.25f);
        }
    }
}

DOCTEST_TEST_CASE("encoder_bipolar: reset with an active gesture restores the signed default")
{
    EncoderRig rig(true, -0.5f);
    Cell& carrier = rig.Carrier();
    carrier.SetValue(0.625f, true);
    carrier.m_modulators.AddGesture(&carrier, 0);
    Cell& gesture = *carrier.m_modulators.m_gestures[0];
    gesture.SetActive(true);
    gesture.SetValue(1.0f, true);
    rig.Sources().m_gestureWeights[0] = 1.0f;
    rig.Compute();
    carrier.InitSlewState(1.0f);
    DOCTEST_REQUIRE(carrier.GetValueNoSlew(0) == 1.0f);
    DOCTEST_REQUIRE(carrier.m_gesturesAffecting.Get(0));

    carrier.RevertToDefault(false);
    carrier.Compute();
    DOCTEST_CHECK(carrier.GetValue() == -0.5f);
    DOCTEST_CHECK(carrier.GetValueNoSlew(0) == -0.5f);
    DOCTEST_CHECK(carrier.GetSlewedValue(0) == -0.5f);
    DOCTEST_CHECK(carrier.m_gesturesAffecting.IsZero());
    DOCTEST_CHECK(carrier.m_modulators.m_gestures[0].get() == nullptr);

    const float weights[] = {0.0f, 0.5f, 1.0f};
    for (float weight : weights)
    {
        rig.Sources().m_gestureWeights[0] = weight;
        rig.Sources().ComputeChanged();
        carrier.Compute();
        DOCTEST_CHECK(carrier.GetValueNoSlew(0) == -0.5f);
    }
}

DOCTEST_TEST_CASE("encoder events: nested paths distinguish gestures and modulators in patch units")
{
    EncoderRig rig(false, 0.25f);
    auto& depth = rig.Depth(rig.Carrier(), 2);
    depth.m_modulators.AddGesture(&depth, 3);
    auto& gesture = *depth.m_modulators.m_gestures[3];
    gesture.SetAndRecordValue(0.75f, 2);
    const auto event = ParamEvent::MkEncoderSet(&gesture, 2, 123);
    DOCTEST_CHECK(std::string(event.m_name) == "Carrier");
    DOCTEST_CHECK(event.m_encoderPath[0] == 2);
    DOCTEST_CHECK(event.m_encoderPath[1] == 131);
    DOCTEST_CHECK(event.m_encoderPath[2] == -1);
    float value = 0;
    std::memcpy(&value, event.m_value, sizeof(value));
    DOCTEST_CHECK(value == 0.5f);
    const auto root = ParamEvent::MkEncoderSet(&rig.Carrier(), 0, 123);
    DOCTEST_CHECK(root.m_encoderPath[0] == -1);
    gesture.SetActive(true, 2);
    const auto active = ParamEvent::MkEncoderActivate(&gesture, 2, 123);
    DOCTEST_CHECK(active.m_encoderPath[0] == 2);
    DOCTEST_CHECK(active.m_encoderPath[1] == 131);
    DOCTEST_CHECK(active.m_encoderPath[2] == -1);
    DOCTEST_CHECK(active.m_value[0] == 1);
}

DOCTEST_TEST_CASE("state events: copying another scene captures that scene's stored value")
{
    SmartGridOneContext context;
    uint8_t live = 3;
    State state("test", sizeof(live), reinterpret_cast<char*>(&live), 8, &context.m_paramEventLogger);
    uint8_t stored = 7;
    state.SetBytes(reinterpret_cast<char*>(&stored), 2);
    const auto event = ParamEvent::MkStateChange(&state, 2, 99);
    DOCTEST_CHECK(event.m_scene == 2);
    DOCTEST_CHECK(event.m_value[0] == 7);
    DOCTEST_CHECK(live == 3);
}

DOCTEST_TEST_CASE("state events: default scene survives switching before the first save")
{
    SmartGridOneContext context;
    ScenedStateSaver saver(&context);
    int value = -1;
    State* state = saver.Insert("default", &value);
    saver.Finalize();
    state->LoadValFromScene(1);
    DOCTEST_CHECK(value == -1);
    state->LoadValFromScene(0);
    DOCTEST_CHECK(value == -1);
}

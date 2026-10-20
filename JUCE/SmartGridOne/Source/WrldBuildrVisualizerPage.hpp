#pragma once

#include "EncoderComponent.hpp"
#include "EncoderModulatorVisualizers.hpp"
#include "NonagonWrapper.hpp"
#include "MeterComponent.hpp"
#include "MasteringComponents.hpp"
#include "PartialMachineComponents.hpp"
#include "QuadDelayEnvelopeVisualizerComponent.hpp"
#include "SampleTrioWaveformVisualizerComponent.hpp"
#include "SequencerMelodyVisualizerComponent.hpp"
#include "VoiceConfigComponent.hpp"
#include "VisualizerRegion.hpp"
#include <array>
#include <vector>

struct WrldBuildrVisualizerPage : public juce::Component
{
    static constexpr int x_gridWidth = 64;
    static constexpr int x_gridHeight = 48;
    static constexpr int x_visualizerSize = 21;
    static constexpr int x_numVisualizerSlots = 3;
    static constexpr float x_visualizerGap = static_cast<float>(
        x_gridWidth - x_numVisualizerSlots * x_visualizerSize) / (x_numVisualizerSlots - 1);
    static constexpr int x_encoderSize = 8;
    static constexpr int x_badgeHeight = 2;
    static constexpr int x_encoderRowHeight = x_encoderSize + x_badgeHeight;
    static constexpr int x_encoderY = x_gridHeight - 2 * x_encoderRowHeight;
    static constexpr int x_meterWidth = 48;
    static constexpr int x_meterHeight = 4;
    static constexpr int x_meterX = x_gridWidth - x_meterWidth;
    static constexpr int x_meterY = x_encoderY - x_meterHeight;
    static constexpr int x_visualizerY = x_meterY - x_visualizerSize;
    static constexpr int x_configHeight = 15;

    struct VisualizerSlot
    {
        SmartGridOneEncoders::Bank m_bank;
        int m_slot;
        int m_slotSpan;
        VisualizerRegion m_region;
        std::unique_ptr<SmartGridOneMainVisualizerComponent> m_visualizer;
    };

    NonagonWrapper* m_nonagon;
    int m_scopeVoiceOffset = -1;
    ModulatorVisualizers m_modulatorVisualizers;
    MeteringComponent m_meteringComponent;
    std::vector<VisualizerSlot> m_slots;
    std::array<std::unique_ptr<EncoderComponent>, 16> m_encoders;
    VoiceConfigComponent m_voiceConfigComponent;
    float m_cellSize = 0.0f;
    juce::Point<float> m_gridOrigin;

    explicit WrldBuildrVisualizerPage(NonagonWrapper* nonagon)
        : m_nonagon(nonagon)
        , m_meteringComponent(nonagon->GetUIState())
        , m_voiceConfigComponent(nonagon)
    {
        auto* uiState = m_nonagon->GetUIState();
#define F(name, bank, slot, span, region, ctor, flags) \
        static_assert(slot >= 0 && span > 0 && slot + span <= x_numVisualizerSlots, \
            "Visualizer " #name " must fit within the three-slot row"); \
        { \
            auto visualizer = ctor; \
            visualizer->m_sourceMachineFlags = flags; \
            m_slots.push_back({SmartGridOneEncoders::Bank::bank, slot, span, \
                VisualizerRegion::region, std::move(visualizer)}); \
        }
#include "ForEachSmartGridOneVisualizer.hpp"
#undef F

        InitializeEncoderModulatorVisualizers(m_modulatorVisualizers, uiState);
        addAndMakeVisible(m_meteringComponent);
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                auto& encoder = m_encoders[row * 4 + column];
                encoder = std::make_unique<EncoderComponent>(
                    nonagon->MkEncoderBankUI(), column, row, row % 2 == 1,
                    m_modulatorVisualizers);
                addAndMakeVisible(encoder.get());
            }
        }

        addChildComponent(m_voiceConfigComponent);
        UpdateConfigModeVisibility();
    }

    juce::Rectangle<int> GetGridBounds(float x, float y, float width, float height) const
    {
        int left = juce::roundToInt(m_gridOrigin.x + x * m_cellSize);
        int top = juce::roundToInt(m_gridOrigin.y + y * m_cellSize);
        int right = juce::roundToInt(m_gridOrigin.x + (x + width) * m_cellSize);
        int bottom = juce::roundToInt(m_gridOrigin.y + (y + height) * m_cellSize);
        return {left, top, right - left, bottom - top};
    }

    juce::Rectangle<int> GetSlotBounds(const VisualizerSlot& slot) const
    {
        auto bounds = GetGridBounds(slot.m_slot * (x_visualizerSize + x_visualizerGap),
            x_visualizerY, slot.m_slotSpan * x_visualizerSize
                + (slot.m_slotSpan - 1) * x_visualizerGap, x_visualizerSize);
        return GetVisualizerRegionBounds(bounds, slot.m_region);
    }

    void resized() override
    {
        m_cellSize = std::min(static_cast<float>(getWidth()) / x_gridWidth,
            static_cast<float>(getHeight()) / x_gridHeight);
        m_gridOrigin = {(getWidth() - x_gridWidth * m_cellSize) * 0.5f,
            (getHeight() - x_gridHeight * m_cellSize) * 0.5f};

        m_meteringComponent.setBounds(
            GetGridBounds(x_meterX, x_meterY, x_meterWidth, x_meterHeight));
        m_voiceConfigComponent.setBounds(
            GetGridBounds(x_meterX, x_encoderY - x_configHeight, x_meterWidth, x_configHeight));

        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                int displayColumn = (row / 2) * 4 + column;
                int x = displayColumn * x_encoderSize;
                int y = x_encoderY + (row % 2) * x_encoderRowHeight;
                m_encoders[row * 4 + column]->setBounds(
                    GetGridBounds(x, y, x_encoderSize, x_encoderRowHeight));
            }
        }
    }

    // The controls remain owned by MainComponent, retaining their existing
    // callbacks and CPU source. Only their position changes on this page.
    //
    void LayoutStatusControls(juce::Label& cpu, juce::TextButton& file, juce::TextButton& config)
    {
        auto origin = getPosition();
        int buttonWidth = x_meterX / 3;
        int cpuWidth = x_meterX - 2 * buttonWidth;
        file.setBounds(GetGridBounds(0, x_meterY, buttonWidth, x_meterHeight) + origin);
        config.setBounds(GetGridBounds(buttonWidth, x_meterY, buttonWidth, x_meterHeight) + origin);
        cpu.setBounds(GetGridBounds(2 * buttonWidth, x_meterY, cpuWidth, x_meterHeight) + origin);
        cpu.toFront(false);
        file.toFront(false);
        config.toFront(false);
    }

    bool ShouldShowSlot(const VisualizerSlot& slot)
    {
        auto bank = SmartGridOneEncoders::BankForTrio(
            m_nonagon->GetSelectedEncoderBank(), 0);
        if (bank != slot.m_bank)
        {
            return false;
        }

        auto* uiState = m_nonagon->GetSquiggleBoyUIState();
        size_t baseVoiceIx = uiState->m_activeTrack.load() * SquiggleBoy::x_voicesPerTrack;
        for (size_t i = 0; i < SquiggleBoy::x_voicesPerTrack; ++i)
        {
            auto machine = uiState->m_voiceSourceUIState[baseVoiceIx + i].m_sourceMachine.load();
            if (slot.m_visualizer->ShouldShowForMachine(machine))
            {
                return true;
            }
        }

        return false;
    }

    void UpdateConfigModeVisibility()
    {
        bool isConfigMode = m_nonagon->GetGridsMode()
            == TheNonagonSquiggleBoyWrldBldr::GridsMode::Config;
        m_voiceConfigComponent.setVisible(isConfigMode);
        m_meteringComponent.setVisible(!isConfigMode);
    }

    void paint(juce::Graphics& g) override
    {
        UpdateConfigModeVisibility();
        g.fillAll(juce::Colours::black);
        for (const auto& slot : m_slots)
        {
            if (ShouldShowSlot(slot))
            {
                auto bounds = GetSlotBounds(slot);
                if (bounds.isEmpty())
                {
                    continue;
                }

                juce::Graphics::ScopedSaveState savedState(g);
                g.reduceClipRegion(bounds);
                g.setOrigin(bounds.getPosition());
                slot.m_visualizer->Draw(g, bounds.withPosition(0, 0));
            }
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        for (const auto& slot : m_slots)
        {
            auto bounds = GetSlotBounds(slot);
            if (bounds.contains(event.getPosition()) && ShouldShowSlot(slot))
            {
                slot.m_visualizer->OnClick(event.withNewPosition(event.position - bounds.getPosition().toFloat()));
            }
        }
    }
};

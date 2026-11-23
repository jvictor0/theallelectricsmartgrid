#pragma once

#include "EncoderComponent.hpp"
#include "EncoderModulatorVisualizers.hpp"
#include "FourteenSegmentDisplayComponent.hpp"
#include "LFOTargetVisualizers.hpp"
#include "NonagonWrapper.hpp"
#include "MeterComponent.hpp"
#include "MasteringComponents.hpp"
#include "PartialMachineComponents.hpp"
#include "QuadDelayEnvelopeVisualizerComponent.hpp"
#include "SampleTrioWaveformVisualizerComponent.hpp"
#include "SequencerMelodyVisualizerComponent.hpp"
#include "TuningVisualizerComponent.hpp"
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
    static constexpr int x_statusButtonWidth = 5;
    static constexpr int x_statusDisplayWidth = 8;
    static constexpr int x_statusGap = 1;
    static constexpr float x_statusTopPadding = 0.3f;
    static constexpr float x_statusBottomPadding = 0.6f;
    static constexpr int x_configButtonX = x_statusButtonWidth + x_statusGap;
    static constexpr int x_cpuDisplayX = x_configButtonX + x_statusButtonWidth + x_statusGap;
    static constexpr int x_recordingDisplayX = x_cpuDisplayX + x_statusDisplayWidth + x_statusGap;

    struct StatusDisplay : FourteenSegmentDisplayComponent
    {
        static constexpr int x_numStatusChars = 6;

        StatusDisplay()
        {
            SetNumChars(x_numStatusChars);
            SetOnColor(juce::Colours::lightgrey);
            SetOffColor(juce::Colour(0xff343434));
        }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            const float inset = bounds.getHeight() * 0.12f;
            DrawDisplay(g, bounds.reduced(inset));
        }
    };

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
    std::array<SmartGridOneMainVisualizerComponent*, SmartGridOneParamVisualizers::x_numVisualizers>
        m_visualizers{};
    std::array<std::unique_ptr<EncoderComponent>, 16> m_encoders;
    VoiceConfigComponent m_voiceConfigComponent;
    StatusDisplay m_cpuDisplay;
    StatusDisplay m_recordingDisplay;
    uint64_t m_displayedRecordingSeconds = 0;
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
            m_visualizers[static_cast<size_t>(SmartGridOneParamVisualizers::Visualizer::name)] = visualizer.get(); \
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
        m_cpuDisplay.SetText("  0.0%");
        m_recordingDisplay.SetText("  0:00");
        m_recordingDisplay.SetOnColor(juce::Colours::white);
        addAndMakeVisible(m_cpuDisplay);
        addAndMakeVisible(m_recordingDisplay);
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

    juce::Rectangle<int> GetSlotBounds(int slot, int span, VisualizerRegion region) const
    {
        auto bounds = GetGridBounds(slot * (x_visualizerSize + x_visualizerGap),
            x_visualizerY, span * x_visualizerSize
                + (span - 1) * x_visualizerGap, x_visualizerSize);
        return GetVisualizerRegionBounds(bounds, region);
    }

    juce::Rectangle<int> GetStatusBounds(float x, float width) const
    {
        return GetGridBounds(x, x_statusTopPadding, width,
            x_visualizerY - x_statusTopPadding - x_statusBottomPadding);
    }

    void resized() override
    {
        m_cellSize = std::min(static_cast<float>(getWidth()) / x_gridWidth,
            static_cast<float>(getHeight()) / x_gridHeight);
        m_gridOrigin = {(getWidth() - x_gridWidth * m_cellSize) * 0.5f,
            (getHeight() - x_gridHeight * m_cellSize) * 0.5f};

        m_cpuDisplay.setBounds(GetStatusBounds(x_cpuDisplayX, x_statusDisplayWidth));
        m_recordingDisplay.setBounds(GetStatusBounds(x_recordingDisplayX, x_statusDisplayWidth));
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

    void LayoutStatusButtons(juce::TextButton& file, juce::TextButton& config)
    {
        auto origin = getPosition();
        file.setBounds(GetStatusBounds(0, x_statusButtonWidth) + origin);
        config.setBounds(GetStatusBounds(x_configButtonX, x_statusButtonWidth) + origin);
        file.toFront(false);
        config.toFront(false);
    }

    void UpdateStatusDisplays(float cpuUsage)
    {
        cpuUsage = juce::jlimit(0.0f, 100.0f, cpuUsage);
        auto cpuText = juce::String(cpuUsage, 1) + "%";
        m_cpuDisplay.SetText(cpuText.paddedLeft(' ', StatusDisplay::x_numStatusChars));

        auto cpuColor = juce::Colours::lightgrey;
        if (cpuUsage > 90.0f)
        {
            cpuColor = juce::Colours::orange.interpolatedWith(juce::Colours::red, (cpuUsage - 90.0f) / 10.0f);
        }
        else if (cpuUsage > 80.0f)
        {
            cpuColor = juce::Colours::yellow.interpolatedWith(juce::Colours::orange, (cpuUsage - 80.0f) / 10.0f);
        }

        m_cpuDisplay.SetOnColor(cpuColor);

        m_recordingDisplay.SetOnColor(m_nonagon->IsRecording() ? juce::Colours::red : juce::Colours::white);
        static constexpr uint64_t x_maxSeconds = 999 * 60 + 59;
        const uint64_t elapsedSeconds = std::min(m_nonagon->GetRecordingElapsedSeconds(), x_maxSeconds);
        if (elapsedSeconds != m_displayedRecordingSeconds)
        {
            m_displayedRecordingSeconds = elapsedSeconds;
            auto recordingText = juce::String::formatted("%d:%02d",
                static_cast<int>(elapsedSeconds / 60), static_cast<int>(elapsedSeconds % 60));
            m_recordingDisplay.SetText(recordingText.paddedLeft(' ', StatusDisplay::x_numStatusChars));
        }
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

    template<typename Func>
    void ForEachVisibleVisualizer(Func func)
    {
        for (const auto& slot : m_slots)
        {
            if (ShouldShowSlot(slot))
            {
                func(*slot.m_visualizer, GetSlotBounds(slot.m_slot, slot.m_slotSpan, slot.m_region));
            }
        }

        auto bank = SmartGridOneEncoders::BankForTrio(m_nonagon->GetSelectedEncoderBank(), 0);
        if (bank != SmartGridOneEncoders::Bank::VoiceLFOs && bank != SmartGridOneEncoders::Bank::QuadLFOs)
        {
            return;
        }

        auto* uiState = m_nonagon->GetSquiggleBoyUIState();
        auto targets = LFOTargetVisualizers::Find(uiState->m_modulationMatrixUIState,
            bank == SmartGridOneEncoders::Bank::VoiceLFOs
                ? LFOTargetVisualizers::Page::Voice : LFOTargetVisualizers::Page::Quad,
            uiState->m_activeTrack.load());
        for (size_t i = 0; i < targets.m_count; ++i)
        {
            auto* visualizer = m_visualizers[static_cast<size_t>(targets.m_visualizers[i])];
            assert(visualizer != nullptr);
            func(*visualizer, GetSlotBounds(2, 1, targets.GetRegion(i)));
        }
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
        ForEachVisibleVisualizer([&g](SmartGridOneMainVisualizerComponent& visualizer, juce::Rectangle<int> bounds)
        {
            if (bounds.isEmpty())
            {
                return;
            }

            juce::Graphics::ScopedSaveState savedState(g);
            g.reduceClipRegion(bounds);
            g.setOrigin(bounds.getPosition());
            visualizer.Draw(g, bounds.withPosition(0, 0));
        });
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        ForEachVisibleVisualizer([&event](SmartGridOneMainVisualizerComponent& visualizer, juce::Rectangle<int> bounds)
        {
            if (bounds.contains(event.getPosition()))
            {
                visualizer.OnClick(event.withNewPosition(event.position - bounds.getPosition().toFloat()));
            }
        });
    }
};

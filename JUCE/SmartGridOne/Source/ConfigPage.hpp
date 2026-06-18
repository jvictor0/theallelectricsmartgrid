#pragma once

#include <JuceHeader.h>
#include "NonagonWrapper.hpp"
#include "Configuration.hpp"

struct ConfigDropdownRow : public juce::Component
{
    static constexpr int x_labelWidth = 75;

    juce::Label m_label;
    juce::ComboBox m_combo;
    std::function<void()> m_onChange;
    bool m_isUpdating;

    ConfigDropdownRow(const juce::String& labelText)
        : m_label({}, labelText)
        , m_isUpdating(false)
    {
        m_label.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(m_label);
        addAndMakeVisible(m_combo);

        m_combo.onChange = [this]()
        {
            if (!m_isUpdating && m_onChange)
            {
                m_onChange();
            }
        };
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        m_label.setBounds(bounds.removeFromLeft(x_labelWidth));
        m_combo.setBounds(bounds);
    }

    void SetOnChange(std::function<void()> onChange)
    {
        m_onChange = onChange;
    }

    void SetItems(const juce::StringArray& names)
    {
        m_isUpdating = true;
        m_combo.clear(juce::dontSendNotification);

        for (int i = 0; i < names.size(); ++i)
        {
            m_combo.addItem(names[i], i + 1);
        }

        if (names.size() > 0)
        {
            m_combo.setSelectedItemIndex(0, juce::dontSendNotification);
        }

        m_isUpdating = false;
    }

    void SetSelectedName(const juce::String& name)
    {
        m_isUpdating = true;

        int selectedIndex = 0;
        for (int i = 0; i < m_combo.getNumItems(); ++i)
        {
            if (m_combo.getItemText(i) == name)
            {
                selectedIndex = i;
                break;
            }
        }

        if (m_combo.getNumItems() > 0)
        {
            m_combo.setSelectedItemIndex(selectedIndex, juce::dontSendNotification);
        }

        m_isUpdating = false;
    }

    void SetSelectedIndex(int selectedIndex)
    {
        m_isUpdating = true;

        if (selectedIndex >= 0 && selectedIndex < m_combo.getNumItems())
        {
            m_combo.setSelectedItemIndex(selectedIndex, juce::dontSendNotification);
        }

        m_isUpdating = false;
    }

    int GetSelectedIndex() const
    {
        return m_combo.getSelectedItemIndex();
    }

    juce::String GetSelectedName() const
    {
        int selectedIndex = m_combo.getSelectedItemIndex();
        if (selectedIndex >= 0 && selectedIndex < m_combo.getNumItems())
        {
            return m_combo.getItemText(selectedIndex);
        }

        return {};
    }

    juce::String GetSelectedDeviceName() const
    {
        if (m_combo.getSelectedItemIndex() <= 0)
        {
            return {};
        }

        return GetSelectedName();
    }
};

class ConfigPage : public juce::Component
{
public:
    static constexpr size_t x_numControllers = 8;

    ConfigPage(NonagonWrapper* nonagon, Configuration* configuration, juce::AudioDeviceManager* audioDeviceManager)
        : m_nonagon(nonagon)
        , m_sections{
            ControllerSection("Launchpad Top Left", ControllerSection::Type::Launchpad),
            ControllerSection("Launchpad Top Right", ControllerSection::Type::Launchpad),
            ControllerSection("Launchpad Bottom Left", ControllerSection::Type::Launchpad),
            ControllerSection("Launchpad Bottom Right", ControllerSection::Type::Launchpad),
            ControllerSection("Encoder", ControllerSection::Type::Encoder),
            ControllerSection("Faders", ControllerSection::Type::Generic),
            ControllerSection("WrldBldr", ControllerSection::Type::WrldBldr),
            ControllerSection("KMix", ControllerSection::Type::KMix),
        }
        , m_audioInputRow("Audio In")
        , m_audioOutputRow("Audio Out")
        , m_configuration(configuration)
        , m_audioDeviceManager(audioDeviceManager)
    {
        for (int i = 0; i < static_cast<int>(x_numControllers); ++i)
        {
            addAndMakeVisible(m_sections[i].m_titleLabel);

            if (m_sections[i].HasMidiInputRow())
            {
                addAndMakeVisible(m_sections[i].m_midiInputRow);
                m_sections[i].m_midiInputRow.SetOnChange([this, i]() { OnMidiInChanged(i); });
            }

            if (m_sections[i].HasMidiOutputRow())
            {
                addAndMakeVisible(m_sections[i].m_midiOutputRow);
                m_sections[i].m_midiOutputRow.SetOnChange([this, i]() { OnMidiOutChanged(i); });
            }

            if (m_sections[i].HasControllerShapeRow())
            {
                addAndMakeVisible(m_sections[i].m_controllerShapeRow);
                m_sections[i].m_controllerShapeRow.SetOnChange([this, i]() { OnControllerShapeChanged(i); });
            }
        }

        m_stereoCheckbox.setButtonText("Stereo");
        m_stereoCheckbox.setSize(100, 30);
        addAndMakeVisible(m_stereoCheckbox);
        m_stereoCheckbox.onClick = [this]() { OnStereoCheckboxChanged(); };

        addAndMakeVisible(m_audioInputRow);
        addAndMakeVisible(m_audioOutputRow);

        PopulateMidiDevices();
        PopulateControllerShapes();
        PopulateAudioDevices();
        RefreshCurrentValues();
        RefreshAudioValues();
        RefreshStereoCheckbox();

        m_initialAudioInputDeviceName = GetSelectedAudioInputDeviceName();
        m_initialAudioOutputDeviceName = GetSelectedAudioOutputDeviceName();
    }

    ~ConfigPage() override
    {
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(20.0f));
        g.drawText("Launchpad Configuration", getLocalBounds().removeFromTop(50), juce::Justification::centred);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        bounds.removeFromTop(60);

        auto stereoBounds = bounds.removeFromTop(40);
        m_stereoCheckbox.setBounds(stereoBounds.removeFromLeft(150).reduced(5));

        const int audioRowHeight = 30;
        const int audioRowWidth = 350;
        const int spacing = 15;

        auto audioInputBounds = bounds.removeFromTop(audioRowHeight);
        m_audioInputRow.setBounds(audioInputBounds.removeFromLeft(audioRowWidth));
        bounds.removeFromTop(5);

        auto audioOutputBounds = bounds.removeFromTop(audioRowHeight);
        m_audioOutputRow.setBounds(audioOutputBounds.removeFromLeft(audioRowWidth));
        bounds.removeFromTop(spacing);

        const int sectionHeight = 45;
        const int labelWidth = 150;
        const int comboWidth = 220;
        const int comboHeight = 30;

        for (int i = 0; i < static_cast<int>(x_numControllers); ++i)
        {
            auto sectionBounds = bounds.removeFromTop(sectionHeight);
            m_sections[i].m_titleLabel.setBounds(sectionBounds.removeFromLeft(labelWidth).reduced(5));

            if (m_sections[i].HasMidiInputRow())
            {
                auto midiInBounds = sectionBounds.removeFromLeft(comboWidth);
                m_sections[i].m_midiInputRow.setBounds(midiInBounds.withHeight(comboHeight));
                sectionBounds.removeFromLeft(spacing);
            }

            if (m_sections[i].HasMidiOutputRow())
            {
                auto midiOutBounds = sectionBounds.removeFromLeft(comboWidth);
                m_sections[i].m_midiOutputRow.setBounds(midiOutBounds.withHeight(comboHeight));
            }

            if (m_sections[i].HasControllerShapeRow())
            {
                sectionBounds.removeFromLeft(spacing);
                auto shapeBounds = sectionBounds.removeFromLeft(comboWidth);
                m_sections[i].m_controllerShapeRow.setBounds(shapeBounds.withHeight(comboHeight));
            }

            bounds.removeFromTop(10);
        }
    }

    bool AudioDeviceConfigurationChanged() const
    {
        return GetSelectedAudioInputDeviceName() != m_initialAudioInputDeviceName ||
               GetSelectedAudioOutputDeviceName() != m_initialAudioOutputDeviceName;
    }

    void ApplyAudioConfiguration()
    {
        m_configuration->m_audioInputDeviceName = GetSelectedAudioInputDeviceName();
        m_configuration->m_audioOutputDeviceName = GetSelectedAudioOutputDeviceName();
    }

    juce::String GetSelectedAudioInputDeviceName() const
    {
        return m_audioInputRow.GetSelectedDeviceName();
    }

    juce::String GetSelectedAudioOutputDeviceName() const
    {
        return m_audioOutputRow.GetSelectedDeviceName();
    }

    struct ControllerSection
    {
        enum class Type : int
        {
            Launchpad,
            Encoder,
            Generic,
            WrldBldr,
            KMix
        };

        juce::Label m_titleLabel;
        ConfigDropdownRow m_midiInputRow;
        ConfigDropdownRow m_midiOutputRow;
        ConfigDropdownRow m_controllerShapeRow;
        Type m_type;

        ControllerSection(const juce::String& title, Type type)
            : m_titleLabel({}, title)
            , m_midiInputRow("MIDI In")
            , m_midiOutputRow("MIDI Out")
            , m_controllerShapeRow("Shape")
            , m_type(type)
        {
            m_titleLabel.setJustificationType(juce::Justification::centredLeft);
        }

        bool HasMidiInputRow() const
        {
            return m_type != Type::KMix;
        }

        bool HasMidiOutputRow() const
        {
            return true;
        }

        bool HasControllerShapeRow() const
        {
            return m_type != Type::WrldBldr &&
                   m_type != Type::Generic &&
                   m_type != Type::KMix;
        }

        void PopulateMidiDevices(const juce::StringArray& midiInputNames, const juce::StringArray& midiOutputNames)
        {
            if (HasMidiInputRow())
            {
                m_midiInputRow.SetItems(midiInputNames);
            }

            if (m_type == Type::Generic)
            {
                m_midiOutputRow.SetItems({});
            }
            else
            {
                m_midiOutputRow.SetItems(midiOutputNames);
            }
        }

        void PopulateControllerShapes()
        {
            juce::StringArray shapeNames;

            if (m_type == Type::Launchpad)
            {
                shapeNames.add("LaunchPadX");
                shapeNames.add("LaunchPadProMk3");
                shapeNames.add("LaunchPadMiniMk3");
            }

            if (HasControllerShapeRow())
            {
                m_controllerShapeRow.SetItems(shapeNames);
            }
        }

        void SetCurrentValues(NonagonWrapper* nonagon, int index)
        {
            juce::MidiInput* midiInput = nullptr;
            juce::MidiOutput* midiOutput = nullptr;

            if (m_type == Type::WrldBldr)
            {
                midiInput = nonagon->GetMidiInputWrldBldr();
                midiOutput = nonagon->GetMidiOutputWrldBldr();
            }
            else if (m_type == Type::KMix)
            {
                midiOutput = nonagon->GetKMixOutput();
            }
            else
            {
                midiInput = nonagon->GetMidiInputQuadLaunchpadTwister(index);
                midiOutput = nonagon->GetMidiOutputQuadLaunchpadTwister(index);
            }

            if (HasMidiInputRow())
            {
                m_midiInputRow.SetSelectedName(midiInput ? midiInput->getName() : juce::String{});
            }

            if (HasMidiOutputRow())
            {
                m_midiOutputRow.SetSelectedName(midiOutput ? midiOutput->getName() : juce::String{});
            }

            if (m_type == Type::Launchpad)
            {
                auto shape = nonagon->GetControllerShapeQuadLaunchpadTwister(index);
                m_controllerShapeRow.SetSelectedIndex(static_cast<int>(shape));
            }
        }

        void ApplySettings(NonagonWrapper* nonagon, int index, const juce::StringArray& midiInputIds, const juce::StringArray& midiOutputIds)
        {
            if (HasMidiInputRow())
            {
                int midiInIndex = m_midiInputRow.GetSelectedIndex();
                if (midiInIndex > 0 && midiInIndex < midiInputIds.size())
                {
                    auto deviceId = midiInputIds[midiInIndex];
                    if (m_type == Type::WrldBldr)
                    {
                        nonagon->OpenInputWrldBldr(deviceId);
                    }
                    else
                    {
                        nonagon->OpenInputQuadLaunchpadTwister(index, deviceId);
                    }
                }
            }

            int midiOutIndex = m_midiOutputRow.GetSelectedIndex();
            if (m_type == Type::KMix)
            {
                if (midiOutIndex > 0 && midiOutIndex < midiOutputIds.size())
                {
                    auto deviceId = midiOutputIds[midiOutIndex];
                    nonagon->OpenKMixOutput(deviceId);
                }
            }
            else if (m_type == Type::Launchpad)
            {
                int shapeIndex = m_controllerShapeRow.GetSelectedIndex();

                if (midiOutIndex > 0 && midiOutIndex < midiOutputIds.size() && shapeIndex >= 0)
                {
                    auto deviceId = midiOutputIds[midiOutIndex];
                    auto shape = static_cast<SmartGrid::ControllerShape>(shapeIndex);
                    nonagon->OpenLaunchpadOutputQuadLaunchpadTwister(index, shape, deviceId);
                }
            }
            else if (m_type == Type::Encoder)
            {
                if (midiOutIndex > 0 && midiOutIndex < midiOutputIds.size())
                {
                    auto deviceId = midiOutputIds[midiOutIndex];
                    nonagon->OpenEncoderOutputQuadLaunchpadTwister(deviceId);
                }
            }
            else if (m_type == Type::WrldBldr)
            {
                if (midiOutIndex > 0 && midiOutIndex < midiOutputIds.size())
                {
                    auto deviceId = midiOutputIds[midiOutIndex];
                    nonagon->OpenOutputWrldBldr(deviceId);
                }
            }
        }
    };

    void PopulateMidiDevices()
    {
        m_midiInputNames.clear();
        m_midiOutputNames.clear();
        m_midiInputIds.clear();
        m_midiOutputIds.clear();

        m_midiInputNames.add("None");
        m_midiOutputNames.add("None");
        m_midiInputIds.add("");
        m_midiOutputIds.add("");

        auto midiInputs = juce::MidiInput::getAvailableDevices();
        for (const auto& input : midiInputs)
        {
            m_midiInputNames.add(input.name);
            m_midiInputIds.add(input.identifier);
        }

        auto midiOutputs = juce::MidiOutput::getAvailableDevices();
        for (const auto& output : midiOutputs)
        {
            m_midiOutputNames.add(output.name);
            m_midiOutputIds.add(output.identifier);
        }

        for (int i = 0; i < static_cast<int>(x_numControllers); ++i)
        {
            m_sections[i].PopulateMidiDevices(m_midiInputNames, m_midiOutputNames);
        }
    }

    void PopulateControllerShapes()
    {
        for (int i = 0; i < static_cast<int>(x_numControllers); ++i)
        {
            m_sections[i].PopulateControllerShapes();
        }
    }

    void PopulateAudioDevices()
    {
        m_audioInputNames.clear();
        m_audioOutputNames.clear();

        m_audioInputNames.add("Default");
        m_audioOutputNames.add("Default");

        if (m_audioDeviceManager)
        {
            juce::AudioIODeviceType* deviceType = m_audioDeviceManager->getCurrentDeviceTypeObject();
            if (deviceType)
            {
                deviceType->scanForDevices();
                m_audioInputNames.addArray(deviceType->getDeviceNames(true));
                m_audioOutputNames.addArray(deviceType->getDeviceNames(false));
            }
        }

        m_audioInputRow.SetItems(m_audioInputNames);
        m_audioOutputRow.SetItems(m_audioOutputNames);
    }

    void RefreshCurrentValues()
    {
        for (int i = 0; i < static_cast<int>(x_numControllers); ++i)
        {
            m_sections[i].SetCurrentValues(m_nonagon, i);
        }
    }

    void RefreshAudioValues()
    {
        juce::String audioInputName = m_configuration->m_audioInputDeviceName;
        juce::String audioOutputName = m_configuration->m_audioOutputDeviceName;

        if (m_audioDeviceManager)
        {
            auto setup = m_audioDeviceManager->getAudioDeviceSetup();

            if (audioInputName.isEmpty())
            {
                audioInputName = setup.inputDeviceName;
            }

            if (audioOutputName.isEmpty())
            {
                audioOutputName = setup.outputDeviceName;
            }
        }

        m_audioInputRow.SetSelectedName(audioInputName);
        m_audioOutputRow.SetSelectedName(audioOutputName);
    }

    void OnMidiInChanged(int sectionIndex)
    {
        m_sections[sectionIndex].ApplySettings(m_nonagon, sectionIndex, m_midiInputIds, m_midiOutputIds);
    }

    void OnMidiOutChanged(int sectionIndex)
    {
        m_sections[sectionIndex].ApplySettings(m_nonagon, sectionIndex, m_midiInputIds, m_midiOutputIds);
    }

    void OnControllerShapeChanged(int sectionIndex)
    {
        m_sections[sectionIndex].ApplySettings(m_nonagon, sectionIndex, m_midiInputIds, m_midiOutputIds);
    }

    void OnStereoCheckboxChanged()
    {
        m_configuration->m_stereo = m_stereoCheckbox.getToggleState();
    }

    void RefreshStereoCheckbox()
    {
        m_stereoCheckbox.setToggleState(m_configuration->m_stereo, juce::dontSendNotification);
        m_stereoCheckbox.setEnabled(!m_configuration->m_forceStereo);
    }

    NonagonWrapper* m_nonagon;
    ControllerSection m_sections[x_numControllers];
    juce::StringArray m_midiInputNames;
    juce::StringArray m_midiOutputNames;
    juce::StringArray m_midiInputIds;
    juce::StringArray m_midiOutputIds;
    juce::StringArray m_audioInputNames;
    juce::StringArray m_audioOutputNames;
    ConfigDropdownRow m_audioInputRow;
    ConfigDropdownRow m_audioOutputRow;
    juce::ToggleButton m_stereoCheckbox;
    juce::String m_initialAudioInputDeviceName;
    juce::String m_initialAudioOutputDeviceName;
    Configuration* m_configuration;
    juce::AudioDeviceManager* m_audioDeviceManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConfigPage)
};

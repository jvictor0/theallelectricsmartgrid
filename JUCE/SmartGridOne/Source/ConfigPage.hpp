#pragma once

#include <JuceHeader.h>
#include "NonagonWrapper.hpp"
#include "Configuration.hpp"

//==============================================================================
/*
    Configuration page for launchpad settings
*/
class ConfigPage : public juce::Component
{
public:
    static constexpr size_t x_numControllers = 6;

    //==============================================================================
    ConfigPage(NonagonWrapper* nonagon, Configuration* configuration)
        : m_nonagon(nonagon)
        , m_sections{
            ControllerSection("Launchpad Top Left", ControllerSection::Type::Launchpad),
            ControllerSection("Launchpad Top Right", ControllerSection::Type::Launchpad),
            ControllerSection("Launchpad Bottom Left", ControllerSection::Type::Launchpad),
            ControllerSection("Launchpad Bottom Right", ControllerSection::Type::Launchpad),
            ControllerSection("Twister", ControllerSection::Type::Twister),
            ControllerSection("Faders", ControllerSection::Type::Generic),
        }
        , m_configuration(configuration)
    {
        // Add sections to the component
        for (int i = 0; i < x_numControllers; ++i)
        {
            addAndMakeVisible(m_sections[i].m_titleLabel);
            addAndMakeVisible(m_sections[i].m_midiInCombo);
            addAndMakeVisible(m_sections[i].m_midiOutCombo);
            addAndMakeVisible(m_sections[i].m_controllerShapeCombo);
            
            // Set up combo box listeners
            m_sections[i].m_midiInCombo.onChange = [this, i]() { OnMidiInChanged(i); };
            m_sections[i].m_midiOutCombo.onChange = [this, i]() { OnMidiOutChanged(i); };
            m_sections[i].m_controllerShapeCombo.onChange = [this, i]() { OnControllerShapeChanged(i); };
        }
        
        // Set up stereo checkbox
        //
        m_stereoCheckbox.setButtonText("Stereo");
        m_stereoCheckbox.setSize(100, 30);
        addAndMakeVisible(m_stereoCheckbox);
        m_stereoCheckbox.onClick = [this]() { OnStereoCheckboxChanged(); };
        
        PopulateMidiDevices();
        PopulateControllerShapes();
        RefreshCurrentValues();
        RefreshStereoCheckbox();
    }

    ~ConfigPage() override
    {
    }

    //==============================================================================
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
        bounds.removeFromTop(60); // Space for title
        
        // Position stereo checkbox at the top
        //
        auto stereoBounds = bounds.removeFromTop(40);
        m_stereoCheckbox.setBounds(stereoBounds.removeFromLeft(150).reduced(5));
        
        bounds.removeFromTop(10); // Spacing
        
        const int sectionHeight = 80; // Fixed height for each section
        const int labelWidth = 150;
        const int comboWidth = 200;
        const int comboHeight = 30;
        const int spacing = 15;
        
        for (int i = 0; i < x_numControllers; ++i)
        {
            auto sectionBounds = bounds.removeFromTop(sectionHeight);
            
            // Title label
            m_sections[i].m_titleLabel.setBounds(sectionBounds.removeFromLeft(labelWidth).reduced(5));
            
            // MIDI In combo
            auto midiInBounds = sectionBounds.removeFromLeft(comboWidth);
            m_sections[i].m_midiInCombo.setBounds(midiInBounds.withHeight(comboHeight));
            sectionBounds.removeFromLeft(spacing);
            
            // MIDI Out combo
            auto midiOutBounds = sectionBounds.removeFromLeft(comboWidth);
            m_sections[i].m_midiOutCombo.setBounds(midiOutBounds.withHeight(comboHeight));
            sectionBounds.removeFromLeft(spacing);
            
            // Controller Shape combo
            auto shapeBounds = sectionBounds.removeFromLeft(comboWidth);
            m_sections[i].m_controllerShapeCombo.setBounds(shapeBounds.withHeight(comboHeight));
            
            bounds.removeFromTop(10); // Spacing between sections
        }
    }

    struct ControllerSection
    {
        juce::Label m_titleLabel;
        juce::ComboBox m_midiInCombo;
        juce::ComboBox m_midiOutCombo;
        juce::ComboBox m_controllerShapeCombo;

        enum class Type : int
        {
            Launchpad,
            Twister,
            Generic
        };

        Type m_type;

        ControllerSection(const juce::String& title, Type type)
            : m_titleLabel(title, title)
            , m_type(type)
        {
            m_titleLabel.setJustificationType(juce::Justification::centredLeft);
            m_titleLabel.attachToComponent(&m_midiInCombo, true);
        }

        void PopulateMidiDevices()
        {
            // This will be called from the main PopulateMidiDevices method
        }

        void PopulateControllerShapes()
        {
            m_controllerShapeCombo.clear();
            m_controllerShapeCombo.addItem("LaunchPadX", static_cast<int>(SmartGrid::ControllerShape::LaunchPadX) + 1);
            m_controllerShapeCombo.addItem("LaunchPadProMk3", static_cast<int>(SmartGrid::ControllerShape::LaunchPadProMk3) + 1);
            m_controllerShapeCombo.addItem("LaunchPadMiniMk3", static_cast<int>(SmartGrid::ControllerShape::LaunchPadMiniMk3) + 1);
        }

        void SetCurrentValues(NonagonWrapper* nonagon, int index)
        {
            // Set MIDI input
            auto* midiInput = nonagon->GetMidiInputQuadLaunchpadTwister(index);
            if (midiInput)
            {
                auto deviceName = midiInput->getName();
                int itemIndex = -1;
                for (int i = 0; i < m_midiInCombo.getNumItems(); ++i)
                {
                    if (m_midiInCombo.getItemText(i) == deviceName)
                    {
                        itemIndex = i;
                        break;
                    }
                }
                if (itemIndex >= 0)
                {
                    m_midiInCombo.setSelectedItemIndex(itemIndex);
                }
            }
            else
            {
                m_midiInCombo.setSelectedItemIndex(0); // "None" option
            }
            
            // Set MIDI output
            auto* midiOutput = nonagon->GetMidiOutputQuadLaunchpadTwister(index);
            if (midiOutput)
            {
                auto deviceName = midiOutput->getName();
                int itemIndex = -1;
                for (int i = 0; i < m_midiOutCombo.getNumItems(); ++i)
                {
                    if (m_midiOutCombo.getItemText(i) == deviceName)
                    {
                        itemIndex = i;
                        break;
                    }
                }
                if (itemIndex >= 0)
                {
                    m_midiOutCombo.setSelectedItemIndex(itemIndex);
                }
            }
            else
            {
                m_midiOutCombo.setSelectedItemIndex(0); // "None" option
            }
            
            // Set controller shape
            if (m_type == Type::Launchpad)
            {
                auto shape = nonagon->GetControllerShapeQuadLaunchpadTwister(index);
                m_controllerShapeCombo.setSelectedItemIndex(static_cast<int>(shape));
            }
        }

        void ApplySettings(NonagonWrapper* nonagon, int index, const juce::StringArray& midiInputIds, const juce::StringArray& midiOutputIds)
        {
            // Apply MIDI input
            //
            int midiInIndex = m_midiInCombo.getSelectedItemIndex();
            if (midiInIndex > 0 && midiInIndex <= m_midiInCombo.getNumItems())
            {
                auto deviceId = midiInputIds[midiInIndex];
                nonagon->OpenInputQuadLaunchpadTwister(index, deviceId);
            }
            
            // Apply MIDI output and controller shape
            //
            int midiOutIndex = m_midiOutCombo.getSelectedItemIndex();
            if (m_type == Type::Launchpad)
            {
                int shapeIndex = m_controllerShapeCombo.getSelectedItemIndex();
            
                if (midiOutIndex > 0 && midiOutIndex <= m_midiOutCombo.getNumItems())
                {
                    auto deviceId = midiOutputIds[midiOutIndex];
                    auto shape = static_cast<SmartGrid::ControllerShape>(shapeIndex);
                    nonagon->OpenLaunchpadOutputQuadLaunchpadTwister(index, shape, deviceId);
                }
            }
            else if (m_type == Type::Twister)
            {
                if (midiOutIndex > 0 && midiOutIndex <= m_midiOutCombo.getNumItems())
                {
                    auto deviceId = midiOutputIds[midiOutIndex];
                    nonagon->OpenTwisterOutputQuadLaunchpadTwister(deviceId);
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
        
        // Add "None" option
        //
        m_midiInputNames.add("None");
        m_midiOutputNames.add("None");
        m_midiInputIds.add("");
        m_midiOutputIds.add("");
        
        // Get available MIDI input devices
        //
        auto midiInputs = juce::MidiInput::getAvailableDevices();
        for (const auto& input : midiInputs)
        {
            m_midiInputNames.add(input.name);
            m_midiInputIds.add(input.identifier);
        }
        
        // Get available MIDI output devices
        //
        auto midiOutputs = juce::MidiOutput::getAvailableDevices();
        for (const auto& output : midiOutputs)
        {
            m_midiOutputNames.add(output.name);
            m_midiOutputIds.add(output.identifier);
        }
        
        // Populate all section combo boxes
        //
        for (int i = 0; i < x_numControllers; ++i)
        {
            m_sections[i].m_midiInCombo.clear();
            m_sections[i].m_midiOutCombo.clear();
            
            for (int j = 0; j < m_midiInputNames.size(); ++j)
            {
                m_sections[i].m_midiInCombo.addItem(m_midiInputNames[j], j + 1);
            }

            if (m_sections[i].m_type != ControllerSection::Type::Generic)
            {
                for (int j = 0; j < m_midiOutputNames.size(); ++j)
                {
                    m_sections[i].m_midiOutCombo.addItem(m_midiOutputNames[j], j + 1);
                }
            }
            
            if (m_sections[i].m_type == ControllerSection::Type::Launchpad)
            {
                m_sections[i].PopulateControllerShapes();
            }
        }
    }

    void PopulateControllerShapes()
    {
        for (int i = 0; i < x_numControllers; ++i)
        {
            if (m_sections[i].m_type == ControllerSection::Type::Launchpad)
            {
                m_sections[i].PopulateControllerShapes();
            }
        }
    }

    void RefreshCurrentValues()
    {
        for (int i = 0; i < x_numControllers; ++i)
        {
            m_sections[i].SetCurrentValues(m_nonagon, i);
        }
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
    juce::ToggleButton m_stereoCheckbox;

    Configuration* m_configuration;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConfigPage)
};

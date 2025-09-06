#pragma once

#include <JuceHeader.h>

// Forward declaration
class MainComponent;

//==============================================================================
class MainComponentMenuBarModel : public juce::MenuBarModel
{
public:
    MainComponentMenuBarModel(MainComponent* parent) : m_parent(parent) {}

    juce::StringArray getMenuBarNames() override
    {
        return { "File" };
    }

    juce::PopupMenu getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName) override
    {
        juce::PopupMenu menu;

        if (topLevelMenuIndex == 0) // File menu
        {
            menu.addItem(1, "Open...", true, false);
            menu.addItem(2, "Save", true, false);
            menu.addItem(3, "Save As...", true, false);
        }

        return menu;
    }

    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override
    {
        if (topLevelMenuIndex == 0) // File menu
        {
            switch (menuItemID)
            {
                case 1: 
                {
                    m_parent->LoadCurrentPatch();
                    break;
                }
                case 2: // Save
                {
                    m_parent->SavePatch();
                    break;
                }
                case 3: // Save As
                {
                    m_parent->SavePatchAs();
                    break;
                }
                default:
                    break;
            }
        }
    }

private:
    MainComponent* m_parent;
};

#pragma once

#include <JuceHeader.h>
#include "SyncClient.hpp"

// The page owns every sync resource. Destroying it drains the client before
// navigation returns to the instrument. No engine or audio state is involved.
//
struct SyncPage : juce::Component, juce::ListBoxModel, juce::Timer
{
    explicit SyncPage(const juce::File& root)
        : m_client(root.getFullPathName().toStdString()), m_receivers("Receivers", this)
    {
        m_title.setText("Sync with Mac", juce::dontSendNotification);
        m_title.setFont(juce::FontOptions(24.0f));
        m_title.setJustificationType(juce::Justification::centred);
        m_hint.setText("Run sync_receiver.py on your Mac, then tap its name below.", juce::dontSendNotification);
        m_hint.setJustificationType(juce::Justification::centred);
        m_status.setJustificationType(juce::Justification::centred);
        m_progress.setJustificationType(juce::Justification::centred);
        m_receivers.setRowHeight(56);
        m_cancel.setButtonText("Cancel transfer");
        m_cancel.onClick = [this]()
        {
            m_client.Cancel();
            Refresh();
        };
        m_pair.setButtonText("Pair / change access code");
        m_pair.onClick = [this]()
        {
            Begin(m_receivers.getSelectedRow(), true);
        };
        for (auto* component : std::initializer_list<juce::Component*>{&m_title, &m_hint, &m_receivers, &m_status, &m_progress, &m_cancel, &m_pair})
        {
            addAndMakeVisible(component);
        }

        Refresh();
    }

    void Open()
    {
        jassert(isShowing());
        m_client.Open();
        startTimer(200);
        Refresh();
    }

    void visibilityChanged() override
    {
        if (!isShowing())
        {
            stopTimer();
            m_client.Close();
        }
    }

    ~SyncPage() override
    {
        stopTimer();
        if (m_pairing != nullptr)
        {
            m_pairing->exitModalState(0);
            m_pairing.reset();
        }

        m_client.Close();
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(24);
        m_title.setBounds(area.removeFromTop(48));
        m_hint.setBounds(area.removeFromTop(48));
        auto buttons = area.removeFromBottom(44);
        m_cancel.setBounds(buttons.removeFromRight(160));
        m_pair.setBounds(buttons.removeFromLeft(240));
        m_progress.setBounds(area.removeFromBottom(44));
        m_status.setBounds(area.removeFromBottom(72));
        m_receivers.setBounds(area.reduced(0, 12));
    }

    int getNumRows() override
    {
        return static_cast<int>(m_snapshot.m_receivers.size());
    }

    void paintListBoxItem(int row, juce::Graphics& graphics, int width, int height, bool selected) override
    {
        if (row < 0 || row >= getNumRows())
        {
            return;
        }

        graphics.fillAll(selected ? juce::Colours::darkslategrey : juce::Colours::darkgrey);
        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::FontOptions(20.0f));
        graphics.drawText(juce::String(m_snapshot.m_receivers[static_cast<size_t>(row)].m_name), 16, 0, width - 32, height, juce::Justification::centredLeft);
    }

    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        Begin(row, false);
    }

    void Begin(int row, bool repair)
    {
        if (m_snapshot.m_busy || row < 0 || row >= getNumRows() || m_pairing != nullptr)
        {
            return;
        }

        const auto receiver = m_snapshot.m_receivers[static_cast<size_t>(row)];
        const auto token = repair ? std::string() : SyncClient::LoadToken(receiver);
        if (!token.empty())
        {
            m_client.Start(receiver, token);
            Refresh();
            return;
        }

        m_pairing = std::make_unique<juce::AlertWindow>("Pair with " + juce::String(receiver.m_name),
            "Compare this certificate fingerprint with the Mac receiver's terminal:\n\n"
            + Fingerprint(receiver.m_fingerprint) + "\n\nIf it matches, enter the access code shown there.",
            juce::MessageBoxIconType::NoIcon);
        m_pairing->addTextEditor("code", "", "Access code", true);
        m_pairing->addButton("Pair and sync", 1);
        m_pairing->addButton("Cancel", 0);
        juce::Component::SafePointer<SyncPage> safe(this);
        m_pairing->enterModalState(true, juce::ModalCallbackFunction::create([safe, receiver](int result)
        {
            if (safe == nullptr || safe->m_pairing == nullptr)
            {
                return;
            }

            const auto code = safe->m_pairing->getTextEditorContents("code").trim().toStdString();
            safe->m_pairing.reset();
            if (result == 1 && !code.empty())
            {
                SyncClient::SaveToken(receiver, code);
                safe->m_client.Start(receiver, code);
                safe->Refresh();
            }
        }), false);
    }

    static juce::String Fingerprint(const std::string& fingerprint)
    {
        juce::String result;
        for (size_t offset = 0; offset < fingerprint.size(); offset += 4)
        {
            if (offset != 0)
            {
                result += " ";
            }

            result += juce::String(fingerprint.substr(offset, 4));
        }

        return result;
    }

    void timerCallback() override
    {
        Refresh();
    }

    void Refresh()
    {
        m_snapshot = m_client.Snapshot();
        m_receivers.updateContent();
        m_receivers.repaint();
        m_status.setText(juce::String(m_snapshot.m_message), juce::dontSendNotification);
        m_cancel.setEnabled(m_snapshot.m_busy);
        m_pair.setEnabled(!m_snapshot.m_busy);
        constexpr double x_mib = 1024.0 * 1024.0;
        m_progress.setText(m_snapshot.m_busy && m_snapshot.m_total != 0
            ? juce::String(m_snapshot.m_sent / x_mib, 1) + " / " + juce::String(m_snapshot.m_total / x_mib, 1)
                + " MiB    " + juce::String(m_snapshot.m_bytesPerSecond / x_mib, 1) + " MiB/s"
            : juce::String(), juce::dontSendNotification);
    }

    SyncClient m_client;
    SyncClient::Status m_snapshot;
    juce::Label m_title;
    juce::Label m_hint;
    juce::Label m_status;
    juce::Label m_progress;
    juce::ListBox m_receivers;
    juce::TextButton m_cancel;
    juce::TextButton m_pair;
    std::unique_ptr<juce::AlertWindow> m_pairing;
};

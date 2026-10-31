#pragma once

#include <JuceHeader.h>
#include "PublishingClient.hpp"
#include "PublishingArtwork.hpp"
#include <map>

struct PublishingPanel : juce::Component, juce::ListBoxModel
{
    struct Draft
    {
        bool m_useDefault = true;
        juce::String m_title;
        PublishingArtwork::Image m_image;
        PublishingClient::Upload m_request;
        bool m_hasRequest = false;
    };

    explicit PublishingPanel(PublishingClient& client) : m_client(client), m_rows("Mac recordings", this)
    {
        m_rows.setRowHeight(72);
        m_title.setInputRestrictions(256);
        m_title.setTextToShowWhenEmpty("Automatic nightly title", juce::Colours::grey);
        m_title.onTextChange = [this]()
        {
            if (!m_loading && Selected() != nullptr)
            {
                auto& draft = CurrentDraft();
                draft.m_useDefault = false;
                draft.m_title = m_title.getText();
                UpdateActions();
            }
        };
        m_default.setButtonText("Use default");
        m_default.onClick = [this]()
        {
            if (Selected() != nullptr)
            {
                CurrentDraft().m_useDefault = true;
                CurrentDraft().m_title.clear();
                LoadDraft();
            }
        };
        m_files.setButtonText("Choose image (Files)");
        m_files.onClick = [this]() { ChooseImage(false); };
        m_photos.setButtonText("Photos");
        m_photos.onClick = [this]() { ChooseImage(true); };
        m_remove.setButtonText("Remove image");
        m_remove.onClick = [this]()
        {
            if (Selected() != nullptr)
            {
                CurrentDraft().m_image = {};
                m_thumbnail.setImage({});
                UpdateActions();
            }
        };
        m_upload.onClick = [this]() { Upload(); };
        m_link.setButtonText("Link existing");
        m_link.onClick = [this]()
        {
            const auto* row = Selected();
            const int index = m_tracks.getSelectedId() - 1;
            if (row != nullptr && index >= 0 && index < static_cast<int>(m_snapshot.m_tracks.size()))
            {
                m_client.Link({row->m_id, row->m_version, m_snapshot.m_tracks[static_cast<size_t>(index)].m_id});
            }
        };
        m_tracks.onChange = [this]() { UpdateActions(); };
        m_tracks.setTextWhenNothingSelected("Select an existing account track");
        m_refresh.setButtonText("Refresh catalog");
        m_refresh.onClick = [this]() { m_client.Refresh(); };
        m_account.setButtonText("Open SoundCloud account");
        m_account.onClick = []() { juce::URL("https://soundcloud.com/you/tracks").launchInDefaultBrowser(); };
        m_detail.setJustificationType(juce::Justification::topLeft);
        m_message.setJustificationType(juce::Justification::topLeft);
        for (auto* component : std::initializer_list<juce::Component*>{&m_message, &m_rows, &m_title, &m_default,
            &m_files, &m_photos, &m_remove, &m_thumbnail, &m_upload, &m_tracks, &m_link, &m_detail, &m_refresh, &m_account})
        {
            addAndMakeVisible(component);
        }

        m_photos.setVisible(PublishingArtwork::HasPhotos());
    }

    void SetReceiver(const std::string& identity)
    {
        m_receiver = identity;
        m_selectedId.clear();
        m_rows.deselectAllRows();
    }

    std::string DraftKey() const
    {
        const auto* row = Selected();
        return row == nullptr ? std::string() : m_receiver + ":" + row->m_id + ":"
            + (row->m_draftVersion.empty() ? row->m_version : row->m_draftVersion);
    }

    Draft& CurrentDraft()
    {
        return m_drafts[DraftKey()];
    }

    const PublishingClient::Recording* Selected() const
    {
        for (const auto& row : m_snapshot.m_recordings)
        {
            if (row.m_id == m_selectedId)
            {
                return &row;
            }
        }

        return nullptr;
    }

    void Refresh()
    {
        m_snapshot = m_client.Snapshot();
        m_rows.updateContent();
        int index = -1;
        for (size_t i = 0; i < m_snapshot.m_recordings.size(); ++i)
        {
            if (m_snapshot.m_recordings[i].m_id == m_selectedId)
            {
                index = static_cast<int>(i);
            }
        }

        if (index < 0 && !m_snapshot.m_recordings.empty())
        {
            m_selectedId = m_snapshot.m_recordings.front().m_id;
            index = 0;
            LoadDraft();
        }

        if (index >= 0)
        {
            m_rows.selectRow(index, true, false);
        }

        if (DraftKey() != m_loadedKey)
        {
            LoadDraft();
        }

        m_rows.repaint();
        std::string trackSignature;
        for (const auto& track : m_snapshot.m_tracks)
        {
            trackSignature += track.m_id + "\n" + track.m_title + "\n";
        }

        if (trackSignature != m_trackSignature)
        {
            std::string selectedTrack;
            const int oldIndex = m_tracks.getSelectedId() - 1;
            if (oldIndex >= 0 && oldIndex < static_cast<int>(m_trackIds.size()))
            {
                selectedTrack = m_trackIds[static_cast<size_t>(oldIndex)];
            }

            m_tracks.clear(juce::dontSendNotification);
            m_trackIds.clear();
            for (size_t i = 0; i < m_snapshot.m_tracks.size(); ++i)
            {
                const auto& track = m_snapshot.m_tracks[i];
                m_tracks.addItem(juce::String(track.m_title), static_cast<int>(i) + 1);
                m_trackIds.push_back(track.m_id);
                if (track.m_id == selectedTrack)
                {
                    m_tracks.setSelectedId(static_cast<int>(i) + 1, juce::dontSendNotification);
                }
            }

            m_trackSignature = trackSignature;
        }

        juce::String message = "Select a Mac to publish its recordings to SoundCloud.";
        if (m_snapshot.m_selected)
        {
            if (!m_snapshot.m_error.empty())
            {
                message = juce::String(m_snapshot.m_error);
            }
            else if (m_snapshot.m_available && !m_snapshot.m_account.m_connected)
            {
                message = "Connect SoundCloud on the Mac receiver to enable publishing. Sync is available.";
            }
            else if (m_snapshot.m_available)
            {
                message = "SoundCloud: " + juce::String(m_snapshot.m_account.m_name)
                    + (m_snapshot.m_refreshing ? " — refreshing catalog…" : "");
            }
            else
            {
                message = "Loading SoundCloud publishing from Mac…";
            }
        }

        m_message.setText(message, juce::dontSendNotification);
        const auto* row = Selected();
        if (row != nullptr && m_snapshot.m_pendingRecordingId.empty() && !m_snapshot.m_error.empty() && !m_snapshot.m_busy)
        {
            CurrentDraft().m_hasRequest = false;
        }

        if (row != nullptr && row->m_state == "uploaded")
        {
            auto& draft = CurrentDraft();
            draft.m_image = {};
            draft.m_hasRequest = false;
            draft.m_request = {};
            m_thumbnail.setImage({});
        }

        UpdateActions();
    }

    int getNumRows() override
    {
        return static_cast<int>(m_snapshot.m_recordings.size());
    }

    void paintListBoxItem(int index, juce::Graphics& graphics, int width, int height, bool selected) override
    {
        if (index < 0 || index >= getNumRows())
        {
            return;
        }

        const auto& row = m_snapshot.m_recordings[static_cast<size_t>(index)];
        graphics.fillAll(selected ? juce::Colours::darkslategrey : juce::Colours::darkgrey);
        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::FontOptions(16.0f));
        graphics.drawText(juce::String(row.m_title.empty() ? row.m_suggestedTitle : row.m_title),
            12, 8, width - 24, 26, juce::Justification::centredLeft);
        const auto minutes = static_cast<int>(row.m_durationSeconds / 60);
        const bool transferring = row.m_state == "pending" || row.m_state == "waiting";
        juce::String detail = juce::String(row.m_date) + "    "
            + (transferring ? juce::String() : juce::String(minutes) + " min    ")
            + (row.m_state == "pending" ? "Pending transfer" : row.m_state == "waiting" ? "Queued after transfer" : juce::String(row.m_state));
        if (row.m_state == "uploading")
        {
            detail += " " + juce::String(row.m_progress * 100, 0) + "%";
        }

        graphics.setFont(juce::FontOptions(13.0f));
        graphics.drawText(detail, 12, 36, width - 24, height - 40, juce::Justification::centredLeft);
    }

    void selectedRowsChanged(int index) override
    {
        if (index >= 0 && index < getNumRows())
        {
            const auto id = m_snapshot.m_recordings[static_cast<size_t>(index)].m_id;
            if (id != m_selectedId)
            {
                m_selectedId = id;
                LoadDraft();
            }
        }
    }

    void LoadDraft()
    {
        const auto* row = Selected();
        m_loadedKey = DraftKey();
        m_loading = true;
        m_title.setText(row == nullptr ? juce::String() : CurrentDraft().m_title, false);
        m_thumbnail.setImage(row == nullptr ? juce::Image() : CurrentDraft().m_image.m_thumbnail);
        m_loading = false;
        if (row != nullptr && m_tracks.getSelectedId() == 0 && !row->m_candidateId.empty())
        {
            for (size_t i = 0; i < m_snapshot.m_tracks.size(); ++i)
            {
                if (m_snapshot.m_tracks[i].m_id == row->m_candidateId)
                {
                    m_tracks.setSelectedId(static_cast<int>(i) + 1, juce::dontSendNotification);
                }
            }
        }

        UpdateActions();
    }

    void UpdateActions()
    {
        const auto* row = Selected();
        const bool connected = m_snapshot.m_selected && m_snapshot.m_available && m_snapshot.m_account.m_connected;
        const bool transferring = row != nullptr && (row->m_state == "pending" || row->m_state == "waiting");
        const bool editable = connected && row != nullptr && (row->m_state == "pending" || row->m_state == "ready" || row->m_state == "failed" || row->m_state == "unknown");
        const bool pending = row != nullptr && m_snapshot.m_pendingRecordingId == row->m_id;
        for (auto* component : std::initializer_list<juce::Component*>{&m_title, &m_default, &m_files, &m_photos, &m_remove})
        {
            component->setEnabled(editable && row != nullptr && m_snapshot.CanEditRecording(row->m_id));
        }

        m_refresh.setEnabled(m_snapshot.m_selected && !m_snapshot.m_busy);
        m_tracks.setEnabled(editable && !transferring && !m_snapshot.m_busy && !pending);
        m_link.setEnabled(editable && !transferring && !m_snapshot.m_busy && !pending && m_tracks.getSelectedId() > 0);
        m_account.setVisible(row != nullptr && row->m_state == "unknown");
        juce::String detail;
        if (row != nullptr)
        {
            detail = juce::String(row->m_filename) + "\n";
            if (row->m_state == "uploaded")
            {
                detail += juce::String(row->m_title) + "\n" + juce::String(row->m_permalink);
            }
            else
            {
                detail += "Default: " + juce::String(row->m_suggestedTitle);
            }

            if (!row->m_error.empty())
            {
                detail += "\n" + juce::String(row->m_error);
            }

            if (transferring)
            {
                detail += row->m_state == "waiting"
                    ? "\nSoundCloud upload saved on the Mac. Waiting for transfer and stereo extraction. Resume sync if the transfer stops."
                    : "\nWaiting for transfer to the Mac. Upload queues SoundCloud publication after transfer and stereo extraction.";
            }

            if (row->m_state == "unknown")
            {
                detail += "\nSoundCloud may have accepted this upload. Check the account or link an existing track. Upload again may create a duplicate.";
            }

            if (!m_snapshot.m_pendingRequestId.empty() && !pending)
            {
                detail += "\nResolve the pending upload before uploading another recording. This draft can still be edited.";
            }

            if (pending)
            {
                detail += "\nChecking the accepted request. A request retry uses the same identity.";
            }

            if (!CurrentDraft().m_image.m_error.isEmpty())
            {
                detail += "\n" + CurrentDraft().m_image.m_error;
            }
        }

        m_detail.setText(detail, juce::dontSendNotification);
        m_upload.setButtonText(row != nullptr && row->m_state == "uploaded" ? "Open on SoundCloud"
            : pending ? "Retry same request"
            : row != nullptr && row->m_state == "failed" ? "Retry"
            : row != nullptr && row->m_state == "unknown" ? "Upload again"
            : row != nullptr && row->m_state == "waiting" ? "Queued after transfer"
            : "Upload to SoundCloud");
        const bool validTitle = row != nullptr && (CurrentDraft().m_useDefault ? m_snapshot.m_catalogReady : !CurrentDraft().m_title.trim().isEmpty());
        m_upload.setEnabled(row != nullptr && (row->m_state == "uploaded" ? !row->m_permalink.empty()
            : editable && m_snapshot.CanSubmitRecording(row->m_id) && validTitle));
    }

    void Upload()
    {
        const auto* row = Selected();
        if (row == nullptr)
        {
            return;
        }

        if (row->m_state == "uploaded")
        {
            juce::URL(juce::String(row->m_permalink)).launchInDefaultBrowser();
            return;
        }

        if (!m_snapshot.CanSubmitRecording(row->m_id))
        {
            return;
        }

        auto& draft = CurrentDraft();
        const bool pending = m_snapshot.m_pendingRecordingId == row->m_id;
        if (!pending || !draft.m_hasRequest)
        {
            draft.m_request = {};
            draft.m_request.m_recordingId = row->m_id;
            draft.m_request.m_version = row->m_version;
            draft.m_request.m_requestId = PublishingClient::NewRequestId();
            draft.m_request.m_useDefault = draft.m_useDefault;
            draft.m_request.m_title = draft.m_title.trim().toStdString();
            draft.m_request.m_artwork = draft.m_image.m_bytes;
            draft.m_request.m_artworkType = "image/jpeg";
            draft.m_request.m_retryOf = row->m_state == "failed" || row->m_state == "unknown" ? row->m_jobId : std::string();
            draft.m_hasRequest = true;
        }

        m_client.Submit(draft.m_request);
        Refresh();
    }

    void ChooseImage(bool photos)
    {
        const auto key = DraftKey();
        juce::Component::SafePointer<PublishingPanel> safe(this);
        m_picker = std::make_unique<PublishingArtwork>([safe, key](PublishingArtwork::Image image)
        {
            if (safe != nullptr)
            {
                auto& draft = safe->m_drafts[key];
                if (image.m_error.isEmpty())
                {
                    draft.m_image = std::move(image);
                }
                else
                {
                    draft.m_image.m_error = image.m_error;
                }

                if (key == safe->DraftKey())
                {
                    safe->LoadDraft();
                }
            }
        });
        if (photos)
        {
            m_picker->ChoosePhotos();
        }
        else
        {
            m_picker->ChooseFiles();
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();
        m_message.setBounds(area.removeFromTop(56));
        auto footer = area.removeFromBottom(40);
        m_refresh.setBounds(footer.removeFromLeft(160));
        m_account.setBounds(footer.removeFromRight(230));
        auto list = area.removeFromLeft(static_cast<int>(area.getWidth() * 0.45f));
        m_rows.setBounds(list.reduced(0, 6));
        area = area.reduced(12, 6);
        auto title = area.removeFromTop(40);
        m_default.setBounds(title.removeFromRight(110));
        m_title.setBounds(title.reduced(0, 3));
        area.removeFromTop(8);
        auto artwork = area.removeFromTop(96);
        m_thumbnail.setBounds(artwork.removeFromLeft(96));
        artwork = artwork.reduced(8, 0);
        m_files.setBounds(artwork.removeFromTop(32));
        auto imageButtons = artwork.removeFromTop(32);
        m_remove.setBounds(imageButtons.removeFromRight(120));
        m_photos.setBounds(imageButtons);
        area.removeFromTop(8);
        m_upload.setBounds(area.removeFromTop(40));
        area.removeFromTop(8);
        auto links = area.removeFromTop(40);
        m_link.setBounds(links.removeFromRight(110));
        m_tracks.setBounds(links.reduced(0, 3));
        m_detail.setBounds(area.reduced(0, 8));
    }

    PublishingClient& m_client;
    PublishingClient::Status m_snapshot;
    juce::ListBox m_rows;
    juce::Label m_message;
    juce::Label m_detail;
    juce::TextEditor m_title;
    juce::TextButton m_default;
    juce::TextButton m_files;
    juce::TextButton m_photos;
    juce::TextButton m_remove;
    juce::ImageComponent m_thumbnail;
    juce::TextButton m_upload;
    juce::ComboBox m_tracks;
    juce::TextButton m_link;
    juce::TextButton m_refresh;
    juce::TextButton m_account;
    std::string m_selectedId;
    std::string m_receiver;
    std::string m_trackSignature;
    std::string m_loadedKey;
    std::vector<std::string> m_trackIds;
    std::map<std::string, Draft> m_drafts;
    std::unique_ptr<PublishingArtwork> m_picker;
    bool m_loading = false;
};

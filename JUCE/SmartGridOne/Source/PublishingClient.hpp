#pragma once

#include "SyncClient.hpp"
#include <memory>
#include <string>
#include <vector>

// A foreground, page-owned control channel, independent of bulk sync.
// Public controls run on the message thread; snapshots are thread safe.
//
struct PublishingClient
{
    struct Account
    {
        std::string m_id;
        std::string m_name;
        bool m_connected = false;
    };

    struct Recording
    {
        std::string m_id;
        std::string m_version;
        std::string m_filename;
        std::string m_recordedAt;
        std::string m_date;
        double m_durationSeconds = 0;
        uint64_t m_size = 0;
        std::string m_suggestedTitle;
        std::string m_state;
        std::string m_title;
        std::string m_permalink;
        std::string m_jobId;
        std::string m_error;
        double m_progress = 0;
        std::string m_candidateId;
    };

    struct Track
    {
        std::string m_id;
        std::string m_title;
        std::string m_permalink;
        double m_durationMs = 0;
    };

    struct Status
    {
        bool m_available = false;
        bool m_catalogReady = false;
        bool m_refreshing = false;
        bool m_busy = false;
        bool m_selected = false;
        Account m_account;
        std::string m_error;
        std::vector<Recording> m_recordings;
        std::vector<Track> m_tracks;
        std::string m_pendingRecordingId;
        std::string m_pendingRequestId;

        bool CanEditRecording(const std::string& recordingId) const
        {
            return !m_busy && (m_pendingRequestId.empty() || m_pendingRecordingId != recordingId);
        }

        bool CanSubmitRecording(const std::string& recordingId) const
        {
            return !m_busy && (m_pendingRequestId.empty() || m_pendingRecordingId == recordingId);
        }
    };

    struct Upload
    {
        std::string m_recordingId;
        std::string m_version;
        std::string m_requestId;
        bool m_useDefault = true;
        std::string m_title;
        std::string m_retryOf;
        std::vector<unsigned char> m_artwork;
        std::string m_artworkType;
    };

    struct LinkRequest
    {
        std::string m_recordingId;
        std::string m_version;
        std::string m_trackId;
    };

    PublishingClient();
    ~PublishingClient();
    void Open();
    void Close();
    void SetForeground(bool active);
    void Select(const SyncClient::Receiver& receiver, const std::string& token);
    void Refresh();
    void Submit(const Upload& request);
    void Link(const LinkRequest& request);
    void SetSyncBusy(bool busy);
    Status Snapshot() const;
    static std::string NewRequestId();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

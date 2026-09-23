#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// UI-owned sync. No dependency on the audio engine or its thread/state.
// All entry points are called on the message thread. Close drains work.
//
struct SyncClient
{
    struct Receiver
    {
        std::string m_id;
        std::string m_name;
        std::string m_host;
        std::string m_fingerprint;
        int m_port = 0;
    };

    struct Status
    {
        std::vector<Receiver> m_receivers;
        std::string m_message = "Open Sync to discover receivers";
        uint64_t m_sent = 0;
        uint64_t m_total = 0;
        double m_bytesPerSecond = 0;
        bool m_busy = false;
        bool m_failed = false;
    };

    explicit SyncClient(std::string root);
    ~SyncClient();
    void Open();
    void Close();
    void Start(const Receiver& receiver, const std::string& token);
    void Cancel();
    Status Snapshot() const;
    static std::string LoadToken(const Receiver& receiver);
    static bool SaveToken(const Receiver& receiver, const std::string& token);
    void SetForeground(bool active);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#include "PublishingClient.hpp"
#import <Foundation/Foundation.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <functional>

static bool Wait(const std::function<bool()>& predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(12);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (predicate())
        {
            return true;
        }

        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.02]];
    }

    return false;
}

int main(int argc, char** argv)
{
    @autoreleasepool
    {
        if (argc != 6)
        {
            return 2;
        }

        SyncClient::Receiver receiver{"test", "Loopback", "127.0.0.1", argv[3], std::stoi(argv[2])};
        const std::string mode = argv[4];
        PublishingClient client;
        client.Select(receiver, "test-code");
        client.Refresh();
        if (client.Snapshot().m_busy)
        {
            return 3;
        }

        client.Open();
        SyncClient sync(argv[1]);
        sync.Open();
        if (mode == "concurrent")
        {
            sync.Start(receiver, "test-code");
        }

        client.Select(receiver, "test-code");
        if (mode == "wrong" || mode == "old")
        {
            if (!Wait([&]() { return !client.Snapshot().m_error.empty(); }))
            {
                return 4;
            }

            if (client.Snapshot().m_available || !client.Snapshot().m_recordings.empty())
            {
                return 5;
            }

            client.Close();
            std::cout << "unavailable" << std::endl;
            return 0;
        }

        if (!Wait([&]() { return client.Snapshot().m_catalogReady && !client.Snapshot().m_recordings.empty(); }))
        {
            std::cerr << client.Snapshot().m_error << std::endl;
            return 10;
        }

        const auto initial = client.Snapshot();
        if (initial.m_recordings.front().m_date != "2026/10/02")
        {
            return 11;
        }

        if (mode == "concurrent")
        {
            if (!sync.Snapshot().m_busy)
            {
                return 12;
            }

            sync.Cancel();
        }

        PublishingClient::Upload request;
        request.m_recordingId = initial.m_recordings.front().m_id;
        request.m_version = initial.m_recordings.front().m_version;
        request.m_requestId = "native-request";
        request.m_title = "Custom title";
        if (mode == "retry-lost")
        {
            request.m_retryOf = initial.m_recordings.front().m_jobId;
        }

        request.m_useDefault = false;
        request.m_artworkType = "image/png";
        std::ifstream image(argv[5], std::ios::binary);
        request.m_artwork.assign(std::istreambuf_iterator<char>(image), {});
        if (mode == "default")
        {
            request.m_useDefault = true;
            request.m_artwork.clear();
        }

        if (mode == "large-metadata")
        {
            request.m_title.assign(70000, 'x');
        }

        client.Submit(request);
        if (mode == "large-metadata")
        {
            if (!Wait([&]() { auto s = client.Snapshot(); return !s.m_busy && !s.m_error.empty(); }))
            {
                return 27;
            }

            client.Close();
            sync.Close();
            return 0;
        }

        if (mode == "retry-lost")
        {
            if (!Wait([&]() { auto s = client.Snapshot(); return !s.m_busy && !s.m_error.empty(); }))
            {
                return 24;
            }

            [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:2.3]];
            if (client.Snapshot().m_pendingRequestId != "native-request")
            {
                return 25;
            }

            client.Submit(request);
        }

        if (mode == "close" || mode == "background")
        {
            if (!Wait([&]() { auto s = client.Snapshot(); return !s.m_recordings.front().m_jobId.empty(); }))
            {
                return 13;
            }

            if (mode == "background")
            {
                client.SetForeground(false);
            }
            else
            {
                client.Close();
            }

            client.Refresh();
            client.Submit(request);
            [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:2.4]];
            if (mode == "background")
            {
                client.SetForeground(true);
            }
            else
            {
                client.Open();
            }

            if (client.Snapshot().m_selected)
            {
                return 26;
            }

            client.Select(receiver, "test-code");
        }

        if (!Wait([&]() { auto s = client.Snapshot(); return !s.m_recordings.empty() && s.m_recordings.front().m_state == "uploaded"; }))
        {
            std::cerr << client.Snapshot().m_error << std::endl;
            return 20;
        }

        const auto snapshot = client.Snapshot();
        if (snapshot.m_recordings.front().m_state != "uploaded"
            || snapshot.m_recordings.front().m_title != (mode == "default" ? "Smart Grid Nightly 2026/10/02" : "Custom title")
            || snapshot.m_recordings.front().m_permalink.empty())
        {
            return 21;
        }

        const auto older = snapshot.m_recordings.back();
        client.Link({older.m_id, older.m_version, "historic-track"});
        if (!Wait([&]() { return client.Snapshot().m_recordings.back().m_state == "uploaded"; }))
        {
            return 22;
        }

        client.Close();
        if (client.Snapshot().m_selected || client.Snapshot().m_busy)
        {
            return 23;
        }

        client.Refresh();
        client.Submit(request);
        client.Select(receiver, "test-code");
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:2.2]];
        sync.Close();
        std::cout << "uploaded and linked; closed" << std::endl;
        return 0;
    }
}

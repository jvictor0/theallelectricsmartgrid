#include "PublishingClient.hpp"
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <fstream>
#include <iostream>
#include <functional>

static dispatch_semaphore_t x_taskEntered;
static dispatch_semaphore_t x_taskRelease;
static IMP x_originalCreate;
static std::atomic<bool> x_taskPaused{false};

static id PausedTaskCreate(id session, SEL selector, NSURLRequest* request, id completion)
{
    bool expected = false;
    if (x_taskPaused.compare_exchange_strong(expected, true))
    {
        dispatch_semaphore_signal(x_taskEntered);
        dispatch_semaphore_wait(x_taskRelease, DISPATCH_TIME_FOREVER);
    }

    using Create = id (*)(id, SEL, NSURLRequest*, id);
    return reinterpret_cast<Create>(x_originalCreate)(session, selector, request, completion);
}

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
        if (mode == "close-race")
        {
            x_taskEntered = dispatch_semaphore_create(0);
            x_taskRelease = dispatch_semaphore_create(0);
            NSURLSession* probe = [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration ephemeralSessionConfiguration]];
            const SEL selector = @selector(dataTaskWithRequest:completionHandler:);
            Method method = class_getInstanceMethod([probe class], selector);
            x_originalCreate = method_setImplementation(method, reinterpret_cast<IMP>(PausedTaskCreate));
            [probe invalidateAndCancel];
            client.Select(receiver, "test-code");
            if (dispatch_semaphore_wait(x_taskEntered, dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC)) != 0)
            {
                return 28;
            }

            std::thread release([]()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                dispatch_semaphore_signal(x_taskRelease);
            });
            client.Close();
            release.join();
            method_setImplementation(method, x_originalCreate);
            if (client.Snapshot().m_busy || client.Snapshot().m_selected)
            {
                return 29;
            }

            std::cout << "closed during task creation" << std::endl;
            return 0;
        }

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

        if (mode == "final-sync")
        {
            if (!Wait([&]() { auto state = client.Snapshot(); return !state.m_busy && !state.m_refreshing; }))
            {
                return 36;
            }

            if (client.Snapshot().m_recordings.size() != 2)
            {
                return 37;
            }

            const auto started = std::chrono::steady_clock::now();
            client.SetSyncBusy(true);
            sync.Start(receiver, "test-code");
            if (!Wait([&]() { return !sync.Snapshot().m_busy; }) || sync.Snapshot().m_failed)
            {
                return 38;
            }

            if (std::chrono::steady_clock::now() - started >= std::chrono::milliseconds(1900))
            {
                return 39;
            }

            client.SetSyncBusy(false);
            if (!Wait([&]()
            {
                auto state = client.Snapshot();
                return state.m_recordings.size() == 3 && state.m_recordings.front().m_date == "2026/10/03";
            }))
            {
                return 40;
            }

            client.Close();
            client.SetSyncBusy(true);
            client.SetSyncBusy(false);
            sync.Close();
            [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:2.2]];
            std::cout << "final synced recording visible" << std::endl;
            return 0;
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

        PublishingClient::Upload second;
        if (mode == "two-recordings")
        {
            if (!Wait([&]() { auto status = client.Snapshot(); return !status.m_busy && !status.m_error.empty(); }))
            {
                return 30;
            }

            second = request;
            second.m_recordingId = initial.m_recordings.back().m_id;
            second.m_version = initial.m_recordings.back().m_version;
            second.m_requestId = "second-request";
            second.m_title = "Old second title";
            const auto controls = client.Snapshot();
            if (controls.CanEditRecording(request.m_recordingId) || !controls.CanEditRecording(second.m_recordingId)
                || !controls.CanSubmitRecording(request.m_recordingId) || controls.CanSubmitRecording(second.m_recordingId))
            {
                return 34;
            }

            client.Submit(second);
            [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.3]];
            const auto uncertain = client.Snapshot();
            if (uncertain.m_pendingRequestId != "native-request" || uncertain.m_pendingRecordingId != request.m_recordingId)
            {
                return 31;
            }

            client.Submit(request);
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

        if (mode == "two-recordings")
        {
            if (!client.Snapshot().CanSubmitRecording(second.m_recordingId) || !client.Snapshot().CanEditRecording(second.m_recordingId))
            {
                return 35;
            }

            second.m_title = "Second current title";
            second.m_artwork.push_back('B');
            client.Submit(second);
            if (!Wait([&]() { return client.Snapshot().m_recordings.back().m_state == "uploaded"; }))
            {
                return 32;
            }

            if (client.Snapshot().m_recordings.back().m_title != "Second current title")
            {
                return 33;
            }

            client.Close();
            sync.Close();
            [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:2.2]];
            return 0;
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

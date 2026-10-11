#include "PublishingClient.hpp"
#include "PinnedReceiver.hpp"
#import <TargetConditionals.h>
#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
#endif
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>

static NSString* PublishingNative(const std::string& text)
{
    return [NSString stringWithUTF8String:text.c_str()];
}

static std::string PublishingText(id value)
{
    return [value isKindOfClass:[NSString class]] ? std::string([value UTF8String]) : std::string();
}

@interface SGPublishingDelegate : NSObject <NSURLSessionTaskDelegate>
{
@public
    std::string m_fingerprint;
    dispatch_semaphore_t m_invalidated;
}
@end

@implementation SGPublishingDelegate
- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential*))completion
{
    if (![challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust])
    {
        completion(NSURLSessionAuthChallengePerformDefaultHandling, nil);
        return;
    }

    SecTrustRef trust = challenge.protectionSpace.serverTrust;
    const bool matched = ReceiverCertificateMatches(trust, m_fingerprint);
    completion(matched ? NSURLSessionAuthChallengeUseCredential : NSURLSessionAuthChallengeCancelAuthenticationChallenge,
        matched ? [NSURLCredential credentialForTrust:trust] : nil);
}

- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task willPerformHTTPRedirection:(NSHTTPURLResponse*)response newRequest:(NSURLRequest*)request completionHandler:(void (^)(NSURLRequest*))completion
{
    completion(nil);
}

- (void)URLSession:(NSURLSession*)session didBecomeInvalidWithError:(NSError*)error
{
    dispatch_semaphore_signal(m_invalidated);
}
@end

struct PublishingClient::Impl
{
    enum struct Kind
    {
        Refresh,
        Upload,
        Link
    };

    struct Command
    {
        Kind m_kind;
        Upload m_upload;
        LinkRequest m_link;
    };

    mutable std::mutex m_statusMutex;
    Status m_status;
    std::mutex m_commandsMutex;
    std::condition_variable m_wake;
    std::deque<Command> m_commands;
    std::atomic<bool> m_cancelled{true};
    std::atomic<bool> m_syncBusy{false};
    std::thread m_worker;
    bool m_open = false;
    bool m_foreground = true;
    id m_inactiveObserver = nil;
    id m_activeObserver = nil;
    SyncClient::Receiver m_receiver;
    std::string m_token;
    std::string m_pendingPreviousJobId;
    NSURLSession* m_session = nil;
    NSOperationQueue* m_delegateQueue = nil;
    SGPublishingDelegate* m_delegate = nil;

    void Stop()
    {
        m_cancelled.store(true);
        [m_session invalidateAndCancel];
        m_wake.notify_all();
        if (m_worker.joinable())
        {
            m_worker.join();
        }

        if (m_session != nil)
        {
            dispatch_semaphore_wait(m_delegate->m_invalidated, DISPATCH_TIME_FOREVER);
            [m_delegateQueue waitUntilAllOperationsAreFinished];
        }

        m_session = nil;
        m_delegate = nil;
        m_delegateQueue = nil;
        {
            std::lock_guard<std::mutex> lock(m_commandsMutex);
            m_commands.clear();
        }

        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_status.m_busy = false;
        m_status.m_selected = false;
    }

    void Queue(Command command)
    {
        if (m_cancelled.load())
        {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(m_commandsMutex);
            m_commands.push_back(std::move(command));
        }

        {
            std::lock_guard<std::mutex> lock(m_statusMutex);
            m_status.m_busy = true;
        }

        m_wake.notify_all();
    }

    struct Response
    {
        NSData* m_data = nil;
        NSError* m_error = nil;
        NSInteger m_code = 0;
        dispatch_semaphore_t m_done = dispatch_semaphore_create(0);
    };

    NSDictionary* Request(const std::string& method, const std::string& path, NSData* body, NSString* type)
    {
        if (m_cancelled.load())
        {
            throw std::runtime_error("Publishing controls closed");
        }

        NSURLComponents* url = [[NSURLComponents alloc] init];
        url.scheme = @"https";
        url.host = PublishingNative(m_receiver.m_host);
        url.port = @(m_receiver.m_port);
        url.path = PublishingNative("/v1/publishing" + path);
        NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:url.URL];
        request.HTTPMethod = PublishingNative(method);
        [request setValue:PublishingNative("Bearer " + m_token) forHTTPHeaderField:@"Authorization"];
        request.HTTPBody = body;
        if (type != nil)
        {
            [request setValue:type forHTTPHeaderField:@"Content-Type"];
        }

        const auto response = std::make_shared<Response>();
        NSURLSessionDataTask* task = [m_session dataTaskWithRequest:request completionHandler:^(NSData* data, NSURLResponse* reply, NSError* error)
        {
            response->m_data = data;
            response->m_error = error;
            response->m_code = [reply isKindOfClass:[NSHTTPURLResponse class]] ? static_cast<NSHTTPURLResponse*>(reply).statusCode : 0;
            dispatch_semaphore_signal(response->m_done);
        }];
        [task resume];
        dispatch_semaphore_wait(response->m_done, DISPATCH_TIME_FOREVER);
        if (m_cancelled.load())
        {
            throw std::runtime_error("Publishing controls closed");
        }

        if (response->m_error != nil)
        {
            throw std::runtime_error(PublishingText(response->m_error.localizedDescription));
        }

        if (response->m_code == 404)
        {
            std::lock_guard<std::mutex> lock(m_statusMutex);
            m_status.m_available = false;
            throw std::runtime_error("This Mac receiver does not support SoundCloud publishing. Update the receiver; sync remains available.");
        }

        id json = response->m_data == nil ? nil : [NSJSONSerialization JSONObjectWithData:response->m_data options:0 error:nil];
        if (response->m_code < 200 || response->m_code >= 300)
        {
            if (path == "/uploads")
            {
                std::lock_guard<std::mutex> lock(m_statusMutex);
                m_status.m_pendingRecordingId.clear();
                m_status.m_pendingRequestId.clear();
            }

            const std::string message = [json isKindOfClass:[NSDictionary class]] ? PublishingText(json[@"error"]) : std::string();
            throw std::runtime_error(message.empty() ? "Mac publishing request failed (" + std::to_string(response->m_code) + ")" : message);
        }

        if (![json isKindOfClass:[NSDictionary class]])
        {
            throw std::runtime_error("Invalid publishing response from Mac");
        }

        return json;
    }

    void ReadSnapshot()
    {
        NSDictionary* json = Request("GET", "", nil, nil);
        Status next;
        next.m_selected = true;
        next.m_available = [json[@"available"] boolValue];
        next.m_catalogReady = [json[@"catalog_ready"] boolValue];
        next.m_refreshing = [json[@"refreshing"] boolValue];
        next.m_error = PublishingText(json[@"error"]);
        NSDictionary* account = json[@"account"];
        if ([account isKindOfClass:[NSDictionary class]])
        {
            next.m_account = {PublishingText(account[@"id"]), PublishingText(account[@"name"]), static_cast<bool>([account[@"connected"] boolValue])};
        }

        id rows = json[@"recordings"];
        if ([rows isKindOfClass:[NSArray class]])
        {
            for (NSDictionary* row in rows)
            {
                if (![row isKindOfClass:[NSDictionary class]])
                {
                    continue;
                }

                Recording recording;
                recording.m_id = PublishingText(row[@"id"]);
                recording.m_version = PublishingText(row[@"version"]);
                recording.m_filename = PublishingText(row[@"filename"]);
                recording.m_recordedAt = PublishingText(row[@"recorded_at"]);
                recording.m_date = PublishingText(row[@"date"]);
                recording.m_durationSeconds = [row[@"duration_seconds"] doubleValue];
                recording.m_size = [row[@"size"] unsignedLongLongValue];
                recording.m_suggestedTitle = PublishingText(row[@"suggested_title"]);
                recording.m_state = PublishingText(row[@"state"]);
                recording.m_title = PublishingText(row[@"title"]);
                recording.m_permalink = PublishingText(row[@"permalink"]);
                recording.m_jobId = PublishingText(row[@"job_id"]);
                recording.m_error = PublishingText(row[@"error"]);
                recording.m_progress = [row[@"progress"] doubleValue];
                recording.m_candidateId = PublishingText(row[@"candidate_id"]);
                next.m_recordings.push_back(std::move(recording));
            }
        }

        std::stable_sort(next.m_recordings.begin(), next.m_recordings.end(), [](const Recording& first, const Recording& second)
        {
            return first.m_recordedAt > second.m_recordedAt;
        });
        id tracks = json[@"tracks"];
        if ([tracks isKindOfClass:[NSArray class]])
        {
            for (NSDictionary* track in tracks)
            {
                if ([track isKindOfClass:[NSDictionary class]])
                {
                    next.m_tracks.push_back({PublishingText(track[@"id"]), PublishingText(track[@"title"]),
                        PublishingText(track[@"permalink"]), [track[@"duration_ms"] doubleValue]});
                }
            }
        }

        std::lock_guard<std::mutex> lock(m_statusMutex);
        next.m_pendingRequestId = m_status.m_pendingRequestId;
        next.m_pendingRecordingId = m_status.m_pendingRecordingId;
        for (const auto& row : next.m_recordings)
        {
            if (row.m_id == next.m_pendingRecordingId && !row.m_jobId.empty() && row.m_jobId != m_pendingPreviousJobId)
            {
                next.m_pendingRecordingId.clear();
                next.m_pendingRequestId.clear();
            }
        }

        m_status = std::move(next);
    }

    static NSData* JSON(NSDictionary* json)
    {
        return [NSJSONSerialization dataWithJSONObject:json options:0 error:nil];
    }

    void Execute(const Command& command)
    {
        if (command.m_kind == Kind::Refresh)
        {
            Request("POST", "/refresh", JSON(@{}), @"application/json");
        }
        else if (command.m_kind == Kind::Link)
        {
            const auto& link = command.m_link;
            Request("POST", "/links", JSON(@{@"recording_id": PublishingNative(link.m_recordingId),
                @"version": PublishingNative(link.m_version), @"track_id": PublishingNative(link.m_trackId)}), @"application/json");
        }
        else
        {
            const auto& upload = command.m_upload;
            constexpr size_t x_maxArtwork = 10 * 1024 * 1024;
            if (upload.m_artwork.size() > x_maxArtwork || upload.m_requestId.empty())
            {
                throw std::runtime_error("Artwork exceeds 10 MiB or request identity is missing");
            }

            NSDictionary* metadata = @{@"recording_id": PublishingNative(upload.m_recordingId), @"version": PublishingNative(upload.m_version),
                @"request_id": PublishingNative(upload.m_requestId), @"title": upload.m_useDefault ? [NSNull null] : PublishingNative(upload.m_title),
                @"retry_of": upload.m_retryOf.empty() ? [NSNull null] : PublishingNative(upload.m_retryOf)};
            NSData* metadataData = JSON(metadata);
            constexpr NSUInteger x_maxMetadata = 32 * 1024;
            if (metadataData == nil || metadataData.length > x_maxMetadata)
            {
                throw std::runtime_error("Publishing metadata exceeds receiver limit");
            }

            const std::string boundary = "SmartGrid-" + NewRequestId();
            NSMutableData* body = [NSMutableData data];
            auto append = [&](const std::string& text)
            {
                [body appendData:[PublishingNative(text) dataUsingEncoding:NSUTF8StringEncoding]];
            };
            append("--" + boundary + "\r\nContent-Disposition: form-data; name=\"metadata\"\r\nContent-Type: application/json\r\n\r\n");
            [body appendData:metadataData];
            if (!upload.m_artwork.empty())
            {
                if (upload.m_artworkType != "image/jpeg" && upload.m_artworkType != "image/png")
                {
                    throw std::runtime_error("Choose JPEG or PNG artwork");
                }

                append("\r\n--" + boundary + "\r\nContent-Disposition: form-data; name=\"artwork\"; filename=\"cover\"\r\nContent-Type: " + upload.m_artworkType + "\r\n\r\n");
                [body appendBytes:upload.m_artwork.data() length:upload.m_artwork.size()];
            }

            append("\r\n--" + boundary + "--\r\n");
            {
                std::lock_guard<std::mutex> lock(m_statusMutex);
                m_status.m_pendingRecordingId = upload.m_recordingId;
                m_status.m_pendingRequestId = upload.m_requestId;
                m_pendingPreviousJobId = upload.m_retryOf;
            }

            Request("POST", "/uploads", body, PublishingNative("multipart/form-data; boundary=" + boundary));
        }

        ReadSnapshot();
    }

    bool PollNeeded()
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        if (m_syncBusy.load() || m_status.m_refreshing || !m_status.m_pendingRequestId.empty())
        {
            return true;
        }

        return std::any_of(m_status.m_recordings.begin(), m_status.m_recordings.end(), [](const Recording& row)
        {
            return row.m_state == "queued" || row.m_state == "uploading";
        });
    }

    void Run()
    {
        @autoreleasepool
        {
            while (!m_cancelled.load())
            {
                Command command{Kind::Refresh, {}, {}};
                bool hasCommand = false;
                {
                    std::unique_lock<std::mutex> lock(m_commandsMutex);
                    if (m_commands.empty())
                    {
                        m_wake.wait_for(lock, std::chrono::seconds(2), [&]() { return m_cancelled.load() || !m_commands.empty(); });
                    }

                    if (m_cancelled.load())
                    {
                        break;
                    }

                    if (!m_commands.empty())
                    {
                        command = std::move(m_commands.front());
                        m_commands.pop_front();
                        hasCommand = true;
                    }
                }

                if (!hasCommand && !PollNeeded())
                {
                    continue;
                }

                {
                    std::lock_guard<std::mutex> lock(m_statusMutex);
                    m_status.m_busy = true;
                }

                try
                {
                    if (hasCommand)
                    {
                        Execute(command);
                    }
                    else
                    {
                        ReadSnapshot();
                    }
                }
                catch (const std::exception& error)
                {
                    if (!m_cancelled.load())
                    {
                        std::lock_guard<std::mutex> lock(m_statusMutex);
                        m_status.m_error = error.what();
                    }
                }

                std::lock_guard<std::mutex> lock(m_statusMutex);
                m_status.m_busy = false;
            }
        }
    }
};

PublishingClient::PublishingClient() : m_impl(std::make_unique<Impl>())
{
}

PublishingClient::~PublishingClient()
{
    Close();
}

void PublishingClient::Open()
{
    assert([NSThread isMainThread]);
    m_impl->m_open = true;
#if TARGET_OS_IOS
    auto* center = [NSNotificationCenter defaultCenter];
    if (m_impl->m_inactiveObserver == nil)
    {
        m_impl->m_inactiveObserver = [center addObserverForName:UIApplicationWillResignActiveNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification*)
        {
            SetForeground(false);
        }];
        m_impl->m_activeObserver = [center addObserverForName:UIApplicationDidBecomeActiveNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification*)
        {
            SetForeground(true);
        }];
    }

    SetForeground([UIApplication sharedApplication].applicationState == UIApplicationStateActive);
#else
    SetForeground(true);
#endif
}

void PublishingClient::Close()
{
    assert([NSThread isMainThread]);
    m_impl->m_open = false;
    [[NSNotificationCenter defaultCenter] removeObserver:m_impl->m_inactiveObserver];
    [[NSNotificationCenter defaultCenter] removeObserver:m_impl->m_activeObserver];
    m_impl->m_inactiveObserver = nil;
    m_impl->m_activeObserver = nil;
    m_impl->Stop();
}

void PublishingClient::SetForeground(bool active)
{
    assert([NSThread isMainThread]);
    m_impl->m_foreground = active;
    if (!active)
    {
        m_impl->Stop();
    }
}

void PublishingClient::Select(const SyncClient::Receiver& receiver, const std::string& token)
{
    assert([NSThread isMainThread]);
    auto& state = *m_impl;
    if (!state.m_open || !state.m_foreground || token.empty())
    {
        return;
    }

    state.Stop();
    if (state.m_receiver.m_id != receiver.m_id || state.m_receiver.m_fingerprint != receiver.m_fingerprint)
    {
        std::lock_guard<std::mutex> lock(state.m_statusMutex);
        state.m_status = {};
    }

    state.m_receiver = receiver;
    state.m_token = token;
    state.m_cancelled.store(false);
    state.m_delegate = [[SGPublishingDelegate alloc] init];
    state.m_delegate->m_fingerprint = receiver.m_fingerprint;
    state.m_delegate->m_invalidated = dispatch_semaphore_create(0);
    state.m_delegateQueue = [[NSOperationQueue alloc] init];
    state.m_delegateQueue.maxConcurrentOperationCount = 1;
    auto* configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
    configuration.URLCache = nil;
    configuration.HTTPCookieStorage = nil;
    configuration.URLCredentialStorage = nil;
    configuration.timeoutIntervalForRequest = 15;
    configuration.timeoutIntervalForResource = 30;
    configuration.waitsForConnectivity = NO;
    state.m_session = [NSURLSession sessionWithConfiguration:configuration delegate:state.m_delegate delegateQueue:state.m_delegateQueue];
    {
        std::lock_guard<std::mutex> lock(state.m_statusMutex);
        state.m_status.m_selected = true;
    }

    state.Queue({Impl::Kind::Refresh, {}, {}});
    state.m_worker = std::thread([&state]() { state.Run(); });
}

void PublishingClient::Refresh()
{
    m_impl->Queue({Impl::Kind::Refresh, {}, {}});
}

void PublishingClient::Submit(const Upload& request)
{
    m_impl->Queue({Impl::Kind::Upload, request, {}});
}

void PublishingClient::Link(const LinkRequest& request)
{
    m_impl->Queue({Impl::Kind::Link, {}, request});
}

void PublishingClient::SetSyncBusy(bool busy)
{
    m_impl->m_syncBusy.store(busy);
}

PublishingClient::Status PublishingClient::Snapshot() const
{
    std::lock_guard<std::mutex> lock(m_impl->m_statusMutex);
    return m_impl->m_status;
}

std::string PublishingClient::NewRequestId()
{
    return PublishingText([[NSUUID UUID] UUIDString]);
}

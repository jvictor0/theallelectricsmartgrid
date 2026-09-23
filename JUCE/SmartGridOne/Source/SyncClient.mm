#include "SyncClient.hpp"

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <CommonCrypto/CommonDigest.h>
#import <TargetConditionals.h>
#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
#endif
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstring>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

static NSString* Native(const std::string& value)
{
    return [NSString stringWithUTF8String:value.c_str()];
}

static std::string Text(NSString* value)
{
    return [value isKindOfClass:[NSString class]] ? std::string(value.UTF8String) : std::string();
}

static std::string Hex(const unsigned char* bytes, size_t count)
{
    static constexpr char x_digits[] = "0123456789abcdef";
    std::string result;
    for (size_t i = 0; i < count; ++i)
    {
        result += x_digits[bytes[i] >> 4];
        result += x_digits[bytes[i] & 15];
    }

    return result;
}

@interface SGSyncDelegate : NSObject <NSNetServiceBrowserDelegate, NSNetServiceDelegate, NSURLSessionTaskDelegate>
{
@public
    SyncClient::Impl* m_owner;
    dispatch_semaphore_t m_invalidated;
}
@end

struct SyncClient::Impl
{
    explicit Impl(std::string root) : m_root(std::move(root))
    {
    }

    fs::path m_root;
    mutable std::mutex m_statusMutex;
    Status m_status;
    bool m_open = false;
    bool m_requestedOpen = false;
    id m_inactiveObserver = nil;
    id m_activeObserver = nil;
    std::atomic<bool> m_cancelled{true};
    std::thread m_worker;
    std::mutex m_networkMutex;
    NSURLSession* m_session = nil;
    NSURLSessionTask* m_task = nil;
    NSOperationQueue* m_delegateQueue = nil;
    SGSyncDelegate* m_delegate = nil;
    NSNetServiceBrowser* m_browser = nil;
    NSMutableArray<NSNetService*>* m_services = nil;
    Receiver m_receiver;
    std::string m_token;
    std::chrono::steady_clock::time_point m_transferStart;

    void Check() const
    {
        if (m_cancelled.load())
        {
            throw std::runtime_error("Sync cancelled; unacknowledged recordings retained");
        }
    }

    void Message(const std::string& message)
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_status.m_message = message;
    }

    void Progress(int64_t sent, int64_t total)
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_status.m_sent = static_cast<uint64_t>(std::max<int64_t>(0, sent));
        m_status.m_total = static_cast<uint64_t>(std::max<int64_t>(0, total));
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_transferStart).count();
        m_status.m_bytesPerSecond = seconds > 0 ? m_status.m_sent / seconds : 0;
    }

    std::string Hash(const fs::path& path)
    {
        Check();
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Cannot read " + path.filename().string());
        }

        CC_SHA256_CTX context;
        CC_SHA256_Init(&context);
        char buffer[256 * 1024];
        while (file)
        {
            Check();
            file.read(buffer, sizeof(buffer));
            CC_SHA256_Update(&context, buffer, static_cast<CC_LONG>(file.gcount()));
        }

        if (!file.eof())
        {
            throw std::runtime_error("Read failed: " + path.filename().string());
        }

        unsigned char digest[CC_SHA256_DIGEST_LENGTH];
        CC_SHA256_Final(digest, &context);
        return Hex(digest, sizeof(digest));
    }

    static bool SafePath(const std::string& relative)
    {
        const fs::path path(relative);
        if (relative.empty() || path.is_absolute() || relative.find('\\') != std::string::npos || relative.find('\0') != std::string::npos)
        {
            return false;
        }

        for (const auto& part : path)
        {
            if (part == ".." || part == "." || part.empty())
            {
                return false;
            }
        }

        return true;
    }

    fs::path Local(const std::string& kind, const std::string& relative)
    {
        if (!SafePath(relative))
        {
            throw std::runtime_error("Receiver supplied an unsafe path");
        }

        auto path = m_root;
        for (const auto& part : fs::path(kind) / relative)
        {
            path /= part;
            if (fs::is_symlink(fs::symlink_status(path)))
            {
                throw std::runtime_error("Sync refuses symbolic links");
            }
        }

        return path;
    }

    std::vector<std::string> Files(const std::string& kind)
    {
        std::vector<std::string> result;
        const auto root = m_root / kind;
        if (!fs::exists(root))
        {
            return result;
        }

        if (fs::is_symlink(root))
        {
            throw std::runtime_error("Sync refuses symbolic links");
        }

        for (const auto& entry : fs::recursive_directory_iterator(root))
        {
            Check();
            if (entry.is_symlink())
            {
                throw std::runtime_error("Sync refuses symbolic links");
            }

            if (entry.is_regular_file())
            {
                result.push_back(entry.path().lexically_relative(root).generic_string());
            }
        }

        std::sort(result.begin(), result.end());
        return result;
    }

    static uint64_t Little(const unsigned char* data, size_t count)
    {
        uint64_t value = 0;
        for (size_t i = 0; i < count; ++i)
        {
            value |= static_cast<uint64_t>(data[i]) << (8 * i);
        }

        return value;
    }

    bool CompleteRecording(const fs::path& path)
    {
        Check();
        const auto size = fs::file_size(path);
        std::ifstream file(path, std::ios::binary);
        unsigned char header[48]{};
        file.read(reinterpret_cast<char*>(header), sizeof(header));
        if (file.gcount() < 16)
        {
            return false;
        }

        if (memcmp(header, "SMRTGRID", 8) == 0)
        {
            file.clear();
            file.seekg(-16, std::ios::end);
            char tail[16]{};
            file.read(tail, sizeof(tail));
            return file.gcount() == 16 && memcmp(tail, "END1", 4) == 0;
        }

        if (memcmp(header + 8, "WAVE", 4) != 0)
        {
            return false;
        }

        if (memcmp(header, "RIFF", 4) == 0)
        {
            return Little(header + 4, 4) + 8 == size;
        }

        return size >= 48 && memcmp(header, "RF64", 4) == 0
            && memcmp(header + 12, "ds64", 4) == 0 && Little(header + 20, 8) + 8 == size;
    }

    struct Response
    {
        NSData* m_data = nil;
        NSError* m_error = nil;
        NSInteger m_code = 0;
        dispatch_semaphore_t m_done = dispatch_semaphore_create(0);
    };

    NSData* Request(const std::string& method, const std::string& endpoint,
        const std::string& relative = {}, const fs::path& upload = {},
        const std::string& digest = {}, const fs::path& download = {})
    {
        Check();
        NSURLComponents* url = [[NSURLComponents alloc] init];
        url.scheme = @"https";
        url.host = Native(m_receiver.m_host);
        url.port = @(m_receiver.m_port);
        url.path = Native("/v1/" + endpoint);
        NSMutableArray<NSURLQueryItem*>* query = [NSMutableArray array];
        if (!relative.empty())
        {
            [query addObject:[NSURLQueryItem queryItemWithName:@"path" value:Native(relative)]];
        }

        if (!digest.empty() && method == "GET")
        {
            [query addObject:[NSURLQueryItem queryItemWithName:@"sha256" value:Native(digest)]];
        }

        url.queryItems = query;
        NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:url.URL];
        request.HTTPMethod = Native(method);
        [request setValue:Native("Bearer " + m_token) forHTTPHeaderField:@"Authorization"];
        if (!upload.empty())
        {
            [request setValue:Native(digest) forHTTPHeaderField:@"X-SHA256"];
            [request setValue:Native(std::to_string(fs::file_size(upload))) forHTTPHeaderField:@"Content-Length"];
            [request setValue:@"application/octet-stream" forHTTPHeaderField:@"Content-Type"];
        }

        auto response = std::make_shared<Response>();
        const auto finish = ^(NSData* data, NSURLResponse* result, NSError* error)
        {
            response->m_data = data;
            response->m_error = error;
            response->m_code = [result isKindOfClass:[NSHTTPURLResponse class]] ? static_cast<NSHTTPURLResponse*>(result).statusCode : 0;
            dispatch_semaphore_signal(response->m_done);
        };
        {
            std::lock_guard<std::mutex> lock(m_networkMutex);
            Check();
            if (!upload.empty())
            {
                m_task = [m_session uploadTaskWithRequest:request fromFile:[NSURL fileURLWithPath:Native(upload.string())] completionHandler:finish];
            }
            else if (!download.empty())
            {
                m_task = [m_session downloadTaskWithRequest:request completionHandler:^(NSURL* location, NSURLResponse* result, NSError* error)
                {
                    NSError* moveError = error;
                    if (location != nil && error == nil && !m_cancelled.load())
                    {
                        [[NSFileManager defaultManager] moveItemAtURL:location toURL:[NSURL fileURLWithPath:Native(download.string())] error:&moveError];
                    }

                    finish(nil, result, moveError);
                }];
            }
            else
            {
                m_task = [m_session dataTaskWithRequest:request completionHandler:finish];
            }

            m_transferStart = std::chrono::steady_clock::now();
            Progress(0, upload.empty() ? 0 : static_cast<int64_t>(fs::file_size(upload)));
            [m_task resume];
        }

        dispatch_semaphore_wait(response->m_done, DISPATCH_TIME_FOREVER);
        {
            std::lock_guard<std::mutex> lock(m_networkMutex);
            m_task = nil;
        }

        Check();
        if (response->m_error != nil)
        {
            throw std::runtime_error(Text(response->m_error.localizedDescription));
        }

        if (response->m_code < 200 || response->m_code >= 300)
        {
            std::string detail;
            if (response->m_data != nil)
            {
                id body = [NSJSONSerialization JSONObjectWithData:response->m_data options:0 error:nil];
                if ([body isKindOfClass:[NSDictionary class]] && [body[@"error"] isKindOfClass:[NSString class]])
                {
                    detail = ": " + Text(body[@"error"]);
                }
            }

            throw std::runtime_error("Receiver HTTP " + std::to_string(response->m_code) + detail);
        }

        return response->m_data;
    }

    NSDictionary* Json(NSData* data)
    {
        id result = data == nil ? nil : [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
        if (![result isKindOfClass:[NSDictionary class]])
        {
            throw std::runtime_error("Invalid receiver response");
        }

        return result;
    }

    void DownloadPatch(const std::string& relative, const std::string& digest)
    {
        const auto destination = Local("patches", relative);
        if (fs::exists(destination))
        {
            return;
        }

        fs::create_directories(destination.parent_path());
        auto partial = destination;
        partial += ".sync-" + Text([NSUUID UUID].UUIDString);
        try
        {
            Message("Receiving patch: " + relative);
            Request("GET", "patch", relative, {}, {}, partial);
            if (Hash(partial) != digest)
            {
                throw std::runtime_error("Patch checksum mismatch: " + relative);
            }

            Check();
            if (::link(partial.c_str(), destination.c_str()) != 0 && errno != EEXIST)
            {
                throw std::runtime_error("Cannot publish patch: " + relative);
            }

            fs::remove(partial);
        }
        catch (...)
        {
            fs::remove(partial);
            throw;
        }
    }

    void UploadLog(const std::string& relative)
    {
        const auto source = Local("logs", relative);
        const auto snapshot = fs::path(Text(NSTemporaryDirectory())) / ("smartgrid-log-" + Text([NSUUID UUID].UUIDString));
        try
        {
            std::ifstream input(source, std::ios::binary);
            std::ofstream output(snapshot, std::ios::binary);
            uint64_t remaining = fs::file_size(source);
            char bytes[256 * 1024];
            while (remaining > 0)
            {
                Check();
                const auto count = static_cast<std::streamsize>(std::min<uint64_t>(sizeof(bytes), remaining));
                if (!input.read(bytes, count) || !output.write(bytes, count))
                {
                    throw std::runtime_error("Cannot snapshot log: " + relative);
                }

                remaining -= static_cast<uint64_t>(count);
            }

            output.close();
            if (!output)
            {
                throw std::runtime_error("Cannot finish log snapshot");
            }

            Message("Sending log: " + relative);
            Request("PUT", "log", relative, snapshot, Hash(snapshot));
            fs::remove(snapshot);
        }
        catch (...)
        {
            fs::remove(snapshot);
            throw;
        }
    }

    void Run()
    {
        @autoreleasepool
        {
            try
            {
                Message("Comparing patches");
                NSDictionary* manifest = Json(Request("GET", "manifest"));
                if (![manifest[@"version"] isKindOfClass:[NSNumber class]] || [manifest[@"version"] intValue] != 1 || ![manifest[@"patches"] isKindOfClass:[NSArray class]])
                {
                    throw std::runtime_error("Unsupported receiver manifest");
                }

                std::map<std::string, std::string> remote;
                for (id patch in manifest[@"patches"])
                {
                    if (![patch isKindOfClass:[NSDictionary class]] || ![patch[@"path"] isKindOfClass:[NSString class]] || ![patch[@"sha256"] isKindOfClass:[NSString class]])
                    {
                        throw std::runtime_error("Invalid patch manifest");
                    }

                    const auto relative = Text(patch[@"path"]);
                    Local("patches", relative);
                    remote[relative] = Text(patch[@"sha256"]);
                }

                for (const auto& relative : Files("patches"))
                {
                    if (remote.count(relative) == 0)
                    {
                        const auto path = Local("patches", relative);
                        Message("Sending patch: " + relative);
                        Request("PUT", "patch", relative, path, Hash(path));
                    }
                }

                for (const auto& patch : remote)
                {
                    DownloadPatch(patch.first, patch.second);
                }

                struct Recording
                {
                    std::string m_relative;
                    std::string m_digest;
                    uintmax_t m_size;
                    fs::file_time_type m_modified;
                };
                std::vector<Recording> recordings;
                size_t skipped = 0;
                for (const auto& relative : Files("recordings"))
                {
                    const auto path = Local("recordings", relative);
                    if (!CompleteRecording(path))
                    {
                        ++skipped;
                        continue;
                    }

                    const auto size = fs::file_size(path);
                    const auto modified = fs::last_write_time(path);
                    Message("Checking recording: " + relative);
                    const auto digest = Hash(path);
                    Check();
                    Message("Sending recording " + std::to_string(recordings.size() + 1) + ": " + relative);
                    Request("PUT", "recording", relative, path, digest);
                    recordings.push_back({relative, digest, size, modified});
                }

                for (const auto& relative : Files("logs"))
                {
                    UploadLog(relative);
                }

                for (const auto& recording : recordings)
                {
                    Message("Extracting stereo: " + recording.m_relative);
                    while (true)
                    {
                        NSDictionary* receipt = Json(Request("GET", "recording", recording.m_relative, {}, recording.m_digest));
                        const auto state = Text(receipt[@"state"]);
                        if (state == "failed")
                        {
                            throw std::runtime_error("Stereo extraction failed: " + recording.m_relative + ": " + Text(receipt[@"error"]));
                        }

                        if (state == "complete" && Text(receipt[@"sha256"]) == recording.m_digest)
                        {
                            const auto path = Local("recordings", recording.m_relative);
                            if (fs::file_size(path) != recording.m_size || fs::last_write_time(path) != recording.m_modified || Hash(path) != recording.m_digest)
                            {
                                throw std::runtime_error("Recording changed; retained on iPad: " + recording.m_relative);
                            }

                            Check();
                            fs::remove(path);
                            break;
                        }

                        if (state != "extracting")
                        {
                            throw std::runtime_error("Invalid extraction acknowledgement");
                        }

                        for (int tick = 0; tick < 10; ++tick)
                        {
                            Check();
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        }
                    }
                }

                Message("Sync complete" + (skipped == 0 ? std::string() : "; " + std::to_string(skipped) + " unfinished recordings retained"));
            }
            catch (const std::exception& error)
            {
                std::lock_guard<std::mutex> lock(m_statusMutex);
                m_status.m_message = error.what();
                m_status.m_failed = !m_cancelled.load();
            }

            std::lock_guard<std::mutex> lock(m_statusMutex);
            m_status.m_busy = false;
        }
    }
};

@implementation SGSyncDelegate
- (void)netServiceBrowser:(NSNetServiceBrowser*)browser didFindService:(NSNetService*)service moreComing:(BOOL)moreComing
{
    if (m_owner == nullptr || !m_owner->m_open)
    {
        return;
    }

    [m_owner->m_services addObject:service];
    service.delegate = self;
    [service resolveWithTimeout:5.0];
}

- (void)netServiceBrowser:(NSNetServiceBrowser*)browser didRemoveService:(NSNetService*)service moreComing:(BOOL)moreComing
{
    if (m_owner == nullptr)
    {
        return;
    }

    [service stop];
    service.delegate = nil;
    [m_owner->m_services removeObject:service];
    std::lock_guard<std::mutex> lock(m_owner->m_statusMutex);
    auto& receivers = m_owner->m_status.m_receivers;
    receivers.erase(std::remove_if(receivers.begin(), receivers.end(), [&](const auto& receiver)
    {
        return receiver.m_name == Text(service.name);
    }), receivers.end());
}

- (void)netServiceBrowser:(NSNetServiceBrowser*)browser didNotSearch:(NSDictionary*)error
{
    if (m_owner != nullptr)
    {
        m_owner->Message("Discovery unavailable. Allow Local Network access in Settings.");
    }
}

- (void)netServiceDidResolveAddress:(NSNetService*)service
{
    if (m_owner == nullptr || !m_owner->m_open)
    {
        return;
    }

    NSDictionary* txt = [NSNetService dictionaryFromTXTRecordData:service.TXTRecordData];
    const auto value = [&](NSString* key)
    {
        return Text([[NSString alloc] initWithData:txt[key] encoding:NSUTF8StringEncoding]);
    };
    const auto fingerprint = value(@"fingerprint");
    const auto id = value(@"id");
    if (value(@"version") != "1" || id.empty() || fingerprint.size() != 64 || service.port <= 0)
    {
        return;
    }

    SyncClient::Receiver receiver{id, Text(service.name), Text(service.hostName), fingerprint, static_cast<int>(service.port)};
    std::lock_guard<std::mutex> lock(m_owner->m_statusMutex);
    auto& receivers = m_owner->m_status.m_receivers;
    auto found = std::find_if(receivers.begin(), receivers.end(), [&](const auto& item)
    {
        return item.m_id == id;
    });
    if (found == receivers.end())
    {
        receivers.push_back(receiver);
    }
    else
    {
        *found = receiver;
    }
}

- (void)URLSession:(NSURLSession*)session didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential*))completion
{
    if (m_owner == nullptr || m_owner->m_cancelled.load())
    {
        completion(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);
        return;
    }

    if (![challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust])
    {
        completion(NSURLSessionAuthChallengePerformDefaultHandling, nil);
        return;
    }

    SecTrustRef trust = challenge.protectionSpace.serverTrust;
    SecCertificateRef certificate = trust == nullptr || SecTrustGetCertificateCount(trust) == 0
        ? nullptr : SecTrustGetCertificateAtIndex(trust, 0);
    CFDataRef der = certificate == nullptr ? nullptr : SecCertificateCopyData(certificate);
    bool matched = false;
    if (der != nullptr)
    {
        unsigned char digest[CC_SHA256_DIGEST_LENGTH];
        CC_SHA256(CFDataGetBytePtr(der), static_cast<CC_LONG>(CFDataGetLength(der)), digest);
        matched = Hex(digest, sizeof(digest)) == m_owner->m_receiver.m_fingerprint;
        CFRelease(der);
    }

    completion(matched ? NSURLSessionAuthChallengeUseCredential : NSURLSessionAuthChallengeCancelAuthenticationChallenge,
        matched ? [NSURLCredential credentialForTrust:trust] : nil);
}

- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task willPerformHTTPRedirection:(NSHTTPURLResponse*)response newRequest:(NSURLRequest*)request completionHandler:(void (^)(NSURLRequest*))completion
{
    completion(nil);
}

- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task didSendBodyData:(int64_t)bytesSent totalBytesSent:(int64_t)sent totalBytesExpectedToSend:(int64_t)total
{
    if (m_owner != nullptr)
    {
        m_owner->Progress(sent, total);
    }
}

- (void)URLSession:(NSURLSession*)session didBecomeInvalidWithError:(NSError*)error
{
    dispatch_semaphore_signal(m_invalidated);
}
@end

SyncClient::SyncClient(std::string root) : m_impl(std::make_unique<Impl>(std::move(root)))
{
}

SyncClient::~SyncClient()
{
    Close();
}

void SyncClient::Open()
{
    assert([NSThread isMainThread]);
    auto& state = *m_impl;
    state.m_requestedOpen = true;
#if TARGET_OS_IOS
    if (state.m_inactiveObserver == nil)
    {
        auto* notifications = [NSNotificationCenter defaultCenter];
        state.m_inactiveObserver = [notifications addObserverForName:UIApplicationWillResignActiveNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification*)
        {
            SetForeground(false);
        }];
        state.m_activeObserver = [notifications addObserverForName:UIApplicationDidBecomeActiveNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification*)
        {
            SetForeground(true);
        }];
    }

    SetForeground([UIApplication sharedApplication].applicationState == UIApplicationStateActive);
#else
    SetForeground(true);
#endif
}

void SyncClient::SetForeground(bool active)
{
    assert([NSThread isMainThread]);
    auto& state = *m_impl;
    if (!active)
    {
        state.m_open = false;
        state.m_browser.delegate = nil;
        [state.m_browser stop];
        for (NSNetService* service in state.m_services)
        {
            service.delegate = nil;
            [service stop];
        }

        state.m_browser = nil;
        state.m_services = nil;
        Cancel();
        if (state.m_delegate != nil)
        {
            state.m_delegate->m_owner = nullptr;
        }

        state.m_delegate = nil;
        std::lock_guard<std::mutex> lock(state.m_statusMutex);
        state.m_status.m_receivers.clear();
        state.m_status.m_busy = false;
        return;
    }

    if (!state.m_requestedOpen || state.m_open)
    {
        return;
    }

    state.m_open = true;
    state.m_services = [NSMutableArray array];
    state.m_delegate = [[SGSyncDelegate alloc] init];
    state.m_delegate->m_owner = &state;
    state.m_browser = [[NSNetServiceBrowser alloc] init];
    state.m_browser.delegate = state.m_delegate;
    state.Message("Choose a Mac receiver on this network");
    [state.m_browser searchForServicesOfType:@"_sgsync._tcp." inDomain:@"local."];
}

void SyncClient::Cancel()
{
    assert([NSThread isMainThread]);
    auto& state = *m_impl;
    {
        std::lock_guard<std::mutex> lock(state.m_networkMutex);
        state.m_cancelled.store(true);
        [state.m_task cancel];
        [state.m_session invalidateAndCancel];
    }

    if (state.m_worker.joinable())
    {
        state.m_worker.join();
    }

    if (state.m_session != nil)
    {
        dispatch_semaphore_wait(state.m_delegate->m_invalidated, DISPATCH_TIME_FOREVER);
        [state.m_delegateQueue waitUntilAllOperationsAreFinished];
    }

    state.m_task = nil;
    state.m_session = nil;
    state.m_delegateQueue = nil;
}

void SyncClient::Close()
{
    assert([NSThread isMainThread]);
    auto& state = *m_impl;
    state.m_requestedOpen = false;
    if (state.m_inactiveObserver != nil)
    {
        [[NSNotificationCenter defaultCenter] removeObserver:state.m_inactiveObserver];
        [[NSNotificationCenter defaultCenter] removeObserver:state.m_activeObserver];
    }

    state.m_inactiveObserver = nil;
    state.m_activeObserver = nil;
    SetForeground(false);
}

void SyncClient::Start(const Receiver& receiver, const std::string& token)
{
    assert([NSThread isMainThread]);
    auto& state = *m_impl;
    if (!state.m_open)
    {
        return;
    }

    Cancel();
    state.m_receiver = receiver;
    state.m_token = token;
    state.m_cancelled.store(false);
    state.m_delegate->m_invalidated = dispatch_semaphore_create(0);
    state.m_delegateQueue = [[NSOperationQueue alloc] init];
    state.m_delegateQueue.maxConcurrentOperationCount = 1;
    NSURLSessionConfiguration* configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
    configuration.URLCache = nil;
    configuration.HTTPCookieStorage = nil;
    configuration.URLCredentialStorage = nil;
    configuration.timeoutIntervalForRequest = 30;
    configuration.timeoutIntervalForResource = 24 * 60 * 60;
    configuration.waitsForConnectivity = NO;
    state.m_session = [NSURLSession sessionWithConfiguration:configuration delegate:state.m_delegate delegateQueue:state.m_delegateQueue];
    {
        std::lock_guard<std::mutex> lock(state.m_statusMutex);
        state.m_status.m_busy = true;
        state.m_status.m_failed = false;
        state.m_status.m_sent = 0;
        state.m_status.m_total = 0;
    }

    state.m_worker = std::thread([&state]()
    {
        state.Run();
    });
}

SyncClient::Status SyncClient::Snapshot() const
{
    std::lock_guard<std::mutex> lock(m_impl->m_statusMutex);
    return m_impl->m_status;
}

static NSMutableDictionary* CredentialQuery(const SyncClient::Receiver& receiver)
{
    return [@{(__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: @"com.theallelectricsmartgrid.sync",
        (__bridge id)kSecAttrAccount: Native(receiver.m_id + ":" + receiver.m_fingerprint)} mutableCopy];
}

std::string SyncClient::LoadToken(const Receiver& receiver)
{
    NSMutableDictionary* query = CredentialQuery(receiver);
    query[(__bridge id)kSecReturnData] = @YES;
    CFTypeRef value = nullptr;
    if (SecItemCopyMatching((__bridge CFDictionaryRef)query, &value) != errSecSuccess)
    {
        return {};
    }

    NSData* data = CFBridgingRelease(value);
    return Text([[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]);
}

bool SyncClient::SaveToken(const Receiver& receiver, const std::string& token)
{
    NSMutableDictionary* query = CredentialQuery(receiver);
    NSData* data = [Native(token) dataUsingEncoding:NSUTF8StringEncoding];
    OSStatus status = SecItemUpdate((__bridge CFDictionaryRef)query,
        (__bridge CFDictionaryRef)@{(__bridge id)kSecValueData: data});
    if (status == errSecItemNotFound)
    {
        query[(__bridge id)kSecValueData] = data;
        query[(__bridge id)kSecAttrAccessible] = (__bridge id)kSecAttrAccessibleWhenUnlockedThisDeviceOnly;
        status = SecItemAdd((__bridge CFDictionaryRef)query, nullptr);
    }

    return status == errSecSuccess;
}

#pragma once

#include "RecordingFormat.hpp"
#include "CircularQueue.hpp"
#include "AsyncLogger.hpp"
#include "ThreadId.hpp"
#include <cerrno>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <memory>
#include <fcntl.h>
#include <unistd.h>

struct StreamingRecorder
{
    static constexpr size_t x_pageFrames = 1024;
    static constexpr size_t x_queuePages = 32;

    enum class State
    {
        Idle, Starting, Recording, Stopping, Error
    };

    enum class Error
    {
        None, InvalidConfiguration, Open, Write, Close, Overrun, InvalidSample
    };

    // The worker owns the sink. Close appends completion only on a successful finish.
    //
    struct Sink
    {
        virtual ~Sink() = default;
        virtual bool Open(const std::string& path) = 0;
        virtual bool Write(const uint8_t* bytes, size_t size) = 0;
        virtual bool Close(const std::vector<uint8_t>& completion) = 0;
    };

    struct FileSink : Sink
    {
        int m_fd = -1;
        off_t m_bytes = 0;
        std::string m_path;

        ~FileSink() override
        {
            if (m_fd >= 0)
            {
                ::close(m_fd);
            }
        }

        bool Open(const std::string& path) override
        {
            m_path = path;
            m_bytes = 0;
            m_fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
            return m_fd >= 0;
        }

        bool Write(const uint8_t* bytes, size_t size) override
        {
            while (size != 0)
            {
                const ssize_t written = ::write(m_fd, bytes, size);
                if (written < 0 && errno == EINTR)
                {
                    continue;
                }

                if (written <= 0)
                {
                    return false;
                }

                bytes += written;
                size -= static_cast<size_t>(written);
                m_bytes += written;
            }

            return true;
        }

        bool Close(const std::vector<uint8_t>& completion) override
        {
            const off_t prefixBytes = m_bytes;
            bool success = Write(completion.data(), completion.size());
            if (::close(m_fd) != 0)
            {
                success = false;
            }

            m_fd = -1;
            if (!success && !completion.empty())
            {
                // Best effort: a reported close failure must not leave a clean marker.
                //
                ::truncate(m_path.c_str(), prefixBytes);
            }

            return success;
        }
    };

    struct Page
    {
        std::vector<float> m_samples;
        uint32_t m_frames = 0;
    };

    using Ring = CircularQueue<Page, x_queuePages>;
    static_assert(std::atomic<State>::is_always_lock_free);
    static_assert(std::atomic<Error>::is_always_lock_free);
    static_assert(std::atomic<uint64_t>::is_always_lock_free);

    RecordingFormat::Session m_session;
    std::string m_directory;
    std::unique_ptr<Sink> m_sink;
    std::unique_ptr<Ring> m_ring;
    std::vector<size_t> m_trackOffsets;
    std::vector<uint8_t> m_positions;
    std::vector<float> m_staging;
    std::vector<int32_t> m_block;
    std::vector<uint8_t> m_encoded;
    std::thread m_worker;
    std::atomic<State> m_state{State::Idle};
    std::atomic<Error> m_error{Error::None};
    std::atomic<bool> m_ready{false};
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_shutdown{false};
    std::atomic<uint64_t> m_writtenFrames{0};
    std::atomic<uint64_t> m_writtenBytes{0};
    std::atomic<uint64_t> m_queueHighWater{0};
    std::atomic<uint64_t> m_maxBlockMicros{0};
    bool m_prepared = false;

    // Producer-only state, also accessed after the audio callback has quiesced.
    //
    Page* m_page = nullptr;
    bool m_frameOpen = false;
    uint64_t m_acceptedFrames = 0;
    bool m_errorLogged = false;

    ~StreamingRecorder()
    {
        Shutdown();
    }

    // Preparation and shutdown belong to the owner outside the audio callback.
    //
    bool Prepare(RecordingFormat::Session session, std::string directory,
        std::unique_ptr<Sink> sink = {})
    {
        Shutdown();
        m_error = Error::None;
        m_state = State::Idle;
        m_errorLogged = false;
        if (!RecordingFormat::Validate(session) || directory.empty())
        {
            m_error = Error::InvalidConfiguration;
            m_state = State::Error;
            ReportError();
            return false;
        }

        m_session = std::move(session);
        m_directory = std::move(directory);
        m_sink = sink ? std::move(sink) : std::make_unique<FileSink>();
        m_ring = std::make_unique<Ring>();
        const size_t streams = m_session.StreamCount();
        for (auto& page : m_ring->m_data)
        {
            page.m_samples.resize(streams * x_pageFrames);
        }

        m_staging.resize(streams);
        m_block.resize(streams * m_session.m_blockFrames);
        m_encoded.reserve(streams * m_session.m_blockFrames * 3 + 4096);
        m_trackOffsets.clear();
        m_positions.clear();
        for (const auto& track : m_session.m_tracks)
        {
            m_trackOffsets.push_back(m_positions.size());
            for (size_t stream = 0; stream < RecordingFormat::StreamCount(track.m_type); ++stream)
            {
                m_positions.push_back(track.m_type == RecordingFormat::TrackType::PannedMono && stream != 0);
            }
        }

        m_shutdown = false;
        m_prepared = true;
        m_worker = std::thread([this]() { Worker(); });
        return true;
    }

    bool Start()
    {
        const State state = m_state.load();
        if (!m_prepared || (state != State::Idle && state != State::Error))
        {
            return false;
        }

        ReportError();
        m_page = nullptr;
        m_frameOpen = false;
        m_acceptedFrames = 0;
        m_writtenFrames = 0;
        m_writtenBytes = 0;
        m_queueHighWater = 0;
        m_maxBlockMicros = 0;
        m_ready = false;
        m_stopRequested = false;
        m_error = Error::None;
        m_errorLogged = false;
        m_state = State::Starting;
        return true;
    }

    void PublishPage()
    {
        if (m_page != nullptr && m_page->m_frames != 0)
        {
            m_ring->CompletePush();
            m_queueHighWater = std::max(m_queueHighWater.load(), static_cast<uint64_t>(m_ring->Size()));
        }

        m_page = nullptr;
    }

    void Stop()
    {
        const State state = m_state.load();
        if (state != State::Starting && state != State::Recording)
        {
            return;
        }

        m_frameOpen = false;
        PublishPage();
        m_state = State::Stopping;
        m_stopRequested = true;
    }

    void SetError(Error error)
    {
        Error expected = Error::None;
        m_error.compare_exchange_strong(expected, error);
    }

    static const char* ErrorName(Error error)
    {
        switch (error)
        {
            case Error::None: return "None";
            case Error::InvalidConfiguration: return "InvalidConfiguration";
            case Error::Open: return "Open";
            case Error::Write: return "Write";
            case Error::Close: return "Close";
            case Error::Overrun: return "Overrun";
            case Error::InvalidSample: return "InvalidSample";
        }

        return "Unknown";
    }

    // Report from the capture owner, or after shutdown has quiesced audio.
    // The worker must not read the sample clock or share the sampler writer's log queue.
    //
    void ReportError()
    {
        const Error error = m_error.load();
        if (error == Error::None || m_errorLogged)
        {
            return;
        }

        m_errorLogged = true;
        INFO("Recording error=%s state=%d accepted_frames=%llu written_frames=%llu written_bytes=%llu queue_high_water=%llu",
            ErrorName(error), static_cast<int>(m_state.load()),
            static_cast<unsigned long long>(m_acceptedFrames),
            static_cast<unsigned long long>(m_writtenFrames.load()),
            static_cast<unsigned long long>(m_writtenBytes.load()),
            static_cast<unsigned long long>(m_queueHighWater.load()));
    }

    void Fail(Error error)
    {
        SetError(error);
        ReportError();
        Stop();
    }

    bool BeginFrame()
    {
        if (m_error != Error::None)
        {
            ReportError();
            Stop();
            return false;
        }

        if (m_state == State::Starting && m_ready)
        {
            m_state = State::Recording;
        }

        m_frameOpen = m_state == State::Recording;
        if (m_frameOpen)
        {
            std::fill(m_staging.begin(), m_staging.end(), 0.0f);
        }

        return m_frameOpen;
    }

    void Submit(size_t trackIndex, const float* samples, size_t count)
    {
        if (!m_frameOpen)
        {
            return;
        }

        if (trackIndex >= m_session.m_tracks.size()
            || count != RecordingFormat::StreamCount(m_session.m_tracks[trackIndex].m_type))
        {
            Fail(Error::InvalidConfiguration);
            return;
        }

        for (size_t i = 0; i < count; ++i)
        {
            if (!std::isfinite(samples[i]))
            {
                Fail(Error::InvalidSample);
                return;
            }

            m_staging[m_trackOffsets[trackIndex] + i] = samples[i];
        }
    }

    void CommitFrame()
    {
        if (!m_frameOpen)
        {
            return;
        }

        m_frameOpen = false;
        if (m_error != Error::None)
        {
            Stop();
            return;
        }

        if (m_page == nullptr)
        {
            m_page = m_ring->NextToPush();
            if (m_page == nullptr)
            {
                Fail(Error::Overrun);
                return;
            }

            m_page->m_frames = 0;
        }

        std::copy(m_staging.begin(), m_staging.end(),
            m_page->m_samples.begin() + m_page->m_frames * m_staging.size());
        ++m_page->m_frames;
        ++m_acceptedFrames;
        if (m_page->m_frames == x_pageFrames)
        {
            PublishPage();
        }
    }

    State GetState() const
    {
        return m_error == Error::None ? m_state.load() : State::Error;
    }

    Error GetError() const
    {
        return m_error.load();
    }

    bool IsRecording() const
    {
        const State state = GetState();
        return state == State::Starting || state == State::Recording;
    }

    void Shutdown()
    {
        if (m_worker.joinable())
        {
            Stop();
            m_shutdown = true;
            m_worker.join();
        }

        m_prepared = false;
        ReportError();
    }

    bool WriteBlock(uint32_t frames)
    {
        const auto start = std::chrono::steady_clock::now();
        if (!RecordingFormat::EncodeBlock(m_session, m_block.data(), frames,
                m_writtenFrames.load(), m_encoded)
            || !m_sink->Write(m_encoded.data(), m_encoded.size()))
        {
            SetError(Error::Write);
            return false;
        }

        m_writtenFrames += frames;
        m_writtenBytes += m_encoded.size();
        const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count();
        m_maxBlockMicros = std::max(m_maxBlockMicros.load(), static_cast<uint64_t>(micros));
        return true;
    }

    std::string SessionPath()
    {
        const auto now = std::chrono::system_clock::now();
        const auto seconds = std::chrono::system_clock::to_time_t(now);
        std::tm utc{};
        gmtime_r(&seconds, &utc);
        char date[32];
        std::strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%SZ", &utc);
        m_session.m_recordedAtUtc = date;
        std::tm local{};
        localtime_r(&seconds, &local);
        std::strftime(date, sizeof(date), "%Y-%m-%dT%H%M%S", &local);
        static std::atomic<uint64_t> sequence{0};
        const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        return (std::filesystem::path(m_directory) / (std::string("recording-") + date + "-"
            + std::to_string(micros % 1000000) + "-" + std::to_string(sequence++) + ".sgrec")).string();
    }

    void Worker()
    {
        SetCurrentThreadId(ThreadId::FileWriter);
        bool active = false;
        bool opened = false;
        bool writable = false;
        uint32_t blockFrames = 0;
        for (;;)
        {
            try
            {
                if (!active && m_state == State::Starting)
                {
                    active = true;
                    blockFrames = 0;
                    const std::string path = SessionPath();
                    opened = m_sink->Open(path);
                    writable = opened;
                    if (!opened)
                    {
                        SetError(Error::Open);
                    }
                    else if (!RecordingFormat::EncodeHeader(m_session, m_encoded)
                        || !m_sink->Write(m_encoded.data(), m_encoded.size()))
                    {
                        writable = false;
                        SetError(Error::Write);
                    }
                    else
                    {
                        m_writtenBytes += m_encoded.size();
                        std::fprintf(stderr, "Recording: %s\n", path.c_str());
                        m_ready = true;
                    }
                }

                Page* page = m_ring->PeekPtr();
                if (page != nullptr)
                {
                    if (writable)
                    {
                        const size_t streams = m_staging.size();
                        for (size_t frame = 0; frame < page->m_frames; ++frame)
                        {
                            for (size_t stream = 0; stream < streams; ++stream)
                            {
                                m_block[stream * m_session.m_blockFrames + blockFrames] =
                                    RecordingFormat::Quantize(page->m_samples[frame * streams + stream], m_positions[stream]);
                            }

                            if (++blockFrames == m_session.m_blockFrames)
                            {
                                writable = WriteBlock(blockFrames);
                                blockFrames = 0;
                                if (!writable)
                                {
                                    break;
                                }
                            }
                        }
                    }

                    // Keep this slot owned until every sample has been consumed.
                    //
                    m_ring->Pop();
                    continue;
                }

                if (m_stopRequested)
                {
                    // Stop publishes its final page before the flag. Recheck after
                    // observing that flag so a page arriving after PeekPtr is drained.
                    //
                    if (!m_ring->IsEmpty())
                    {
                        continue;
                    }

                    if (writable && blockFrames != 0)
                    {
                        writable = WriteBlock(blockFrames);
                    }

                    m_encoded.clear();
                    if (writable && m_error == Error::None)
                    {
                        assert(m_writtenFrames == m_acceptedFrames);
                        RecordingFormat::EncodeEnd(m_writtenFrames.load(), m_encoded);
                    }

                    bool closed = true;
                    try
                    {
                        closed = !opened || m_sink->Close(m_encoded);
                    }
                    catch (const std::exception&)
                    {
                        closed = false;
                    }

                    if (!closed)
                    {
                        SetError(Error::Close);
                    }

                    m_writtenBytes += m_encoded.size();
                    active = false;
                    opened = false;
                    writable = false;
                    blockFrames = 0;
                    m_stopRequested = false;
                    m_ready = false;
                    m_state = m_error == Error::None ? State::Idle : State::Error;
                }

                if (m_shutdown && !active)
                {
                    return;
                }
            }
            catch (const std::exception&)
            {
                SetError(Error::Write);
                writable = false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

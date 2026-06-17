#pragma once

#include "AudioBuffer.hpp"
#include "CircularQueue.hpp"
#include "DirectoryExplorer.hpp"
#include "RecordingBuffer.hpp"
#include "ThreadId.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <limits>
#include <string>
#include <thread>

struct IoTaskElement
{
    static constexpr size_t x_pathBufferSize = 4096;

    enum class TaskType
    {
        None,
        DirectoryExplorerCommand,
        InitializeDirectoryExplorer,
        LoadAudioBufferBankFromDirectory,
        ReloadDirectory,
        DeleteAudioBuffer,
        PersistRecording,
    };

    TaskType m_taskType;
    DirectoryExplorer::MessageType m_messageType;
    DirectoryExplorer* m_directoryExplorer;
    void* m_result;
    void* m_sink;
    AudioBufferBank* m_audioBufferBank;
    AudioBufferBank* m_pendingDeleteAudioBufferBank;
    RecordingBuffer* m_recordingBuffer;
    bool m_createDirectory;
    int m_voiceID;
    char m_pathRelative[x_pathBufferSize];

    IoTaskElement()
    {
        Reset();
    }

    void Reset()
    {
        m_taskType = TaskType::None;
        m_messageType = DirectoryExplorer::MessageType::Up;
        m_directoryExplorer = nullptr;
        m_result = nullptr;
        m_sink = nullptr;
        m_audioBufferBank = nullptr;
        m_pendingDeleteAudioBufferBank = nullptr;
        m_recordingBuffer = nullptr;
        m_createDirectory = false;
        m_voiceID = -1;
        std::memset(m_pathRelative, 0, x_pathBufferSize);
    }

    static void CopyPathIntoBuffer(char* dest, size_t capacity, const char* src)
    {
        if (!dest || capacity == 0)
        {
            return;
        }

        if (!src)
        {
            dest[0] = '\0';
            return;
        }

        std::strncpy(dest, src, capacity - 1);
        dest[capacity - 1] = '\0';
    }

    void SetDirectoryExplorerCommand(DirectoryExplorer* directoryExplorer, DirectoryExplorer::MessageType messageType)
    {
        Reset();
        m_taskType = TaskType::DirectoryExplorerCommand;
        m_directoryExplorer = directoryExplorer;
        m_messageType = messageType;
    }

    void SetDirectoryExplorerCommand(DirectoryExplorer* directoryExplorer, DirectoryExplorer::MessageType messageType, AudioBufferBank** audioBufferBankSink)
    {
        Reset();
        m_taskType = TaskType::DirectoryExplorerCommand;
        m_directoryExplorer = directoryExplorer;
        m_messageType = messageType;
        m_sink = audioBufferBankSink;
    }

    void SetDirectoryExplorerInit(const char* relativePath, DirectoryExplorer* directoryExplorer)
    {
        Reset();
        m_taskType = TaskType::InitializeDirectoryExplorer;
        m_directoryExplorer = directoryExplorer;
        CopyPathIntoBuffer(m_pathRelative, x_pathBufferSize, relativePath);
    }

    void SetLoadAudioBufferBankFromDirectory(const char* relativePath, AudioBufferBank** sink)
    {
        Reset();
        m_taskType = TaskType::LoadAudioBufferBankFromDirectory;
        m_sink = sink;
        CopyPathIntoBuffer(m_pathRelative, x_pathBufferSize, relativePath);
    }

    void SetReloadDirectory(const char* relativePath, AudioBufferBank** sink)
    {
        Reset();
        m_taskType = TaskType::ReloadDirectory;
        m_sink = sink;
        m_audioBufferBank = *sink;
        CopyPathIntoBuffer(m_pathRelative, x_pathBufferSize, relativePath);
    }

    void SetDeleteAudioBuffer(AudioBufferBank* audioBufferBank)
    {
        Reset();
        m_taskType = TaskType::DeleteAudioBuffer;
        m_audioBufferBank = audioBufferBank;
    }

    void SetPersistRecording(const char* relativePath, RecordingBuffer* recordingBuffer, bool createDirectory, void* sink, int voiceID)
    {
        Reset();
        m_taskType = TaskType::PersistRecording;
        m_recordingBuffer = recordingBuffer;
        m_createDirectory = createDirectory;
        m_voiceID = voiceID;
        m_sink = sink;
        CopyPathIntoBuffer(m_pathRelative, x_pathBufferSize, relativePath);
    }

    bool IsSafeRelativePath(const std::filesystem::path& relativePath) const
    {
        if (relativePath.is_absolute())
        {
            return false;
        }

        for (const std::filesystem::path& part : relativePath)
        {
            if (part == std::filesystem::path(".."))
            {
                return false;
            }
        }

        return true;
    }

    bool ResolveAudioBufferBankDirectoryPath(const char* relativePath, const std::filesystem::path& rootPath, std::string& absolutePathOut, std::string& relativePathOut)
    {
        if (!relativePath || relativePath[0] == '\0' || rootPath.empty())
        {
            return false;
        }

        std::filesystem::path relativePathValue(relativePath);

        if (!IsSafeRelativePath(relativePathValue))
        {
            return false;
        }

        relativePathValue = relativePathValue.lexically_normal();

        if (relativePathValue.empty() || relativePathValue == std::filesystem::path("."))
        {
            return false;
        }

        relativePathOut = relativePathValue.generic_string();
        absolutePathOut = (rootPath / relativePathValue).string();
        return true;
    }

    bool IsRecordingFileName(const std::string& fileName, size_t& indexOut) const
    {
        static constexpr const char* x_prefix = "_recording_";
        static constexpr size_t x_prefixSize = 11;

        if (fileName.size() <= x_prefixSize + 4)
        {
            return false;
        }

        if (fileName.compare(0, x_prefixSize, x_prefix) != 0)
        {
            return false;
        }

        size_t extensionOffset = fileName.size() - 4;
        bool wavExtension =
            fileName[extensionOffset] == '.'
            && (fileName[extensionOffset + 1] == 'w' || fileName[extensionOffset + 1] == 'W')
            && (fileName[extensionOffset + 2] == 'a' || fileName[extensionOffset + 2] == 'A')
            && (fileName[extensionOffset + 3] == 'v' || fileName[extensionOffset + 3] == 'V');

        if (!wavExtension)
        {
            return false;
        }

        size_t index = 0;
        for (size_t i = x_prefixSize; i < extensionOffset; ++i)
        {
            char c = fileName[i];
            if (c < '0' || c > '9')
            {
                return false;
            }

            size_t digit = static_cast<size_t>(c - '0');
            if (index > (std::numeric_limits<size_t>::max() - digit) / 10)
            {
                return false;
            }

            index = index * 10 + digit;
        }

        indexOut = index;
        return true;
    }

    size_t FindNextRecordingFileIndex(const std::filesystem::path& dirPath, std::error_code& ec) const
    {
        size_t highestIndex = 0;
        std::filesystem::directory_iterator it(dirPath, ec);
        if (ec)
        {
            return 1;
        }

        while (it != std::filesystem::directory_iterator())
        {
            bool isRegularFile = it->is_regular_file(ec);
            if (ec)
            {
                return 1;
            }

            if (isRegularFile)
            {
                size_t index = 0;
                std::string fileName = it->path().filename().string();
                if (IsRecordingFileName(fileName, index) && highestIndex < index)
                {
                    highestIndex = index;
                }
            }

            it.increment(ec);
            if (ec)
            {
                return 1;
            }
        }

        return highestIndex + 1;
    }

    std::filesystem::path MakeNextRecordingFilePath(const std::filesystem::path& dirPath, std::error_code& ec) const
    {
        size_t nextIndex = FindNextRecordingFileIndex(dirPath, ec);
        if (ec)
        {
            return {};
        }

        while (true)
        {
            char suffix[32];
            std::snprintf(suffix, sizeof(suffix), "%05zu", nextIndex);
            std::filesystem::path filePath = dirPath / (std::string("_recording_") + suffix + ".wav");

            bool exists = std::filesystem::exists(filePath, ec);
            if (ec || !exists)
            {
                return ec ? std::filesystem::path() : filePath;
            }

            ++nextIndex;
        }
    }

    void ProcessPersistRecording(const std::filesystem::path& rootPath)
    {
        if (rootPath.empty() || m_recordingBuffer == nullptr)
        {
            return;
        }

        std::filesystem::path relativePath(m_pathRelative);

        if (!IsSafeRelativePath(relativePath))
        {
            return;
        }

        relativePath = relativePath.lexically_normal();

        if (relativePath == std::filesystem::path("."))
        {
            relativePath.clear();
        }

        std::filesystem::path dirPath = rootPath / relativePath;
        std::error_code ec;

        if (m_createDirectory)
        {
            auto now = std::chrono::system_clock::now();
            auto timeT = std::chrono::system_clock::to_time_t(now);
            std::tm timeInfo{};
            localtime_r(&timeT, &timeInfo);

            char dirName[64];
            std::snprintf(
                dirName,
                sizeof(dirName),
                "%04d-%02d-%02dT%02d-%02d-%02d_voice%d",
                timeInfo.tm_year + 1900,
                timeInfo.tm_mon + 1,
                timeInfo.tm_mday,
                timeInfo.tm_hour,
                timeInfo.tm_min,
                timeInfo.tm_sec,
                m_voiceID);

            relativePath /= dirName;
            dirPath = rootPath / relativePath;
            std::filesystem::create_directories(dirPath, ec);
            if (ec)
            {
                return;
            }

            CopyPathIntoBuffer(m_pathRelative, x_pathBufferSize, relativePath.generic_string().c_str());
        }

        if (!std::filesystem::is_directory(dirPath, ec) || ec)
        {
            return;
        }

        std::filesystem::path filePath = MakeNextRecordingFilePath(dirPath, ec);
        if (ec)
        {
            return;
        }

        if (filePath.empty())
        {
            return;
        }

        bool writeSuccess = m_recordingBuffer->WriteToFile(filePath.string().c_str());
        if (!writeSuccess)
        {
            return;
        }

        if (m_sink)
        {
            LoadAudioBufferBankFromRelativePath(m_pathRelative, rootPath);
        }
    }

    void LoadAudioBufferBankFromRelativePath(const char* relativePathRaw, const std::filesystem::path& rootPath)
    {
        std::string absolutePath;
        std::string relativePath;

        if (!ResolveAudioBufferBankDirectoryPath(relativePathRaw, rootPath, absolutePath, relativePath))
        {
            return;
        }

        AudioBufferBank* audioBufferBank = new AudioBufferBank();
        audioBufferBank->LoadFromDirectory(absolutePath.c_str(), relativePath.c_str(), nullptr);
        m_result = audioBufferBank;
    }

    void ReloadAudioBufferBankFromRelativePath(const char* relativePathRaw, const std::filesystem::path& rootPath)
    {
        if (!m_audioBufferBank)
        {
            LoadAudioBufferBankFromRelativePath(relativePathRaw, rootPath);
            return;
        }

        std::string absolutePath;
        std::string relativePath;

        if (!ResolveAudioBufferBankDirectoryPath(relativePathRaw, rootPath, absolutePath, relativePath))
        {
            return;
        }

        m_result = m_audioBufferBank->ReloadFromDirectory(absolutePath.c_str(), relativePath.c_str());
    }

    void InstallAudioBufferBankIntoSink(AudioBufferBank* audioBufferBank)
    {
        AudioBufferBank** sink = static_cast<AudioBufferBank**>(m_sink);

        if (sink)
        {
            if (*sink && *sink != audioBufferBank)
            {
                m_pendingDeleteAudioBufferBank = *sink;
            }

            *sink = audioBufferBank;
        }
        else
        {
            m_pendingDeleteAudioBufferBank = audioBufferBank;
        }
    }

    void AcknowledgeAudioBufferBankIntoSink()
    {
        AudioBufferBank* audioBufferBank = static_cast<AudioBufferBank*>(m_result);

        if (!audioBufferBank)
        {
            return;
        }

        InstallAudioBufferBankIntoSink(audioBufferBank);
        m_result = nullptr;
    }

    void AcknowledgePersistRecording()
    {
        if (!m_recordingBuffer)
        {
            return;
        }

        m_recordingBuffer->Reset();
    }

    void Process(const std::filesystem::path& rootPath)
    {
        switch (m_taskType)
        {
            case TaskType::DirectoryExplorerCommand:
            {
                if (!m_directoryExplorer)
                {
                    break;
                }

                if (m_messageType == DirectoryExplorer::MessageType::Yes)
                {
                    const std::string relativeString = m_directoryExplorer->SelectedDirectoryRelativePathString();
                    LoadAudioBufferBankFromRelativePath(relativeString.c_str(), rootPath);
                    m_directoryExplorer->Reset();
                }
                else
                {
                    m_directoryExplorer->Process(m_messageType);
                    m_result = nullptr;
                }

                break;
            }

            case TaskType::InitializeDirectoryExplorer:
            {
                if (!m_directoryExplorer)
                {
                    break;
                }

                std::filesystem::path relative(m_pathRelative);
                m_directoryExplorer->Init(rootPath, relative);
                break;
            }

            case TaskType::LoadAudioBufferBankFromDirectory:
            {
                LoadAudioBufferBankFromRelativePath(m_pathRelative, rootPath);
                break;
            }

            case TaskType::ReloadDirectory:
            {
                ReloadAudioBufferBankFromRelativePath(m_pathRelative, rootPath);
                break;
            }

            case TaskType::DeleteAudioBuffer:
            {
                if (m_audioBufferBank)
                {
                    delete m_audioBufferBank;
                    m_audioBufferBank = nullptr;
                }

                break;
            }

            case TaskType::PersistRecording:
            {
                ProcessPersistRecording(rootPath);
                break;
            }

            case TaskType::None:
            default:
            {
                break;
            }
        }

        if (m_directoryExplorer &&
            (m_taskType == TaskType::DirectoryExplorerCommand || m_taskType == TaskType::InitializeDirectoryExplorer))
        {
            m_directoryExplorer->PopulateUIStates();
        }
    }

    void Acknowledge()
    {
        const bool acknowledgeBank =
            m_taskType == TaskType::LoadAudioBufferBankFromDirectory
            || m_taskType == TaskType::ReloadDirectory
            || (m_taskType == TaskType::PersistRecording && m_sink)
            || (m_taskType == TaskType::DirectoryExplorerCommand
                && m_messageType == DirectoryExplorer::MessageType::Yes);

        if (acknowledgeBank)
        {
            AcknowledgeAudioBufferBankIntoSink();
        }

        if (m_taskType == TaskType::PersistRecording)
        {
            AcknowledgePersistRecording();
        }
    }
};

struct IoTaskThread
{
    static constexpr size_t x_queueSize = 128;

    CircularQueue<IoTaskElement, x_queueSize> m_taskQueue;
    CircularQueue<IoTaskElement, x_queueSize> m_acknowledgmentQueue;
    std::atomic<bool> m_running;
    std::filesystem::path m_sampleDirectoryRootAbsolute;
    std::thread m_thread;

    IoTaskThread()
        : m_running(true)
        , m_sampleDirectoryRootAbsolute()
        , m_thread(&IoTaskThread::Run, this)
    {
    }

    // Stop and join the worker thread. Idempotent. Owners that hold objects the
    // in-flight tasks reference (recording buffers, sample banks) should call
    // this before tearing those objects down, otherwise a task still executing
    // on the worker thread dereferences freed memory.
    //
    void Shutdown()
    {
        m_running.store(false);
        if (m_thread.joinable())
        {
            m_thread.join();
        }
    }

    ~IoTaskThread()
    {
        Shutdown();
    }

    bool PushDirectoryExplorerCommand(DirectoryExplorer* directoryExplorer, DirectoryExplorer::MessageType messageType)
    {
        IoTaskElement task;
        task.SetDirectoryExplorerCommand(directoryExplorer, messageType);
        return m_taskQueue.Push(task);
    }

    bool PushDirectoryExplorerCommand(DirectoryExplorer* directoryExplorer, DirectoryExplorer::MessageType messageType, AudioBufferBank** audioBufferBankSink)
    {
        IoTaskElement task;
        task.SetDirectoryExplorerCommand(directoryExplorer, messageType, audioBufferBankSink);
        return m_taskQueue.Push(task);
    }

    void SetSampleDirectoryRootAbsolute(const std::filesystem::path& absolutePath)
    {
        m_sampleDirectoryRootAbsolute = absolutePath;
    }

    bool HasSampleDirectoryRootAbsolute() const
    {
        return !m_sampleDirectoryRootAbsolute.empty();
    }

    bool PushInitializeDirectoryExplorer(const char* relativePath, DirectoryExplorer* directoryExplorer)
    {
        IoTaskElement task;
        task.SetDirectoryExplorerInit(relativePath, directoryExplorer);
        return m_taskQueue.Push(task);
    }

    bool PushLoadAudioBufferBankFromDirectory(const char* relativePath, AudioBufferBank** sink)
    {
        IoTaskElement task;
        task.SetLoadAudioBufferBankFromDirectory(relativePath, sink);
        return m_taskQueue.Push(task);
    }

    bool PushReloadDirectory(const char* relativePath, AudioBufferBank** sink)
    {
        IoTaskElement task;
        task.SetReloadDirectory(relativePath, sink);
        return m_taskQueue.Push(task);
    }

    bool PushDeleteAudioBuffer(AudioBufferBank* audioBufferBank)
    {
        IoTaskElement task;
        task.SetDeleteAudioBuffer(audioBufferBank);
        return m_taskQueue.Push(task);
    }

    bool PushPersistRecording(const char* relativePath, RecordingBuffer* recordingBuffer, bool createDirectory, void* sink, int voiceID)
    {
        IoTaskElement task;
        task.SetPersistRecording(relativePath, recordingBuffer, createDirectory, sink, voiceID);
        return m_taskQueue.Push(task);
    }

    void Run()
    {
        SetCurrentThreadId(ThreadId::AsyncIo);

        while (m_running.load())
        {
            IoTaskElement task;
            if (m_taskQueue.Pop(task))
            {
                task.Process(m_sampleDirectoryRootAbsolute);
                PushAcknowledgment(task);
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }

    void PushAcknowledgment(const IoTaskElement& task)
    {
        while (m_running.load() && !m_acknowledgmentQueue.Push(task))
        {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    // Called from the audio thread during ProcessFrame().
    //
    void Acknowledge()
    {
        IoTaskElement task;
        while (m_acknowledgmentQueue.Pop(task))
        {
            task.Acknowledge();

            if (task.m_pendingDeleteAudioBufferBank)
            {
                PushDeleteAudioBuffer(task.m_pendingDeleteAudioBufferBank);
            }
        }
    }
};

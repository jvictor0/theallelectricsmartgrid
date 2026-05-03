#pragma once

#include "AudioBuffer.hpp"
#include "CircularQueue.hpp"
#include "DirectoryExplorer.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
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
    };

    TaskType m_taskType;
    DirectoryExplorer::MessageType m_messageType;
    DirectoryExplorer* m_directoryExplorer;
    void* m_result;
    void* m_sink;
    char m_pathAbsolute[x_pathBufferSize];
    char m_pathRelative[x_pathBufferSize];

    IoTaskElement()
        : m_taskType(TaskType::None)
        , m_messageType(DirectoryExplorer::MessageType::Up)
        , m_directoryExplorer(nullptr)
        , m_result(nullptr)
        , m_sink(nullptr)
        , m_pathAbsolute{}
        , m_pathRelative{}
    {
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
        m_taskType = TaskType::DirectoryExplorerCommand;
        m_directoryExplorer = directoryExplorer;
        m_messageType = messageType;
        m_result = nullptr;
        m_sink = nullptr;
    }

    void SetDirectoryExplorerCommand(DirectoryExplorer* directoryExplorer, DirectoryExplorer::MessageType messageType, AudioBufferBank** audioBufferBankSink)
    {
        m_taskType = TaskType::DirectoryExplorerCommand;
        m_directoryExplorer = directoryExplorer;
        m_messageType = messageType;
        m_result = nullptr;
        m_sink = audioBufferBankSink;
    }

    void SetDirectoryExplorerInit(const char* absolutePath, const char* relativePath, DirectoryExplorer* directoryExplorer)
    {
        m_taskType = TaskType::InitializeDirectoryExplorer;
        m_directoryExplorer = directoryExplorer;
        m_messageType = DirectoryExplorer::MessageType::Up;
        m_result = nullptr;
        m_sink = nullptr;
        CopyPathIntoBuffer(m_pathAbsolute, x_pathBufferSize, absolutePath);
        CopyPathIntoBuffer(m_pathRelative, x_pathBufferSize, relativePath);
    }

    void Process()
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
                    const std::string directoryString = m_directoryExplorer->SelectedDirectoryPath().string();
                    const std::string relativeString = m_directoryExplorer->SelectedDirectoryRelativePathString();
                    AudioBufferBank* audioBufferBank = new AudioBufferBank();
                    audioBufferBank->LoadFromDirectory(directoryString.c_str(), relativeString.c_str());
                    m_result = audioBufferBank;
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

                std::filesystem::path absolute(m_pathAbsolute);
                std::filesystem::path relative(m_pathRelative);
                m_directoryExplorer->Init(absolute, relative);
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
        switch (m_taskType)
        {
            case TaskType::DirectoryExplorerCommand:
            {
                if (m_messageType != DirectoryExplorer::MessageType::Yes)
                {
                    break;
                }

                AudioBufferBank* audioBufferBank = static_cast<AudioBufferBank*>(m_result);

                if (!audioBufferBank)
                {
                    break;
                }

                AudioBufferBank** sink = static_cast<AudioBufferBank**>(m_sink);

                if (sink)
                {
                    if (*sink && *sink != audioBufferBank)
                    {
                        delete *sink;
                    }

                    *sink = audioBufferBank;
                }
                else
                {
                    delete audioBufferBank;
                }

                m_result = nullptr;
                break;
            }

            case TaskType::InitializeDirectoryExplorer:
            {
                break;
            }

            case TaskType::None:
            default:
            {
                break;
            }
        }
    }
};

struct IoTaskThread
{
    static constexpr size_t x_queueSize = 128;

    CircularQueue<IoTaskElement, x_queueSize> m_taskQueue;
    CircularQueue<IoTaskElement, x_queueSize> m_acknowledgmentQueue;
    std::atomic<bool> m_running;
    std::thread m_thread;

    IoTaskThread()
        : m_running(true)
        , m_thread(&IoTaskThread::Run, this)
    {
    }

    ~IoTaskThread()
    {
        m_running.store(false);
        if (m_thread.joinable())
        {
            m_thread.join();
        }
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

    bool PushInitializeDirectoryExplorer(const char* absolutePath, const char* relativePath, DirectoryExplorer* directoryExplorer)
    {
        IoTaskElement task;
        task.SetDirectoryExplorerInit(absolutePath, relativePath, directoryExplorer);
        return m_taskQueue.Push(task);
    }

    void Run()
    {
        while (m_running.load())
        {
            IoTaskElement task;
            if (m_taskQueue.Pop(task))
            {
                task.Process();
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

    void Acknowledge()
    {
        IoTaskElement task;
        while (m_acknowledgmentQueue.Pop(task))
        {
            task.Acknowledge();
        }
    }
};

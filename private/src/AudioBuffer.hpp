#pragma once

#include "WavReader.hpp"

#include <cctype>
#include <cmath>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <vector>

struct AudioBuffer
{
    std::vector<float> m_buffer;

    // Loads a WAV file; stereo (or more) sums channels 0 and 1 into mono floats.
    // Unsupported format leaves the buffer empty.
    //
    void LoadFromFile(const char* fileName)
    {
        m_buffer.clear();

        WavReader wavReader;
        if (!wavReader.LoadFromFile(fileName))
        {
            return;
        }

        wavReader.WriteLeftRightSum(m_buffer);
    }

    // Normalized position in [0, 1] mapped across the buffer length
    //
    double GetRealTime(double t) const
    {
        size_t n = m_buffer.size();
        if (n == 0)
        {
            return 0.0;
        }

        return static_cast<double>(t) * static_cast<double>(n - 1);
    }

    // Fractional sample index in [0, size - 1]; linear interpolation between neighbors
    //
    float ReadRealTime(double realTime) const
    {
        size_t n = m_buffer.size();
        if (n == 0)
        {
            return 0.0f;
        }

        if (n == 1)
        {
            return m_buffer[0];
        }

        double maxIndex = static_cast<double>(n - 1);
        if (realTime <= 0.0)
        {
            return m_buffer[0];
        }

        if (maxIndex <= realTime)
        {
            return m_buffer[n - 1];
        }

        size_t i0 = static_cast<size_t>(std::floor(realTime));
        size_t i1 = i0 + 1;
        double alpha = realTime - std::floor(realTime);
        float y0 = m_buffer[i0];
        float y1 = m_buffer[i1];
        return y0 + static_cast<float>(alpha * static_cast<double>(y1 - y0));
    }

    float Get(float t) const
    {
        return ReadRealTime(GetRealTime(t));
    }
};

struct AudioBufferBank
{
    std::vector<AudioBuffer> m_audioBuffers;
    size_t m_audioBufferIndex{0};
    std::string m_directoryName;

    void LoadFromDirectory(const char* absoluteDirectoryPath, const char* relativeDirectoryPath)
    {
        m_audioBuffers.clear();
        m_audioBufferIndex = 0;
        m_directoryName = relativeDirectoryPath ? relativeDirectoryPath : "";

        DIR* directory = opendir(absoluteDirectoryPath);
        if (!directory)
        {
            return;
        }

        struct dirent* entry = readdir(directory);
        while (entry)
        {
            std::string fileName = GetFileName(absoluteDirectoryPath, entry->d_name);
            if (IsWavFile(fileName.c_str()) && IsRegularFile(fileName.c_str()))
            {
                m_audioBuffers.push_back(AudioBuffer());
                m_audioBuffers.back().LoadFromFile(fileName.c_str());
            }

            entry = readdir(directory);
        }

        closedir(directory);
    }

    std::string GetFileName(const char* directoryName, const char* fileName) const
    {
        std::string result = directoryName;
        if (!result.empty() && result[result.size() - 1] != '/')
        {
            result += "/";
        }

        result += fileName;
        return result;
    }

    bool IsWavFile(const char* fileName) const
    {
        std::string name = fileName;
        if (name.size() < 4)
        {
            return false;
        }

        size_t extensionOffset = name.size() - 4;
        return std::tolower(static_cast<unsigned char>(name[extensionOffset + 0])) == '.'
            && std::tolower(static_cast<unsigned char>(name[extensionOffset + 1])) == 'w'
            && std::tolower(static_cast<unsigned char>(name[extensionOffset + 2])) == 'a'
            && std::tolower(static_cast<unsigned char>(name[extensionOffset + 3])) == 'v';
    }

    bool IsRegularFile(const char* fileName) const
    {
        struct stat fileStatus;
        if (stat(fileName, &fileStatus) != 0)
        {
            return false;
        }

        return S_ISREG(fileStatus.st_mode);
    }

    const AudioBuffer* GetAudioBuffer() const
    {
        if (m_audioBuffers.empty())
        {
            return nullptr;
        }

        return &m_audioBuffers[m_audioBufferIndex % m_audioBuffers.size()];
    }

    double GetRealTime(double t) const
    {
        const AudioBuffer* audioBuffer = GetAudioBuffer();
        if (!audioBuffer)
        {
            return 0.0;
        }

        return audioBuffer->GetRealTime(t);
    }

    float ReadRealTime(double realTime) const
    {
        const AudioBuffer* audioBuffer = GetAudioBuffer();
        if (!audioBuffer)
        {
            return 0.0f;
        }

        return audioBuffer->ReadRealTime(realTime);
    }

    float Get(float t) const
    {
        const AudioBuffer* audioBuffer = GetAudioBuffer();
        if (!audioBuffer)
        {
            return 0.0f;
        }

        return audioBuffer->Get(t);
    }
};

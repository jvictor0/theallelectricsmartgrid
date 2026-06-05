#pragma once

#include "BufferResampler.hpp"
#include "SampleTimer.hpp"
#include "SnapshotUIState.hpp"
#include "WavReader.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <dirent.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <vector>

struct AudioBuffer
{
    static constexpr size_t x_numSections = 1024;

    struct UISnapshot
    {
        std::array<float, x_numSections> m_sectionMaximums{};
        std::array<float, x_numSections> m_sectionMinimums{};
    };

    struct UIState : SnapshotUIState<UISnapshot>
    {
    };

    std::vector<float> m_buffer;
    std::string m_fileName;
    std::array<float, x_numSections> m_sectionMaximums{};
    std::array<float, x_numSections> m_sectionMinimums{};

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

        const double fileRate = static_cast<double>(wavReader.m_sampleRate);
        const double hostRate = static_cast<double>(SampleTimer::x_sampleRate);
        if (0.0 < fileRate)
        {
            const double rel = std::abs(fileRate - hostRate) / hostRate;
            if (BufferResampler::x_rateMatchEpsilon <= rel)
            {
                const size_t inFrames = m_buffer.size();
                const size_t outFrames = BufferResampler::OutputFrameCount(inFrames, fileRate, hostRate);
                if (inFrames == 0 || outFrames == 0)
                {
                    return;
                }

                std::unique_ptr<float[]> inHold(new float[inFrames]);
                std::memcpy(inHold.get(), m_buffer.data(), inFrames * sizeof(float));

                m_buffer.resize(outFrames);

                size_t written = 0;
                if (!BufferResampler::ResampleToRate(
                        inHold.get(),
                        inFrames,
                        fileRate,
                        hostRate,
                        m_buffer.data(),
                        outFrames,
                        &written))
                {
                    m_buffer.clear();
                    return;
                }

                if (written != outFrames)
                {
                    m_buffer.resize(written);
                }
            }
        }

        ComputeSectionExtrema();
    }

    void PopulateUIState(UIState* uiState)
    {
        UISnapshot& snapshot = uiState->BeginSnapshot();

        if (m_buffer.empty())
        {
            snapshot.m_sectionMaximums.fill(0.0f);
            snapshot.m_sectionMinimums.fill(0.0f);
        }
        else
        {
            snapshot.m_sectionMaximums = m_sectionMaximums;
            snapshot.m_sectionMinimums = m_sectionMinimums;
        }

        uiState->CommitSnapshot();
    }

    void ClearSectionExtrema()
    {
        m_sectionMaximums.fill(0.0f);
        m_sectionMinimums.fill(0.0f);
    }

    void ComputeSectionExtrema()
    {
        ClearSectionExtrema();

        size_t n = m_buffer.size();
        if (n == 0)
        {
            return;
        }

        for (size_t section = 0; section < x_numSections; ++section)
        {
            size_t begin = (section * n) / x_numSections;
            size_t end = ((section + 1) * n) / x_numSections;
            if (begin == end)
            {
                continue;
            }

            float minimum = m_buffer[begin];
            float maximum = m_buffer[begin];
            for (size_t i = begin + 1; i < end; ++i)
            {
                float sample = m_buffer[i];
                if (sample < minimum)
                {
                    minimum = sample;
                }

                if (maximum < sample)
                {
                    maximum = sample;
                }
            }

            m_sectionMinimums[section] = minimum;
            m_sectionMaximums[section] = maximum;
        }
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
    using UISnapshot = AudioBuffer::UISnapshot;

    struct UIState : SnapshotUIState<UISnapshot>
    {
    };

    std::vector<std::shared_ptr<AudioBuffer>> m_audioBuffers;
    float m_bankPosition{0.0f};
    std::string m_directoryName;

    struct BankBlend
    {
        size_t m_bankA;
        size_t m_bankB;
        float m_blendB;
        bool m_single;
    };

    BankBlend ComputeBankBlend() const
    {
        BankBlend result{};
        result.m_bankA = 0;
        result.m_bankB = 0;
        result.m_blendB = 0.0f;
        result.m_single = true;

        size_t n = m_audioBuffers.size();
        if (n <= 1)
        {
            return result;
        }

        float u = std::clamp(m_bankPosition, 0.0f, 1.0f);
        size_t numSegments = 2 * n - 1;
        double scaled = static_cast<double>(u) * static_cast<double>(numSegments);
        size_t seg = static_cast<size_t>(scaled);
        if (seg >= numSegments)
        {
            seg = numSegments - 1;
        }

        double frac = scaled - static_cast<double>(seg);

        if (seg % 2 == 0)
        {
            result.m_bankA = seg / 2;
            result.m_single = true;
            return result;
        }

        result.m_bankA = (seg - 1) / 2;
        result.m_bankB = result.m_bankA + 1;
        result.m_blendB = static_cast<float>(frac);
        result.m_single = false;
        return result;
    }

    void LoadFromDirectory(const char* absoluteDirectoryPath, const char* relativeDirectoryPath, std::unordered_map<std::string, std::shared_ptr<AudioBuffer>>* existingByFileName)
    {
        m_audioBuffers.clear();
        m_bankPosition = 0.0f;
        m_directoryName = relativeDirectoryPath ? relativeDirectoryPath : "";

        DIR* directory = opendir(absoluteDirectoryPath);
        if (!directory)
        {
            return;
        }

        struct dirent* entry = readdir(directory);
        while (entry)
        {
            const char* baseName = entry->d_name;
            std::string absoluteFileName = GetFileName(absoluteDirectoryPath, baseName);
            if (IsWavFile(absoluteFileName.c_str()) && IsRegularFile(absoluteFileName.c_str()))
            {
                bool reused = false;

                if (existingByFileName)
                {
                    auto reuseIt = existingByFileName->find(baseName);
                    if (reuseIt != existingByFileName->end())
                    {
                        m_audioBuffers.push_back(reuseIt->second);
                        existingByFileName->erase(reuseIt);
                        reused = true;
                    }
                }

                if (!reused)
                {
                    std::shared_ptr<AudioBuffer> buf = std::make_shared<AudioBuffer>();
                    buf->m_fileName = baseName;
                    buf->LoadFromFile(absoluteFileName.c_str());
                    m_audioBuffers.push_back(buf);
                }
            }

            entry = readdir(directory);
        }

        closedir(directory);
    }

    AudioBufferBank* ReloadFromDirectory(const char* absoluteDirectoryPath, const char* relativeDirectoryPath)
    {
        std::unordered_map<std::string, std::shared_ptr<AudioBuffer>> existingByFileName;
        for (const std::shared_ptr<AudioBuffer>& buf : m_audioBuffers)
        {
            existingByFileName[buf->m_fileName] = buf;
        }

        AudioBufferBank* bank = new AudioBufferBank();
        bank->LoadFromDirectory(absoluteDirectoryPath, relativeDirectoryPath, &existingByFileName);
        bank->m_bankPosition = m_bankPosition;
        return bank;
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

    double GetRealTime(double t) const
    {
        size_t n = m_audioBuffers.size();
        if (n == 0)
        {
            return 0.0;
        }

        BankBlend blend = ComputeBankBlend();
        const AudioBuffer& bufA = *m_audioBuffers[blend.m_bankA];

        return bufA.GetRealTime(t);
    }

    float ReadRealTime(double realTime) const
    {
        size_t n = m_audioBuffers.size();
        if (n == 0)
        {
            return 0.0f;
        }

        BankBlend blend = ComputeBankBlend();
        const AudioBuffer& bufA = *m_audioBuffers[blend.m_bankA];

        if (blend.m_single || n == 1)
        {
            return bufA.ReadRealTime(realTime);
        }

        const AudioBuffer& bufB = *m_audioBuffers[blend.m_bankB];
        size_t nA = bufA.m_buffer.size();
        size_t nB = bufB.m_buffer.size();
        double denomA = static_cast<double>((nA <= 1) ? 1 : (nA - 1));
        double denomB = static_cast<double>((nB <= 1) ? 1 : (nB - 1));
        double p = realTime / denomA;
        double realTimeB = p * denomB;

        float sA = bufA.ReadRealTime(realTime);
        float sB = bufB.ReadRealTime(realTimeB);
        float oneMinus = 1.0f - blend.m_blendB;
        return oneMinus * sA + blend.m_blendB * sB;
    }

    float Get(float t) const
    {
        size_t n = m_audioBuffers.size();
        if (n == 0)
        {
            return 0.0f;
        }

        BankBlend blend = ComputeBankBlend();
        const AudioBuffer& bufA = *m_audioBuffers[blend.m_bankA];

        if (blend.m_single || n == 1)
        {
            return bufA.Get(t);
        }

        const AudioBuffer& bufB = *m_audioBuffers[blend.m_bankB];
        float sA = bufA.Get(t);
        float sB = bufB.Get(t);
        float oneMinus = 1.0f - blend.m_blendB;
        return oneMinus * sA + blend.m_blendB * sB;
    }

    void PopulateUIState(UIState* uiState)
    {
        UISnapshot& snapshot = uiState->BeginSnapshot();

        if (m_audioBuffers.empty())
        {
            snapshot.m_sectionMaximums.fill(0.0f);
            snapshot.m_sectionMinimums.fill(0.0f);
        }
        else
        {
            BankBlend blend = ComputeBankBlend();
            const AudioBuffer& bufA = *m_audioBuffers[blend.m_bankA];
            if (blend.m_single || m_audioBuffers.size() == 1)
            {
                for (size_t i = 0; i < AudioBuffer::x_numSections; ++i)
                {
                    snapshot.m_sectionMaximums[i] = bufA.m_sectionMaximums[i];
                    snapshot.m_sectionMinimums[i] = bufA.m_sectionMinimums[i];
                }
            }
            else
            {
                const AudioBuffer& bufB = *m_audioBuffers[blend.m_bankB];
                float oneMinus = 1.0f - blend.m_blendB;
                for (size_t i = 0; i < AudioBuffer::x_numSections; ++i)
                {
                    float maxA = bufA.m_sectionMaximums[i];
                    float maxB = bufB.m_sectionMaximums[i];
                    float minA = bufA.m_sectionMinimums[i];
                    float minB = bufB.m_sectionMinimums[i];
                    snapshot.m_sectionMaximums[i] = oneMinus * maxA + blend.m_blendB * maxB;
                    snapshot.m_sectionMinimums[i] = oneMinus * minA + blend.m_blendB * minB;
                }
            }
        }

        uiState->CommitSnapshot();
    }
};

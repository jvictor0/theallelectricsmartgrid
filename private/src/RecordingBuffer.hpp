#pragma once

#include "SampleTimer.hpp"
#include "TheoryOfTime.hpp"
#include "WavWriter.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

struct RecordingBuffer
{
    static constexpr size_t x_byteCapacity = 16 * 1024 * 1024;
    static constexpr size_t x_capacity = x_byteCapacity / sizeof(float);

    enum class State
    {
        Idle,
        Recording,
        Done,
        Error
    };

    State m_state;
    std::vector<float> m_buffer;

    // The audio thread owns state and buffer mutation. The I/O thread only
    // reads these atomics after a completed recording has been handed off for
    // persistence.
    //
    std::atomic<TheoryOfTimeBase*> m_theoryOfTime;
    std::atomic<double> m_loopPositionRecordingStart;
    std::atomic<double> m_loopPositionRecordingStop;
    std::atomic<int> m_recordingRepeats;

    RecordingBuffer()
        : m_state(State::Idle)
        , m_theoryOfTime(nullptr)
        , m_loopPositionRecordingStart(0.0)
        , m_loopPositionRecordingStop(0.0)
        , m_recordingRepeats(0)
    {
        m_buffer.reserve(x_capacity);
    }

    double SampleUnwoundMasterIndependent() const
    {
        size_t j = static_cast<size_t>(SampleTimer::GetUBlockIndex());
        return m_theoryOfTime.load()->GetUnwoundMasterIndependent(j);
    }

    int ComputeRecordingRepeats(double spanAsMasterFraction) const
    {
        TheoryOfTimeBase* theoryOfTime = m_theoryOfTime.load();
        if (theoryOfTime == nullptr || spanAsMasterFraction <= 0.0)
        {
            return 0;
        }

        size_t j = static_cast<size_t>(SampleTimer::GetUBlockIndex());
        const TimeLoop* master = theoryOfTime->GetMasterLoop();
        int masterSize = master->m_loopSize[j];
        if (masterSize <= 0)
        {
            return 0;
        }

        double bestRelativeSize = 0.0;
        int repeats = 0;

        for (int loopIndex = 0; loopIndex < TheoryOfTimeBase::x_numLoops; ++loopIndex)
        {
            int externalMult = theoryOfTime->GetLoopExternalMultiplier(j, loopIndex);
            double relativeSize = 1.0 / static_cast<double>(externalMult);
            if (relativeSize <= spanAsMasterFraction && bestRelativeSize < relativeSize)
            {
                bestRelativeSize = relativeSize;
                repeats = externalMult;
            }
        }

        return repeats;
    }

    void Process(float input)
    {
        if (m_state != State::Recording)
        {
            return;
        }

        if (m_buffer.size() >= x_capacity)
        {
            m_loopPositionRecordingStop.store(SampleUnwoundMasterIndependent());
            m_state = State::Error;
            return;
        }

        m_buffer.push_back(input);
    }

    void StartRecording()
    {
        if (m_state != State::Idle)
        {
            return;
        }

        m_buffer.clear();
        m_recordingRepeats.store(0);
        m_loopPositionRecordingStart.store(SampleUnwoundMasterIndependent());
        m_state = State::Recording;
    }

    void StopRecording()
    {
        if (m_state != State::Recording)
        {
            return;
        }

        double stopPosition = SampleUnwoundMasterIndependent();
        m_loopPositionRecordingStop.store(stopPosition);
        double delta = stopPosition - m_loopPositionRecordingStart.load();
        if (delta > 1.0 || delta <= 0.0)
        {
            m_recordingRepeats.store(0);
            m_state = State::Error;
            return;
        }

        int repeats = ComputeRecordingRepeats(delta);
        m_recordingRepeats.store(repeats);
        if (repeats <= 0)
        {
            m_state = State::Error;
            return;
        }

        m_state = State::Done;
    }

    void Reset()
    {
        m_state = State::Idle;
        m_buffer.clear();
    }

    bool WriteToFile(const char* filename)
    {
        double startPosition = m_loopPositionRecordingStart.load();
        double stopPosition = m_loopPositionRecordingStop.load();
        size_t bufferSize = m_buffer.size();
        int numRepeats = m_recordingRepeats.load();
        double spanAsMasterFraction = stopPosition - startPosition;

        if (bufferSize == 0 || spanAsMasterFraction <= 0.0 || numRepeats <= 0)
        {
            return false;
        }

        double sourceSamplesPerMaster = static_cast<double>(bufferSize) / spanAsMasterFraction;
        size_t masterSamples = static_cast<size_t>(std::lround(sourceSamplesPerMaster));
        if (masterSamples == 0)
        {
            return false;
        }

        auto writer = std::make_unique<MultichannelWavWriter>();
        writer->Open(1, std::string(filename), static_cast<uint32_t>(SampleTimer::x_sampleRate));

        double loopFraction = 1.0 / static_cast<double>(numRepeats);
        double outputSamplesPerMaster = static_cast<double>(masterSamples);
        static constexpr double x_positionEpsilon = 1.0e-9;

        for (size_t i = 0; i < masterSamples; ++i)
        {
            double outputFraction = static_cast<double>(i) / outputSamplesPerMaster;
            double loopPhase = std::fmod(outputFraction, loopFraction);
            if (loopPhase < 0.0)
            {
                loopPhase += loopFraction;
            }

            double sourcePosition =
                std::floor((startPosition - loopPhase) / loopFraction) * loopFraction + loopPhase;
            while (sourcePosition < startPosition - x_positionEpsilon)
            {
                sourcePosition += loopFraction;
            }

            if (sourcePosition < startPosition)
            {
                sourcePosition = startPosition;
            }

            double sample = 0.0;
            if (sourcePosition < stopPosition)
            {
                double sourceOffset = sourcePosition - startPosition;
                size_t sourceIndex = static_cast<size_t>(std::floor(sourceOffset * sourceSamplesPerMaster));
                sourceIndex = std::min(sourceIndex, bufferSize - 1);
                sample = static_cast<double>(m_buffer[sourceIndex]);
            }

            writer->WriteSample(0, sample);
        }

        writer->Close();
        return !writer->m_error;
    }
};

template<size_t Size>
struct RecordingBufferWriter
{
    RecordingBuffer* m_buffers[Size];
    size_t m_count;

    RecordingBufferWriter()
        : m_buffers{}
        , m_count(0)
    {
    }

    void Register(RecordingBuffer* buffer)
    {
        m_buffers[m_count] = buffer;
        ++m_count;
    }

    void Deregister(RecordingBuffer* buffer)
    {
        for (size_t i = 0; i < m_count; ++i)
        {
            if (m_buffers[i] == buffer)
            {
                if (i != m_count - 1)
                {
                    m_buffers[i] = m_buffers[m_count - 1];
                }

                --m_count;
                return;
            }
        }
    }

    void Process(float input)
    {
        for (size_t i = 0; i < m_count; ++i)
        {
            m_buffers[i]->Process(input);
        }
    }
};

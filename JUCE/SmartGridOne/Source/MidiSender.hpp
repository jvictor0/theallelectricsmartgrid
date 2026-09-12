#pragma once

#include <JuceHeader.h>
#include "SmartGridInclude.hpp"
#include "MidiHandlers.hpp"
#include "ThreadId.hpp"
#include "MidiSysexQueue.hpp"

struct MidiSender : public juce::Thread
{
    static constexpr size_t x_maxRoutes = 16;
    static constexpr size_t x_maxSysexMessageBytes = 2048;
    static constexpr size_t x_sysexQueueSize = 64;
    CircularQueue<SmartGrid::BasicMidi, 16384> m_queue;
    SmartGrid::MidiSysexQueue<x_maxSysexMessageBytes, x_sysexQueueSize> m_sysexQueue;
    MidiOutputHandler* m_outputHandlers[x_maxRoutes];
    int m_clockRouteId;
    static constexpr double x_latencyMs = 10;
    static constexpr size_t x_basicBatchSize = 256;
    std::atomic<size_t> m_iterations{0};
    std::atomic<size_t> m_basicFull{0};
    std::atomic<size_t> m_basicLate{0};
    std::atomic<size_t> m_basicInvalid{0};

    std::atomic<bool> m_shutdown;
    std::atomic<bool> m_workerRunning{false};
    std::atomic<size_t> m_sysexEnqueued{0};
    std::atomic<size_t> m_sysexSubmitted{0};
    std::atomic<size_t> m_sysexFull{0};
    std::atomic<size_t> m_sysexInvalid{0};

    MidiSender()
        : juce::Thread("MidiSender")
        , m_shutdown(false)
    {
        for (size_t i = 0; i < x_maxRoutes; i++)
        {
            m_outputHandlers[i] = nullptr;
        }

        m_clockRouteId = -1;

        const bool started = startThread();
        INFO("MidiSenderThread started=%d real_time=0", static_cast<int>(started));
    }

    ~MidiSender()
    {
        Shutdown();
    }

    void run() override
    {
        m_workerRunning.store(true, std::memory_order_relaxed);
        SetCurrentThreadId(ThreadId::MidiSender);

        while (!threadShouldExit())
        {
            m_iterations.fetch_add(1, std::memory_order_relaxed);
#if JUCE_IOS
            for (size_t i = 0; i < x_basicBatchSize && !m_shutdown.load(); ++i)
            {
                if (!HandleScheduledMessage())
                {
                    break;
                }
            }

            for (size_t i = 0; i < x_sysexQueueSize && m_sysexQueue.Peek() != nullptr && !m_shutdown.load(); ++i)
            {
                HandleSysex();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(2));
#else
            HandleMessage();
            HandleSysex();
            std::this_thread::sleep_for(std::chrono::microseconds(100));
#endif
        }

        m_workerRunning.store(false, std::memory_order_relaxed);
        
        INFO("MidiSenderThread stopped");
    }

    size_t AllocateRoute(MidiOutputHandler* outputHandler)
    {
        for (size_t i = 0; i < x_maxRoutes; i++)
        {
            if (m_outputHandlers[i] == nullptr)
            {
                m_outputHandlers[i] = outputHandler;
                outputHandler->m_routeId = i;
                return i;
            }
        }

        return x_maxRoutes;
    }

    void SendMessage(SmartGrid::BasicMidi msg, int routeId)
    {
        msg.m_routeId = routeId;
        if (!m_queue.Push(msg))
        {
            m_basicFull.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // The audio producer copies SysEx into a bounded queue; the worker owns CoreMIDI submission.
    //
    bool SendSysex(const uint8_t* data, size_t size, int routeId)
    {
        if (data == nullptr || size == 0 || size > x_maxSysexMessageBytes
            || routeId < 0 || static_cast<size_t>(routeId) >= x_maxRoutes
            || m_outputHandlers[routeId] == nullptr)
        {
            m_sysexInvalid.fetch_add(1, std::memory_order_relaxed);
            jassertfalse;
            return false;
        }

        if (!m_sysexQueue.TryPush(data, size, routeId))
        {
            m_sysexFull.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        m_sysexEnqueued.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    void RefreshConnections()
    {
#if JUCE_IOS
        for (auto* handler : m_outputHandlers)
        {
            if (handler != nullptr)
            {
                handler->RefreshConnection();
            }
        }
#endif
    }

    void LogDiagnostics() const
    {
#if JUCE_IOS
        const double tickUs = 1000000.0 / static_cast<double>(juce::Time::getHighResolutionTicksPerSecond());
        const auto minimum = SmartGrid::MidiOutputSchedule::s_minLeadTicks.load();
        INFO("MIDI native scheduled=%llu immediate=%llu errors=%llu late=%llu disconnected=%llu missing=%llu min_lead_us=%.1f max_lead_us=%.1f",
            static_cast<unsigned long long>(SmartGrid::MidiOutputSchedule::s_scheduled.load()),
            static_cast<unsigned long long>(SmartGrid::MidiOutputSchedule::s_immediate.load()),
            static_cast<unsigned long long>(SmartGrid::MidiOutputSchedule::s_errors.load()),
            static_cast<unsigned long long>(SmartGrid::MidiOutputSchedule::s_late.load()),
            static_cast<unsigned long long>(SmartGrid::MidiOutputSchedule::s_disconnected.load()),
            static_cast<unsigned long long>(SmartGrid::MidiOutputSchedule::s_missingOutput.load()),
            minimum == UINT64_MAX ? -1.0 : minimum * tickUs,
            SmartGrid::MidiOutputSchedule::s_maxLeadTicks.load() * tickUs);
#endif
        INFO("MIDI worker running=%d iterations=%zu basic_queued=%zu basic_full=%zu basic_late=%zu basic_invalid=%zu sysex_queued=%zu sysex_enqueued=%zu sysex_submitted=%zu sysex_full=%zu sysex_invalid=%zu",
            static_cast<int>(m_workerRunning.load(std::memory_order_relaxed)),
            m_iterations.load(), m_queue.Size(), m_basicFull.load(), m_basicLate.load(), m_basicInvalid.load(),
            m_sysexQueue.Size(), m_sysexEnqueued.load(), m_sysexSubmitted.load(),
            m_sysexFull.load(), m_sysexInvalid.load());
    }

    // Terminal shutdown on the owner/control thread after audio producers stop.
    // Join before final LED clearing or route destruction; never force-kill while
    // the worker holds a packet or the output handler lock.
    //
    void Shutdown()
    {
        jassert(juce::Thread::getCurrentThread() != this);
        m_shutdown.store(true);
        signalThreadShouldExit();
        waitForThreadToExit(-1);
    }

    void HandleSysex()
    {
        auto* packet = m_sysexQueue.Peek();
        if (packet == nullptr)
        {
            return;
        }

        if (m_shutdown.load())
        {
            m_sysexQueue.Pop();
            return;
        }

        const int routeId = packet->m_routeId;
        if (routeId < 0 || static_cast<size_t>(routeId) >= x_maxRoutes
            || m_outputHandlers[routeId] == nullptr)
        {
            m_sysexInvalid.fetch_add(1, std::memory_order_relaxed);
            jassertfalse;
            m_sysexQueue.Pop();
            return;
        }

        juce::MidiMessage message(packet->m_data, static_cast<int>(packet->m_size));
        m_outputHandlers[routeId]->SendMessage(message);
        m_sysexSubmitted.fetch_add(1, std::memory_order_relaxed);
        m_sysexQueue.Pop();
    }

    bool HandleScheduledMessage()
    {
        SmartGrid::BasicMidi msg;
        if (!m_queue.Pop(msg))
        {
            return false;
        }

        if (msg.m_routeId < 0 || static_cast<size_t>(msg.m_routeId) >= x_maxRoutes
            || m_outputHandlers[msg.m_routeId] == nullptr)
        {
            m_basicInvalid.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        const auto plan = SmartGrid::MidiOutputSchedule::Plan(msg.m_timestamp,
            juce::Time::getMillisecondCounterHiRes() * 1000.0,
            static_cast<double>(juce::Time::getHighResolutionTicksPerSecond()));
        if (plan.m_drop)
        {
            m_basicLate.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        juce::MidiMessage message(msg.m_msg, msg.Size());
        m_outputHandlers[msg.m_routeId]->SendMessage(message, plan.m_hostTicks);
        return true;
    }

    void HandleMessage()
    {
        SmartGrid::BasicMidi msg;
        
        // Peek at the next message without removing it
        //
        if (!m_queue.Peek(msg))
        {
            return;
        }

        if (m_shutdown.load())
        {
            m_queue.Pop(msg);
            return;
        }
        
        // Get current time in milliseconds using JUCE wall clock
        //
        double currentTimeMs = juce::Time::getMillisecondCounterHiRes();
        
        if (msg.m_timestamp == 0)
        {
            // Send immediately
            //
            m_queue.Pop(msg);
            juce::MidiMessage message(msg.m_msg, msg.Size());
            m_outputHandlers[msg.m_routeId]->SendMessage(message);
        }
        else
        {
            // Convert timestamp from microseconds to milliseconds
            //
            double msgTimestampMs = static_cast<double>(msg.m_timestamp) / 1000.0;
            
            // Wait until it's time to send (with small latency buffer)
            //
            double targetTimeMs = msgTimestampMs + x_latencyMs;
            if (currentTimeMs >= targetTimeMs)
            {
                // Time to send
                //
                m_queue.Pop(msg);
                juce::MidiMessage message(msg.m_msg, msg.Size());
                m_outputHandlers[msg.m_routeId]->SendMessage(message);
            }
        }
    }

    void ProcessMessagesOut(SmartGrid::MessageOutBuffer& buffer, size_t timestamp)
    {
        for (auto itr = buffer.begin(); itr != buffer.end(); ++itr)
        {
            switch (itr->m_mode)
            {
                case SmartGrid::MessageOut::Mode::Clock:
                {
                    SendMessage(SmartGrid::BasicMidi::Clock(timestamp), m_clockRouteId);
                    break;
                }
                case SmartGrid::MessageOut::Mode::Start:
                {
                    SendMessage(SmartGrid::BasicMidi::TransportStart(timestamp), m_clockRouteId);
                    break;
                }
                case SmartGrid::MessageOut::Mode::Stop:
                {
                    SendMessage(SmartGrid::BasicMidi::TransportStop(timestamp), m_clockRouteId);
                    break;
                }
                default:
                {
                    break;
                }
            }
        }

        buffer.Clear();
    }
};

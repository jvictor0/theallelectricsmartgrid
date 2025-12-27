#pragma once

#include <JuceHeader.h>
#include "SmartGridInclude.hpp"
#include "MidiHandlers.hpp"

struct MidiSender : public juce::Thread
{
    static constexpr size_t x_maxRoutes = 16;
    CircularQueue<SmartGrid::BasicMidi, 16384> m_queue;
    MidiOutputHandler* m_outputHandlers[x_maxRoutes];
    int m_clockRouteId;
    static constexpr double x_latencyMs = 10;
    
    MidiSender()
        : juce::Thread("MidiSender")
    {
        for (size_t i = 0; i < x_maxRoutes; i++)
        {
            m_outputHandlers[i] = nullptr;
        }

        m_clockRouteId = -1;
        
        // Configure real-time options for tight MIDI timing
        //
        juce::Thread::RealtimeOptions realTimeOptions;
        realTimeOptions = realTimeOptions.withPriority(10)
                                      .withPeriodMs(0.1)
                                      .withProcessingTimeMs(0.05);
        startRealtimeThread(realTimeOptions);
        
        INFO("MidiSenderThread started with real-time scheduling");
    }

    ~MidiSender()
    {
        stopThread(1000);
    }

    void run() override
    {
        while (!threadShouldExit())
        {
            HandleMessage();
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        
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
        m_queue.Push(msg);
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
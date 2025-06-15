#pragma once

#include <CoreMIDI/CoreMIDI.h>
#include <mach/mach_time.h>
#include <atomic>
#include <thread>

#include "CircularQueue.h"

struct MidiMessage
{
    uint8_t m_data[3];
    uint8_t m_size;
    size_t m_timestamp;

    MidiMessage()
     : m_size(0)
     , m_timestamp(0)
    {
    }
    
    void SetChannel(uint8_t channel)
    {
        m_data[0] = (m_data[0] & 0xF0) | channel;
    }

    void SetStatus(uint8_t status)
    {
        m_data[0] = (m_data[0] & 0x0F) | status;
    }

    void SetNote(uint8_t note, uint8_t velocity)
    {
        SetStatus(0x90);
        m_size = 3;
        m_data[1] = note;
        m_data[2] = velocity;
    }

    void SetCC(uint8_t cc, uint8_t value)
    {
        SetStatus(0xB0);
        m_size = 3;
        m_data[1] = cc;
        m_data[2] = value;
    }

    void SetPitchBend(uint16_t value)
    {
        SetStatus(0xE0);
        m_size = 3;
        m_data[1] = (value >> 7) & 0x7F;
        m_data[2] = value & 0x7F;
    }
};

struct MidiBus
{
    CircularQueue<MidiMessage, 8192> m_buffer;
    uint8_t m_lastNoteSent[16];
    std::thread m_midiSendThread;
    std::atomic<bool> m_running;

    MidiBus()
    {
        for (int i = 0; i < 16; i++)
        {
            m_lastNoteSent[i] = 255;
        }

        InitMIDI();
        m_running = true;
        m_midiSendThread = std::thread(&MidiBus::SendLoop, this);
    }

    ~MidiBus()
    {
        m_running = false;
        m_midiSendThread.join();
    }

    void SendMessage(MidiMessage& message)
    {
        m_buffer.Push(message);
    }

    void SendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel)
    {
        MidiMessage message;
        message.SetNote(note, velocity);
        message.SetChannel(channel);
        SendMessage(message);
        m_lastNoteSent[channel] = note;
    }

    void SendNoteOff(uint8_t channel)
    {
        if (m_lastNoteSent[channel] == 255)
        {
            return;
        }

        MidiMessage message;
        message.SetNote(m_lastNoteSent[channel], 0);
        message.SetChannel(channel);
        m_lastNoteSent[channel] = 255;
        SendMessage(message);
    }

    void SendNoteOnIfOff(uint8_t note, uint8_t velocity, uint8_t channel)
    {
        if (m_lastNoteSent[channel] == 255)
        {
            SendNoteOn(note, velocity, channel);
        }
    }

    void SendCC(uint8_t cc, uint8_t value, uint8_t channel)
    {
        MidiMessage message;
        message.SetCC(cc, value);
        message.SetChannel(channel);
        SendMessage(message);
    }
    
    void SendPitchBend(uint16_t value, uint8_t channel)
    {
        MidiMessage message;
        message.SetPitchBend(value);
        message.SetChannel(channel);
        SendMessage(message);
    }

    bool IsChannelPlaying(uint8_t channel)
    {
        return m_lastNoteSent[channel] != 255;
    }

    MIDIClientRef m_client;
    MIDIPortRef m_outputPort;
    MIDIEndpointRef m_dest;

    void InitMIDI()
    {
        MIDIClientCreate(CFSTR("TheNonagon"), nullptr, nullptr, &m_client);
        MIDIOutputPortCreate(m_client, CFSTR("TheNonagonOutPort"), &m_outputPort);

        ItemCount numDests = MIDIGetNumberOfDestinations();

        if (numDests > 0)
        {
            m_dest = MIDIGetDestination(numDests - 1);
        }
    }

    void SendLoop()
    {
        while (m_running) 
        {
            MidiMessage message;
            while (m_buffer.Pop(message)) 
            {
                if (!m_dest)
                {
                    continue;
                }
                
                MIDIPacketList pktList;
                MIDIPacket* pkt = MIDIPacketListInit(&pktList);
                pkt = MIDIPacketListAdd(
                    &pktList, 
                    sizeof(pktList), 
                    pkt,
                    message.m_timestamp, 
                    message.m_size, 
                    message.m_data);
                if (pkt)
                {
                    OSStatus result = MIDISend(m_outputPort, m_dest, &pktList);
                    if (result != noErr) 
                    {
                        INFO("MIDISend failed: %d", result);
                    }
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

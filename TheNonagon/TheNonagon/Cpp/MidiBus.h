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

    bool IsTransportStart() const
    {
        return m_data[0] == 0xFA;
    }

    bool IsTransportStop() const
    {
        return m_data[0] == 0xFC;
    }

    bool IsClock() const
    {
        return m_data[0] == 0xF8;
    }
};

struct MidiBus
{
    CircularQueue<MidiMessage, 8192> m_inputBuffer;
    CircularQueue<MidiMessage, 8192> m_outputBuffer;
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
        m_inputBuffer.Push(message);
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
    MIDIPortRef m_inputPort;
    MIDIEndpointRef m_dest;
    MIDIEndpointRef m_src;
    std::atomic<int> m_desiredOutputIndex;
    std::atomic<int> m_desiredInputIndex;
    int m_outputIndex;
    int m_inputIndex;

    void SetOutput(int index)
    {
        INFO("Setting desired MIDI output to %d\n", index);
        m_desiredOutputIndex = index;
    }

    void SetInput(int index)
    {
        INFO("Setting desired MIDI input to %d\n", index);
        m_desiredInputIndex = index;
    }

    static void MidiInputCallback(const MIDIPacketList* pktList, void* readProcRefCon, void* srcConnRefCon)
    {
        MidiBus* midiBus = static_cast<MidiBus*>(readProcRefCon);
        const MIDIPacket* packet = &pktList->packet[0];
        
        for (int i = 0; i < pktList->numPackets; i++)
        {
            MidiMessage message;
            message.m_timestamp = packet->timeStamp;
            message.m_size = packet->length > 3 ? 3 : packet->length;
            
            for (int j = 0; j < message.m_size; j++)
            {
                message.m_data[j] = packet->data[j];
            }
            
            midiBus->m_outputBuffer.Push(message);
            
            packet = MIDIPacketNext(packet);
        }
    }

    bool PopIfTimestampReached(UInt64 timestamp, MidiMessage& message)
    {
        MidiMessage peekMessage;
        if (m_outputBuffer.Peek(peekMessage))
        {
            if (peekMessage.m_timestamp <= timestamp)
            {
                return m_outputBuffer.Pop(message);
            }
        }

        return false;
    }

    void InitMIDI()
    {
        MIDIClientCreate(CFSTR("TheNonagon"), nullptr, nullptr, &m_client);
        MIDIOutputPortCreate(m_client, CFSTR("TheNonagonOutPort"), &m_outputPort);
        MIDIInputPortCreate(m_client, CFSTR("TheNonagonInPort"), MidiInputCallback, this, &m_inputPort);

        ItemCount numDests = MIDIGetNumberOfDestinations();
        ItemCount numSources = MIDIGetNumberOfSources();

        if (numDests > 0)
        {
            m_desiredOutputIndex = 0;
            m_outputIndex = m_desiredOutputIndex;
            m_dest = MIDIGetDestination(m_outputIndex);
        }
        else
        {
            m_desiredOutputIndex = -1;
            m_outputIndex = m_desiredOutputIndex;
        }

        if (numSources > 0)
        {
            m_desiredInputIndex = 0;
            m_inputIndex = m_desiredInputIndex;
            m_src = MIDIGetSource(m_inputIndex);
            MIDIPortConnectSource(m_inputPort, m_src, nullptr);
        }
        else
        {
            m_desiredInputIndex = -1;
            m_inputIndex = m_desiredInputIndex;
        }
    }

    void SendLoop()
    {
        while (m_running) 
        {
            if (m_outputIndex != m_desiredOutputIndex)
            {
                m_outputIndex = m_desiredOutputIndex;
                if (m_outputIndex >= 0)
                {
                    m_dest = MIDIGetDestination(m_outputIndex);
                    INFO("MIDI output changed to %d\n", m_outputIndex);
                }
            }

            if (m_inputIndex != m_desiredInputIndex)
            {
                if (m_inputIndex >= 0)
                {
                    MIDIPortDisconnectSource(m_inputPort, m_src);
                }
                
                m_inputIndex = m_desiredInputIndex;
                if (m_inputIndex >= 0)
                {
                    m_src = MIDIGetSource(m_inputIndex);
                    MIDIPortConnectSource(m_inputPort, m_src, nullptr);
                    INFO("MIDI input changed to %d\n", m_inputIndex);
                }
            }

            MidiMessage message;
            while (m_inputBuffer.Pop(message)) 
            {
                if (!m_dest || m_outputIndex < 0)
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
                        INFO("MIDISend failed: %d\n", result);
                    }
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

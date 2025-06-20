#pragma once

#define IOS_BUILD
#include "TheNonagon.hpp"
#include "CircularQueue.h"
#include "MidiBus.h"
#include "GridHandle.h"
#include <thread>
#include <CoreAudio/CoreAudioTypes.h>


struct NonagonGridRouter
{
    struct Message
    {
        int m_x;
        int m_y;
        int m_velocity;
        GridHandle m_gridHandle;

        Message()
            : m_x(0)
            , m_y(0)
            , m_velocity(0)
            , m_gridHandle(GridHandle_NumGrids)
        {
        }

        Message(GridHandle gridHandle, int x, int y, int velocity)
            : m_x(x)
            , m_y(y)
            , m_velocity(velocity)
            , m_gridHandle(gridHandle)
        {
        }
    };

    TheNonagonSmartGrid* m_nonagon;
    SmartGrid::Grid* m_leftGrid;
    SmartGrid::Grid* m_rightGrid;
    size_t m_gridIndex;
    CircularQueue<Message, 1024> m_ioQueue;

    NonagonGridRouter(TheNonagonSmartGrid* nonagon)
    {  
        m_nonagon = nonagon;
        m_leftGrid = m_nonagon->m_lameJuisCoMuteGrid;
        m_rightGrid = m_nonagon->m_theoryOfTimeTopologyGrid;
        m_gridIndex = 0;
    }
    
    ~NonagonGridRouter()
    {
    }

    void ProcessMessages()
    {
        Message message;
        while (m_ioQueue.Pop(message))
        {
            ProcessMessage(message);
        }
    }

    void ProcessMessage(Message& message)
    {
        if (message.m_velocity > 0)
        {
            HandlePress(message.m_gridHandle, message.m_x, message.m_y);
        }
        else
        {
            HandleRelease(message.m_gridHandle, message.m_x, message.m_y);
        }
    }

    void HandlePress(GridHandle gridHandle, int x, int y)
    {
        if (x < 8)
        {
            GetGridByHandle(gridHandle)->OnPress(x, y, 128);
        }
        else
        {
            m_rightGrid->OnPress(x - 8, y, 128);
        }
    }

    void HandleRelease(GridHandle gridHandle, int x, int y)
    {
        if (x < 8)
        {
            GetGridByHandle(gridHandle)->OnRelease(x, y);
        }
        else
        {
            m_rightGrid->OnRelease(x - 8, y);
        }
    }

    void QueuePress(GridHandle gridHandle, int x, int y)
    {
        m_ioQueue.Push(Message(gridHandle, x, y, 128));
    }
    
    void QueueRelease(GridHandle gridHandle, int x, int y)
    {
        m_ioQueue.Push(Message(gridHandle, x, y, 0));
    }

    RGBColor GetColor(GridHandle gridHandle, int x, int y)
    {
        SmartGrid::Color color;
        if (x < 8)
        {
            color = GetGridByHandle(gridHandle)->GetColor(x, y);
        }
        else
        {
            color = m_rightGrid->GetColor(x - 8, y);
        }

        return RGBColor(color.m_red, color.m_green, color.m_blue);
    }

    void HandleRightMenuPress(int index)
    {
        m_leftGrid = GetLeftGridByIndex(index);
        m_rightGrid = GetRightGridByIndex(index);
        m_gridIndex = index;
    }

    RGBColor GetRightMenuColor(int index)
    {
        SmartGrid::Color color;
        if (index == m_gridIndex)
        {
            color = GetRightGridByIndex(index)->m_onColor;

        }
        else
        {
            color = GetRightGridByIndex(index)->m_offColor;
        }

        return RGBColor(color.m_red, color.m_green, color.m_blue);
    }

    SmartGrid::Grid* GetLeftGridByIndex(size_t index)
    {
        switch (index)
        {
            case 0: return m_nonagon->m_lameJuisCoMuteGrid;
            case 1: return m_nonagon->m_theoryOfTimeSwingGrid;
            case 2: return m_nonagon->m_lameJuisMatrixGrid;
            case 3: return m_nonagon->m_timbreAndMuteFireGrid;
            case 4: return m_nonagon->m_timbreAndMuteEarthGrid;
            case 5: return m_nonagon->m_timbreAndMuteWaterGrid;
            default: return m_nonagon->m_lameJuisCoMuteGrid;
        }
    }

    SmartGrid::Grid* GetRightGridByIndex(size_t index)
    {
        switch (index)
        {
            case 0: return m_nonagon->m_theoryOfTimeTopologyGrid;
            case 1: return m_nonagon->m_lameJuisIntervalGrid;
            case 2: return m_nonagon->m_lameJuisLHSGrid;
            case 3: return m_nonagon->m_indexArpFireGrid;
            case 4: return m_nonagon->m_indexArpEarthGrid;
            case 5: return m_nonagon->m_indexArpWaterGrid;
            default: return m_nonagon->m_theoryOfTimeTopologyGrid;
        }
    }

    SmartGrid::Grid* GetGridByHandle(GridHandle handle)
    {
        switch (handle)
        {
            case GridHandle_LameJuisCoMute: return m_nonagon->m_lameJuisCoMuteGrid;
            case GridHandle_TheoryOfTimeSwing: return m_nonagon->m_theoryOfTimeSwingGrid;
            case GridHandle_LameJuisMatrix: return m_nonagon->m_lameJuisMatrixGrid;
            case GridHandle_TimbreAndMuteFire: return m_nonagon->m_timbreAndMuteFireGrid;
            case GridHandle_TimbreAndMuteEarth: return m_nonagon->m_timbreAndMuteEarthGrid;
            case GridHandle_TimbreAndMuteWater: return m_nonagon->m_timbreAndMuteWaterGrid;
            case GridHandle_TheoryOfTimeTopology: return m_nonagon->m_theoryOfTimeTopologyGrid;
            case GridHandle_LameJuisInterval: return m_nonagon->m_lameJuisIntervalGrid;
            case GridHandle_LameJuisLHS: return m_nonagon->m_lameJuisLHSGrid;
            case GridHandle_IndexArpFire: return m_nonagon->m_indexArpFireGrid;
            case GridHandle_IndexArpEarth: return m_nonagon->m_indexArpEarthGrid;
            case GridHandle_IndexArpWater: return m_nonagon->m_indexArpWaterGrid;
            default: return nullptr;
        }
    }
};

struct NonagonMidiSender
{
    MidiBus* m_midiBus;
    TheNonagonSmartGrid* m_nonagon;

    NonagonMidiSender(MidiBus* midiBus, TheNonagonSmartGrid* nonagon)
    {
        m_midiBus = midiBus;
        m_nonagon = nonagon;
    }

    void SendVoltPerOctave(int channel, float volts)
    {
        if (m_midiBus->IsChannelPlaying(channel))
        {
            return;
        }

        int note = 12 * volts + 60;
        float prePitchBend = 12 * volts + 60 - note;
        int pitchBend = prePitchBend * 4096;
        m_midiBus->SendNoteOnIfOff(note, 127, channel);
        m_midiBus->SendPitchBend(pitchBend, channel);
    }

    void StopNote(int channel)
    {
        m_midiBus->SendNoteOff(channel);
    }

    void SendMidiNotes()
    {
        TheNonagonInternal::Output& output = m_nonagon->m_nonagon.m_output;
        for (int i = 0; i < TheNonagonInternal::x_numVoices; i++)
        {
            if (output.m_gate[i])
            {
                SendVoltPerOctave(i, output.m_voltPerOct[i]);
            }
            else
            {
                StopNote(i);
            }
        }
    }

    void SetMidiOutput(int32_t index)
    {
        m_midiBus->SetOutput(index);
    }
};

struct NonagonMidiReceiver
{
    MidiBus* m_midiBus;
    TheNonagonSmartGrid* m_nonagon;

    NonagonMidiReceiver(MidiBus* midiBus, TheNonagonSmartGrid* nonagon)
    {
        m_midiBus = midiBus;
        m_nonagon = nonagon;
    }

    void ProcessMessages(UInt64 timestamp)
    {
        MidiMessage message;
        while (m_midiBus->PopIfTimestampReached(timestamp, message))
        {
            ProcessMessage(message);
        }
    }

    void ProcessMessage(MidiMessage& message)
    {
        if (message.IsTransportStart())
        {
            m_nonagon->m_state.m_running = true;
        }
        else if (message.IsTransportStop())
        {
            m_nonagon->m_state.m_running = false;
        }
    }

    void SetMidiInput(int32_t index)
    {
        m_midiBus->SetInput(index);
    }
};

struct NonagonHolder
{
    TheNonagonSmartGrid m_nonagon;
    NonagonGridRouter m_gridRouter;
    NonagonMidiSender m_midiSender;
    NonagonMidiReceiver m_midiReceiver;
    MidiBus m_midiBus;

    NonagonHolder() 
        : m_gridRouter(&m_nonagon)
        , m_midiSender(&m_midiBus, &m_nonagon)
        , m_midiReceiver(&m_midiBus, &m_nonagon)
    {
    }
    
    ~NonagonHolder() 
    { 
    }

    void Process(float dt, UInt64 frameTimestamp)
    {
        m_midiReceiver.ProcessMessages(frameTimestamp);
        m_nonagon.Process(dt);
        m_midiSender.SendMidiNotes();
        m_gridRouter.ProcessMessages();
    }

    void HandlePress(GridHandle gridHandle, int x, int y)
    {
        m_gridRouter.QueuePress(gridHandle, x, y);
    }
    
    void HandleRelease(GridHandle gridHandle, int x, int y)
    {
        m_gridRouter.QueueRelease(gridHandle, x, y);
    }

    RGBColor GetColor(GridHandle gridHandle, int x, int y)
    {
        return m_gridRouter.GetColor(gridHandle, x, y);
    }

    void HandleRightMenuPress(int index)
    {
        m_gridRouter.HandleRightMenuPress(index);
    }

    RGBColor GetRightMenuColor(int index)
    {
        return m_gridRouter.GetRightMenuColor(index);
    }
    
    void SetMidiInput(int32_t index)
    {
        m_midiReceiver.SetMidiInput(index);
    }
    
    void SetMidiOutput(int32_t index)
    {
        m_midiSender.SetMidiOutput(index);
    }
};  
#pragma once

#define IOS_BUILD
#include "TheNonagon.hpp"
#include "CircularQueue.h"
#include "MidiBus.h"
#include <thread>

struct NonagonGridRouter
{
    TheNonagonSmartGrid* m_nonagon;
    SmartGrid::Grid* m_leftGrid;
    SmartGrid::Grid* m_rightGrid;
    size_t m_gridIndex;
    CircularQueue<SmartGrid::Message, 1024> m_ioQueue;

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
        SmartGrid::Message message;
        while (m_ioQueue.Pop(message))
        {
            ProcessMessage(message);
        }
    }

    void ProcessMessage(SmartGrid::Message& message)
    {
        if (message.m_velocity > 0)
        {
            HandlePress(message.m_x, message.m_y);
        }
        else
        {
            HandleRelease(message.m_x, message.m_y);
        }
    }

    void HandlePress(int x, int y)
    {
        if (x < 8)
        {
            m_leftGrid->OnPress(x, y, 128);
        }
        else
        {
            m_rightGrid->OnPress(x - 8, y, 128);
        }
    }

    void HandleRelease(int x, int y)
    {
        if (x < 8)
        {
            m_leftGrid->OnRelease(x, y);
        }
        else
        {
            m_rightGrid->OnRelease(x - 8, y);
        }
    }

    void QueuePress(int x, int y)
    {
        m_ioQueue.Push(SmartGrid::Message(x, y, 128));
    }
    
    void QueueRelease(int x, int y)
    {
        m_ioQueue.Push(SmartGrid::Message(x, y, 0));
    }

    RGBColor GetColor(int x, int y)
    {
        SmartGrid::Color color;
        if (x < 8)
        {
            color = m_leftGrid->GetColor(x, y);
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
};

struct NonagonHolder
{
    TheNonagonSmartGrid m_nonagon;
    NonagonGridRouter m_gridRouter;
    NonagonMidiSender m_midiSender;

    MidiBus m_midiBus;

    NonagonHolder() 
        : m_gridRouter(&m_nonagon)
        , m_midiSender(&m_midiBus, &m_nonagon)
    {
    }
    
    ~NonagonHolder() 
    { 
    }

    void Process(float dt)
    {
        m_nonagon.Process(dt);
        m_midiSender.SendMidiNotes();
        m_gridRouter.ProcessMessages();
    }

    void HandlePress(int x, int y)
    {
        m_gridRouter.QueuePress(x, y);
    }
    
    void HandleRelease(int x, int y)
    {
        m_gridRouter.QueueRelease(x, y);
    }

    RGBColor GetColor(int x, int y)
    {
        return m_gridRouter.GetColor(x, y);
    }

    void HandleRightMenuPress(int index)
    {
        m_gridRouter.HandleRightMenuPress(index);
    }

    RGBColor GetRightMenuColor(int index)
    {
        return m_gridRouter.GetRightMenuColor(index);
    }
};  
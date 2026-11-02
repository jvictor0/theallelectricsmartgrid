#pragma once
#include "plugin.hpp"
#include "HostStubs.hpp"
#include <memory>
#include <cstddef>
#include <cmath>
#include <algorithm>
#include <vector>
#include <atomic>
#include "DebugLog.hpp"
#include "HSV.hpp"
#include "Color.hpp"
#include "State.hpp"
#include "MessageIn.hpp"

namespace SmartGrid
{

template<class BusInput>
struct SmartBusGeneric;

typedef SmartBusGeneric<Color> SmartBusColor;

static uint8_t x_maxVelocity = 127;
    
struct Cell
{
    virtual ~Cell()
    {
    }

    bool m_pressureSensitive;
    uint8_t m_velocity;

    Cell()
    {
        m_pressureSensitive = false;
        m_velocity = 0;
    }

    virtual Color GetColor()
    {
        return Color::Off;
    }

    bool IsPressed()
    {
        return m_velocity > 0;
    }

    void OnPressStatic(uint8_t velocity)
    {
        if (!IsPressed())
        {
            m_velocity = velocity;
            OnPress(velocity);
        }
        else if (m_pressureSensitive)
        {
            m_velocity = velocity;
            OnPressureChange(velocity);
        }
    }

    virtual void OnPress(uint8_t velocity)
    {
    }

    void OnReleaseStatic()
    {
        if (IsPressed())
        {
            m_velocity = 0;
            OnRelease();
        }
    }
    
    virtual void OnRelease()
    {
    }
    
    virtual void OnPressureChange(uint8_t velocity)
    {
    }

    void SetPressureSensitive()
    {
        m_pressureSensitive = true;
    }
    
    void OnPressureChangeStatic(uint8_t velocity)
    {
        if (m_pressureSensitive)
        {
            m_velocity = velocity;
            if (IsPressed())
            {                
                OnPressureChange(velocity);
            }
            else
            {
                OnPress(velocity);
            }            
        }
    }
};

struct NoFlash
{
    bool IsFlashing()
    {
        return false;
    }
};

struct BoolFlash
{
    bool* m_flash;
    BoolFlash(bool* flash) : m_flash(flash)
    {
    }

    bool IsFlashing()
    {
        return *m_flash;
    }
};

template<class StateClass = size_t>
struct Flash
{
    StateClass* m_state;
    StateClass m_myState;
    Flash(
        StateClass* state,
        StateClass myState)
        : m_state(state)
        , m_myState(myState)
    {
    }

    bool IsFlashing()
    {
        return *m_state == m_myState;
    }
};

template<class StateClass, class FlashClass = NoFlash>
struct StateCell : public Cell
{
    Color m_offColor;
    Color m_onColor;
    Color m_offFlashColor;
    Color m_onFlashColor;

    State* m_state;
    FlashClass m_flash;

    StateClass m_myState;
    StateClass m_offState;

    enum class Mode : int
    {
        Toggle,
        Momentary,
        SetOnly
    };

    Mode m_mode;

    virtual ~StateCell()
    {
    }

    StateCell(
        Color offColor,
        Color onColor,
        Color offFlashColor,
        Color onFlashColor,
        State* state,
        FlashClass flash,
        StateClass myState,
        StateClass offState,
        Mode mode)
        : m_offColor(offColor)
        , m_onColor(onColor)
        , m_offFlashColor(offFlashColor)
        , m_onFlashColor(onFlashColor)
        , m_state(state)
        , m_flash(flash)
        , m_myState(myState)
        , m_offState(offState)
        , m_mode(mode)
    {
    }

    StateCell(
        Color offColor,
        Color onColor,
        State* state,
        StateClass myState,
        StateClass offState = 0,        
        Mode mode = Mode::Toggle) 
        : StateCell(offColor, onColor, onColor, offColor, state, FlashClass(), myState, offState, mode)
    {
    }

    virtual Color GetColor() override
    {
        if (m_flash.IsFlashing())
        {
            return m_state->Get<StateClass>() == m_myState ? m_onFlashColor : m_offFlashColor;
        }
        else
        {
            return m_state->Get<StateClass>() == m_myState ? m_onColor : m_offColor;
        }
    }

    virtual void OnPress(uint8_t) override
    {
        switch (m_mode)
        {
            case Mode::Momentary:
            {
                m_state->Set(m_myState);
                break;
            }
            case Mode::SetOnly:
            {
                m_state->Set(m_myState);
                break;
            }
            case Mode::Toggle:
            {
                StateClass newState = m_state->Get<StateClass>() == m_myState ? m_offState : m_myState;
                m_state->Set(newState);
                break;
            }
        }
    }

    virtual void OnRelease() override
    {
        if (m_mode == Mode::Momentary)
        {
            m_state->Set(m_offState);
        }
    }
};

struct RuntimeStateCell : public Cell
{
    enum class Mode : int
    {
        Toggle,
        Momentary,
        SetOnly,
        ShowOnly
    };

    Color m_onColor;
    Color m_offColor;
    bool* m_state;
    Mode m_mode;

    RuntimeStateCell(
        Color onColor,
        Color offColor,
        bool* state,
        Mode mode)
        : m_onColor(onColor)
        , m_offColor(offColor)
        , m_state(state)
        , m_mode(mode)
    {
    }

    virtual Color GetColor() override
    {
        return *m_state ? m_onColor : m_offColor;
    }

    virtual void OnPress(uint8_t) override
    {
        switch (m_mode)
        {
            case Mode::Toggle:
            {
                *m_state = !*m_state;
                break;
            }
            case Mode::Momentary:
            {
                *m_state = true;
                break;
            }
            case Mode::SetOnly:
            {
                *m_state = true;
                break;
            }
            case Mode::ShowOnly:
            {
                break;
            }
        }
    }

    virtual void OnRelease() override
    {
        if (m_mode == Mode::Momentary)
        {
            *m_state = false;
        }
    }
};

template<class StateClass, class FlashClass=NoFlash>
struct CycleCell : Cell
{
    ColorScheme m_colors;
    ColorScheme m_flashColors;
    State* m_state;
    FlashClass m_flash;

    virtual ~CycleCell()
    {
    }

    CycleCell(
        ColorScheme colorScheme,
        ColorScheme flashColorScheme,
        State* state,
        FlashClass flash)
        : m_colors(colorScheme)
        , m_flashColors(flashColorScheme)
        , m_state(state)
        , m_flash(flash)
    {
    }

    CycleCell(
        ColorScheme colorScheme,
        State* state)
        : m_colors(colorScheme)
        , m_state(state)
    {
    }

    void Cycle()
    {
        m_state->Set(static_cast<StateClass>((static_cast<size_t>(m_state->Get<StateClass>()) + 1) % m_colors.size()));
    }

    virtual Color GetColor() override
    {
        if (m_flash.IsFlashing())
        {
            return m_flashColors[static_cast<size_t>(m_state->Get<StateClass>())];
        }
        else
        {
            return m_colors[static_cast<size_t>(m_state->Get<StateClass>())];
        }
    }

    virtual void OnPress(uint8_t) override
    {
        Cycle();
    }    
};

template<class StateClass>
struct BinaryCell : Cell
{
    Color m_offColor;
    Color m_onColor;
    StateClass m_mask;
    StateClass* m_state;

    BinaryCell(
        Color offColor,
        Color onColor,
        size_t ix,
        StateClass* state)
        : m_offColor(offColor)
        , m_onColor(onColor)
        , m_mask(1 << ix)
        , m_state(state)
    {
    }

    virtual Color GetColor() override
    {
        if (m_mask & (*m_state))
        {
            return m_onColor;
        }
        else
        {
            return m_offColor;
        }
    }

    virtual void OnPress(uint8_t) override
    {
        *m_state = (*m_state) ^ m_mask;
    }
};

static constexpr int x_baseGridSize = 8;

static constexpr int x_gridXMin = -1;
static constexpr int x_gridXMax = 9;
static constexpr int x_gridYMin = -1;
static constexpr int x_gridYMax = 10;
static constexpr int x_gridMaxSize = 11;

static constexpr int x_gridXSize = x_gridXMax - x_gridXMin;
static constexpr int x_gridYSize = x_gridYMax - x_gridYMin;

enum class ControllerShape : int
{
    LaunchPadX = 0,
    LaunchPadProMk3 = 1,
    LaunchPadMiniMk3 = 2,
    MidiFighterTwister = 3,
    NumShapes = 4
};

inline const char* ControllerShapeToString(ControllerShape shape)
{
    switch (shape)
    {
        case ControllerShape::LaunchPadX:
        {
            return "LaunchPadX";
        }
        case ControllerShape::LaunchPadProMk3:
        {
            return "LaunchPadProMk3";
        }
        case ControllerShape::LaunchPadMiniMk3:
        {
            return "LaunchPadMiniMk3";
        }
        case ControllerShape::MidiFighterTwister:
        {
            return "MidiFighterTwister";
        }
        default:
        {
            return "Unknown";
        }
    }
}

struct Message
{
    enum class Mode : int
    {
        NoMessage,
        Note,
        Color,
        Increment
    };
    
    int m_x;
    int m_y;
    uint8_t m_velocity;
    Color m_color;
    Mode m_mode;
    
    Message()
        : m_mode(Mode::NoMessage)
    {
    }
    
    Message(
        int x,
        int y,
        uint8_t velocity)
        : m_x(x)
        , m_y(y)
        , m_velocity(velocity)
        , m_mode(Mode::Note)
    {
    }

    Message(
        int x,
        int y,
        uint8_t velocity,
        Mode mode)
        : m_x(x)
        , m_y(y)
        , m_velocity(velocity)
        , m_mode(mode)
    {
    }

    Message(
        int x,
        int y,
        Color color)
        : m_x(x)
        , m_y(y)
        , m_color(color)
        , m_mode(Mode::Color)
    {
    }

    static Message Off(int x, int y)
    {
        return Message(x, y, 0);
    }
    
    Message(
        std::pair<int, int> xy,
        uint8_t velocity)
        : m_x(xy.first)
        , m_y(xy.second)
        , m_velocity(velocity)
        , m_mode(Mode::Note)
    {
    }
    
    Message(Cell* cell, int x, int y)
        : m_x(x)
        , m_y(y)
    {
        m_color = cell->GetColor();
        m_mode = Mode::Color;
    }

    bool ShapeSupports(ControllerShape shape)
    {
        if (shape == ControllerShape::LaunchPadX &&
            (m_x < 0 || m_y < 0))
        {
            return false;
        }

        if (shape == ControllerShape::LaunchPadMiniMk3 &&
            (m_x < 0 || m_y < 0))
        {
            return false;
        }

        if (shape == ControllerShape::MidiFighterTwister &&
            (m_x < 0 || m_y < 0 || m_x >= 4 || m_y >= 4))
        {
            return false;
        }

        return true;
    }
    
    
    bool NoMessage()
    {
        return m_mode == Mode::NoMessage;
    }

    void Populate(ControllerShape shape, int64_t frame, midi::Message* messages)
    {
        if (shape == ControllerShape::MidiFighterTwister)
        {
            uint8_t cc = 4 * m_y + m_x;
            for (int i = 0; i < 3; ++i)
            {
                messages[i].setFrame(frame);
                messages[i].setChannel(i);
                messages[i].setStatus(0xb);
                messages[i].setNote(cc);
            }

            messages[0].setValue(m_color.m_red / 2);
            messages[1].setValue(std::min<uint8_t>(126, std::max<uint8_t>(1, m_color.m_green / 2)));
            messages[2].setValue(m_color.m_blue);
        }
    }

    static Message Decode(ControllerShape shape, const midi::Message& msg)
    {
        switch (shape)
        {
            case ControllerShape::LaunchPadX:
            {
                return FromLPMidi(msg);
            }
            case ControllerShape::LaunchPadProMk3:
            {
                return FromLPMidi(msg);
            }
            case ControllerShape::LaunchPadMiniMk3:
            {
                return FromLPMidi(msg);
            }
            case ControllerShape::MidiFighterTwister:
            {
                return FromMidiFighterTwister(msg);
            }
            default:
            {
                return Message();
            }
        }
    }

    static Message FromMidiFighterTwister(const midi::Message& msg)
    {
        if (msg.getStatus() == 0xb)
        {
            if (msg.getChannel() == 0)
            {
                return Message(msg.getNote() % 4, msg.getNote() / 4, msg.getValue(), Mode::Increment);
            }
            else if (msg.getChannel() == 1)
            {
                return Message(msg.getNote() % 4 + 4, msg.getNote() / 4, msg.getValue(), Mode::Note);
            }
        }

        return Message();
    }
    
    static uint8_t LPPosToNote(int x, int y)
    {
        y = x_baseGridSize - y - 1;
        
        // Remap y to match the bottom two button rows of LPProMk3
        //
        if (y == -1)
        {
            y = 9;
        }
        else if (y == -2)
        {
            y = -1;
        }
        
        return 11 + 10 * y + x;
    }
    
    static std::pair<int, int> NoteToLPPos(uint8_t note)
    {
        if (note < 10)
        {
            // The LaunchpadPro's bottom two rows are in the "wrong" order.
            // Remap.
            //
            return std::make_pair(note - 1, 9);
        }
        
        int y = (note - 11) / 10;
        int x = (note - 11) % 10;

        // Send overflows -1
        //
        if (y == 9)
        {
            y = -1;
        }
        
        if (x == 9)
        {
            x = -1;
            y += 1;
        }                
        
        return std::make_pair(x, x_baseGridSize - y - 1);
    }
    
    static Message FromLPMidi(const midi::Message& msg)
    {
        switch (msg.getStatus())
        {
			case 0x8:
            {
                return Message(NoteToLPPos(msg.getNote()), 0);
			} 
			case 0x9:
            case 0xb:
            {
				if (msg.getValue() > 0)
                {
                    return Message(NoteToLPPos(msg.getNote()), msg.getValue());
                }
                else
                {
                    return Message(NoteToLPPos(msg.getNote()), 0);
                }
            } 
            case 0xa:
            {
                return Message(NoteToLPPos(msg.getNote()), msg.getValue());
            }
            
            default:
            {
                return Message();
            }
        }
    }    
};

struct AbstractGrid
{
    virtual ~AbstractGrid()
    {
    }

    void OutputToBus(SmartBusColor* bus);
    
    virtual void Apply(Message msg) = 0;
    virtual Color GetColor(int i, int j) = 0;

    virtual void Apply(MessageIn msg)
    {
    }

    virtual Color GetOnColor()
    {
        return Color::White;
    }

    virtual Color GetOffColor()
    {
        return GetOnColor().Dim();
    }

    void AllOff()
    {
        for (int i = x_gridXMin; i < x_gridXMax; ++i)
        {
            for (int j = x_gridYMin; j < x_gridYMax; ++j)
            {
                Apply(Message::Off(i, j));
            }
        }        
    }
};

struct Grid : public AbstractGrid
{
    std::unique_ptr<Cell> m_grid[x_gridMaxSize][x_gridMaxSize];
    Color m_onColor;
    Color m_offColor;

    virtual ~Grid()
    {
    }

    Grid(Color onColor, Color offColor)
        : m_onColor(onColor)
        , m_offColor(offColor)
    {
    }

    Grid(Color onColor)
        : m_onColor(onColor)
        , m_offColor(onColor.Dim())
    {
    }

    Grid()
        : m_onColor(Color::White)
        , m_offColor(Color::White.Dim())
    {
    }

    void SetColors(Color on, Color off)
    {
        m_onColor = on;
        m_offColor = off;
    }

    void SetColors(Color on)
    {
        m_onColor = on;
        m_offColor = on.Dim();
    }

    virtual Color GetOnColor() override
    {
        return m_onColor;
    }        

    virtual Color GetOffColor() override
    {
        return m_offColor;
    }        

    Cell* Get(int i, int j)
    {
        if (i < x_gridXMin || i >= x_gridXMax || j < x_gridYMin || j >= x_gridYMax)
        {
            return nullptr;
        }

        return m_grid[i - x_gridXMin][j - x_gridYMin].get();
    }

    void Put(int i, int j, Cell* cell)
    {
        m_grid[i - x_gridXMin][j - x_gridYMin].reset(cell);
    }

    virtual Color GetColor(int i, int j) override
    {
        return Get(i, j) ? Get(i, j)->GetColor() : Color::Off;
    }

    void OnPress(int x, int y, uint8_t velocity)
    {
        if (Get(x, y))
        { 
            Get(x, y)->OnPressStatic(velocity);
        }
    }

    void OnRelease(int x, int y)
    {
        if (Get(x, y))
        {
            Get(x, y)->OnReleaseStatic();
        }
    }

    virtual void Apply(Message msg) override
    {
        if (msg.m_mode == Message::Mode::Note)
        {
            if (msg.m_velocity == 0)
            {
                OnRelease(msg.m_x, msg.m_y);
            }
            else
            {
                OnPress(msg.m_x, msg.m_y, msg.m_velocity);
            }
        }
    }

    virtual void Apply(MessageIn msg) override
    {
        if (msg.m_mode == MessageIn::Mode::PadPress ||
            msg.m_mode == MessageIn::Mode::PadPressure)
        {
            if (msg.m_amount == 0)
            {
                OnRelease(msg.m_x, msg.m_y);
            }
            else
            {
                OnPress(msg.m_x, msg.m_y, msg.m_amount);
            }
        }
        else if (msg.m_mode == MessageIn::Mode::PadRelease)
        {
            OnRelease(msg.m_x, msg.m_y);
        }
    }
};

#include "SmartBus.hpp"

struct GridHolder
{
    std::vector<std::shared_ptr<AbstractGrid>> m_grids;
    
    void AddGrid(AbstractGrid* grid)
    {
        AddGrid(std::shared_ptr<AbstractGrid>(grid));
    }

    void AddGrid(std::shared_ptr<AbstractGrid> grid)
    {
        m_grids.push_back(grid);
    }

    AbstractGrid* Get(size_t ix)
    {
        return m_grids[ix].get();
    }

    size_t Size()
    {
        return m_grids.size();
    }
};

}

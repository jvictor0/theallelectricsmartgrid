#pragma once
#include "plugin.hpp"
#include "ios_stubs.hpp"
#include <memory>
#include <cstddef>
#include <cmath>
#include <algorithm>
#include <vector>
#include <atomic>
#include "DebugLog.hpp"
#include "HSV.hpp"
#include "Color.hpp"
#include "MessageIn.hpp"

namespace SmartGrid
{

template<class BusInput>
struct SmartBusGeneric;

typedef SmartBusGeneric<Color> SmartBusColor;

static uint8_t x_maxVelocity = 127;
static constexpr size_t x_maxChannels = 8;
    
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

    StateClass* m_state;
    FlashClass m_flash;

    StateClass m_myState;
    StateClass m_offState;

    enum class Mode : int
    {
        Toggle,
        Momentary,
        SetOnly,
        ShowOnly
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
        StateClass* state,
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
        StateClass* state,
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
            return (*m_state) == m_myState ? m_onFlashColor : m_offFlashColor;
        }
        else
        {
            return (*m_state) == m_myState ? m_onColor : m_offColor;
        }
    }

    virtual void OnPress(uint8_t) override
    {
        switch (m_mode)
        {
            case Mode::Momentary:
            {
                *m_state = m_myState;
                break;
            }
            case Mode::SetOnly:
            {
                *m_state = m_myState;
                break;
            }
            case Mode::Toggle:
            {
                StateClass newState = (*m_state) == m_myState ? m_offState : m_myState;
                *m_state = newState;
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
            *m_state = m_offState;
        }
    }
};

template<class StateClass, class FlashClass=NoFlash>
struct CycleCell : Cell
{
    ColorScheme m_colors;
    ColorScheme m_flashColors;
    StateClass* m_state;
    FlashClass m_flash;

    virtual ~CycleCell()
    {
    }

    CycleCell(
        ColorScheme colorScheme,
        ColorScheme flashColorScheme,
        StateClass* state,
        FlashClass flash)
        : m_colors(colorScheme)
        , m_flashColors(flashColorScheme)
        , m_state(state)
        , m_flash(flash)
    {
    }

    CycleCell(
        ColorScheme colorScheme,
        StateClass* state)
        : m_colors(colorScheme)
        , m_state(state)
    {
    }

    void Cycle()
    {
        *m_state = static_cast<StateClass>((static_cast<size_t>(*m_state) + 1) % m_colors.size());
    }

    virtual Color GetColor() override
    {
        if (m_flash.IsFlashing())
        {
            return m_flashColors[static_cast<size_t>(*m_state)];
        }
        else
        {
            return m_colors[static_cast<size_t>(*m_state)];
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

#ifndef SMART_BOX
static constexpr int x_gridXMin = -1;
static constexpr int x_gridXMax = 9;
static constexpr int x_gridYMin = -1;
static constexpr int x_gridYMax = 10;
static constexpr int x_gridMaxSize = 11;
#else
static constexpr int x_gridXMin = 0;
static constexpr int x_gridXMax = 20;
static constexpr int x_gridYMin = 0;
static constexpr int x_gridYMax = 8;
static constexpr int x_gridMaxSize = 20;
#endif

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

static constexpr size_t x_numGridIds = 256;

struct GridIdAllocator
{
    std::atomic<bool> m_isAllocated[x_numGridIds];

    GridIdAllocator()
    {
        for (size_t i = 0; i < x_numGridIds; ++i)
        {
            m_isAllocated[i] = false;
        }
    }

    size_t Alloc()
    {
        for (size_t i = 0; i < x_numGridIds; ++i)
        {
            if (!m_isAllocated[i].exchange(true))
            {
                return i;
            }
        }

        WARN("SmartGrid Grid Id Allocator cannot allocate %lu Smart Grids", x_numGridIds);
        return x_numGridIds;
    }

    void Free(size_t id)
    {
        m_isAllocated[id].store(false);
    }
};

#ifndef IOS_BUILD
extern GridIdAllocator g_gridIds;

inline size_t AllocGridId()
{
    return g_gridIds.Alloc();
}

inline void FreeGridId(size_t id)
{
    g_gridIds.Free(id);
}
#else
inline size_t AllocGridId()
{
    return x_numGridIds;
}

inline void FreeGridId(size_t id)
{
}
#endif

struct AbstractGrid
{
    size_t m_gridId;
    uint64_t m_gridInputEpoch;
    
    virtual ~AbstractGrid()
    {
        if (m_gridId != x_numGridIds)
        {
            FreeGridId(m_gridId);
        }
    }

    void RemoveGridId()
    {
        if (m_gridId != x_numGridIds)
        {
            FreeGridId(m_gridId);
        }

        m_gridId = x_numGridIds;
    }
    
    static constexpr float x_busIOInterval = 0.005;
    float m_timeToNextBusIO;
    
    AbstractGrid()
    {
        m_gridId = AllocGridId();
        m_gridInputEpoch = 0;
        m_timeToNextBusIO = -1;
    }

    template<typename Iter>
    void Apply(Iter start, Iter end)
    {
        while (start != end)
        {
            Message msg = *start;
            if (!msg.NoMessage())
            {
                Apply(msg);
            }

            ++start;
        }
    }

    void OutputToBus(SmartBusColor* bus);
    void OutputToBus();
    void ApplyFromBus();
    
    virtual void Apply(Message msg) = 0;
    virtual Color GetColor(int i, int j) = 0;

    virtual void Apply(MessageIn msg)
    {
    }
    
    virtual void Process(float dt)
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

    void ProcessStatic(float dt)
    {
        Process(dt);

        ApplyFromBus();
        m_timeToNextBusIO -= dt;
        if (m_timeToNextBusIO <= 0)
        {
            m_timeToNextBusIO = x_busIOInterval;
            OutputToBus();
        }
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
    std::shared_ptr<Cell> m_grid[x_gridMaxSize][x_gridMaxSize];
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
    
    std::shared_ptr<Cell>& GetShared(size_t i, size_t j)
    {
        return m_grid[i - x_gridXMin][j - x_gridYMin];
    }

    Cell* Get(size_t i, size_t j)
    {
        return m_grid[i - x_gridXMin][j - x_gridYMin].get();        
    }       

    void Put(int i, int j, Cell* cell)
    {
        m_grid[i - x_gridXMin][j - x_gridYMin].reset(cell);
    }

    void Put(int i, int j, std::shared_ptr<Cell> cell)
    {
        m_grid[i - x_gridXMin][j - x_gridYMin] = cell;
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

struct CompositeGrid : public Grid
{
    virtual ~CompositeGrid()
    {
    }

    CompositeGrid()
    {
    }
        
    std::vector<std::shared_ptr<Grid>> m_grids;

    void AddGrid(int xOff, int yOff, Grid* grid)
    {
        AddGrid(xOff, yOff, std::shared_ptr<Grid>(grid));
    }

    void AddGrid(int xOff, int yOff, std::shared_ptr<Grid> grid)
    {
        m_grids.push_back(grid);
        Place(xOff, yOff, grid.get());
    }

    void AddGrid(Grid* grid)
    {
        AddGrid(std::shared_ptr<Grid>(grid));
    }

    void AddGrid(std::shared_ptr<Grid> grid)
    {
        m_grids.push_back(grid);
    }

    void Place(int xOff, int yOff, Grid* grid)
    {
        for (int i = x_gridXMin; i < x_gridXMax; ++i)
        {
            for (int j = x_gridYMin; j < x_gridYMax; ++j)
            {
                if (grid->Get(i, j))
                {
                    if (Get(i + xOff, j + yOff))
                    {
                        Get(i + xOff, j + yOff)->OnReleaseStatic();
                    }
                    
                    Put(i + xOff, j + yOff, grid->GetShared(i, j));
                }
            }
        }
    }

    virtual void Process(float dt) override
    {
        for (std::shared_ptr<Grid>& grid : m_grids)
        {
            grid->ProcessStatic(dt);
        }
    }
};

#include "SmartBus.hpp"

struct GridHolder
{
    std::vector<std::shared_ptr<AbstractGrid>> m_grids;
    
    size_t AddGrid(AbstractGrid* grid)
    {
        return AddGrid(std::shared_ptr<AbstractGrid>(grid));
    }

    size_t AddGrid(std::shared_ptr<AbstractGrid> grid)
    {
        m_grids.push_back(grid);
        return grid->m_gridId;
    }

    Color GetOnColor(size_t gridIx)
    {
        return SmartBusGetOnColor(GridId(gridIx));
    }

    Color GetOffColor(size_t gridIx)
    {
        return SmartBusGetOffColor(GridId(gridIx));
    }

    AbstractGrid* Get(size_t ix)
    {
        return m_grids[ix].get();
    }

    size_t GridId(size_t ix)
    {
        return Get(ix)->m_gridId;
    }

    size_t Size()
    {
        return m_grids.size();
    }

    void Process(float dt)
    {
        for (std::shared_ptr<AbstractGrid>& grid : m_grids)
        {
            grid->ProcessStatic(dt);
        }
    }    
};

struct GridSwitcher : public AbstractGrid
{
    size_t m_selectedGridId;
    size_t m_lastGridId;
    std::shared_ptr<AbstractGrid> m_menuGrid;

    virtual ~GridSwitcher()
    {
    }
    
    GridSwitcher(AbstractGrid* menu)
        : m_selectedGridId(x_numGridIds)
        , m_lastGridId(x_numGridIds)
        , m_menuGrid(std::shared_ptr<AbstractGrid>(menu))
    {
    }

    GridSwitcher(std::shared_ptr<AbstractGrid> menu)
        : m_selectedGridId(x_numGridIds)
        , m_lastGridId(x_numGridIds)
        , m_menuGrid(menu)
    {
    }

    virtual size_t GetGridId()
    {
        return m_selectedGridId;
    }

    virtual void Apply(Message msg) override
    {
        if (m_menuGrid)
        {
            m_menuGrid->Apply(msg);
        }
        
        if (m_selectedGridId != x_numGridIds)
        {
            SmartBusPutVelocity(m_selectedGridId, msg.m_x, msg.m_y, msg.m_velocity);
        }
    }

    virtual Color GetColor(int i, int j) override
    {
        if (m_menuGrid)
        {
            Color c = m_menuGrid->GetColor(i, j);
            if (c != Color::Off)
            {
                return c;
            }
        }
        
        if (m_selectedGridId != x_numGridIds)
        {
            return SmartBusGetColor(m_selectedGridId, i, j);
        }
        else
        {
            return Color::Off;
        }
    }

    virtual Color GetOnColor() override
    {
        return SmartBusGetOnColor(m_selectedGridId);
    }        

    virtual Color GetOffColor() override
    {
        return SmartBusGetOffColor(m_selectedGridId);
    }        


    virtual void Process(float dt) override
    {
        if (m_menuGrid)
        {
            m_menuGrid->ProcessStatic(dt);
        }

        m_selectedGridId = GetGridId();

        if (m_lastGridId != m_selectedGridId)
        {
            if (m_lastGridId != x_numGridIds)
            {
                SmartBusClearVelocities(m_lastGridId);
            }
            
            m_lastGridId = m_selectedGridId;
        }
    }
};

struct Fader : public Grid
{
    virtual ~Fader()
    {
    }
    
    size_t m_height;
    
    Color m_color;
    float m_minSpeed;
    float m_maxSpeed;

    float m_maxValue;
    float m_minValue;
    float m_logMaxOverMin;

    float DeNormalize(float normVal)
    {
        if (IsBipolar())
        {
            return m_maxValue * normVal;
        }
        else if (IsExponential())
        {
            return m_minValue * std::exp2f(normVal * m_logMaxOverMin);
        }
        else
        {
            return m_minValue + (m_maxValue - m_minValue) * normVal;
        }
    }

    float Normalize(float absVal)
    {
        if (IsBipolar())
        {
            return absVal / m_maxValue;
        }
        else if (IsExponential())
        {
            return std::log2f(absVal / m_minValue) / m_logMaxOverMin;
        }
        else
        {
            return (absVal - m_minValue) / (m_maxValue - m_minValue);
        }
    }
    
    float* m_state;
    float m_lastAbsState;
    float m_target;
    float m_speed;
    bool m_moving;
    bool m_targetingBipolarZero;
    bool m_isBipolarZero;

    bool m_pressureSensitive;

    enum class Structure : int
    {
        Linear,
        Bipolar,
        Exponential
    };

    Structure m_structure;

    bool IsBipolar()
    {
        return m_structure == Structure::Bipolar;
    }

    bool IsExponential()
    {
        return m_structure == Structure::Exponential;
    }
    
    enum class Mode : int
    {
        Relative,
        Absolute
    };

    Mode m_mode;

    struct FaderCell : public Cell
    {
        Fader* m_owner;
        size_t m_fromBottom;
        int m_fromCenter;
        uint8_t m_velocity;
        
        FaderCell(Fader* owner, int fromBottom)
            : m_owner(owner)
            , m_fromBottom(fromBottom)
            , m_velocity(0)
        {
            if (m_owner->m_pressureSensitive)
            {
                SetPressureSensitive();
            }

            m_fromCenter = m_fromBottom - m_owner->m_height / 2;
            if (m_owner->m_height % 2 == 0 && m_fromBottom >= m_owner->m_height / 2)
            {
                // If there is no exact middle, bump the upper half to have non-zero 'm_fromCenter'.
                //
                ++m_fromCenter;
            }
        }

        virtual Color GetColor() override
        {
            if (m_owner->IsBipolar() && m_owner->m_isBipolarZero)
            {
                if (m_fromCenter == 0 ||
                    (m_owner->m_height % 2 == 0 && std::abs(m_fromCenter) <= 1))
                {
                    return m_owner->m_color.Dim();
                }
                else
                {
                    return Color::Off;
                }
            }
            else if (m_owner->IsBipolar() &&
                (m_fromCenter == 0 ||
                 (std::abs(m_fromCenter) < std::abs(m_owner->m_posFromCenter) &&
                  (m_fromCenter > 0) == (m_owner->m_posFromCenter > 0))))
            {
                return m_owner->m_color;
            }
            else if (!m_owner->IsBipolar() &&
                     m_fromBottom < m_owner->m_posFromBottom)
            {
                return m_owner->m_color;
            }
            else if (m_fromBottom == m_owner->m_posFromBottom)
            {
                return m_owner->m_color.AdjustBrightness(m_owner->m_valueWithinCell);
            }
            else
            {
                return Color::Off;                        
            }
        }
        
        float GetSpeed()
        {
            uint8_t vel = m_velocity;
            if (m_owner->m_mode == Mode::Relative)
            {
                size_t towardsCenter = m_owner->m_height / 2 - std::abs(m_fromCenter);
                vel = vel >> (2 * towardsCenter);
            }

            float frac = static_cast<float>(vel) / x_maxVelocity;
            float speed = m_owner->m_minSpeed + (m_owner->m_maxSpeed - m_owner->m_minSpeed) * frac;
            return speed;
        }

        float GetNonReducedSpeed()
        {
            float frac = static_cast<float>(m_velocity) / x_maxVelocity;
            float speed = m_owner->m_minSpeed + (m_owner->m_maxSpeed - m_owner->m_minSpeed) * frac;
            return speed;
        }

        void Stop()
        {
            m_owner->m_moving = false;
        }
        
        virtual void OnPress(uint8_t velocity) override
        {
            m_velocity = velocity;
            m_owner->SetSpeedAndTarget();
        }
        
        virtual void OnRelease() override
        {
            m_velocity = 0;
            m_owner->SetSpeedAndTarget();
        }

        virtual void OnPressureChange(uint8_t velocity) override
        {
            OnPress(velocity);
        }
        
        virtual ~FaderCell()
        {
        }
    };

    void SetSpeedAndTarget()
    {
        float speed = 0;
        bool isEven = m_height % 2 == 0;
        bool isCenterTouched = Get(0, m_height / 2)->IsPressed();
        bool isNonCenterTouched = false;
        bool anyTouched = false;
        if (isCenterTouched && isEven)
        {
            isCenterTouched = Get(0, m_height / 2 - 1)->IsPressed();
        }
        
        for (size_t i = 0; i < m_height; ++i)
        {            
            FaderCell* cell = static_cast<FaderCell*>(Get(0, i));
            if (cell->IsPressed())
            {
                anyTouched = true;
                if ((!isEven && cell->m_fromCenter != 0) || std::abs(cell->m_fromCenter) != 1)
                {
                    isNonCenterTouched = true;
                }
                
                if (m_mode == Mode::Relative)
                {
                    float speedComponent = cell->GetSpeed();
                    if (cell->m_fromCenter > 0)
                    {
                        speed += speedComponent;
                    }
                    else if (cell->m_fromCenter < 0)
                    {
                        speed -= speedComponent;
                    }
                }
                else
                {
                }
            }
        }

        if (m_mode == Mode::Relative)
        {
            if (isNonCenterTouched || !anyTouched)
            {
                m_targetingBipolarZero = false;
            }
            
            if (IsBipolar() &&
                (m_targetingBipolarZero || (isCenterTouched && !isNonCenterTouched)))
            {
                // Make sure zero is easy to get at for bipolar faders.
                //
                m_target = 0;
                m_speed = static_cast<FaderCell*>(Get(0, m_height / 2))->GetNonReducedSpeed();
                m_moving = true;
                m_targetingBipolarZero = true;
            }
            else if (!IsBipolar() && isCenterTouched && !isEven)
            {
                // For odd size relative unipolar faders, the middle button is auto-stop!
                //
                m_moving = false;
            }
            else if (speed != 0)
            {
                m_speed = std::abs(speed);
                m_moving = true;
                if (speed > 0)
                {
                    m_target = 1;
                }
                else if (IsBipolar())
                {
                    m_target = -1;
                }
                else
                {
                    m_target = 0;
                }
            }
            else
            {
                m_moving = false;
            }
        }
    }    
    
    Fader(
        float* state,
        int height,
        Color color,
        float minValue = 0,
        float maxValue = 1,
        float minSpeed = 1,
        float maxSpeed = 100,
        bool pressureSensitive = true,
        Structure structure = Structure::Linear,
        Mode mode = Mode::Relative)
        : m_height(height)
        , m_color(color)
        , m_minSpeed(minSpeed)
        , m_maxSpeed(maxSpeed)
        , m_maxValue(maxValue)
        , m_minValue(minValue)
        , m_state(state)
        , m_speed(0)
        , m_moving(false)
        , m_targetingBipolarZero(false)
        , m_isBipolarZero(false)
        , m_pressureSensitive(pressureSensitive)
        , m_structure(structure)
        , m_mode(mode)
    {
        RemoveGridId();
        for (int i = 0; i < height; ++i)
        {
            Put(0, i, new FaderCell(this, height - i - 1));
        }

        if (IsExponential())
        {
            m_logMaxOverMin = std::log2f(m_maxValue / m_minValue);
        }

        m_lastAbsState = *m_state;

        ComputeFaderPos(Normalize(*m_state));
    }

    float m_valueWithinCell;
    size_t m_posFromBottom;
    int m_posFromCenter;
    
    void ComputeFaderPos(float normState)
    {
        if (IsBipolar())
        {
            size_t h = m_height / 2;
            float perBlock = std::abs(h * normState);
            float posFromCenter = std::max<size_t>(0, std::min<size_t>(h - 1, static_cast<size_t>(perBlock)));
            m_isBipolarZero = false;
            if (normState < 0)
            {
                m_posFromBottom = h - posFromCenter - 1;
                m_valueWithinCell = std::max<float>(0, std::min<float>(1, perBlock - m_posFromBottom));
            }
            else if (normState == 0)
            {
                m_isBipolarZero = true;
            }
            else
            {
                if (m_height % 2 == 1)
                {
                    m_posFromBottom = h + posFromCenter + 1;
                }
                else
                {                
                    m_posFromBottom = h + posFromCenter;
                }
            }
            
            m_valueWithinCell = std::max<float>(0, std::min<float>(1, perBlock - posFromCenter));
        }
        else
        {
            float perBlock = m_height * normState;
            m_posFromBottom = std::max<size_t>(0, std::min<size_t>(m_height - 1, static_cast<size_t>(perBlock)));
            m_valueWithinCell = std::max<float>(0, std::min<float>(1, perBlock - m_posFromBottom));
        }

        m_posFromCenter = m_posFromBottom - m_height / 2;
        if (m_height % 2 == 0 && m_posFromBottom >= m_height / 2)
        {
            ++m_posFromCenter;
        }
    }

    virtual void Process(float dt) override
    {
        if (m_moving)
        {
            float dx = dt * m_speed;
            float normState = Normalize(*m_state);
            if (std::abs(normState - m_target) < dx)
            {
                normState = m_target;
                *m_state = DeNormalize(m_target);
                m_moving = false;
            }
            else if (normState < m_target)
            {
                normState += dx;
                *m_state = DeNormalize(normState);
            }
            else
            {
                normState -= dx;
                *m_state = DeNormalize(normState);
            }

            m_lastAbsState = *m_state;            
            ComputeFaderPos(normState);           
        }
        else if (*m_state != m_lastAbsState)
        {
            float normState = Normalize(*m_state);
            ComputeFaderPos(normState);           
            m_lastAbsState = *m_state;
        }
    }    
};

struct LPRGBSysEx
{
    static constexpr size_t x_maxMessages = x_gridMaxSize * x_gridMaxSize;
    struct ColorSpec
    {
        uint8_t m_pos;
        Color m_color;
    };

    ColorSpec m_messages[x_maxMessages];
    size_t m_numMessages;
    ControllerShape m_shape;

    LPRGBSysEx(ControllerShape shape)
        : m_numMessages(0)
        , m_shape(shape)
    {
    }

    void Add(uint8_t pos, Color c)
    {
        m_messages[m_numMessages].m_pos = pos;
        m_messages[m_numMessages].m_color = c;
        ++m_numMessages;
    }

    bool Empty()
    {
        return m_numMessages == 0;
    }

    void Populate(midi::Message& msg, int64_t frame)
    {
        msg.setFrame(frame);
        msg.bytes.clear();
        msg.bytes.reserve(8 + 5 * m_numMessages + 1);
        
        msg.bytes.push_back(240);
        msg.bytes.push_back(0);
        msg.bytes.push_back(32);
        msg.bytes.push_back(41);
        msg.bytes.push_back(2);
        
        if (m_shape == ControllerShape::LaunchPadX)
        {
            msg.bytes.push_back(12);
        }
        else if (m_shape == ControllerShape::LaunchPadProMk3)
        {
            msg.bytes.push_back(14);
        }

        msg.bytes.push_back(3);

        for (size_t i = 0; i < m_numMessages; ++i)
        {
            msg.bytes.push_back(3);
            msg.bytes.push_back(m_messages[i].m_pos);
            msg.bytes.push_back(m_messages[i].m_color.m_red / 2);
            msg.bytes.push_back(m_messages[i].m_color.m_green / 2);
            msg.bytes.push_back(m_messages[i].m_color.m_blue / 2);
        }
        
        msg.bytes.push_back(247);
    }
};

#ifndef SMART_BOX
struct MidiInterchangeSingle
{    
    midi::InputQueue m_input;
    midi::Output m_output;
    int m_device;
    bool m_isLoopback;

    static constexpr size_t x_maxNote = 128;

    Color m_lastSent[x_maxNote];
    ControllerShape m_shape;
    int m_frameSinceLastReceive;

    void SetShape(ControllerShape shape)
    {
        m_shape = shape;
    }

    MidiInterchangeSingle()
        : MidiInterchangeSingle(true)
    {
    }
    
    MidiInterchangeSingle(bool isLoopback)
        : m_device(-1)
        , m_isLoopback(isLoopback)
        , m_shape(ControllerShape::LaunchPadX)
        , m_frameSinceLastReceive(0)
    {
        if (m_isLoopback)
        {
            Init();
        }

        ClearLastSent();
    }
    
    static constexpr int x_loopbackId = -12;

    bool IsActive()
    {
        return !m_isLoopback || m_device >= 0;
    }
    
    void Init()
    {
        m_input.setDriverId(x_loopbackId);
        m_output.setDriverId(x_loopbackId);
    }

    void ClearLastSent()
    {
        for (size_t i = 0; i < x_maxNote; ++i)
        {
            m_lastSent[i] = Color::InvalidColor;
        }
    }

    void SetLoopbackDevice(int device, int64_t frame)
    {
        if (m_device == device)
        {
            return;
        }
        
        if (IsActive())
        {
            Drain(frame);
        }

        m_device = device;

        m_input.setDeviceId(2 * device);
        m_output.setDeviceId(2 * device + 1);

        if (IsActive())
        {
            Drain(frame);
        }        
    }

    void SetInactive(int64_t frame)
    {
        SetLoopbackDevice(-1, frame);
    }

    void PrepareSysExSend(Message& message, LPRGBSysEx& sysEx)
    {
        if (!IsActive())
        {
            return;
        }

        if (!message.ShapeSupports(m_shape))
        {
            return;
        }
        
        uint8_t pos = Message::LPPosToNote(message.m_x, message.m_y);

        if (m_lastSent[pos] == Color::InvalidColor || m_lastSent[pos] != message.m_color)
        {
            m_lastSent[pos] = message.m_color;
            sysEx.Add(pos, message.m_color);
        }
    }
    
    Message Pop(int64_t frame)
    {
        if (!IsActive())
        {
            return Message();
        }
        
        midi::Message msg;
        while (true)
        {
            if (!m_input.tryPop(&msg, frame))
            {
                return Message();
            }

            Message ret = Message::Decode(m_shape, msg);
            if (!ret.NoMessage())
            {
                m_frameSinceLastReceive = 0;
                return ret;
            }
        }
    }

    void Drain(int64_t frame)
    {
        midi::Message msg;
        while (true)
        {
            if (!m_input.tryPop(&msg, frame))
            {
                return;
            }
        }        
    }

    void ApplyMidiToBus(int64_t frame, size_t gridId)
    {
        if (gridId != x_numGridIds)
        {
            while (true)
            {
                Message msg = Pop(frame);
                if (msg.NoMessage())
                {
                    break;
                }

                if (msg.m_mode == Message::Mode::Increment)
                {
                    int currentVelocity = static_cast<int8_t>(SmartBusGetVelocity(gridId, msg.m_x, msg.m_y));
                    int delta = msg.m_velocity - 64;
                    int newVelocity = std::max(-128, std::min(127, currentVelocity + delta));
                    SmartBusPutVelocity(gridId, msg.m_x, msg.m_y, newVelocity);
                }
                else
                {
                    SmartBusPutVelocity(gridId, msg.m_x, msg.m_y, msg.m_velocity);
                }
            }

            if (m_frameSinceLastReceive == 2 &&
                m_shape == ControllerShape::MidiFighterTwister)
            {
                SmartBusClearVelocities(gridId);
            }
            
            ++m_frameSinceLastReceive;
        }
    }

    void ApplyMidi(int64_t frame, AbstractGrid* grid)
    {
        while (true)
        {
            Message msg = Pop(frame);
            if (msg.NoMessage())
            {
                break;
            }

            grid->Apply(msg);
        }
    }

    void SendMidiFromBus(int64_t frame, size_t gridId, uint64_t* epoch)
    {
        if (m_shape == ControllerShape::MidiFighterTwister)
        {
            SendMidiFromBusMessages(frame, gridId, epoch);
        }
        else
        {
            SendMidiFromBusSysEx(frame, gridId, epoch);
        }
    }

    void SendMidiFromBusSysEx(int64_t frame, size_t gridId, uint64_t* epoch)
    {
        if (gridId != x_numGridIds)
        {
            LPRGBSysEx sysEx(m_shape);
            for (auto itr = SmartBusOutputBegin(gridId, epoch); itr != SmartBusOutputEnd(); ++itr)
            {
                Message msg = *itr;
                if (!msg.NoMessage())
                {
                    PrepareSysExSend(msg, sysEx);
                }
            }

            if (!sysEx.Empty())
            {
                midi::Message msg;
                sysEx.Populate(msg, frame);
                m_output.sendMessage(msg);
            }
        }
    }

    void SendMidiFromBusMessages(int64_t frame, size_t gridId, uint64_t* epoch)
    {
        if (gridId != x_numGridIds)
        {
            for (auto itr = SmartBusOutputBegin(gridId, epoch); itr != SmartBusOutputEnd(); ++itr)
            {
                Message msg = *itr;
                if (!msg.NoMessage() && msg.ShapeSupports(m_shape))
                {
                    midi::Message midiMsg[3];
                    msg.Populate(m_shape, frame, midiMsg);
                    int channel = m_output.getChannel();
                    
                    m_output.setChannel(midiMsg[0].getChannel());
                    m_output.sendMessage(midiMsg[0]);
                    m_output.setChannel(midiMsg[1].getChannel());
                    m_output.sendMessage(midiMsg[1]);
                    m_output.setChannel(midiMsg[2].getChannel());
                    m_output.sendMessage(midiMsg[2]);

                    m_output.setChannel(channel);
                }
            }
        }
    }

    void SendMidi(int64_t frame, AbstractGrid* grid)
    {
        LPRGBSysEx sysEx(m_shape);
        for (int i = x_gridXMin; i < x_gridXMax; ++i)
        {
            for (int j = x_gridYMin; j < x_gridYMax; ++j)
            {
                Color c = grid->GetColor(i, j);
                Message msg(i, j, c);
                PrepareSysExSend(msg, sysEx);
            }
        }

        if (!sysEx.Empty())
        {
            midi::Message msg;
            sysEx.Populate(msg, frame);
            m_output.sendMessage(msg);
        }
    }
};

struct MidiInterchange
{
    MidiInterchangeSingle m_channels[x_maxChannels];
    size_t m_numChannels;
    size_t m_numActive;
    
    MidiInterchange()
        : m_numChannels(0)
        , m_numActive(0)
    {
    }

    struct Input
    {
        size_t m_numChannels;
        int m_channels[x_maxChannels];

        Input()
        {
            m_numChannels = 0;
            for (size_t i = 0; i < x_maxChannels; ++i)
            {
                m_channels[i] = -1;
            }
        }
    };

    void ProcessActive(Input& input, int64_t frame)
    {
        if (input.m_numChannels != m_numChannels)
        {
            for (size_t i = std::min(input.m_numChannels, m_numChannels); i < std::max(input.m_numChannels, m_numChannels); ++i)
            {
                m_channels[i].SetInactive(frame);
            }                     
        }

        m_numChannels = input.m_numChannels;
        m_numActive = 0;
        for (size_t i = 0; i < m_numChannels; ++i)
        {
            m_channels[i].SetLoopbackDevice(i, frame);
            if (m_channels[i].IsActive())
            {
                ++m_numActive;
            }
        }
    }
};

struct SmartGrid
{
    MidiInterchange m_midi;
    std::vector<std::shared_ptr<AbstractGrid>> m_grids;
    GridHolder m_gridHolder;
    float m_timeToCheck;
    static constexpr float x_checkTime = 0.01;

    struct Input
    {
        MidiInterchange::Input m_midiSwitchInput;
    };

    Input m_state;

    SmartGrid() :
        m_timeToCheck(-1)
    {
    }

    void AddToplevelGrid(AbstractGrid* grid)
    {
        AddToplevelGrid(std::shared_ptr<AbstractGrid>(grid));
    }

    void AddToplevelGrid(std::shared_ptr<AbstractGrid> grid)
    {
        m_grids.push_back(grid);
        m_gridHolder.AddGrid(grid);
    }

    size_t AddGrid(AbstractGrid* grid)
    {
        return m_gridHolder.AddGrid(grid);
    }

    size_t AddGrid(std::shared_ptr<AbstractGrid> grid)
    {
        return m_gridHolder.AddGrid(grid);
    }

    void ApplyMidi(int64_t frame)
    {
        for (size_t i = 0; i < m_midi.m_numActive; ++i)
        {
            m_midi.m_channels[i].ApplyMidi(frame, m_grids[i].get());
        }
    }

    void SendMidi(int64_t frame)
    {
        for (size_t i = 0; i < m_midi.m_numActive; ++i)
        {
            m_midi.m_channels[i].SendMidi(frame, m_grids[i].get());
        }
    }
    
    void BeforeModuleProcess(float dt, int64_t frame)
    {
        m_timeToCheck -= dt;
        if (m_timeToCheck < 0)
        {
            m_midi.ProcessActive(m_state.m_midiSwitchInput, frame);
            ApplyMidi(frame);
        }

        m_gridHolder.Process(dt);        
    }

    void AfterModuleProcess(int64_t frame)
    {
        if (m_timeToCheck < 0)
        {            
            SendMidi(frame);
            m_timeToCheck = x_checkTime;
        }
    }
};

#endif

}

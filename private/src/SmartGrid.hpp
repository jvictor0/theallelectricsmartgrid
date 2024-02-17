#pragma once
#include "plugin.hpp"
#include <memory>
#include <cstddef>
#include <cmath>
#include <algorithm>
#include <vector>
#include <atomic>

namespace SmartGrid
{

static uint8_t x_maxVelocity = 127;
static constexpr size_t x_maxChannels = 8;

struct Color
{
    constexpr Color()
        : m_color(0)
    {
    }
    
    constexpr Color(uint8_t c) : m_color(c)
    {
    }

    bool operator == (const Color& c)
    {
        return m_color == c.m_color;
    }

    bool operator != (const Color& c)
    {
        return m_color != c.m_color;
    }
   
    uint8_t m_color;

    static Color Off;
    static Color Dim;
    static Color Grey;
    static Color White;
    static Color Red;
    static Color Orange;
    static Color Yellow;
    static Color Green;
    static Color SeaGreen;
    static Color Ocean;
    static Color Blue;
    static Color Fuscia;
    static Color Indigo;
    static Color Purple;
    static Color DimPurple;
};

struct ColorScheme
{
    Color back()
    {
        return m_colors.back();
    }

    Color operator[] (size_t ix)
    {
        return m_colors[ix];
    }

    size_t size()
    {
        return m_colors.size();
    }

    ColorScheme(const std::vector<Color>& colors)
        : m_colors(colors)
    {
    }

    static ColorScheme Hues(Color c)
    {
        uint8_t cbit = c.m_color;
        return ColorScheme(std::vector<Color>({cbit + 2, cbit + 1, cbit}));
    }
    
    std::vector<Color> m_colors;

    static ColorScheme Whites;
    static ColorScheme Rainbow;
    static ColorScheme Reds;
    static ColorScheme Greens;
    static ColorScheme Blues;
    static ColorScheme RedHues;
    static ColorScheme GreenHues;
    static ColorScheme BlueHues;
};
    
struct Cell
{
    virtual ~Cell()
    {
    }

    bool m_pressureSensitive;
    bool m_velocity;

    Cell()
    {
        m_pressureSensitive = false;
        m_velocity = false;
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
            case Mode::SetOnly:
            {
                *m_state = m_myState;
                break;
            }
            case Mode::Toggle:
            {
                *m_state = (*m_state) == m_myState ? m_offState : m_myState;
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

template<class StateClass>
struct CycleCell : Cell
{
    ColorScheme m_colors;
    StateClass* m_state;

    virtual ~CycleCell()
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
        return m_colors[static_cast<size_t>(*m_state)];
    }

    virtual void OnPress(uint8_t) override
    {
        Cycle();
    }    
};

static constexpr int x_gridSize = 8;
static constexpr int x_gridXMin = -1;
static constexpr int x_gridXMax = 9;
static constexpr int x_gridYMin = -2;
static constexpr int x_gridYMax = 9;
static constexpr size_t x_gridMaxSize = 11;

struct Message
{
    enum class Mode : int
    {
        NoMessage,
        Note,
        Color
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
        , m_mode(velocity == 0xFF ? Mode::NoMessage : Mode::Note)
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
        , m_mode(velocity == 0xFF ? Mode::NoMessage : Mode::Note)
    {
    }
    
    Message(Cell* cell, int x, int y)
        : m_x(x)
        , m_y(y)
    {
        m_color = cell->GetColor();
        m_mode = Mode::Color;
    }
    
    bool NoMessage()
    {
        return m_mode == Mode::NoMessage;
    }
    
    static uint8_t LPPosToNote(int x, int y)
    {
        return 11 + 10 * y + x;
    }
    
    static std::pair<int, int> NoteToLPPos(uint8_t note)
    {
        int y = (note - 11) / 10;
        int x = (note - 11) % 10;
        return std::make_pair(x, y);
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
    
    void PopulateMessage(midi::Message& msg)
    {
        msg.setStatus(0xb);        
		msg.setNote(LPPosToNote(m_x, m_y));
		msg.setValue(m_color.m_color);
        msg.setChannel(1);
    }
};

static constexpr size_t x_numGridIds = 128;

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

extern GridIdAllocator g_gridIds;

struct AbstractGrid
{
    size_t m_gridId;
    
    virtual ~AbstractGrid()
    {
        if (m_gridId != x_numGridIds)
        {
            g_gridIds.Free(m_gridId);
        }
    }
    
    static constexpr float x_busIOInterval = 0.05;
    float m_timeToNextBusIO;
    
    AbstractGrid()
    {
        m_gridId = g_gridIds.Alloc();
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

    void OutputToBus();
    void ApplyFromBus(bool ignoreChanged);
    
    virtual void Apply(Message msg) = 0;
    virtual Color GetColor(int i, int j) = 0;
    
    virtual void Process(float dt)
    {
    }

    void ProcessStatic(float dt)
    {
        Process(dt);

        m_timeToNextBusIO -= dt;
        if (m_timeToNextBusIO <= 0)
        {
            m_timeToNextBusIO = x_busIOInterval;
            ApplyFromBus(false /*ignoreChanged*/);
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

    virtual ~Grid()
    {
    }
    
    std::shared_ptr<Cell>& GetShared(size_t i, size_t j)
    {
        return m_grid[i + 1][j + 2];
    }

    Cell* Get(size_t i, size_t j)
    {
        return m_grid[i + 1][j + 2].get();        
    }       

    void Put(int i, int j, Cell* cell)
    {
        m_grid[i + 1][j + 2].reset(cell);
    }

    void Put(int i, int j, std::shared_ptr<Cell> cell)
    {
        m_grid[i + 1][j + 2] = cell;
    }

    virtual Color GetColor(int i, int j) override
    {
        return Get(i, j) ? Get(i, j)->GetColor() : Color::Off;
    }

    virtual void Apply(Message msg) override
    {
        if (msg.m_mode == Message::Mode::Note)
        {
            if (Get(msg.m_x, msg.m_y))
            {
                if (msg.m_velocity == 0)
                {
                    Get(msg.m_x, msg.m_y)->OnReleaseStatic();
                }
                else
                {
                    Get(msg.m_x, msg.m_y)->OnPressStatic(msg.m_velocity);
                }
            }
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
        for (int i = x_gridXMin; i < x_gridXMax; ++i)
        {
            for (int j = x_gridYMin; j < x_gridYMax; ++j)
            {
                if (grid->Get(i, j))
                {
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

    size_t GridId(size_t ix)
    {
        return Get(ix)->m_gridId;
    }

    void Process(float dt)
    {
        for (std::shared_ptr<AbstractGrid>& grid : m_grids)
        {
            grid->ProcessStatic(dt);
        }
    }    
};

struct GridSwitcher : AbstractGrid
{
    size_t m_gridId;
    size_t m_lastGridId;
    std::shared_ptr<AbstractGrid> m_menuGrid;

    virtual ~GridSwitcher()
    {
    }
    
    GridSwitcher(AbstractGrid* menu)
        : m_gridId(x_numGridIds)
        , m_lastGridId(x_numGridIds)
        , m_menuGrid(std::shared_ptr<AbstractGrid>(menu))
    {
    }

    GridSwitcher(std::shared_ptr<AbstractGrid> menu)
        : m_gridId(x_numGridIds)
        , m_lastGridId(x_numGridIds)
        , m_menuGrid(menu)
    {
    }

    virtual void Apply(Message msg) override
    {
        m_menuGrid->Apply(msg);
        if (m_gridId != x_numGridIds)
        {
            g_smartBus.PutVelocity(m_gridId, msg.m_x, msg.m_y, msg.m_velocity);
        }
    }

    virtual Color GetColor(int i, int j) override
    {
        Color c = m_menuGrid->GetColor(i, j);
        if (c != Color::Off)
        {
            return c;
        }
        
        if (m_gridId != x_numGridIds)
        {
            return g_smartBus.GetColor(m_gridId, i, j);
        }
        else
        {
            return Color::Off;
        }
    }

    virtual void Process(float dt) override
    {
        m_menuGrid->ProcessStatic(dt);

        if (m_lastGridId != m_gridId)
        {
            if (m_lastGridId != x_numGridIds)
            {
                g_smartBus.ClearVelocities(m_lastGridId);
            }
            
            m_lastGridId = m_gridId;
        }
    }
};

struct GridUnion
{
    std::vector<std::shared_ptr<Grid>> m_grids;
    std::vector<std::shared_ptr<Grid>> m_menuGrids;
    size_t m_pageIx[x_maxChannels];
    size_t m_lastPageIx[x_maxChannels];
    size_t m_numChannels;

    GridUnion()
        : m_numChannels(0)
    {
    }        
    
    void AddGrid(Grid* grid)
    {
        AddGrid(std::shared_ptr<Grid>(grid));
    }

    void AddGrid(std::shared_ptr<Grid> grid)
    {
        m_grids.push_back(grid);
    }

    void AddMenu(Grid* grid)
    {
        AddMenu(std::shared_ptr<Grid>(grid));
    }

    void AddMenu(std::shared_ptr<Grid> grid)
    {
        m_menuGrids.push_back(grid);
        m_pageIx[m_numChannels] = 0;
        m_lastPageIx[m_numChannels] = 0;
        ++m_numChannels;
    }

    Grid* GetPage(size_t channel)
    {
        return m_grids[m_pageIx[channel]].get();
    }

    Grid* GetMenu(size_t channel)
    {
        return m_menuGrids[channel].get();
    }

    void Process(float dt)
    {
        for (std::shared_ptr<Grid>& grid : m_grids)
        {
            grid->ProcessStatic(dt);
        }

        for (size_t i = 0; i < m_numChannels; ++i)
        {
            // Send an 'OnRelease' to any pushed down cell on page change.
            //
            if (m_lastPageIx[i] != m_pageIx[i])
            {
                m_lastPageIx[i] = m_pageIx[i];
                GetPage(i)->AllOff();
            }
        }
    }    
};

struct Fader : public Grid
{
    virtual ~Fader()
    {
    }
    
    size_t m_height;
    
    ColorScheme m_colorScheme;
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
            if (m_owner->IsBipolar() &&
                (m_fromCenter == 0 ||
                 (std::abs<int>(m_fromCenter) < std::abs<int>(m_owner->m_posFromCenter) &&
                  (m_fromCenter > 0) == (m_owner->m_posFromCenter > 0))))
            {
                return m_owner->m_colorScheme.back();
            }
            else if (!m_owner->IsBipolar() &&
                     m_fromBottom < m_owner->m_posFromBottom)
            {
                return m_owner->m_colorScheme.back();
            }
            else if (m_fromBottom == m_owner->m_posFromBottom)
            {
                size_t ix = static_cast<size_t>(m_owner->m_valueWithinCell * m_owner->m_colorScheme.size());
                ix = std::max<size_t>(0, std::min(m_owner->m_colorScheme.size() - 1, ix));
                return m_owner->m_colorScheme[ix];
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
        if (isCenterTouched && isEven)
        {
            isCenterTouched = Get(0, m_height / 2 - 1)->IsPressed();
        }
        
        for (size_t i = 0; i < m_height; ++i)
        {            
            FaderCell* cell = static_cast<FaderCell*>(Get(0, i));
            if (cell->IsPressed())
            {
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
            if (IsBipolar() && isCenterTouched && !isNonCenterTouched)
            {
                // Make sure zero is easy to get at for bipolar faders.
                //
                m_target = 0;
                m_speed = static_cast<FaderCell*>(Get(0, m_height / 2))->GetNonReducedSpeed();
                m_moving = true;
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
        ColorScheme colorScheme,
        float minValue = 0,
        float maxValue = 1,
        float minSpeed = 1,
        float maxSpeed = 100,
        bool pressureSensitive = true,
        Structure structure = Structure::Linear,
        Mode mode = Mode::Relative)
        : m_height(height)
        , m_colorScheme(colorScheme)
        , m_minSpeed(minSpeed)
        , m_maxSpeed(maxSpeed)
        , m_maxValue(maxValue)
        , m_minValue(minValue)
        , m_state(state)
        , m_speed(0)
        , m_moving(false)
        , m_pressureSensitive(pressureSensitive)
        , m_structure(structure)
        , m_mode(mode)
    {
        for (int i = 0; i < height; ++i)
        {
            Put(0, i, new FaderCell(this, i));
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
            if (normState < 0)
            {
                m_posFromBottom = h - posFromCenter - 1;
                m_valueWithinCell = std::max<float>(0, std::min<float>(1, perBlock - m_posFromBottom));
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

struct MidiInterchangeSingle
{    
    midi::InputQueue m_input;
    midi::Output m_output;
    int m_device;

    static constexpr size_t x_maxNote = 128;

    uint8_t m_lastSent[x_maxNote];

    MidiInterchangeSingle()
        : m_device(-1)
    {
        Init();
    }
    
    static constexpr int x_loopbackId = -12;

    bool IsActive()
    {
        return m_device >= 0;
    }
    
    void Init()
    {
        m_input.setDriverId(x_loopbackId);
        m_output.setDriverId(x_loopbackId);
        ClearLastSent();
    }

    void ClearLastSent()
    {
        for (size_t i = 0; i < x_maxNote; ++i)
        {
            m_lastSent[i] = 0xFF;
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

    void Send(Message& message, int64_t frame)
    {
        if (!IsActive())
        {
            return;
        }

        midi::Message msg;
        message.PopulateMessage(msg);
        msg.setFrame(frame);

        if (m_lastSent[msg.getNote()] == 0xFF || m_lastSent[msg.getNote()] != message.m_velocity)
        {
            m_output.sendMessage(msg);
            m_lastSent[msg.getNote()] = message.m_color.m_color;
        }
    }

    void SendIfSet(Message& message, int64_t frame)
    {
        if (!IsActive())
        {
            return;
        }

        midi::Message msg;
        message.PopulateMessage(msg);
        msg.setFrame(frame);
        
        if (m_lastSent[msg.getNote()] != 0xFF)
        {
            m_output.sendMessage(msg);
            m_lastSent[msg.getNote()] = message.m_color.m_color;
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

            Message ret = Message::FromLPMidi(msg);
            if (!ret.NoMessage())
            {
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

    void SendMidi(int64_t frame, AbstractGrid* grid)
    {
        for (int i = x_gridXMin; i < x_gridXMax; ++i)
        {
            for (int j = x_gridYMin; j < x_gridYMax; ++j)
            {
                Color c = grid->GetColor(i, j);
                Message msg(i, j, c);
                Send(msg, frame);
            }
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

    void ApplyMidi(int64_t frame, GridUnion* grids)
    {
        for (size_t i = 0; i < m_numActive; ++i)
        {
            Grid* grid = grids->GetPage(i);
            Grid* menu = grids->GetMenu(i);
            while (true)
            {
                Message msg = m_channels[i].Pop(frame);
                if (msg.NoMessage())
                {
                    break;
                }

                grid->Apply(msg);
                menu->Apply(msg);
            }
        }
    }

    void SendMidi(int64_t frame, GridUnion* grids)
    {
        for (size_t i = 0; i < m_numActive; ++i)
        {
            Grid* grid = grids->GetPage(i);
            Grid* menu = grids->GetMenu(i);
            for (int j = x_gridXMin; j < x_gridXMax; ++j)
            {
                for (int k = x_gridYMin; k < x_gridYMax; ++k)
                {                    
                    Cell* cell = menu->Get(j, k);
                    if (cell)
                    {
                        Message msg(cell, j, k);
                        m_channels[i].Send(msg, frame);
                    }
                    else
                    {
                        cell = grid->Get(j, k);
                        if (cell)
                        {
                            Message msg(cell, j, k);
                            m_channels[i].Send(msg, frame);
                        }
                        else
                        {
                            Message msg = Message::Off(j, k);
                            m_channels[i].SendIfSet(msg, frame);
                        }                        
                    }
                }
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

    void AddGrid(AbstractGrid* grid)
    {
        m_gridHolder.AddGrid(grid);
    }

    void AddGrid(std::shared_ptr<AbstractGrid> grid)
    {
        m_gridHolder.AddGrid(grid);
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

}

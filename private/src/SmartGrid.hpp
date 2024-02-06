#pragma once
#include "plugin.hpp"
#include <memory>
#include <cstddef>
#include <cmath>
#include <algorithm>
#include <vector>

namespace SmartGrid
{

static uint8_t x_maxVelocity = 127;
static constexpr size_t x_maxChannels = 8;

struct Color
{
    Color(uint8_t c) : m_color(c)
    {
    }

    bool operator == (const Color& c)
    {
        return m_color == c.m_color;
    }
    
    uint8_t m_color;

    static Color Off;
    static Color Dim;
    static Color White;
    static Color Red;
    static Color Yellow;
    static Color Green;
    static Color SeaGreen;
    static Color Ocean;
    static Color Blue;
    static Color Fuscia;
    static Color Magenta;
    static Color Purple;
    static Color DimPurple;
};

Color Color::Off(0);
Color Color::Dim(1);
Color Color::White(3);
Color Color::Red(5);
Color Color::Yellow(13);
Color Color::Green(21);
Color Color::SeaGreen(25);
Color Color::Ocean(33);
Color Color::Blue(45);
Color Color::Fuscia(53);
Color Color::Magenta(80);
Color Color::Purple(81);
Color Color::DimPurple(82);

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
    
    std::vector<Color> m_colors;

    static ColorScheme Whites;
};

ColorScheme ColorScheme::Whites(std::vector<Color>({1, 2, 3}));

struct Cell
{
    virtual ~Cell()
    {
        m_pressureSensitive = false;
    }

    bool m_pressureSensitive;
    
    virtual Color GetColor()
    {
        return Color::Off;
    }

    virtual void OnPress(uint8_t velocity)
    {
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
            OnPressureChange(velocity);
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

struct GridBounds
{
    static constexpr int x_bigNum = 100;

    GridBounds(int xStart, int yStart, int xEnd, int yEnd)
        : m_xStart(xStart)
        , m_yStart(yStart)
        , m_xEnd(xEnd)
        , m_yEnd(yEnd)
    {
    }

    GridBounds(int xEnd, int yEnd)
        : GridBounds(0, 0, xEnd, yEnd)
    {
    }

    GridBounds()
        : GridBounds(x_bigNum, x_bigNum, -x_bigNum, -x_bigNum)
    {
    }

    GridBounds Expand(GridBounds other)
    {
        return GridBounds(
            std::min(m_xStart, other.m_xStart),
            std::min(m_yStart, other.m_yStart),
            std::max(m_xEnd, other.m_xEnd),
            std::max(m_yEnd, other.m_yEnd));
    }
    
    int m_xStart;
    int m_yStart;
    int m_xEnd;
    int m_yEnd;
};

struct Grid
{
    static constexpr size_t x_maxSize = 12;
    bool m_needsProcess;
    GridBounds m_bounds;

    std::shared_ptr<Cell> m_grid[x_maxSize][x_maxSize];

    virtual ~Grid()
    {
    }
    
    Grid()
    {
    }

    Grid(int width, int height)
        : Grid(0, 0, width, height)
    {
    }

    Grid(int xStart, int yStart, int xEnd, int yEnd)
        : m_bounds(xStart, yStart, xEnd, yEnd)
    {
        m_needsProcess = false;
    }

    int XStart()
    {
        return m_bounds.m_xStart;
    }
    
    int YStart()
    {
        return m_bounds.m_yStart;
    }
    
    int XEnd()
    {
        return m_bounds.m_xEnd;
    }
    
    int YEnd()
    {
        return m_bounds.m_yEnd;
    }

    size_t Width()
    {
        return XEnd() - XStart();
    }

    size_t Height()
    {
        return YEnd() - YStart();
    }
    
    void SetNeedsProcess()
    {
        m_needsProcess = true;
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

    Color GetColor(int i, int j)
    {
        return Get(i, j) ? Get(i, j)->GetColor() : Color::Off;
    }

    virtual void Process(float dt)
    {
    }

    void ProcessStatic(float dt)
    {
        if (m_needsProcess)
        {
            Process(dt);
        }
    }
};

struct CompositeGrid : public Grid
{
    virtual ~CompositeGrid()
    {
    }

    CompositeGrid(int xEnd, int yEnd)
        : CompositeGrid(0, 0, xEnd, yEnd)
    {
    }
    
    CompositeGrid(int xStart, int yStart, int xEnd, int yEnd)
        : Grid(xStart, yStart, xEnd, yEnd)
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
        for (int i = grid->XStart(); i < grid->XEnd(); ++i)
        {
            for (int j = grid->YStart(); j < grid->YEnd(); ++j)
            {
                if (grid->Get(i, j))
                {
                    Put(i - xOff, j - yOff, grid->GetShared(i, j));
                }
            }
        }

        if (m_grids.back()->m_needsProcess)
        {
            SetNeedsProcess();
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

struct GridUnion
{
    std::vector<std::shared_ptr<Grid>> m_grids;
    std::vector<std::shared_ptr<Grid>> m_menuGrids;
    size_t m_pageIx[x_maxChannels];
    size_t m_numChannels;
    GridBounds m_unionBounds;
    bool m_needsProcess;

    GridUnion()
        : m_numChannels(0)
        , m_needsProcess(false)
    {
    }        
    
    void AddGrid(Grid* grid)
    {
        AddGrid(std::shared_ptr<Grid>(grid));
    }

    int XStart()
    {
        return m_unionBounds.m_xStart;
    }
    
    int XEnd()
    {
        return m_unionBounds.m_xEnd;
    }

    int YStart()
    {
        return m_unionBounds.m_yStart;
    }
    
    int YEnd()
    {
        return m_unionBounds.m_yEnd;
    }
    
    void AddGrid(std::shared_ptr<Grid> grid)
    {
        m_grids.push_back(grid);

        if (m_grids.back()->m_needsProcess)
        {
            m_needsProcess = true;
        }

        m_unionBounds = m_unionBounds.Expand(grid->m_bounds);
    }

    void AddMenu(Grid* grid)
    {
        AddMenu(std::shared_ptr<Grid>(grid));
    }

    void AddMenu(std::shared_ptr<Grid> grid)
    {
        m_menuGrids.push_back(grid);
        m_pageIx[m_numChannels] = 0;
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
        if (m_needsProcess)
        {
            for (std::shared_ptr<Grid>& grid : m_grids)
            {
                grid->ProcessStatic(dt);
            }
        }
    }    
};

struct Fader : public Grid
{
    virtual ~Fader()
    {
    }
    
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
        float m_multiplier;
        bool m_isNoOp;
        bool m_isUpper;
        
        FaderCell(Fader* owner, int fromBottom)
            : m_owner(owner)
            , m_fromBottom(fromBottom)
            , m_multiplier(1)
            , m_isNoOp(false)
            , m_isUpper(false)
        {
            if (m_owner->m_pressureSensitive)
            {
                SetPressureSensitive();
            }

            if (m_owner->m_mode == Mode::Relative)
            {
                if (m_owner->Height() % 2 == 1 && m_fromBottom == m_owner->Height() / 2)
                {
                    m_isNoOp = true;            
                    m_multiplier = 0;
                }
                else if (m_fromBottom < m_owner->Height() / 2)
                {
                    for (size_t i = 0; i < m_fromBottom; ++i)
                    {
                        m_multiplier /= 4;
                    }
                }
                else
                {
                    m_isUpper = true;
                    for (int i = m_owner->Height() - 1; i > static_cast<int>(m_fromBottom); --i)
                    {
                        m_multiplier /= 4;
                    }
                }
            }
        }

        virtual Color GetColor() override
        {
            if (m_owner->IsBipolar())
            {
                return Color::Off;                        
            }
            else
            {
                if (m_fromBottom < m_owner->m_posFromBottom)
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
        }
        
        void SetSpeed(uint8_t velocity)
        {
            m_owner->m_moving = true;
            float frac = m_multiplier * static_cast<float>(velocity) / x_maxVelocity;
            float speed = m_owner->m_minSpeed + (m_owner->m_maxSpeed - m_owner->m_minSpeed) * frac;

            m_owner->m_speed = speed;
        }

        void Stop()
        {
            m_owner->m_moving = false;
        }
        
        virtual void OnPress(uint8_t velocity) override
        {
            if (m_isNoOp)
            {
                return;
            }
            
            switch (m_owner->m_mode)
            {
                case Mode::Relative:
                {
                    if (m_isUpper)
                    {
                        m_owner->m_target = 1;
                    }
                    else
                    {
                        m_owner->m_target = m_owner->IsBipolar() ? -1 : 0;
                    }
                    
                    SetSpeed(velocity);
                    
                    break;
                }
                case Mode::Absolute:
                {
                    break;
                }
            }
        }
        
        virtual void OnRelease() override
        {
            if (m_isNoOp)
            {
                return;
            }
            
            if (m_owner->m_mode == Mode::Relative)
            {
                Stop();
            }
        }

        virtual void OnPressureChange(uint8_t velocity) override
        {
            if (m_owner->m_moving)
            {
                SetSpeed(velocity);
            }
        }
        
        virtual ~FaderCell()
        {
        }
    };
    
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
        : Grid(1 /*width*/, height)
        , m_colorScheme(colorScheme)
        , m_minSpeed(minSpeed)
        , m_maxSpeed(maxSpeed)
        , m_maxValue(maxValue)
        , m_minValue(minValue)
        , m_state(state)
        , m_pressureSensitive(pressureSensitive)
        , m_structure(structure)
        , m_mode(mode)
    {
        SetNeedsProcess();
        for (int i = YStart(); i < YEnd(); ++i)
        {
            Put(0, i, new FaderCell(this, i - YStart()));
        }

        if (IsExponential())
        {
            m_logMaxOverMin = std::log2f(m_maxValue / m_minValue);
        }

        m_lastAbsState = *m_state;
    }

    float m_valueWithinCell;
    size_t m_posFromBottom;
    
    void ComputeFaderPos(float normState)
    {
        if (IsBipolar())
        {
        }
        else
        {
            float perBlock = Height() * normState;
            m_posFromBottom = std::max<size_t>(0, std::min<size_t>(Height() - 1, static_cast<size_t>(perBlock)));
            m_valueWithinCell = std::max<float>(0, std::min<float>(1, perBlock - m_posFromBottom));
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

struct Message
{
    enum class Mode : int
    {
        NoMessage,
        NoteOn,
        NoteOff,
        Pressure
    };
    
    int m_x;
    int m_y;
    uint8_t m_velocity;
    Mode m_mode;

    Message()
        : m_mode(Mode::NoMessage)
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

    static Message Off(int x, int y)
    {
        return Message(x, y, 0, Mode::NoteOff);
    }

    Message(
        std::pair<int, int> xy,
        uint8_t velocity,
        Mode mode)
        : m_x(xy.first)
        , m_y(xy.second)
        , m_velocity(velocity)
        , m_mode(mode)
    {
    }

    Message(int x, int y, Cell* cell)
        : m_x(x)
        , m_y(y)
    {
        Color c = cell->GetColor();
        if (c == Color::Off)
        {
            m_velocity = 0;
            m_mode = Mode::NoteOff;
        }
        else
        {
            m_velocity = c.m_color;
            m_mode = Mode::NoteOn;
        }
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
                return Message(NoteToLPPos(msg.getNote()), 0, Mode::NoteOff);
			} 
			case 0x9:
            case 0xb:
            {
				if (msg.getValue() > 0)
                {
                    return Message(NoteToLPPos(msg.getNote()), msg.getValue(), Mode::NoteOn);
				}
				else
                {
                    return Message(NoteToLPPos(msg.getNote()), 0, Mode::NoteOff);
    			}
			} 
			case 0xa:
            {
                return Message(NoteToLPPos(msg.getNote()), msg.getValue(), Mode::Pressure);
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
		msg.setValue(m_velocity);
        msg.setChannel(1);
    }

    void Apply(Cell* cell)
    {
        switch (m_mode)
        {
            case Mode::NoteOff:
            {
                cell->OnRelease();
                break;
            }
            case Mode::NoteOn:
            {
                cell->OnPress(m_velocity);
                break;
            }
            case Mode::Pressure:
            {
                cell->OnPressureChange(m_velocity);
                break;
            }
            default:
            {
                break;
            }
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
            m_lastSent[msg.getNote()] = message.m_velocity;
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
                
                Cell* cell = menu->Get(msg.m_x, msg.m_y);
                if (cell)
                {
                    msg.Apply(cell);
                }
                else
                {
                    cell = grid->Get(msg.m_x, msg.m_y);
                    if (cell)
                    {
                        msg.Apply(cell);
                    }                    
                }
            }
        }
    }

    void SendMidi(int64_t frame, GridUnion* grids)
    {
        for (size_t i = 0; i < m_numActive; ++i)
        {
            Grid* grid = grids->GetPage(i);
            Grid* menu = grids->GetMenu(i);
            for (Grid* g : {grid, menu})
            {
                for (int j = g->XStart(); j < g->XEnd(); ++j)
                {
                    for (int k = g->YStart(); k < g->YEnd(); ++k)
                    {
                        Cell* cell = g->Get(j, k);
                        if (cell)
                        {
                            Message msg(j, k, cell);
                            m_channels[i].Send(msg, frame);
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
    GridUnion* m_grids;
    float m_timeToCheck;
    static constexpr float x_checkTime = 0.01;

    struct Input
    {
        MidiInterchange::Input m_midiSwitchInput;
    };

    Input m_state;

    SmartGrid(GridUnion* grids) :
        m_grids(grids),
        m_timeToCheck(-1)
    {
    }

    void BeforeModuleProcess(float dt, int64_t frame)
    {
        m_timeToCheck -= dt;
        if (m_timeToCheck < 0)
        {
            m_midi.ProcessActive(m_state.m_midiSwitchInput, frame);
            m_midi.ApplyMidi(frame, m_grids);
        }

        m_grids->Process(dt);        
    }

    void AfterModuleProcess(int64_t frame)
    {
        if (m_timeToCheck < 0)
        {            
            m_midi.SendMidi(frame, m_grids);
            m_timeToCheck = x_checkTime;
        }
    }
};

}

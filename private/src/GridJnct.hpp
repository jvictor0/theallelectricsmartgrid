#pragma once
#include "SmartGrid.hpp"
#include "PeriodChecker.hpp"
#include "SmartGridWidget.hpp"
#include "StateSaver.hpp"
#include "ColorHelper.hpp"
#ifndef SMART_BOX
#include "MidiWidget.hpp"
#endif

namespace SmartGrid
{

struct MenuButtonRow
{
    enum class RowPos : int
    {
        Top = 0,
        Right = 1,
        Left = 2,
        Bottom = 3,
        SubBottom = 4,
        Unused = 5
    };

    static std::string RowPosToString(RowPos pos)
    {
        switch (pos)
        {
            case RowPos::Top: return "Top";
            case RowPos::Right: return "Right";
            case RowPos::Left: return "Left";
            case RowPos::Bottom: return "Bottom";
            case RowPos::SubBottom: return "SubBottom";
            case RowPos::Unused: return "";
            default: return "";
        }
    }

    RowPos m_pos;
    int m_start;
    int m_end;

    std::shared_ptr<Cell> m_cells[x_baseGridSize];
    bool m_isMenuButton[x_baseGridSize];

    MenuButtonRow()
        : m_pos(RowPos::Unused)
        , m_start(x_baseGridSize)
        , m_end(x_baseGridSize)
    {
        for (size_t i = 0; i < x_baseGridSize; ++i)
        {
            m_isMenuButton[i] = false;
        }
    }    
    
    MenuButtonRow(RowPos pos, int start=0, int end=x_baseGridSize)
        : m_pos(pos)
        , m_start(start)
        , m_end(end)
    {
        for (size_t i = 0; i < x_baseGridSize; ++i)
        {
            m_isMenuButton[i] = false;
        }
    }

    static int GetX(RowPos pos, size_t ix)
    {
        switch (pos)
        {
            case RowPos::Top:
            case RowPos::Bottom:
            case RowPos::SubBottom:
            {
                return ix;
            }
            case RowPos::Right:
            {
                return x_baseGridSize;
            }
            case RowPos::Left:
            case RowPos::Unused:
            {
                return x_gridXMin;
            }
            default: return 0;
        }
    }

    static int GetY(RowPos pos, size_t ix)
    {
        switch (pos)
        {
            case RowPos::Top:
            {
                return x_gridYMin;
            }
            case RowPos::Bottom:
            {
                return x_gridYMax - 2;
            }
            case RowPos::SubBottom:
            case RowPos::Unused:
            {
                return x_gridYMax - 1;
            }
            case RowPos::Right:
            case RowPos::Left:
            {
                return ix;
            }
            default: return 0;
        }
    }

    static RowPos GetPos(int i, int j)
    {
        if (j == x_gridYMin && 0 <= i && i < x_baseGridSize)
        {
            return RowPos::Top;
        }
        else if (j == x_gridYMax - 2 && 0 <= i && i < x_baseGridSize)
        {
            return RowPos::Bottom;
        }
        else if (j == x_gridYMax - 1 && 0 <= i && i < x_baseGridSize)
        {
            return RowPos::SubBottom;
        }
        else if (i == x_baseGridSize && 0 <= j && j < x_baseGridSize)
        {
            return RowPos::Right;
        }
        else if (i == x_gridXMin && 0 <= j && j < x_baseGridSize)
        {
            return RowPos::Left;
        }
        else
        {
            return RowPos::Unused;
        }
    }

    int OnRow(int i, int j)
    {
        switch (m_pos)
        {
            case RowPos::Top:
            {
                return j == x_gridYMin && m_start <= i && i < m_end;
            }
            case RowPos::Bottom:
            {
                return j == x_gridYMax - 2 && m_start <= i && i < m_end;
            }
            case RowPos::SubBottom:
            {
                return j == x_gridYMax - 1 && m_start <= i && i < m_end;
            }
            case RowPos::Right:
            {
                return i == x_baseGridSize && m_start <= j && j < m_end;
            }        
            case RowPos::Left:
            {
                return i == x_gridXMin && m_start <= j && j < m_end;
            }
            case RowPos::Unused:
            default:
            {
                return false;
            }
        }        
    }

    int GetIx(int i, int j)
    {
        if (!OnRow(i, j))
        {
            return -1;
        }
                
        switch (m_pos)
        {
            case RowPos::Top:
            case RowPos::Bottom:
            case RowPos::SubBottom:
            {
                return i;
            }
            case RowPos::Right:
            case RowPos::Left:
            {
                return j;
            }
            case RowPos::Unused:
            default:
            {
                return -1;
            }
        }
    }

    void PutMenuButton(size_t ix, Cell* cell)
    {
        m_cells[ix].reset(cell);
        m_isMenuButton[ix] = true;
    }    

    void Put(size_t ix, Cell* cell)
    {
        m_cells[ix].reset(cell);
        m_isMenuButton[ix] = false;
    }    

    struct Iterator
    {
        RowPos m_pos;
        int m_ix;
        MenuButtonRow* m_owner;
        Iterator(RowPos pos, int ix, MenuButtonRow* owner)
            : m_pos(pos)
            , m_ix(ix)
            , m_owner(owner)
        {
        }

        Cell* operator*() const
        {
            return m_owner->m_cells[m_ix].get();
        }

        Iterator operator++()
        {
            ++m_ix;
            return *this;
        }

        bool operator!=(const Iterator& other)
        {
            return std::tie(m_pos, m_ix) != std::tie(other.m_pos, other.m_ix);
        }
    };

    Iterator begin()
    {
        return Iterator(m_pos, m_start, this);
    }
    
    Iterator end()
    {
        return Iterator(m_pos, m_end, this);
    }

    void Apply(Message msg)
    {
        int ix = GetIx(msg.m_x, msg.m_y);
        if (ix != -1 && m_start <= ix && ix < m_end)
        {
            if (msg.m_velocity > 0)
            {
                m_cells[ix]->OnPressStatic(msg.m_velocity);
            }
            else
            {
                m_cells[ix]->OnReleaseStatic();
            }
        }
    }

    Color GetColor(int i, int j)
    {
        int ix = GetIx(i, j);        
        if (ix != -1 &&
            m_start <= ix &&
            ix < m_end &&
            m_cells[ix])
        {
            return m_cells[ix]->GetColor();
        }
        else
        {
            return Color::Off;
        }
    }
};

struct MenuGrid : public AbstractGrid
{
    static constexpr size_t x_numMenuRows = 5;
    MenuButtonRow m_menuButtonRows[x_numMenuRows];
    static constexpr size_t x_invalidAbsPos = x_numMenuRows * x_baseGridSize;

    virtual ~MenuGrid()
    {
    }

    MenuGrid(StateSaver& saver)
        : m_selectedAbsPos(x_invalidAbsPos)
    {
        RemoveGridId();
        saver.Insert("SelectedAbsPos", &m_selectedAbsPos);
    }

    size_t AbsPos(MenuButtonRow::RowPos pos, size_t ix)
    {
        return static_cast<size_t>(pos) * x_baseGridSize + ix;
    }

    std::pair<MenuButtonRow::RowPos, size_t> PairPos(size_t absPos)
    {
        return std::make_pair(static_cast<MenuButtonRow::RowPos>(absPos / x_baseGridSize), absPos % x_baseGridSize);
    }
    
    struct MenuButton : public Cell
    {
        enum class Mode : int
        {
            Grid,
            Momentary,
            Toggle
        };

        bool m_gateOut;
        bool m_gateOutConnected;
        bool m_hasInput;
        size_t m_gridId;
        size_t m_absPos;
        MenuGrid* m_owner;
        Color m_color;
        Mode m_mode;
        
        struct Input
        {
            bool m_gateOutConnected;
            bool m_hasInput;
            size_t m_gridId;
            Mode m_mode;
            ColorDecode m_color;

            Input()
                : m_gateOutConnected(false)
                , m_hasInput(false)
                , m_gridId(x_numGridIds)
                , m_mode(Mode::Grid)
            {
            }
        };
        
        virtual ~MenuButton()
        {
        }

        MenuButton(MenuGrid* owner, size_t absPos)
            : m_gateOut(false)
            , m_gateOutConnected(false)
            , m_hasInput(false)
            , m_gridId(x_numGridIds)
            , m_absPos(absPos)
            , m_owner(owner)
            , m_color(Color::Off)
            , m_mode(Mode::Grid)
        {
        }

        void Process(Input& input)
        {
            m_gateOutConnected = input.m_gateOutConnected;
            m_mode = input.m_mode;
            m_hasInput = input.m_hasInput;
            if (m_mode == Mode::Grid)
            {
                bool gridIdChanged = input.m_gridId != m_gridId;
                m_gridId = input.m_gridId;
                bool hasGridId = m_gridId != x_numGridIds;
                if (IsSelected() && gridIdChanged && hasGridId)
                {
                    m_owner->Select(this);
                }
                else if (IsSelected() && !hasGridId)
                {
                    m_owner->DeSelect();
                }
            }
            else
            {
                if (IsSelected())
                {
                    m_owner->DeSelect();
                }
                                
                if (input.m_hasInput)
                {
                    m_color = input.m_color.m_color;
                }
            }
        }

        bool IsSelected()
        {
            return m_owner->m_selectedAbsPos == m_absPos;
        }

        virtual Color GetColor() override
        {
            if (m_mode == Mode::Grid)
            {
                if (m_hasInput && m_gridId != x_numGridIds)
                {
                    return IsSelected() ? g_smartBus.GetOnColor(m_gridId) : g_smartBus.GetOffColor(m_gridId);
                }
                else if (m_gateOutConnected)
                {
                    return m_gateOut ? Color::White : Color::White.Dim();
                }
                else
                {
                    return Color::Off;
                }
            }
            else
            {
                if (m_hasInput)
                {
                    return m_color;
                }
                else
                {
                    return m_gateOut ? Color::White : Color::White.Dim();
                }
            }
        }

        virtual void OnPress(uint8_t) override
        {
            if (m_hasInput && m_mode == Mode::Grid)
            {
                if (m_gridId != x_numGridIds)
                {
                    m_owner->Select(this);
                    m_gateOut = true;
                }
            }
            else if (m_gateOutConnected &&
                     (!m_gateOut || m_mode == Mode::Momentary))
            {
                m_gateOut = true;
            }
            else
            {
                m_gateOut = false;
            }
        }

        virtual void OnRelease() override
        {
            if (m_mode == Mode::Momentary)
            {
                m_gateOut = false;
            }
        }
    };
   
    size_t m_selectedAbsPos;

    Cell* Get(MenuButtonRow::RowPos pos, size_t ix)
    {
        return m_menuButtonRows[static_cast<size_t>(pos)].m_cells[ix].get();
    }

    MenuButton* GetMenuButton(MenuButtonRow::RowPos pos, size_t ix)
    {
        if (m_menuButtonRows[static_cast<size_t>(pos)].m_isMenuButton[ix])
        {
            return static_cast<MenuButton*>(Get(pos, ix));
        }

        return nullptr;
    }
    
    MenuButton* GetSelected()
    {
        if (m_selectedAbsPos == x_invalidAbsPos)
        {
            return nullptr;
        }
        
        auto pairPos = PairPos(m_selectedAbsPos);
        return static_cast<MenuButton*>(Get(pairPos.first, pairPos.second));
    }

    size_t GetSelectedGridId()
    {
        MenuButton* selected = GetSelected();
        if (selected)
        {
            return selected->m_gridId;
        }
        else
        {
            return x_numGridIds;
        }
    }
               
    void Select(MenuButton* cell)
    {
        MenuButton* selectedCell = GetSelected();
        if (selectedCell && selectedCell != cell)
        {
            selectedCell->m_gateOut = false;
        }

        m_selectedAbsPos = cell->m_absPos;
    }

    void DeSelect()
    {
        MenuButton* selectedCell = GetSelected();
        if (selectedCell)
        {
            selectedCell->m_gateOut = false;
        }

        m_selectedAbsPos = x_invalidAbsPos;
    }

    void AddMenuRow(MenuButtonRow::RowPos pos, bool populate=true, int start=0, int end=x_baseGridSize)
    {        
        m_menuButtonRows[static_cast<size_t>(pos)] = MenuButtonRow(pos, start, end);
        if (populate)
        {
            for (int i = start; i < end; ++i)
            {
                size_t absPos = AbsPos(pos, i);
                m_menuButtonRows[static_cast<size_t>(pos)].PutMenuButton(i, new MenuButton(this, absPos));                
            }
        }
    }

    void Put(MenuButtonRow::RowPos pos, size_t ix, Cell* cell)
    {
        m_menuButtonRows[static_cast<size_t>(pos)].Put(ix, cell);
    }        

    void SetGridId(MenuButtonRow::RowPos pos, size_t ix, size_t gridId)
    {
        static_cast<MenuButton*>(Get(pos, ix))->m_gridId = gridId;
        static_cast<MenuButton*>(Get(pos, ix))->m_hasInput = true;
    }

    virtual Color GetColor(int i, int j) override
    {
        MenuButtonRow::RowPos pos = MenuButtonRow::GetPos(i, j);
        if (pos != MenuButtonRow::RowPos::Unused)
        {
            return m_menuButtonRows[static_cast<size_t>(pos)].GetColor(i, j);
        }
        
        return Color::Off;
    }

    virtual void Apply(Message msg) override
    {
        MenuButtonRow::RowPos pos = MenuButtonRow::GetPos(msg.m_x, msg.m_y);
        if (pos != MenuButtonRow::RowPos::Unused)
        {
            m_menuButtonRows[static_cast<size_t>(pos)].Apply(msg);
        }
    }

    struct Input
    {
        MenuButton::Input m_inputs[x_numMenuRows][x_baseGridSize];
    };    

    void ProcessInput(Input& input)
    {
        for (size_t i = 0; i < x_numMenuRows; ++i)
        {
            for (int j = m_menuButtonRows[i].m_start; j < m_menuButtonRows[i].m_end; ++j)
            {
                Cell* cell = m_menuButtonRows[i].m_cells[j].get();
                if (cell && m_menuButtonRows[i].m_isMenuButton[j])
                {
                    static_cast<MenuButton*>(cell)->Process(input.m_inputs[i][j]);
                }
            }
        }
    }
};

struct MenuGridSwitcher : public GridSwitcher
{
    virtual ~MenuGridSwitcher()
    {
    }

    MenuGridSwitcher(StateSaver& saver)
        : GridSwitcher(new MenuGrid(saver))
    {
    }

    MenuGrid* GetMenuGrid()
    {
        return static_cast<MenuGrid*>(m_menuGrid.get());        
    }

    virtual size_t GetGridId() override
    {        
        return GetMenuGrid()->GetSelectedGridId();
    }
};

#ifndef IOS_BUILD
template<ControllerShape Shape>
struct GridJnct : Module
{
    static constexpr size_t GetIOId(MenuButtonRow::RowPos pos, size_t ix)
    {
        return static_cast<size_t>(pos) + ix;
    }

    static constexpr size_t GetNumButtonRows()
    {
        return Shape == ControllerShape::LaunchPadX
            ? 2
            : Shape == ControllerShape::LaunchPadProMk3
            ? 5
            : 0;
    }

    static constexpr size_t GetNumIOIds()
    {
        return GetNumButtonRows() * x_baseGridSize;
    }

    static constexpr size_t x_numParams = GetNumIOIds();
    
    static constexpr size_t x_numInputs = GetNumIOIds();
    
    static constexpr size_t x_gridIdOutId = GetNumIOIds();
    static constexpr size_t x_activeGridIdOutId = x_gridIdOutId + 1;
    static constexpr size_t x_numOutputs = x_activeGridIdOutId + 1;

    StateSaver m_stateSaver;
    MenuGridSwitcher m_switcher;
    MenuGrid::Input m_state;
    PeriodChecker m_periodChecker;

    GridJnct()
        : m_switcher(m_stateSaver)
    {
        config(x_numParams, GetNumIOIds(), x_numOutputs, 0);

        for (size_t i = 0; i < GetNumButtonRows(); ++i)
        {
            m_switcher.GetMenuGrid()->AddMenuRow(static_cast<MenuButtonRow::RowPos>(i));
        }

        for (size_t i = 0; i < GetNumIOIds(); ++i)
        {
            auto pairPos = m_switcher.GetMenuGrid()->PairPos(i);
            configInput(i, (MenuButtonRow::RowPosToString(pairPos.first) + " " + std::to_string(pairPos.second) + " Grid Id "));
            configOutput(i, (MenuButtonRow::RowPosToString(pairPos.first) + " " + std::to_string(pairPos.second) + " Gate "));
            configSwitch(i, 0, 2, 0, (MenuButtonRow::RowPosToString(pairPos.first) + " " + std::to_string(pairPos.second) + " Mode "), {"Grid", "Momentary", "Toggle"});
        }

        configOutput(x_gridIdOutId, "Grid Id");
        configOutput(x_activeGridIdOutId, "Selected Grid Id");
    }

    void ReadState()
    {
        for (size_t i = 0; i < GetNumIOIds(); ++i)
        {
            auto pairPos = m_switcher.GetMenuGrid()->PairPos(i);
            size_t posIx = static_cast<size_t>(pairPos.first);
            MenuGrid::MenuButton::Input& input = m_state.m_inputs[posIx][pairPos.second];
            input.m_gateOutConnected = outputs[i].isConnected();
            input.m_mode = static_cast<MenuGrid::MenuButton::Mode>(static_cast<int>(params[i].getValue()));
            input.m_hasInput = inputs[i].isConnected();
            if (input.m_mode == MenuGrid::MenuButton::Mode::Grid)
            {
                size_t gridId = static_cast<size_t>(inputs[i].getVoltage() + 0.5);
                if (gridId > x_numGridIds)
                {
                    gridId = x_numGridIds;
                }
                
                input.m_gridId = gridId;
            }
            else
            {
                if (input.m_hasInput)
                {
                    input.m_color.Process(Color::FloatToZ(inputs[i].getVoltage() / 10));
                }
            }
        }
    }

    void SetOutputs()
    {
        for (size_t i = 0; i < GetNumIOIds(); ++i)
        {
            auto pairPos = m_switcher.GetMenuGrid()->PairPos(i);
            MenuGrid::MenuButton* cell = m_switcher.GetMenuGrid()->GetMenuButton(pairPos.first, pairPos.second);
            if (cell)
            {
                outputs[i].setVoltage(cell->m_gateOut ? 10 : 0);
            }
        }

        outputs[x_gridIdOutId].setVoltage(m_switcher.m_gridId);
        outputs[x_activeGridIdOutId].setVoltage(m_switcher.m_selectedGridId);        
    }

    json_t* dataToJson() override
    {
        return m_stateSaver.ToJSON();
    }

    void dataFromJson(json_t* rootJ) override
    {
        m_stateSaver.SetFromJSON(rootJ);
    }

    void process(const ProcessArgs &args) override
    {
        m_switcher.ProcessStatic(args.sampleTime);
        if (m_periodChecker.Process(args.sampleTime))
        {
            ReadState();
            m_switcher.GetMenuGrid()->ProcessInput(m_state);
            SetOutputs();
        }        
    }    
};

template<ControllerShape Shape>
struct GridJnctWidget : ModuleWidget
{
    static constexpr float x_sep = 25;
    static constexpr float x_xStart = 50;
    static constexpr float x_yStart = 375;

    Vec GetPos(MenuButtonRow::RowPos pos, size_t ix, bool isIn)
    {
        float x;
        float y;

        if (pos == MenuButtonRow::RowPos::Left ||
            pos == MenuButtonRow::RowPos::Right)
        {
            y = x_yStart - x_sep * (x_baseGridSize - ix - 1) - 5 * x_sep;
            if (pos == MenuButtonRow::RowPos::Left)
            {
                x = x_xStart;
                if (!isIn)
                {
                    x += x_sep;
                }
            }
            else
            {
                x = x_xStart + x_baseGridSize * x_sep + 3 * x_sep;
                if (!isIn)
                {
                    x -= x_sep;
                }
            }
        }
        else
        {
            x = x_xStart + x_sep * ix + 2 * x_sep;
            if (pos == MenuButtonRow::RowPos::Top)
            {
                y = x_yStart - x_sep * x_baseGridSize - 6 * x_sep;
                if (!isIn)
                {
                    y += x_sep;
                }
            }
            else
            {
                y = x_yStart - 3 * x_sep;
                if (pos == MenuButtonRow::RowPos::SubBottom)
                {
                    y += 2 * x_sep;
                }
                
                if (!isIn)
                {
                    y -= x_sep;
                }
            }
        }

        return Vec(x, y);
    }
    
    GridJnctWidget(GridJnct<Shape>* module)
    {
        setModule(module);
        if (Shape == ControllerShape::LaunchPadProMk3)
        {
            setPanel(createPanel(asset::plugin(pluginInstance, "res/GridJnctLPP3.svg")));
        }
        
        for (size_t i = 0; i < module->GetNumIOIds(); ++i)
        {
            auto pairPos = module->m_switcher.GetMenuGrid()->PairPos(i);
            addInput(createInputCentered<PJ301MPort>(GetPos(pairPos.first, pairPos.second, true), module, i));
            addOutput(createOutputCentered<PJ301MPort>(GetPos(pairPos.first, pairPos.second, false), module, i));
            size_t xPos = static_cast<size_t>(pairPos.first) * 25 + 375;
            size_t yPos = 125 + pairPos.second * 25;
            addParam(createParamCentered<Trimpot>(Vec(xPos, yPos), module, i));
        }
        
        addOutput(createOutputCentered<PJ301MPort>(Vec(375, 100), module, module->x_gridIdOutId));
        addOutput(createOutputCentered<PJ301MPort>(Vec(425, 100), module, module->x_activeGridIdOutId));

        AddSmartGridWidget(this, x_xStart + 50, x_yStart - 150, module ? &module->m_switcher.m_gridId : nullptr);
    }        
};

typedef GridJnct<ControllerShape::LaunchPadProMk3> GridJnctLPP3;
typedef GridJnctWidget<ControllerShape::LaunchPadProMk3> GridJnctLPP3Widget;

#ifndef SMART_BOX

struct MidiDeviceRemember
{
    int m_deviceId;
    midi::Port* m_port;
    PeriodChecker m_periodChecker;
    std::string m_lastConnectedName;
    ControllerShape* m_shape;

    void TryFindByName()
    {
        if (m_port)
        {
            for (int deviceId : m_port->getDeviceIds())
            {
                if (m_port->getDeviceName(deviceId) == m_lastConnectedName)
                {
                    m_port->setDeviceId(deviceId);
                    m_deviceId = deviceId;
                    InferShape();
                    return;
                }
            }
        }
    }
    
    void Process(float dt)
    {
        if (m_periodChecker.Process(dt))
        {
            if (m_deviceId == -1 && !m_lastConnectedName.empty())
            {
                TryFindByName();
            }

            if (m_deviceId != m_port->deviceId && m_deviceId != -1)
            {
                m_deviceId = m_port->deviceId;
                m_lastConnectedName = m_port->getDeviceName(m_deviceId);
                InferShape();
            }
        }
    }

    void InferShape()
    {
        if (m_lastConnectedName.find("Launchpad Pro MK3") != std::string::npos)
        {
            *m_shape = ControllerShape::LaunchPadProMk3;
        }
        else if (m_lastConnectedName.find("Launchpad X") != std::string::npos)
        {
            *m_shape = ControllerShape::LaunchPadX;
        }
    }
    
    MidiDeviceRemember(midi::Port* port, ControllerShape* shape)
        : m_deviceId(port->deviceId)
        , m_port(port)
        , m_periodChecker(1)
        , m_shape(shape)
    {
    }            
};

struct GridCnct : public Module
{
    MidiInterchangeSingle m_midi;
    size_t m_gridId;
    uint64_t m_gridColorEpoch;

    StateSaver m_stateSaver;
    
    GridCnct()
        : m_midi(false)
        , m_gridColorEpoch(0)
    {
        m_gridId = x_numGridIds;
        config(0, 1, 0, 0);
        m_stateSaver.Insert("Shape", &m_midi.m_shape);
    }

    void process(const ProcessArgs& args) override
    {
        size_t gridId;
        if (inputs[0].isConnected())
        {
            gridId = static_cast<size_t>(inputs[0].getVoltage() + 0.5);
        }
        else
        {
            gridId = x_numGridIds;
        }
        
        if (gridId > x_numGridIds)
        {
            gridId = x_numGridIds;
        }

        if (gridId != m_gridId)
        {
            m_midi.ClearLastSent();
            m_gridId = gridId;
            m_gridColorEpoch = 0;
        }

        if (m_gridId == x_numGridIds)
        {
            m_midi.Drain(args.frame);
        }

        if (m_gridId != x_numGridIds)
        {
            m_midi.ApplyMidiToBus(args.frame, m_gridId);
            m_midi.SendMidiFromBus(args.frame, m_gridId, &m_gridColorEpoch);
        }
    }

    json_t* dataToJson() override
    {
		json_t* rootJ = json_object();

		json_object_set_new(rootJ, "midiIn", m_midi.m_input.toJson());
		json_object_set_new(rootJ, "midiOut", m_midi.m_output.toJson());
        
        json_object_set_new(rootJ, "state", m_stateSaver.ToJSON());
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override
    {
        json_t* midiJ = json_object_get(rootJ, "midiIn");
		if (midiJ)
        {
			m_midi.m_input.fromJson(midiJ);
        }

        midiJ = json_object_get(rootJ, "midiOut");
		if (midiJ)
        {
			m_midi.m_output.fromJson(midiJ);
        }

        midiJ = json_object_get(rootJ, "state");
        if (midiJ)
        {
            m_stateSaver.SetFromJSON(midiJ);
        }
	}
};

struct GridCnctWidget : public ModuleWidget
{
    GridCnctWidget(GridCnct* module)
    {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/PolyCV_MIDICC.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        MidiWidget* midiInputWidget = createWidget<MidiWidget>(Vec(10.0f, 36.4f));
		midiInputWidget->box.size = Vec(130.0f, 44.6f);
		midiInputWidget->SetMidiPort(module ? &module->m_midi.m_input : NULL, false);
		addChild(midiInputWidget);

		MidiWidget* midiOutputWidget = createWidget<MidiWidget>(Vec(10.0f, 85.0f));
		midiOutputWidget->box.size = Vec(130.0f, 44.6f);
		midiOutputWidget->SetMidiPort(module ? &module->m_midi.m_output : NULL, false);
		addChild(midiOutputWidget);

        ShapeWidget* shapeWidget = createWidget<ShapeWidget>(Vec(10.0f, 133.6f));
        shapeWidget->box.size = Vec(130.0f, 22.3f);
        shapeWidget->SetShape(module ? &module->m_midi.m_shape : nullptr);
        addChild(shapeWidget);

        addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(50, 50)), module, 0));
    }
};

#endif
#endif

}

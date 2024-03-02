#pragma once
#include "SmartGrid.hpp"
#include "PeriodChecker.hpp"
#include "MidiWidget.hpp"
#include "SmartGridWidget.hpp"

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
        }
    }

    RowPos m_pos;
    int m_start;
    int m_end;

    std::shared_ptr<Cell> m_cells[x_gridSize];
    bool m_isMenuButton[x_gridSize];

    MenuButtonRow()
        : m_pos(RowPos::Unused)
        , m_start(x_gridSize)
        , m_end(x_gridSize)
    {
        for (size_t i = 0; i < x_gridSize; ++i)
        {
            m_isMenuButton[i] = false;
        }
    }    
    
    MenuButtonRow(RowPos pos, int start=0, int end=x_gridSize)
        : m_pos(pos)
        , m_start(start)
        , m_end(end)
    {
        for (size_t i = 0; i < x_gridSize; ++i)
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
                return x_gridSize;
            }
            case RowPos::Left:
            case RowPos::Unused:
            {
                return x_gridXMin;
            }
        }
    }

    static int GetY(RowPos pos, size_t ix)
    {
        switch (pos)
        {
            case RowPos::Top:
            {
                return x_gridSize;
            }
            case RowPos::Bottom:
            {
                return x_gridYMin + 1;
            }
            case RowPos::SubBottom:
            case RowPos::Unused:
            {
                return x_gridYMin;
            }
            case RowPos::Right:
            case RowPos::Left:
            {
                return ix;
            }
        }
    }

    static RowPos GetPos(int i, int j)
    {
        if (j == x_gridSize && 0 <= i && i < x_gridSize)
        {
            return RowPos::Top;
        }
        else if (j == x_gridYMin + 1 && 0 <= i && i < x_gridSize)
        {
            return RowPos::Bottom;
        }
        else if (j == x_gridYMin && 0 <= i && i < x_gridSize)
        {
            return RowPos::SubBottom;
        }
        else if (i == x_gridSize && 0 <= j && j < x_gridSize)
        {
            return RowPos::Right;
        }
        else if (i == x_gridXMin && 0 <= j && j < x_gridSize)
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
                return j == x_gridSize && m_start <= i && i < m_end;
            }
            case RowPos::Bottom:
            {
                return j == x_gridYMin + 1 && m_start <= i && i < m_end;
            }
            case RowPos::SubBottom:
            {
                return j == x_gridYMin && m_start <= i && i < m_end;
            }
            case RowPos::Right:
            {
                return i == x_gridSize && m_start <= j && j < m_end;
            }        
            case RowPos::Left:
            {
                return i == x_gridXMin && m_start <= j && j < m_end;
            }
            case RowPos::Unused:
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
    static constexpr size_t x_invalidAbsPos = x_numMenuRows * x_gridSize;

    virtual ~MenuGrid()
    {
    }

    MenuGrid()
        : m_selectedAbsPos(x_invalidAbsPos)
    {
    }

    size_t AbsPos(MenuButtonRow::RowPos pos, size_t ix)
    {
        return static_cast<size_t>(pos) * x_gridSize + ix;
    }

    std::pair<MenuButtonRow::RowPos, size_t> PairPos(size_t absPos)
    {
        return std::make_pair(static_cast<MenuButtonRow::RowPos>(absPos / x_gridSize), absPos % x_gridSize);
    }
    
    struct MenuButton : public Cell
    {
        bool m_gateOut;
        bool m_gateOutConnected;
        bool m_hasGrid;
        size_t m_gridId;
        size_t m_absPos;
        MenuGrid* m_owner;

        struct Input
        {
            bool m_gateOutConnected;
            bool m_hasGrid;
            size_t m_gridId;

            Input()
                : m_gateOutConnected(false)
                , m_hasGrid(false)
                , m_gridId(x_numGridIds)
            {
            }
        };
        
        virtual ~MenuButton()
        {
        }

        MenuButton(MenuGrid* owner, size_t absPos)
            : m_gateOut(false)
            , m_gateOutConnected(false)
            , m_hasGrid(false)
            , m_gridId(x_numGridIds)
            , m_absPos(absPos)
            , m_owner(owner)
        {
        }

        void Process(Input& input)
        {
            m_gateOutConnected = input.m_gateOutConnected;
            m_hasGrid = input.m_hasGrid;
            bool gridIdChanged = input.m_gridId != m_gridId;
            m_gridId = input.m_gridId;
            if (IsSelected() && gridIdChanged && m_hasGrid)
            {
                m_owner->Select(this);
            }
            else if (IsSelected() && !m_hasGrid)
            {
                m_owner->DeSelect();
            }
        }

        bool IsSelected()
        {
            return m_owner->m_selectedAbsPos == m_absPos;
        }

        virtual Color GetColor() override
        {
            if (m_hasGrid)
            {
                return IsSelected() ? g_smartBus.GetOnColor(m_gridId) : g_smartBus.GetOffColor(m_gridId);
            }
            else if (m_gateOutConnected)
            {
                return m_gateOut ? Color::White : Color::Dim;
            }
            else
            {
                return Color::Off;
            }
        }

        virtual void OnPress(uint8_t) override
        {
            if (m_hasGrid)
            {
                m_owner->Select(this);
                m_gateOut = true;
            }
            else if (m_gateOutConnected && !m_gateOut)
            {
                m_gateOut = true;
            }
            else if (m_gateOutConnected)
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

    void AddMenuRow(MenuButtonRow::RowPos pos, bool populate=true, int start=0, int end=x_gridSize)
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
        static_cast<MenuButton*>(Get(pos, ix))->m_hasGrid = true;
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
        MenuButton::Input m_inputs[x_numMenuRows][x_gridSize];        
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

    MenuGridSwitcher()
        : GridSwitcher(new MenuGrid())
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
        return GetNumButtonRows() * x_gridSize;
    }

    static constexpr size_t x_numInputs = GetNumIOIds();
    
    static constexpr size_t x_gridIdOutId = GetNumIOIds();
    static constexpr size_t x_activeGridIdOutId = x_gridIdOutId + 1;
    static constexpr size_t x_numOutputs = x_activeGridIdOutId + 1;


    MenuGridSwitcher m_switcher;
    MenuGrid::Input m_state;
    PeriodChecker m_periodChecker;

    GridJnct()
    {
        config(0, GetNumIOIds(), x_numOutputs, 0);

        for (size_t i = 0; i < GetNumButtonRows(); ++i)
        {
            m_switcher.GetMenuGrid()->AddMenuRow(static_cast<MenuButtonRow::RowPos>(i));
        }

        for (size_t i = 0; i < GetNumIOIds(); ++i)
        {
            auto pairPos = m_switcher.GetMenuGrid()->PairPos(i);
            configInput(i, (MenuButtonRow::RowPosToString(pairPos.first) + " " + std::to_string(pairPos.second) + " Grid Id "));
            configOutput(i, (MenuButtonRow::RowPosToString(pairPos.first) + " " + std::to_string(pairPos.second) + " Gate "));
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
            input.m_hasGrid = inputs[i].isConnected();
            size_t gridId = static_cast<size_t>(inputs[i].getVoltage() + 0.5);
            if (gridId > x_numGridIds)
            {
                gridId = x_numGridIds;
            }
            
            input.m_gridId = gridId;
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
            y = x_yStart - x_sep * ix - 5 * x_sep;
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
                x = x_xStart + x_gridSize * x_sep + 3 * x_sep;
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
                y = x_yStart - x_sep * x_gridSize - 6 * x_sep;
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
        }
        
        addOutput(createOutputCentered<PJ301MPort>(Vec(375, 100), module, module->x_gridIdOutId));
        addOutput(createOutputCentered<PJ301MPort>(Vec(425, 100), module, module->x_activeGridIdOutId));

        AddSmartGridWidget(this, x_xStart + 50, x_yStart - 150, module ? &module->m_switcher.m_gridId : nullptr);
    }        
};

struct GridCnct : public Module
{
    MidiInterchangeSingle m_midi;
    size_t m_gridId;
    GridCnct()
        : m_midi(false)
    {
        m_gridId = x_numGridIds;
        config(0, 1, 0, 0);
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
        }

        if (m_gridId == x_numGridIds)
        {
            m_midi.Drain(args.frame);
        }

        if (m_gridId != x_numGridIds)
        {
            m_midi.ApplyMidiToBus(args.frame, m_gridId);
            m_midi.SendMidiFromBus(args.frame, m_gridId);
        }
    }

    json_t* dataToJson() override
    {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "midiIn", m_midi.m_input.toJson());
		json_object_set_new(rootJ, "midiOut", m_midi.m_output.toJson());
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

        StoermelderPackOne::MidiWidget<>* midiInputWidget = createWidget<StoermelderPackOne::MidiWidget<>>(Vec(10.0f, 36.4f));
		midiInputWidget->box.size = Vec(130.0f, 67.0f);
		midiInputWidget->setMidiPort(module ? &module->m_midi.m_input : NULL);
		addChild(midiInputWidget);

		StoermelderPackOne::MidiWidget<>* midiOutputWidget = createWidget<StoermelderPackOne::MidiWidget<>>(Vec(10.0f, 107.4f));
		midiOutputWidget->box.size = Vec(130.0f, 67.0f);
		midiOutputWidget->setMidiPort(module ? &module->m_midi.m_output : NULL);
		addChild(midiOutputWidget);

        addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(50, 50)), module, 0));
        
	}
};

typedef GridJnct<ControllerShape::LaunchPadProMk3> GridJnctLPP3;
typedef GridJnctWidget<ControllerShape::LaunchPadProMk3> GridJnctLPP3Widget;


}

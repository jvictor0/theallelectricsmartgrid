#pragma once
#include "SmartGrid.hpp"
#include "PeriodChecker.hpp"
#include "StateSaver.hpp"
#include "ColorHelper.hpp"

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
        , m_cells{}
        , m_isMenuButton{}
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
        , m_cells{}
        , m_isMenuButton{}
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
                    return IsSelected() ? SmartBusGetOnColor(m_gridId) : SmartBusGetOffColor(m_gridId);
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

}

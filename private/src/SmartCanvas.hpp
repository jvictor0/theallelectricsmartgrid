#pragma once
#include "SmartGrid.hpp"
#include "StateSaver.hpp"
#include "plugin.hpp"

namespace SmartGrid
{

struct SmartCanvasInternal : public AbstractGrid
{
#ifndef SMART_BOX
    static constexpr int x_gridXSize = 8;
    static constexpr int x_gridYSize = 8;
#else
    static constexpr int x_gridXSize = 20;
    static constexpr int x_gridYSize = 8;
#endif
    
    virtual ~SmartCanvasInternal()
    {
    }
    
    struct Elem
    {
        size_t m_gridId;
        int m_srcX;
        int m_srcY;
        int m_trgX;
        int m_trgY;
        int m_lenX;
        int m_lenY;

        bool m_flipX;
        bool m_flipY;
        int m_rotate;

        Elem()
            : m_gridId(x_numGridIds)
            , m_srcX(0)
            , m_srcY(0)
            , m_trgX(0)
            , m_trgY(0)
            , m_lenX(x_gridXSize)
            , m_lenY(x_gridYSize)
            , m_flipX(false)
            , m_flipY(false)
            , m_rotate(0)
        {
        }

        std::pair<int, int> ApplyFlipAndRotate(int x, int y)
        {
            if (m_flipX)
            {
                x = x_gridXSize - x - 1;
            }

            if (m_flipY)
            {
                y = x_gridYSize - y - 1;
            }

            int rotate = m_rotate;
            while (rotate > 0)
            {
                int xp = y;
                int yp = x_gridYSize - x - 1;
                x = xp;
                y = yp;
                --rotate;
            }

            return std::pair<int, int>(x, y);                
        }

        std::pair<int, int> GetSrcCoords(int x, int y, bool* outOfBounds)
        {
            x = x - m_trgX + m_srcX;
            y = y - m_trgY + m_srcY;
            if (x < 0 || x_gridXSize <= x || y < 0 || x_gridYSize <= y)
            {
                *outOfBounds = true;
                return std::pair<int, int>(0, 0);
            }

            *outOfBounds = false;
            return ApplyFlipAndRotate(x, y);
        }

        bool HasGrid()
        {
            return m_gridId != x_numGridIds;
        }
        
        bool InBounds(int x, int y)
        {
            return m_trgX <= x && x < m_trgX + m_lenX &&
                   m_trgY <= y && y < m_trgY + m_lenY &&
                   0 <= x && x < x_gridXSize &&
                   0 <= y && y < x_gridYSize;
        }
        
        struct Input
        {
            size_t m_gridId;
            int m_srcX;
            int m_srcY;
            int m_trgX;
            int m_trgY;
            int m_lenX;
            int m_lenY;
            
            bool m_flipX;
            bool m_flipY;
            int m_rotate;

            Input()
                : m_gridId(x_numGridIds)
                , m_srcX(0)
                , m_srcY(0)
                , m_trgX(0)
                , m_trgY(0)
                , m_lenX(x_gridXSize)
                , m_lenY(x_gridYSize)
                , m_flipX(false)
                , m_flipY(false)
                , m_rotate(0)
            {
            }
        };

        void Process(Input& input)
        {
            m_gridId = input.m_gridId;
            m_srcX = input.m_srcX;
            m_srcY = input.m_srcY;
            m_trgX = input.m_trgX;
            m_trgY = input.m_trgY;
            m_lenX = input.m_lenX;
            m_lenY = input.m_lenY;
            m_flipX = input.m_flipX;
            m_flipY = input.m_flipY;
            m_rotate = input.m_rotate;
        }
    };

    static constexpr size_t x_numElems = 8;
    
    Color m_onColor;
    Elem m_elems[x_numElems];

    struct Input
    {
        Elem::Input m_inputs[x_numElems];
        ColorDecode m_onColor;

        Input()
            : m_onColor(Color::White)
        {
        }
    };

    void ProcessInput(Input& input)
    {
        m_onColor = input.m_onColor.m_color;
        for (size_t i = 0; i < x_numElems; ++i)
        {
            m_elems[i].Process(input.m_inputs[i]);
        }        
    }

    virtual void Apply(Message msg)
    {
        for (size_t i = 0; i < x_numElems; ++i)
        {
            if (m_elems[i].HasGrid() &&
                m_elems[i].InBounds(msg.m_x, msg.m_y))
            {
                bool oob = false;
                std::pair<int, int> coords = m_elems[i].GetSrcCoords(msg.m_x, msg.m_y, &oob);
                if (!oob)
                {
                    SmartBusPutVelocity(m_elems[i].m_gridId, coords.first, coords.second, msg.m_velocity);
                    break;
                }
            }
        }
    }
    
    virtual Color GetColor(int x, int y)
    {
        for (size_t i = 0; i < x_numElems; ++i)
        {
            if (m_elems[i].HasGrid() &&
                m_elems[i].InBounds(x, y))
            {
                bool oob = false;
                std::pair<int, int> coords = m_elems[i].GetSrcCoords(x, y, &oob);
                if (!oob)
                {
                    return SmartBusGetColor(m_elems[i].m_gridId, coords.first, coords.second);
                }
            }
        }

        return Color::Off;
    }
    
    virtual Color GetOnColor()
    {
        return m_onColor;
    }

    virtual Color GetOffColor()
    {
        return m_onColor.Dim();
    }
};

}

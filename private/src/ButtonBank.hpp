#pragma once
#include "SmartGrid.hpp"
#include "StateSaver.hpp"
#include "plugin.hpp"
#include "VoiceAllocator.hpp"

namespace SmartGrid
{

struct ButtonBankInternal : Grid
{
    Color m_offColor;
    Color m_onColor;
    bool m_momentary;
    size_t m_width[x_baseGridSize];
    VoiceAllocator<CellVoice> m_voiceAllocator;

#ifndef SMART_BOX
    static constexpr size_t x_maxWidth = x_baseGridSize;
#else
    static constexpr size_t x_maxWidth = 16;
#endif

    struct BankedButton : public Cell
    {
        bool m_state;
        Color m_color;
        bool m_hasDirectColor;
        
        int m_x;
        int m_y;
        ButtonBankInternal* m_owner;

        struct Input
        {
            ColorDecode m_color;
            bool m_hasDirectColor;

            Input()
                : m_hasDirectColor(false)
            {
            }
        };

        CellVoice ToCellVoice()
        {
            return CellVoice(&m_state, m_x, m_y);
        }

        void Process(Input& input)
        {
            m_hasDirectColor = input.m_hasDirectColor;
            if (m_hasDirectColor)
            {
                m_color = input.m_color.m_color;
            }
        }
        
        BankedButton(int x, int y, ButtonBankInternal* owner)
            : m_state(false)
            , m_color(Color::White)
            , m_hasDirectColor(false)
            , m_x(x)
            , m_y(y)
            , m_owner(owner)
        {
            m_pressureSensitive = true;
        }

        virtual Color GetColor() override
        {
            if (static_cast<int>(m_owner->m_width[m_y]) <= m_x)
            {
                return Color::Off;
            }            
            else if (m_hasDirectColor)
            {
                return m_color;
            }
            else
            {
                return m_state ? m_owner->m_onColor : m_owner->m_offColor;
            }
        }

        virtual void OnPress(uint8_t) override
        {
            if (m_owner->HasPolyphony())
            {
                if (m_state)
                {
                    m_owner->m_voiceAllocator.DeAllocate(ToCellVoice());
                }
                else
                {
                    m_owner->m_voiceAllocator.Allocate(ToCellVoice());
                }
            }
            else
            {
                m_state = !m_state;
            }
        }

        virtual void OnRelease() override
        {
            if (m_owner->m_momentary)
            {
                if (m_owner->HasPolyphony())
                {
                    if (m_state)
                    {
                        m_owner->m_voiceAllocator.DeAllocate(ToCellVoice());
                    }
                }
                else
                {
                    m_state = !m_state;
                }
            }
        }
    };

    ButtonBankInternal()
        : m_offColor(Color::White.Dim())
        , m_onColor(Color::White)
        , m_momentary(false)
    {
        for (size_t i = 0; i < x_baseGridSize; ++i)
        {
            m_width[i] = x_maxWidth;
            for (size_t j = 0; j < x_maxWidth; ++j)
            {
                Put(j, i, new BankedButton(j, i, this));
            }
        }
    }

    struct Input
    {
        ColorDecode m_offColor;
        ColorDecode m_onColor;
        bool m_momentary;
        size_t m_width[x_baseGridSize];
        BankedButton::Input m_input[x_maxWidth][x_baseGridSize];
        size_t m_maxPolyphony;

        Input()
            : m_offColor(Color::White.Dim())
            , m_onColor(Color::White)
            , m_momentary(false)
            , m_maxPolyphony(0)
        {
            for (size_t i = 0; i < x_baseGridSize; ++i)
            {
                m_width[i] = x_maxWidth;
            }
        }
    };

    bool HasPolyphony()
    {
        return m_voiceAllocator.m_maxPolyphony > 0;
    }
    
    void Clear()
    {
        for (size_t i = 0; i < x_maxWidth; ++i)
        {
            for (size_t j = 0; j < x_baseGridSize; ++j)
            {
                static_cast<BankedButton*>(Get(i, j))->m_state = false;
            }
        }
    }
    
    void ProcessInput(Input& input)
    {
        m_offColor = input.m_offColor.m_color;
        m_onColor = input.m_onColor.m_color;
        m_momentary = input.m_momentary;
        if (m_voiceAllocator.m_maxPolyphony != input.m_maxPolyphony)
        {
            m_voiceAllocator.SetPolyphony(input.m_maxPolyphony);
            Clear();
        }
        
        for (size_t i = 0; i < x_baseGridSize; ++i)
        {
            m_width[i] = input.m_width[i];
            for (size_t j = 0; j < m_width[i]; ++j)
            {
                static_cast<BankedButton*>(Get(j, i))->Process(input.m_input[j][i]);
            }
        }
    }
};

}

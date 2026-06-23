#pragma once

#include <cstdint>
#include <cmath>
#include "BasicMidi.hpp"

namespace SmartGrid
{
    struct MessageIn
    {
        enum class Mode : int
        {
            NoMessage,
            PadPress,
            PadPressure,
            PadRelease,
            EncoderIncDec,
            EncoderPush,
            EncoderRelease,
            ParamSet14,
            ParamSet7,
            EncoderSet,
            MidiClock,
            MidiStart,
            MidiContinue,
            MidiStop
        };

        size_t m_timestamp;
        int m_routeId;

        int m_x;
        int m_y;
        
        int64_t m_amount;

        Mode m_mode;

        MessageIn()
            : m_timestamp(0)
            , m_routeId(-1)
            , m_x(0)
            , m_y(0)
            , m_amount(0)
            , m_mode(Mode::NoMessage)
        {
        }

        MessageIn(Mode mode, int x, int y, int64_t amount)
            : m_timestamp(0)
            , m_routeId(-1)
            , m_x(x)
            , m_y(y)
            , m_amount(amount)
            , m_mode(mode)
        {
        }

        MessageIn(Mode mode, int x, int y)
            : m_timestamp(0)
            , m_routeId(-1)
            , m_x(x)
            , m_y(y)
            , m_amount(0)
            , m_mode(mode)
        {
        }

        MessageIn(size_t timestamp, int routeId, Mode mode, int x, int y, int64_t amount)
            : m_timestamp(timestamp)
            , m_routeId(routeId)
            , m_x(x)
            , m_y(y)
            , m_amount(amount)
            , m_mode(mode)
        {
        }

        MessageIn(size_t timestamp, int routeId, Mode mode, int x, int y)
            : m_timestamp(timestamp)
            , m_routeId(routeId)
            , m_x(x)
            , m_y(y)
            , m_amount(0)
            , m_mode(mode)
        {
        }

        // Absolute-set encoder message: sets an encoder to value01 in [0,1]
        // deterministically (no timestamp-dependent acceleration). The value is
        // stored as 14-bit fixed point in m_amount.
        //
        static MessageIn EncoderSetMsg(int x, int y, float value01)
        {
            float clamped = value01 < 0.0f ? 0.0f : (value01 > 1.0f ? 1.0f : value01);
            return MessageIn(Mode::EncoderSet, x, y, llround(clamped * 16383));
        }

        float SetValueFloat()
        {
            return static_cast<float>(m_amount) / 16383.0f;
        }

        float AmountFloat()
        {
            if (m_mode == Mode::ParamSet14)
            {
                return static_cast<float>(m_amount) / ((1 << 14) - 1);
            }
            else
            {
                return static_cast<float>(m_amount) / ((1 << 7) - 1);
            }
        }

        bool Visible(size_t timestamp)
        {
            return m_timestamp <= timestamp;
        }

        bool NoMessage()
        {
            return m_mode == Mode::NoMessage;
        }

        bool IsRealtime()
        {
            return m_mode == Mode::MidiClock
                || m_mode == Mode::MidiStart
                || m_mode == Mode::MidiContinue
                || m_mode == Mode::MidiStop;
        }

        bool IsMidiClock()
        {
            return m_mode == Mode::MidiClock;
        }

        bool IsParamSet()
        {
            return m_mode == Mode::ParamSet14 || m_mode == Mode::ParamSet7;
        }
    };
}

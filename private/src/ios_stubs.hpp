#pragma once
#include <vector>
#include <cstdint>
#include <cmath>

#ifndef IOS_BUILD
#include "rack.hpp"
#else
#include <os/log.h>

// Global-scope stubs for VCV Rack widget types
struct LightWidget
{
    virtual ~LightWidget() = default;
    virtual void draw(const struct DrawArgs&) {}
};

struct NVGcolor
{
    float r, g, b, a;
};

inline NVGcolor nvgRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    return NVGcolor{r/255.0f, g/255.0f, b/255.0f, a/255.0f};
}

struct DrawArgs
{
    struct
    {
        float x, y;
    } size;
    struct
    {
        float x, y;
    } pos;
    void* vg;
};

namespace rack
{
    namespace engine
    {
        struct Input
        {
            float getVoltage() const { return 0.0f; }
            bool isConnected() const { return false; }
        };
        struct Output
        {
            void setVoltage(float) {}
        };
        struct Param
        {
            float getValue() const { return 0.0f; }
            void setValue(float) {}
        };
        struct Light
        {
            void setBrightness(float) {}
            void setSmoothBrightness(float, float) {}
        };
    }
    namespace dsp
    {
        template <typename T = float>
        struct TSchmittTrigger
        {
            bool process(T) { return false; }
        };
        struct PulseGenerator
        {
            void trigger(float = 0.0f) {}
            bool process(float) { return false; }
        };
    }
}

namespace midi
{
    struct Message
    {
        std::vector<uint8_t> bytes;
        Message() : bytes(3, 0) {}
        uint8_t getStatus() const { return bytes.size() > 0 ? bytes[0] : 0; }
        void setFrame(int64_t) {}
        bool NoMessage() const { return false; }
        bool ShapeSupports(int) const { return true; }
        void setChannel(uint8_t ch) { if (bytes.size() > 0) bytes[0] = (bytes[0] & 0xF0) | (ch & 0x0F); }
        void setStatus(uint8_t status) { if (bytes.size() > 0) bytes[0] = (bytes[0] & 0x0F) | (status & 0xF0); }
        void setNote(uint8_t note) { if (bytes.size() > 1) bytes[1] = note; }
        void setValue(uint8_t value) { if (bytes.size() > 2) bytes[2] = value; }
        uint8_t getChannel() const { return bytes.size() > 0 ? (bytes[0] & 0x0F) : 0; }
        uint8_t getNote() const { return bytes.size() > 1 ? bytes[1] : 0; }
        uint8_t getValue() const { return bytes.size() > 2 ? bytes[2] : 0; }
    };
    struct InputQueue
    {
        bool empty() const { return true; }
        void pop() {}
        Message front() const { return Message(); }
        void setDriverId(int id) {}
        void setDeviceId(int id) {}
        bool tryPop(Message* msg, int64_t frame) { return false; }
    };
    struct Output
    {
        void sendMessage(const Message&) {}
        int getChannel() const { return 0; }
        void setDriverId(int id) {}
        void setDeviceId(int id) {}
        void setChannel(uint8_t channel) {}
    };
}


#define WARN(...) os_log(OS_LOG_DEFAULT, __VA_ARGS__)
#define INFO(...) os_log(OS_LOG_DEFAULT, __VA_ARGS__)

#endif 
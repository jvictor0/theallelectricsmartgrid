#pragma once

#include <string>
#include "State.hpp"
#include "PatchArena.hpp"

namespace SmartGrid
{
struct BankedEncoderCell;
}

struct ParamEvent
{
    static constexpr size_t x_maxEncoderPath = 16;

    enum class Type : uint8_t
    {
        None,

        // Represents a discreet state change, like from a smart grid cell.
        // Uses name, scene and value/len.
        //
        StateChange,

        // Represets a gesture being set.
        // Uses gesture and value/len.
        //
        GestureSet,

        // Represents a blend being set.
        // Uses value/len.
        //
        BlendSet,

        // Represents an encoder being set.
        // Uses name, scene, track, path and value/len.
        //
        EncoderSet,

        // Represents an encoder being activated.
        // Uses name, scene, track, path and value/len.
        //
        EncoderActivate,

        // Apply a loaded patch using its fader restoration policy.
        //
        PatchLoad,

        // Replace the entire patch after a whole-patch reset.
        //
        PatchSnapshot,
    };

    ParamEvent()
      : m_type(Type::None)
      , m_sample(0)
      , m_name(nullptr)
      , m_scene(0)
      , m_track(0)
      , m_gesture(0)
      , m_valueLen(0)
      , m_value{0}
      , m_isGesture(false)
    {
        for (auto& index : m_encoderPath)
        {
            index = -1;
        }
    }

    Type m_type;
    size_t m_sample;
    const char* m_name;
    int m_scene;
    int m_track;
    int m_gesture;

    uint8_t m_valueLen;
    char m_value[State::x_maxValueLen];
    
    // Each hop is a modulator index or 0x80 | gesture index; -1 ends the path.
    //
    int m_encoderPath[x_maxEncoderPath];
    bool m_isGesture;
    PatchArena* m_patchArena = nullptr;
    JSON m_patch;
    bool m_restoreFaders = false;

    // Only the writer supplies serialized bytes, after releasing the arena.
    //
    const char* m_patchText = nullptr;
    size_t m_patchBytes = 0;
    uint32_t m_order = 0;

    bool IsPatch() const
    {
        return m_type == Type::PatchLoad || m_type == Type::PatchSnapshot;
    }

    void ReleasePatch()
    {
        if (m_patchArena != nullptr)
        {
            m_patchArena->Release();
            m_patchArena = nullptr;
            m_patch = JSON::Null();
        }
    }

    static ParamEvent MkPatch(PatchArena& arena, bool restoreFaders, bool snapshot, size_t sample)
    {
        ParamEvent event;
        event.m_type = snapshot ? Type::PatchSnapshot : Type::PatchLoad;
        event.m_sample = sample;
        event.m_patchArena = &arena;
        event.m_patch = arena.m_patch;
        event.m_restoreFaders = restoreFaders;
        return event;
    }

    size_t EncoderPathLength() const
    {
        size_t length = 0;
        while (length < x_maxEncoderPath && m_encoderPath[length] != -1)
        {
            ++length;
        }

        return length;
    }

    static ParamEvent MkStateChange(State* state, int scene, size_t sample)
    {
        ParamEvent event{};
        event.m_isGesture = false;
        event.m_type = Type::StateChange;
        event.m_sample = sample;
        event.m_name = state->m_name.c_str();
        event.m_scene = scene;
        event.m_valueLen = static_cast<uint8_t>(state->m_len);
        std::memcpy(event.m_value, state->m_buf + scene * state->m_len, state->m_len);
        return event;
    }

    static ParamEvent MkGestureSet(int gesture, float value, size_t sample)
    {
        ParamEvent event{};
        event.m_isGesture = false;
        event.m_type = Type::GestureSet;
        event.m_gesture = gesture;
        event.m_sample = sample;
        event.m_valueLen = sizeof(float);
        std::memcpy(event.m_value, &value, sizeof(float));
        return event;
    }

    static ParamEvent MkBlendSet(float value, size_t sample)
    {
        ParamEvent event{};
        event.m_isGesture = false;
        event.m_type = Type::BlendSet;
        event.m_sample = sample;
        event.m_valueLen = sizeof(float);
        std::memcpy(event.m_value, &value, sizeof(float));
        return event;
    }

    static ParamEvent MkEncoderSet(SmartGrid::StateEncoderCell* stateEncoderCell, int scene, int track, size_t sample);
    static ParamEvent MkEncoderActivate(SmartGrid::BankedEncoderCell* cell, int scene, int track, size_t sample);
};

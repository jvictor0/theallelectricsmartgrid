#pragma once

#include "JuceSon.hpp"
#include <cstring>
#include <string>

// Upgrades patch data in the message thread's load arena before publication.
//
struct PatchFormat
{
    static constexpr int x_currentVersion = 1;

    enum class Scope
    {
        Unknown,
        Voice,
        Shared
    };

    static bool IsObject(JSON json)
    {
        return json.m_node && json.m_node->m_type == JsonType::Object;
    }

    static bool IsArray(JSON json)
    {
        return json.m_node && json.m_node->m_type == JsonType::Array;
    }

    static const JsonMember* Members(JSON json)
    {
        return static_cast<const JsonMember*>(json.m_node->m_container.m_entries);
    }

    static uint32_t MemberCount(JSON json)
    {
        return json.m_node->m_container.m_size;
    }

    static bool HasMember(JSON json, const char* key)
    {
        if (!IsObject(json))
        {
            return false;
        }

        const JsonMember* members = Members(json);
        for (uint32_t i = 0; i < MemberCount(json); ++i)
        {
            if (strcmp(members[i].m_key, key) == 0)
            {
                return true;
            }
        }

        return false;
    }

    static Scope ParamScope(const char* name)
    {
#define F(param, shortName, bank, x, y, defaultValue, title, color, machine, flags, discrete, bipolar) \
        if (strcmp(name, #param) == 0) \
        { \
            return strcmp(#bank, "Source") == 0 || strcmp(#bank, "FilterAndAmp") == 0 || \
                strcmp(#bank, "PanningAndSequencing") == 0 || strcmp(#bank, "VoiceLFOs") == 0 \
                    ? Scope::Voice : Scope::Shared; \
        }
#include "ForEachSmartGridOneParam.hpp"
#undef F
        return Scope::Unknown;
    }

    static JSON ConvertValues(JsonArena& arena, JSON source, int trio, bool voice)
    {
        if (!IsObject(source))
        {
            return source;
        }

        JSON oldValues = source.Get("values");
        if (!IsArray(oldValues))
        {
            return source;
        }

        bool hasLegacyRow = false;
        for (size_t scene = 0; scene < oldValues.Size(); ++scene)
        {
            if (IsArray(oldValues.GetAt(scene)))
            {
                hasLegacyRow = true;
                break;
            }
        }

        if (!voice && !hasLegacyRow)
        {
            return source;
        }

        JSON result = arena.Object();
        const JsonMember* members = Members(source);
        for (uint32_t i = 0; i < MemberCount(source); ++i)
        {
            if (strcmp(members[i].m_key, "values") == 0)
            {
                JSON flat = arena.Array();
                for (size_t scene = 0; scene < 8; ++scene)
                {
                    JSON row = oldValues.GetAt(scene);
                    JSON value = row.GetAt(voice ? static_cast<size_t>(trio) : 0);
                    flat.AppendNew(value.IsNull() ? arena.Real(0) : value);
                }

                result.SetNew("values", flat);
            }
            else
            {
                result.SetNew(members[i].m_key, JSON(members[i].m_value));
            }
        }

        return result;
    }

    static JSON ConvertActive(JsonArena& arena, JSON active, int trio, bool voice)
    {
        if (!IsArray(active) || (!voice && active.Size() == 8))
        {
            return active;
        }

        JSON result = arena.Array();
        for (size_t scene = 0; scene < 8; ++scene)
        {
            JSON value = active.GetAt(scene * 16 + static_cast<size_t>(trio));
            result.AppendNew(arena.Boolean(value.BooleanValue()));
        }

        return result;
    }

    static JSON ConvertCell(JsonArena& arena, JSON cell, int trio, bool voice)
    {
        if (!IsObject(cell))
        {
            return cell;
        }

        JSON result = arena.Object();
        const JsonMember* members = Members(cell);
        for (uint32_t i = 0; i < MemberCount(cell); ++i)
        {
            const char* key = members[i].m_key;
            JSON value(members[i].m_value);
            if (strcmp(key, "values") == 0)
            {
                value = ConvertValues(arena, value, trio, voice);
            }
            else if (strcmp(key, "active") == 0)
            {
                value = ConvertActive(arena, value, voice ? trio : 0, voice);
            }
            else if ((strcmp(key, "modulators") == 0 || strcmp(key, "gestures") == 0) && IsArray(value))
            {
                JSON children = arena.Array();
                for (size_t child = 0; child < value.Size(); ++child)
                {
                    children.AppendNew(ConvertCell(arena, value.GetAt(child), trio, voice));
                }

                value = children;
            }

            result.SetNew(key, value);
        }

        return result;
    }

    static JSON ConvertEncoders(JsonArena& arena, JSON encoders)
    {
        if (!IsObject(encoders))
        {
            return encoders;
        }

        const char* suffixes[3] = {"Water", "Fire", "Earth"};
        const JsonMember* members = Members(encoders);
        for (uint32_t i = 0; i < MemberCount(encoders); ++i)
        {
            if (ParamScope(members[i].m_key) != Scope::Voice)
            {
                continue;
            }

            for (int trio = 0; trio < 3; ++trio)
            {
                std::string suffixed = std::string(members[i].m_key) + suffixes[trio];
                if (HasMember(encoders, suffixed.c_str()))
                {
                    return JSON::Null();
                }
            }
        }

        JSON result = arena.Object();
        for (uint32_t i = 0; i < MemberCount(encoders); ++i)
        {
            const char* key = members[i].m_key;
            JSON cell(members[i].m_value);
            Scope scope = ParamScope(key);
            if (scope == Scope::Voice)
            {
                for (int trio = 0; trio < 3; ++trio)
                {
                    std::string suffixed = std::string(key) + suffixes[trio];
                    result.SetNew(suffixed.c_str(), ConvertCell(arena, cell, trio, true));
                }
            }
            else
            {
                result.SetNew(key, scope == Scope::Shared ? ConvertCell(arena, cell, 0, false) : cell);
            }
        }

        return result;
    }

    static JSON Upgrade(JsonArena& arena, JSON patch)
    {
        if (!IsObject(patch))
        {
            return JSON::Null();
        }

        int versionCount = 0;
        int encoderCount = 0;
        const JsonMember* members = Members(patch);
        for (uint32_t i = 0; i < MemberCount(patch); ++i)
        {
            if (strcmp(members[i].m_key, "version") == 0)
            {
                ++versionCount;
            }
            else if (strcmp(members[i].m_key, "squiggleBoy") == 0)
            {
                ++encoderCount;
            }
        }

        if (versionCount > 1 || encoderCount > 1)
        {
            return JSON::Null();
        }

        JSON version = patch.Get("version");
        if (versionCount == 1)
        {
            if (!version.m_node || version.m_node->m_type != JsonType::Integer)
            {
                return JSON::Null();
            }

            if (version.m_node->m_int == x_currentVersion)
            {
                return patch;
            }

            if (version.m_node->m_int != 0)
            {
                return JSON::Null();
            }
        }

        JSON encoders = patch.Get("squiggleBoy");
        JSON converted = ConvertEncoders(arena, encoders);
        if (IsObject(encoders) && converted.IsNull())
        {
            return JSON::Null();
        }

        JSON result = arena.Object();
        for (uint32_t i = 0; i < MemberCount(patch); ++i)
        {
            const char* key = members[i].m_key;
            if (strcmp(key, "version") == 0)
            {
                continue;
            }

            result.SetNew(key, strcmp(key, "squiggleBoy") == 0 ? converted : JSON(members[i].m_value));
        }

        result.SetNew("version", arena.Integer(x_currentVersion));
        return arena.Failed() ? JSON::Null() : result;
    }
};

#pragma once

#include "JuceSon.hpp"
#include "SceneManager.hpp"
#include "SmartGridOneLayout.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

// Upgrades patch data in the message thread's load arena before publication.
//
struct PatchFormat
{
    static constexpr int x_shapeSrrPolarityVersion = 2;
    static constexpr int x_physicalModelingSrrPolarityVersion = 3;
    static constexpr int x_continuousDelayFactorVersion = 4;
    static constexpr int x_currentVersion = x_continuousDelayFactorVersion;

    enum class Scope
    {
        Unknown,
        Voice,
        Shared
    };

    enum class EncoderMigration
    {
        ReverseSrr,
        ContinuousDelayFactor
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
        static constexpr const char* x_names[] =
        {
#define F(param, shortName, mode, defaultValue, title, color, discrete, bipolar) #param,
#include "ForEachSmartGridOneParam.hpp"
#undef F
        };

        for (size_t i = 0; i < SmartGridOneLayout::x_numParams; ++i)
        {
            if (strcmp(name, x_names[i]) == 0)
            {
                return SmartGridOneLayout::GetParamMode(static_cast<SmartGridOneLayout::Param>(i)) ==
                    SmartGridOneLayout::ParamMode::Voice ? Scope::Voice : Scope::Shared;
            }
        }

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

    static bool NeedsSrrPolarityUpgrade(const char* name, int64_t version)
    {
        return (version < x_shapeSrrPolarityVersion
                && (strcmp(name, "SampleRateReductionWater") == 0
                    || strcmp(name, "SampleRateReductionFire") == 0
                    || strcmp(name, "SampleRateReductionEarth") == 0))
            || (version < x_physicalModelingSrrPolarityVersion
                && (strcmp(name, "PMSampleRateReductionWater") == 0
                    || strcmp(name, "PMSampleRateReductionFire") == 0
                    || strcmp(name, "PMSampleRateReductionEarth") == 0));
    }

    static float ReverseSrrValue(float value, bool bipolar)
    {
        return bipolar ? -value : 1.0f - value;
    }

    static bool NeedsDelayFactorUpgrade(const char* name, int64_t version)
    {
        return version < x_continuousDelayFactorVersion && strcmp(name, "DelayTimeFactor") == 0;
    }

    static float ConvertDelayFactorValue(float value)
    {
        static constexpr float x_fractions[] = {0.8f, 2.0f / 3.0f, 1.0f, 0.75f, 5.0f / 8.0f};
        size_t index = static_cast<size_t>(std::round(std::clamp(value, 0.0f, 1.0f) * 4.0f));
        return x_fractions[index];
    }

    static JSON MigrateValues(JsonArena& arena, JSON source, bool bipolar, EncoderMigration migration)
    {
        JSON result = arena.Object();
        JSON oldValues = source.Get("values");
        JSON values = arena.Array();
        for (size_t scene = 0; scene < SmartGrid::SceneManager::x_numScenes; ++scene)
        {
            float value = static_cast<float>(oldValues.GetAt(scene).NumberValue());
            float migrated = migration == EncoderMigration::ReverseSrr
                ? ReverseSrrValue(value, bipolar) : ConvertDelayFactorValue(value);
            values.AppendNew(arena.Real(migrated));
        }

        const size_t oldCount = IsArray(oldValues) ? oldValues.Size() : 0;
        for (size_t scene = SmartGrid::SceneManager::x_numScenes; scene < oldCount; ++scene)
        {
            values.AppendNew(oldValues.GetAt(scene));
        }

        result.SetNew("values", values);
        if (IsObject(source))
        {
            const JsonMember* members = Members(source);
            for (uint32_t i = 0; i < MemberCount(source); ++i)
            {
                if (strcmp(members[i].m_key, "values") != 0)
                {
                    result.SetNew(members[i].m_key, JSON(members[i].m_value));
                }
            }
        }

        return result;
    }

    static JSON MigrateCell(JsonArena& arena, JSON cell, bool bipolar, EncoderMigration migration)
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
            bool modulator = migration == EncoderMigration::ReverseSrr && strcmp(key, "modulators") == 0;
            if (strcmp(key, "values") == 0 && !value.IsNull())
            {
                value = MigrateValues(arena, value, bipolar, migration);
            }
            else if ((modulator || strcmp(key, "gestures") == 0) && IsArray(value))
            {
                // Modulation depths are signed and gesture targets inherit their parent's domain.
                //
                JSON children = arena.Array();
                for (size_t child = 0; child < value.Size(); ++child)
                {
                    children.AppendNew(MigrateCell(arena, value.GetAt(child), modulator || bipolar, migration));
                }

                value = children;
            }

            result.SetNew(key, value);
        }

        return result;
    }

    static JSON MigrateEncoderValues(JsonArena& arena, JSON encoders, int64_t sourceVersion)
    {
        if (!IsObject(encoders))
        {
            return encoders;
        }

        JSON result = arena.Object();
        const JsonMember* members = Members(encoders);
        for (uint32_t i = 0; i < MemberCount(encoders); ++i)
        {
            const char* key = members[i].m_key;
            JSON cell(members[i].m_value);
            if (NeedsSrrPolarityUpgrade(key, sourceVersion))
            {
                cell = MigrateCell(arena, cell, false, EncoderMigration::ReverseSrr);
            }
            else if (NeedsDelayFactorUpgrade(key, sourceVersion))
            {
                cell = MigrateCell(arena, cell, false, EncoderMigration::ContinuousDelayFactor);
            }

            result.SetNew(key, cell);
        }

        return result;
    }

    static JSON Upgrade(JsonArena& arena, JSON patch)
    {
        if (arena.Failed() || !IsObject(patch))
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

        int64_t sourceVersion = 0;
        if (versionCount == 1)
        {
            JSON version = patch.Get("version");
            if (!version.m_node || version.m_node->m_type != JsonType::Integer)
            {
                return JSON::Null();
            }

            sourceVersion = version.m_node->m_int;
            if (sourceVersion == x_currentVersion)
            {
                return patch;
            }

            if (sourceVersion < 0 || sourceVersion > x_currentVersion)
            {
                return JSON::Null();
            }
        }

        JSON encoders = patch.Get("squiggleBoy");
        if (sourceVersion == 0)
        {
            JSON converted = ConvertEncoders(arena, encoders);
            if (arena.Failed() || (IsObject(encoders) && converted.IsNull()))
            {
                return JSON::Null();
            }

            encoders = converted;
        }

        encoders = MigrateEncoderValues(arena, encoders, sourceVersion);

        JSON result = arena.Object();
        for (uint32_t i = 0; i < MemberCount(patch); ++i)
        {
            const char* key = members[i].m_key;
            if (strcmp(key, "version") == 0)
            {
                continue;
            }

            result.SetNew(key, strcmp(key, "squiggleBoy") == 0 ? encoders : JSON(members[i].m_value));
        }

        result.SetNew("version", arena.Integer(x_currentVersion));
        return arena.Failed() ? JSON::Null() : result;
    }
};

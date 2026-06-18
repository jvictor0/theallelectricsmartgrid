#pragma once

// Json.hpp — a self-contained, arena-backed JSON library.
//
// Replaces the former dual backend with a single implementation. Every node,
// key, and string is bump-allocated from a caller-owned JsonArena; nothing is
// freed individually — the whole arena is released (or Reset) at once. This is
// what makes audio-thread serialization (ToJSON) real-time safe: the audio
// thread only ever bumps a pointer, never calls the system allocator.
//
// Failure model: when the arena is exhausted, Alloc sets a sticky `failed` flag
// and returns null. Build/read ops are null-tolerant, so a mid-build exhaustion
// collapses the tree to a null root; callers detect failure with one check
// (arena.Failed() or root.IsNull()). The owning (message) thread then frees the
// arena, doubles its capacity, and retries.

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>

// ---------------------------------------------------------------------------
// Compatibility shims for the former jansson/JuceSon surface.
// ---------------------------------------------------------------------------
#ifndef JSON_ENCODE_ANY
#define JSON_ENCODE_ANY 0
#endif

struct json_error_t
{
    int line;
    int column;
    int position;
    char text[128];
};

// ---------------------------------------------------------------------------
// Node representation
// ---------------------------------------------------------------------------
struct JsonArena;
struct JsonNode;

enum class JsonType : uint8_t
{
    Null = 0,
    Object,
    Array,
    String,
    Integer,
    Real,
    Boolean,
};

// One object entry. Arrays store bare JsonNode* instead.
//
struct JsonMember
{
    const char* m_key;
    JsonNode* m_value;
};

// Object/Array payload: a contiguous, copy-on-grow block of entries living in
// the arena, plus a back-pointer to that arena so growth can allocate.
//
struct JsonContainer
{
    JsonArena* m_arena;
    void* m_entries;   // JsonMember[] for Object, JsonNode*[] for Array
    uint32_t m_size;
    uint32_t m_cap;
};

struct JsonNode
{
    JsonType m_type;
    union
    {
        int64_t m_int;
        double m_real;
        bool m_bool;
        const char* m_str;       // String value (arena copy, NUL-terminated)
        JsonContainer m_container;
    };
};

// ---------------------------------------------------------------------------
// JSON handle — a thin, copyable pointer into an arena. nullptr == JSON null.
// ---------------------------------------------------------------------------
struct JSON
{
    JsonNode* m_node;

    JSON()
        : m_node(nullptr)
    {
    }

    JSON(JsonNode* node)
        : m_node(node)
    {
    }

    static JSON Null()
    {
        return JSON(nullptr);
    }

    // Object manipulation
    //
    void SetNew(const char* key, JSON value);

    JSON Get(const char* key) const;

    // Array manipulation
    //
    void AppendNew(JSON value);
    void Append(JSON value);
    JSON GetAt(size_t index) const;
    size_t Size() const;

    // Value extraction — strict by type, matching the prior backends.
    //
    const char* StringValue() const
    {
        return (m_node && m_node->m_type == JsonType::String) ? m_node->m_str : nullptr;
    }

    int IntegerValue() const
    {
        return (m_node && m_node->m_type == JsonType::Integer) ? static_cast<int>(m_node->m_int) : 0;
    }

    double RealValue() const
    {
        return (m_node && m_node->m_type == JsonType::Real) ? m_node->m_real : 0.0;
    }

    double NumberValue() const
    {
        if (!m_node)
        {
            return 0.0;
        }

        if (m_node->m_type == JsonType::Real)
        {
            return m_node->m_real;
        }

        if (m_node->m_type == JsonType::Integer)
        {
            return static_cast<double>(m_node->m_int);
        }

        return 0.0;
    }

    bool BooleanValue() const
    {
        return (m_node && m_node->m_type == JsonType::Boolean) ? m_node->m_bool : false;
    }

    // Type checking
    //
    bool IsNull() const
    {
        return m_node == nullptr || m_node->m_type == JsonType::Null;
    }

    // Reference counting — no-ops; lifetime == arena lifetime.
    //
    JSON Incref() const
    {
        return *this;
    }

    void Decref() const
    {
    }

    // Serialization. Returns a malloc'd, NUL-terminated string the CALLER frees
    // (matches the former contract). nullptr on a null tree.
    //
    char* Dumps(size_t flags) const;

    // Parsing convenience kept on JSON for source-compat at call sites that have
    // an arena in scope is provided by JsonArena::Loads (see below). A JSON-level
    // static parse needs an arena, so it is intentionally NOT offered here.
};

// ---------------------------------------------------------------------------
// JsonArena — owns one contiguous buffer; bump-allocates and never frees
// individual allocations. Also the factory for all values.
// ---------------------------------------------------------------------------
struct JsonArena
{
    static constexpr size_t kDefaultCapacity = 8u * 1024u * 1024u;

    char* m_base;
    size_t m_cap;
    size_t m_off;
    bool m_failed;
    bool m_owns;

    JsonArena()
        : m_base(nullptr)
        , m_cap(0)
        , m_off(0)
        , m_failed(false)
        , m_owns(false)
    {
    }

    explicit JsonArena(size_t capacity)
        : JsonArena()
    {
        Init(capacity);
    }

    ~JsonArena()
    {
        if (m_owns)
        {
            free(m_base);
        }
    }

    JsonArena(const JsonArena&) = delete;
    JsonArena& operator=(const JsonArena&) = delete;

    JsonArena(JsonArena&& other) noexcept
        : m_base(other.m_base)
        , m_cap(other.m_cap)
        , m_off(other.m_off)
        , m_failed(other.m_failed)
        , m_owns(other.m_owns)
    {
        other.m_base = nullptr;
        other.m_cap = 0;
        other.m_off = 0;
        other.m_failed = false;
        other.m_owns = false;
    }

    // Allocate (or reallocate) the backing buffer. Heap allocation — perform on
    // the owning (message) thread, never on the audio thread.
    //
    void Init(size_t capacity)
    {
        if (m_owns)
        {
            free(m_base);
        }

        m_base = static_cast<char*>(malloc(capacity));
        m_cap = m_base ? capacity : 0;
        m_off = 0;
        m_failed = (m_base == nullptr);
        m_owns = true;
    }

    // Free the buffer and reallocate at >= double the previous capacity. The
    // message-thread half of the failure/retry loop. Invalidates every node.
    //
    void GrowAndReset()
    {
        size_t next = (m_cap ? m_cap : kDefaultCapacity) * 2;
        Init(next);
    }

    // Rewind to empty, keeping the buffer. Audio-thread safe (pointer-only).
    //
    void Reset()
    {
        m_off = 0;
        m_failed = false;
    }

    bool Failed() const
    {
        return m_failed;
    }

    size_t Capacity() const
    {
        return m_cap;
    }

    // Core bump allocation. Sets the sticky failed flag and returns nullptr when
    // the request does not fit; never calls the system allocator.
    //
    void* Alloc(size_t n, size_t align)
    {
        size_t aligned = (m_off + (align - 1)) & ~(align - 1);
        if (m_failed || aligned + n > m_cap)
        {
            m_failed = true;
            return nullptr;
        }

        void* ptr = m_base + aligned;
        m_off = aligned + n;
        return ptr;
    }

    const char* CopyBytes(const char* src, size_t n)
    {
        char* dst = static_cast<char*>(Alloc(n + 1, 1));
        if (!dst)
        {
            return nullptr;
        }

        if (n)
        {
            memcpy(dst, src, n);
        }

        dst[n] = '\0';
        return dst;
    }

    const char* CopyString(const char* src)
    {
        if (!src)
        {
            src = "";
        }

        return CopyBytes(src, strlen(src));
    }

    // Grow a container's entry block to >= double, copying existing entries.
    // Abandons the old block in place. Returns nullptr (and sets failed) on
    // exhaustion.
    //
    void* GrowEntries(void* old, uint32_t count, uint32_t& cap, size_t elemSize)
    {
        uint32_t newCap = cap < 4 ? 4 : cap * 2;
        void* block = Alloc(static_cast<size_t>(newCap) * elemSize, alignof(JsonMember));
        if (!block)
        {
            return nullptr;
        }

        if (old && count)
        {
            memcpy(block, old, static_cast<size_t>(count) * elemSize);
        }

        cap = newCap;
        return block;
    }

    JsonNode* NewNode(JsonType type)
    {
        JsonNode* node = static_cast<JsonNode*>(Alloc(sizeof(JsonNode), alignof(JsonNode)));
        if (!node)
        {
            return nullptr;
        }

        node->m_type = type;
        return node;
    }

    // ---- Factory methods: the arena IS the factory ----
    //
    JSON Object()
    {
        JsonNode* node = NewNode(JsonType::Object);
        if (!node)
        {
            return JSON::Null();
        }

        node->m_container.m_arena = this;
        node->m_container.m_entries = nullptr;
        node->m_container.m_size = 0;
        node->m_container.m_cap = 0;
        return JSON(node);
    }

    JSON Array()
    {
        JsonNode* node = NewNode(JsonType::Array);
        if (!node)
        {
            return JSON::Null();
        }

        node->m_container.m_arena = this;
        node->m_container.m_entries = nullptr;
        node->m_container.m_size = 0;
        node->m_container.m_cap = 0;
        return JSON(node);
    }

    JSON String(const char* str)
    {
        JsonNode* node = NewNode(JsonType::String);
        if (!node)
        {
            return JSON::Null();
        }

        node->m_str = CopyString(str);
        return JSON(node);
    }

    JSON Integer(int64_t value)
    {
        JsonNode* node = NewNode(JsonType::Integer);
        if (!node)
        {
            return JSON::Null();
        }

        node->m_int = value;
        return JSON(node);
    }

    JSON Real(double value)
    {
        JsonNode* node = NewNode(JsonType::Real);
        if (!node)
        {
            return JSON::Null();
        }

        node->m_real = value;
        return JSON(node);
    }

    JSON Boolean(bool value)
    {
        JsonNode* node = NewNode(JsonType::Boolean);
        if (!node)
        {
            return JSON::Null();
        }

        node->m_bool = value;
        return JSON(node);
    }

    JSON Null()
    {
        return JSON::Null();
    }

    // Parse a JSON text into this arena. Returns JSON::Null() on parse error or
    // arena exhaustion (check Failed() to distinguish exhaustion → grow+retry).
    //
    JSON Loads(const char* input, size_t flags = 0, json_error_t* error = nullptr);
};

// ---------------------------------------------------------------------------
// JSON method bodies that need the full JsonArena/JsonNode definitions.
// ---------------------------------------------------------------------------
inline void JSON::SetNew(const char* key, JSON value)
{
    if (!m_node || m_node->m_type != JsonType::Object)
    {
        return;
    }

    JsonContainer& c = m_node->m_container;
    if (c.m_size == c.m_cap)
    {
        uint32_t newCap = c.m_cap;
        void* block = c.m_arena->GrowEntries(c.m_entries, c.m_size, newCap, sizeof(JsonMember));
        if (!block)
        {
            return;
        }

        c.m_entries = block;
        c.m_cap = newCap;
    }

    JsonMember* members = static_cast<JsonMember*>(c.m_entries);
    members[c.m_size].m_key = c.m_arena->CopyString(key);
    members[c.m_size].m_value = value.m_node;
    c.m_size++;
}

inline JSON JSON::Get(const char* key) const
{
    if (!m_node || m_node->m_type != JsonType::Object)
    {
        return JSON::Null();
    }

    const JsonContainer& c = m_node->m_container;
    const JsonMember* members = static_cast<const JsonMember*>(c.m_entries);
    for (uint32_t i = 0; i < c.m_size; ++i)
    {
        if (members[i].m_key && strcmp(members[i].m_key, key) == 0)
        {
            return JSON(members[i].m_value);
        }
    }

    return JSON::Null();
}

inline void JSON::Append(JSON value)
{
    if (!m_node || m_node->m_type != JsonType::Array)
    {
        return;
    }

    JsonContainer& c = m_node->m_container;
    if (c.m_size == c.m_cap)
    {
        uint32_t newCap = c.m_cap;
        void* block = c.m_arena->GrowEntries(c.m_entries, c.m_size, newCap, sizeof(JsonNode*));
        if (!block)
        {
            return;
        }

        c.m_entries = block;
        c.m_cap = newCap;
    }

    static_cast<JsonNode**>(c.m_entries)[c.m_size] = value.m_node;
    c.m_size++;
}

inline void JSON::AppendNew(JSON value)
{
    Append(value);
}

inline JSON JSON::GetAt(size_t index) const
{
    if (!m_node || m_node->m_type != JsonType::Array)
    {
        return JSON::Null();
    }

    const JsonContainer& c = m_node->m_container;
    if (index >= c.m_size)
    {
        return JSON::Null();
    }

    return JSON(static_cast<JsonNode* const*>(c.m_entries)[index]);
}

inline size_t JSON::Size() const
{
    if (!m_node || (m_node->m_type != JsonType::Array && m_node->m_type != JsonType::Object))
    {
        return 0;
    }

    return m_node->m_container.m_size;
}

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------
namespace JsonDetail
{

inline void DumpString(std::string& out, const char* s)
{
    out.push_back('"');
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p)
    {
        unsigned char c = *p;
        switch (c)
        {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20)
                {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                }
                else
                {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    out.push_back('"');
}

inline void DumpValue(std::string& out, const JsonNode* node)
{
    if (!node)
    {
        out += "null";
        return;
    }

    switch (node->m_type)
    {
        case JsonType::Null:
            out += "null";
            break;
        case JsonType::Boolean:
            out += node->m_bool ? "true" : "false";
            break;
        case JsonType::Integer:
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(node->m_int));
            out += buf;
            break;
        }
        case JsonType::Real:
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.17g", node->m_real);
            out += buf;
            break;
        }
        case JsonType::String:
            DumpString(out, node->m_str ? node->m_str : "");
            break;
        case JsonType::Array:
        {
            out.push_back('[');
            const JsonContainer& c = node->m_container;
            JsonNode* const* values = static_cast<JsonNode* const*>(c.m_entries);
            for (uint32_t i = 0; i < c.m_size; ++i)
            {
                if (i)
                {
                    out.push_back(',');
                }
                DumpValue(out, values[i]);
            }
            out.push_back(']');
            break;
        }
        case JsonType::Object:
        {
            out.push_back('{');
            const JsonContainer& c = node->m_container;
            const JsonMember* members = static_cast<const JsonMember*>(c.m_entries);
            for (uint32_t i = 0; i < c.m_size; ++i)
            {
                if (i)
                {
                    out.push_back(',');
                }
                DumpString(out, members[i].m_key ? members[i].m_key : "");
                out.push_back(':');
                DumpValue(out, members[i].m_value);
            }
            out.push_back('}');
            break;
        }
    }
}

} // namespace JsonDetail

inline char* JSON::Dumps(size_t flags) const
{
    if (!m_node)
    {
        return nullptr;
    }

    std::string out;
    JsonDetail::DumpValue(out, m_node);

    char* result = static_cast<char*>(malloc(out.size() + 1));
    if (!result)
    {
        return nullptr;
    }

    memcpy(result, out.data(), out.size());
    result[out.size()] = '\0';
    return result;
}

// ---------------------------------------------------------------------------
// Parsing (recursive descent)
// ---------------------------------------------------------------------------
namespace JsonDetail
{

struct Parser
{
    const char* m_p;
    JsonArena* m_arena;
    bool m_error;

    Parser(const char* p, JsonArena* arena)
        : m_p(p)
        , m_arena(arena)
        , m_error(false)
    {
    }

    void SkipWs()
    {
        while (*m_p == ' ' || *m_p == '\t' || *m_p == '\n' || *m_p == '\r')
        {
            ++m_p;
        }
    }

    JSON Fail()
    {
        m_error = true;
        return JSON::Null();
    }

    static void EncodeUtf8(std::string& out, uint32_t cp)
    {
        if (cp < 0x80)
        {
            out.push_back(static_cast<char>(cp));
        }
        else if (cp < 0x800)
        {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else if (cp < 0x10000)
        {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else
        {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    bool ParseHex4(uint32_t& out)
    {
        out = 0;
        for (int i = 0; i < 4; ++i)
        {
            char c = *m_p;
            uint32_t d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else return false;
            out = (out << 4) | d;
            ++m_p;
        }
        return true;
    }

    // Parses a string literal (m_p is at the opening quote) into the arena.
    // Returns the arena copy, or nullptr on error/exhaustion.
    //
    const char* ParseRawString()
    {
        if (*m_p != '"')
        {
            m_error = true;
            return nullptr;
        }
        ++m_p;

        std::string buf;
        while (*m_p && *m_p != '"')
        {
            unsigned char c = static_cast<unsigned char>(*m_p);
            if (c == '\\')
            {
                ++m_p;
                switch (*m_p)
                {
                    case '"':  buf.push_back('"');  ++m_p; break;
                    case '\\': buf.push_back('\\'); ++m_p; break;
                    case '/':  buf.push_back('/');  ++m_p; break;
                    case 'b':  buf.push_back('\b'); ++m_p; break;
                    case 'f':  buf.push_back('\f'); ++m_p; break;
                    case 'n':  buf.push_back('\n'); ++m_p; break;
                    case 'r':  buf.push_back('\r'); ++m_p; break;
                    case 't':  buf.push_back('\t'); ++m_p; break;
                    case 'u':
                    {
                        ++m_p;
                        uint32_t cp;
                        if (!ParseHex4(cp))
                        {
                            m_error = true;
                            return nullptr;
                        }
                        if (cp >= 0xD800 && cp <= 0xDBFF)
                        {
                            // High surrogate; expect a low surrogate.
                            //
                            if (m_p[0] == '\\' && m_p[1] == 'u')
                            {
                                m_p += 2;
                                uint32_t lo;
                                if (!ParseHex4(lo))
                                {
                                    m_error = true;
                                    return nullptr;
                                }
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            }
                        }
                        EncodeUtf8(buf, cp);
                        break;
                    }
                    default:
                        m_error = true;
                        return nullptr;
                }
            }
            else
            {
                buf.push_back(static_cast<char>(c));
                ++m_p;
            }
        }

        if (*m_p != '"')
        {
            m_error = true;
            return nullptr;
        }
        ++m_p;

        const char* copied = m_arena->CopyBytes(buf.data(), buf.size());
        if (!copied)
        {
            m_error = true;  // arena exhaustion
        }
        return copied;
    }

    JSON ParseValue()
    {
        SkipWs();
        switch (*m_p)
        {
            case '{': return ParseObject();
            case '[': return ParseArray();
            case '"':
            {
                const char* s = ParseRawString();
                if (m_error)
                {
                    return JSON::Null();
                }
                JsonNode* node = m_arena->NewNode(JsonType::String);
                if (!node)
                {
                    return Fail();
                }
                node->m_str = s;
                return JSON(node);
            }
            case 't':
                if (strncmp(m_p, "true", 4) == 0) { m_p += 4; JsonNode* n = m_arena->NewNode(JsonType::Boolean); if (!n) return Fail(); n->m_bool = true; return JSON(n); }
                return Fail();
            case 'f':
                if (strncmp(m_p, "false", 5) == 0) { m_p += 5; JsonNode* n = m_arena->NewNode(JsonType::Boolean); if (!n) return Fail(); n->m_bool = false; return JSON(n); }
                return Fail();
            case 'n':
                if (strncmp(m_p, "null", 4) == 0) { m_p += 4; JsonNode* n = m_arena->NewNode(JsonType::Null); if (!n) return Fail(); return JSON(n); }
                return Fail();
            default:
                return ParseNumber();
        }
    }

    JSON ParseNumber()
    {
        const char* start = m_p;
        bool isReal = false;

        if (*m_p == '-')
        {
            ++m_p;
        }

        while ((*m_p >= '0' && *m_p <= '9'))
        {
            ++m_p;
        }
        if (*m_p == '.')
        {
            isReal = true;
            ++m_p;
            while (*m_p >= '0' && *m_p <= '9') ++m_p;
        }
        if (*m_p == 'e' || *m_p == 'E')
        {
            isReal = true;
            ++m_p;
            if (*m_p == '+' || *m_p == '-') ++m_p;
            while (*m_p >= '0' && *m_p <= '9') ++m_p;
        }

        if (m_p == start || (m_p == start + 1 && start[0] == '-'))
        {
            return Fail();
        }

        if (isReal)
        {
            JsonNode* node = m_arena->NewNode(JsonType::Real);
            if (!node) return Fail();
            node->m_real = strtod(start, nullptr);
            return JSON(node);
        }

        JsonNode* node = m_arena->NewNode(JsonType::Integer);
        if (!node) return Fail();
        node->m_int = static_cast<int64_t>(strtoll(start, nullptr, 10));
        return JSON(node);
    }

    JSON ParseArray()
    {
        ++m_p; // '['
        JSON arr = m_arena->Array();
        if (arr.IsNull())
        {
            return Fail();
        }

        SkipWs();
        if (*m_p == ']')
        {
            ++m_p;
            return arr;
        }

        while (true)
        {
            JSON value = ParseValue();
            if (m_error)
            {
                return JSON::Null();
            }
            arr.Append(value);
            if (m_arena->Failed())
            {
                return Fail();
            }

            SkipWs();
            if (*m_p == ',')
            {
                ++m_p;
                continue;
            }
            if (*m_p == ']')
            {
                ++m_p;
                return arr;
            }
            return Fail();
        }
    }

    JSON ParseObject()
    {
        ++m_p; // '{'
        JSON obj = m_arena->Object();
        if (obj.IsNull())
        {
            return Fail();
        }

        SkipWs();
        if (*m_p == '}')
        {
            ++m_p;
            return obj;
        }

        while (true)
        {
            SkipWs();
            const char* key = ParseRawString();
            if (m_error)
            {
                return JSON::Null();
            }

            SkipWs();
            if (*m_p != ':')
            {
                return Fail();
            }
            ++m_p;

            JSON value = ParseValue();
            if (m_error)
            {
                return JSON::Null();
            }

            // Insert directly (key already arena-copied) to avoid re-copying.
            //
            JsonContainer& c = obj.m_node->m_container;
            if (c.m_size == c.m_cap)
            {
                uint32_t newCap = c.m_cap;
                void* block = c.m_arena->GrowEntries(c.m_entries, c.m_size, newCap, sizeof(JsonMember));
                if (!block)
                {
                    return Fail();
                }
                c.m_entries = block;
                c.m_cap = newCap;
            }
            JsonMember* members = static_cast<JsonMember*>(c.m_entries);
            members[c.m_size].m_key = key;
            members[c.m_size].m_value = value.m_node;
            c.m_size++;

            SkipWs();
            if (*m_p == ',')
            {
                ++m_p;
                continue;
            }
            if (*m_p == '}')
            {
                ++m_p;
                return obj;
            }
            return Fail();
        }
    }
};

} // namespace JsonDetail

inline JSON JsonArena::Loads(const char* input, size_t flags, json_error_t* error)
{
    if (error)
    {
        error->line = error->column = error->position = 0;
        error->text[0] = '\0';
    }

    if (!input)
    {
        return JSON::Null();
    }

    JsonDetail::Parser parser(input, this);
    JSON root = parser.ParseValue();
    if (parser.m_error || m_failed)
    {
        return JSON::Null();
    }

    parser.SkipWs();
    if (*parser.m_p != '\0')
    {
        // Trailing garbage after a complete value.
        //
        return JSON::Null();
    }

    return root;
}

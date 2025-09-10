#pragma once

extern "C"
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-volatile"
    #include <jansson.h>
#pragma clang diagnostic pop
}

struct JSON
{
    json_t* m_json;

    JSON(json_t* json)
        : m_json(json)
    {
    }    

    // Static factory methods
    //
    static JSON Object()
    {
        return JSON(json_object());
    }

    static JSON Array()
    {
        return JSON(json_array());
    }
    
    static JSON String(const char* str)
    {
        return JSON(json_string(str));
    }
    
    static JSON Integer(int64_t value)
    {
        return JSON(json_integer(value));
    }
    
    static JSON Real(double value)
    {
        return JSON(json_real(value));
    }
    
    static JSON Boolean(bool value)
    {
        return JSON(json_boolean(value));
    }
    
    static JSON Null()
    {
        return JSON(json_null());
    }

    // Object manipulation
    //
    void SetNew(const char* key, JSON value)
    {
        if (m_json)
        {
            json_object_set_new(m_json, key, static_cast<json_t*>(value.ToJsonT()));
        }
    }
    
    JSON Get(const char* key) const
    {
        if (m_json)
        {
            json_t* value = json_object_get(m_json, key);
            if (value)
            {
                return JSON(value);
            }
        }
        return JSON(nullptr);
    }

    // Array manipulation
    //
    void AppendNew(JSON value)
    {
        if (m_json)
        {
            json_array_append_new(m_json, static_cast<json_t*>(value.ToJsonT()));
        }
    }
    
    void Append(JSON value)
    {
        if (m_json)
        {
            json_array_append(m_json, static_cast<json_t*>(value.ToJsonT()));
        }
    }
    
    JSON GetAt(size_t index) const
    {
        if (m_json)
        {
            json_t* value = json_array_get(m_json, index);
            if (value)
            {
                return JSON(value);
            }
        }
        return JSON(nullptr);
    }
    
    size_t Size() const
    {
        if (m_json)
        {
            return json_array_size(m_json);
        }
        return 0;
    }

    // Value extraction
    //
    const char* StringValue() const
    {
        if (m_json)
        {
            return json_string_value(m_json);
        }
        return nullptr;
    }
    
    int IntegerValue() const
    {
        if (m_json)
        {
            return json_integer_value(m_json);
        }
        return 0;
    }
    
    double RealValue() const
    {
        if (m_json)
        {
            return json_real_value(m_json);
        }
        return 0.0;
    }
    
    bool BooleanValue() const
    {
        if (m_json)
        {
            return json_boolean_value(m_json);
        }
        return false;
    }

    // Type checking
    //
    bool IsNull() const
    {
        return m_json == nullptr || json_is_null(m_json);
    }
    
    // Reference counting
    //
    JSON Incref() const
    {
        if (m_json)
        {
            return JSON(json_incref(m_json));
        }
        return JSON(nullptr);
    }
    
    void Decref() const
    {
        if (m_json)
        {
            json_decref(m_json);
        }
    }
    
    // JSON parsing and serialization
    //
    static JSON Loads(const char* input, size_t flags, json_error_t* error)
    {
        json_t* json = json_loads(input, flags, error);
        return JSON(json);
    }
    
    char* Dumps(size_t flags) const
    {
        if (m_json)
        {
            return json_dumps(m_json, flags);
        }
        return nullptr;
    }
    
    // Conversion to/from json_t* for dataToJson/dataFromJson compatibility
    static JSON FromJsonT(void* jsonPtr)
    {
        return JSON(static_cast<json_t*>(jsonPtr));
    }
    
    void* ToJsonT() const
    {
        return m_json;
    }
};


#pragma once

#include <JuceHeader.h>

// Error type for compatibility
//
struct json_error_t
{
    int line;
    int column;
    int position;
    char text[128];
};

// JSON encoding flags
//
#define JSON_ENCODE_ANY 0

struct JSON
{
    juce::var m_var;

    JSON(juce::var var)
        : m_var(var)
    {
    }    

    // Static factory methods
    //
    static JSON Object()
    {
        return JSON(juce::var(new juce::DynamicObject()));
    }

    static JSON Array()
    {
        return JSON(juce::var(juce::Array<juce::var>()));
    }
    
    static JSON String(const char* str)
    {
        return JSON(juce::var(juce::String(str)));
    }
    
    static JSON Integer(int64_t value)
    {
        return JSON(juce::var(static_cast<int>(value)));
    }
    
    static JSON Real(double value)
    {
        return JSON(juce::var(value));
    }
    
    static JSON Boolean(bool value)
    {
        return JSON(juce::var(value));
    }
    
    static JSON Null()
    {
        return JSON(juce::var());
    }

    // Object manipulation
    //
    void SetNew(const char* key, JSON value)
    {
        if (m_var.isObject())
        {
            m_var.getDynamicObject()->setProperty(key, value.m_var);
        }
    }
    
    JSON Get(const char* key) const
    {
        if (m_var.isObject())
        {
            juce::var value = m_var.getDynamicObject()->getProperty(key);
            if (!value.isVoid())
            {
                return JSON(value);
            }
        }
        return JSON(juce::var());
    }

    // Array manipulation
    //
    void AppendNew(JSON value)
    {
        if (m_var.isArray())
        {
            m_var.getArray()->add(value.m_var);
        }
    }
    
    void Append(JSON value)
    {
        if (m_var.isArray())
        {
            m_var.getArray()->add(value.m_var);
        }
    }
    
    JSON GetAt(size_t index) const
    {
        if (m_var.isArray())
        {
            juce::Array<juce::var>* arr = m_var.getArray();
            if (index < static_cast<size_t>(arr->size()))
            {
                return JSON(arr->getReference(static_cast<int>(index)));
            }
        }
        return JSON(juce::var());
    }
    
    size_t Size() const
    {
        if (m_var.isArray())
        {
            return static_cast<size_t>(m_var.getArray()->size());
        }
        return 0;
    }

    // Value extraction
    //
    const char* StringValue() const
    {
        if (m_var.isString())
        {
            static juce::String temp;
            temp = m_var.toString();
            return temp.toUTF8().getAddress();
        }
        return nullptr;
    }
    
    int IntegerValue() const
    {
        if (m_var.isInt())
        {
            return m_var;
        }
        return 0;
    }
    
    double RealValue() const
    {
        if (m_var.isDouble())
        {
            return m_var;
        }
        return 0.0;
    }
    
    bool BooleanValue() const
    {
        if (m_var.isBool())
        {
            return m_var;
        }
        return false;
    }

    // Type checking
    //
    bool IsNull() const
    {
        return m_var.isVoid();
    }
    
    // Reference counting (no-ops)
    //
    JSON Incref() const
    {
        return *this;
    }
    
    void Decref() const
    {
        // No-op
    }
    
    // JSON parsing and serialization
    //
    static JSON Loads(const char* input, size_t flags, json_error_t* error)
    {
        juce::String inputStr(input);
        juce::var parsed;
        juce::Result result = juce::JSON::parse(inputStr, parsed);
        
        if (result.wasOk())
        {
            return JSON(parsed);
        }
        return JSON(juce::var());
    }
    
    char* Dumps(size_t flags) const
    {
        juce::String jsonStr = juce::JSON::toString(m_var);
        char* result = static_cast<char*>(malloc(jsonStr.length() + 1));
        strcpy(result, jsonStr.toUTF8().getAddress());
        return result;
    }
    
    // Conversion to/from json_t* for dataToJson/dataFromJson compatibility
    // These are no-ops in JuceSon since we don't use json_t*
    static JSON FromJsonT(void* jsonPtr)
    {
        // This should never be called in JuceSon context
        return JSON(juce::var());
    }
    
    void* ToJsonT() const
    {
        // This should never be called in JuceSon context
        return nullptr;
    }
};

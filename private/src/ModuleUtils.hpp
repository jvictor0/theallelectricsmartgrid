#pragma once

#ifndef IOS_BUILD

#include "plugin.hpp"
#include <vector>
#include <string>
#include <memory>
        
struct IOMgr
{
    static float Clamp(float min, float max, float value)
    {
        return std::min(std::max(min, value), max);
    }

    static float Exp(float value, float base=30)
    {
        return (pow(base, value) - 1) / (base - 1);
    }
    
    enum class Type : int
    {
        Unknown, 
        Float,
        UInt64,
        UInt32,
        UInt8,
        SizeT,
        Bool
    };

    enum class TransposeMode
    {
        None,
        Scale,
        ScaleOffset
    };

    static TransposeMode GetTransposeMode(float offset, float scale)
    {
        if (offset == 0 && scale != 1)
        {
            return TransposeMode::Scale;
        }
        else if (offset != 0 && scale != 1)
        {
            return TransposeMode::ScaleOffset;
        }
        else
        {
            return TransposeMode::None;
        }
    }

    struct Value
    {
        Type m_type;
        void* m_value[16];
        int m_channels;

        Value()
        {
            m_type = Type::Unknown;
            m_channels = 1;
            for (int i = 0; i < 16; i++)
            {
                m_value[i] = nullptr;
            }
        }

        void Set(int channel, float* value)
        {
            m_value[channel] = value;
            m_type = Type::Float;
        }

        void Set(int channel, uint64_t* value)
        {
            m_value[channel] = value;
            m_type = Type::UInt64;
        }

        void Set(int channel, uint32_t* value)
        {
            m_value[channel] = value;
            m_type = Type::UInt32;
        }

        void Set(int channel, size_t* value)
        {
            m_value[channel] = value;
            m_type = Type::SizeT;
        }

        void Set(int channel, bool* value)
        {
            m_value[channel] = value;
            m_type = Type::Bool;
        }

        void Set(int channel, uint8_t* value)
        {
            m_value[channel] = value;
            m_type = Type::UInt8;
        }

        void SetChannels(int channels)
        {
            m_channels = channels;
        }

        float ToFloat(int channel)
        {
            switch (m_type)
            {
                case Type::Float:
                    return *static_cast<float*>(m_value[channel]);
                case Type::UInt64:
                    return *static_cast<uint64_t*>(m_value[channel]);
                case Type::UInt32:
                    return *static_cast<uint32_t*>(m_value[channel]);
                case Type::SizeT:
                    return *static_cast<size_t*>(m_value[channel]);
                case Type::Bool:
                    return *static_cast<bool*>(m_value[channel]) ? 10 : 0;
                case Type::UInt8:
                    return *static_cast<uint8_t*>(m_value[channel]);
                default:
                    return 0;
            }
        }

        void FromFloat(int channel, float value)
        {
            switch (m_type)
            {
                case Type::Float:
                    *static_cast<float*>(m_value[channel]) = value;
                    break;
                case Type::UInt64:
                    *static_cast<uint64_t*>(m_value[channel]) = value;
                    break;
                case Type::UInt32:
                    *static_cast<uint32_t*>(m_value[channel]) = value;
                    break;
                case Type::SizeT:
                    *static_cast<size_t*>(m_value[channel]) = value;
                    break;
                case Type::Bool:
                    *static_cast<bool*>(m_value[channel]) = value >= 1;
                    break;
                case Type::UInt8:
                    *static_cast<uint8_t*>(m_value[channel]) = value;
                    break;
                default:
                    break;
            }
        }
    };

    struct Input
    {
        int m_id;
        Value m_value;
        std::string m_name;
        Module* m_module;
        float m_offset;
        float m_scale;
        bool m_isAudio;
        TransposeMode m_transposeMode;
        
        Input(int id, Module* module, std::string name, bool isAudio)
        {
            m_id = id;
            m_module = module;
            m_name = name;
            m_offset = 0;
            m_scale = 1;
            m_isAudio = isAudio;
            m_transposeMode = TransposeMode::None;
        }

        void SetTransposeMode()
        {
            m_transposeMode = GetTransposeMode(m_offset, m_scale);
        }
        
        void SetTarget(int id, float* target)
        {
            m_value.Set(id, target);
        }

        void SetTarget(int id, uint64_t* target)
        {
            m_value.Set(id, target);
        }

        void SetTarget(int id, uint32_t* target)
        {
            m_value.Set(id, target);
        }

        void SetTarget(int id, size_t* target)
        {
            m_value.Set(id, target);
        }

        void SetTarget(int id, bool* target)
        {
            m_value.Set(id, target);
        }

        void SetTarget(int id, uint8_t* target)
        {
            m_value.Set(id, target);
        }

        void SetChannels(int channels)
        {
            m_value.SetChannels(channels);
        }

        void SetInput()
        {
            m_value.m_channels = m_module->inputs[m_id].getChannels();

#pragma unroll(16)
            for (int i = 0; i < m_value.m_channels; i++)
            {
                if (!m_value.m_value[i])
                {
                    break;
                }
                
                m_value.FromFloat(i, m_scale * m_module->inputs[m_id].getVoltage(i) + m_offset);
            }
        }

        float Get(int channel)
        {
            return m_scale * m_module->inputs[m_id].getVoltage(channel) + m_offset;
        }

        void Config()
        {
            m_module->configInput(m_id, m_name.c_str());
            SetTransposeMode();
        }

        void Widget(ModuleWidget* widget, int x, int y)
        {
            widget->addInput(createInputCentered<ThemedPJ301MPort>(Vec(25 * x, 25 * y), m_module, m_id));
        }
    };
    
    struct Output
    {        
        int m_id;
        Value m_value;
        std::string m_name;
        Module* m_module;
        bool m_connected;
        float m_offset;
        float m_scale;
        TransposeMode m_transposeMode;
        
        Output(int id, Module* module, std::string name)
        {
            m_id = id;
            m_module = module;
            m_name = name;
            m_connected = false;
            m_offset = 0;
            m_scale = 1;
            m_transposeMode = TransposeMode::None;
        }

        void SetTransposeMode()
        {
            m_transposeMode = GetTransposeMode(m_offset, m_scale);
        }
        
        void SetSource(int id, float* src)
        {
            m_value.Set(id, src);
        }
        
        void SetSource(int id, uint64_t* src)
        {
            m_value.Set(id, src);
        }

        void SetSource(int id, uint32_t* src)
        {
            m_value.Set(id, src);
        }

        void SetSource(int id, size_t* src)
        {
            m_value.Set(id, src);
        }

        void SetSource(int id, bool* src)
        {
            m_value.Set(id, src);
        }

        void SetChannels(int channels)
        {
            m_value.SetChannels(channels);
        }
        
        void SetOutput()
        {
            m_connected = m_module->outputs[m_id].isConnected();
            if (!m_connected)
            {
                return;
            }
            
            m_module->outputs[m_id].setChannels(m_value.m_channels);

#pragma unroll(16)
            for (int i = 0; i < m_value.m_channels; i++)
            {
                m_module->outputs[m_id].setVoltage(m_value.ToFloat(i) * m_scale + m_offset, i);
            }
        }
        
        void Config()
        {
            m_module->configOutput(m_id, m_name.c_str());
            SetTransposeMode();
        }

        void Widget(ModuleWidget* widget, int x, int y)
        {
            widget->addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(25 * x, 25 * y), m_module, m_id));
        }        
    };

    struct Trigger
    {
        Input* m_input;
        bool m_lastValue[16];
        bool m_value[16];
        bool* m_trigger[16];
        
        Trigger(Input* input)
            : m_input(input)
        {
            for (int i = 0; i < 16; i++)
            {
                m_trigger[i] = nullptr;
                m_lastValue[i] = false;
                m_value[i] = false;
                m_input->SetTarget(i, &m_value[i]);
            }
        }

        void SetTrigger(int channel, bool* trigger)
        {
            m_trigger[channel] = trigger;
        }

        void Process()
        {
            for (int i = 0; i < m_input->m_value.m_channels; i++)
            {
                if (m_trigger[i])
                {
                    if (!m_lastValue[i] && m_value[i])
                    {
                        *m_trigger[i] = true;
                    }
                    else
                    {
                        *m_trigger[i] = false;
                    }
                }

                m_lastValue[i] = m_value[i];
            }
        }

        void Clear()
        {
            for (int i = 0; i < m_input->m_value.m_channels; i++)
            {
                m_value[i] = false;
            }
        }

        void Widget(ModuleWidget* widget, int x, int y)
        {
            m_input->Widget(widget, x, y);
        }
    };

    struct RandomAccessSwitch
    {
        bool m_triggers[16];
        size_t m_numTriggers;
        size_t* m_current;

        RandomAccessSwitch(size_t* current)
            : m_numTriggers(0)
            , m_current(current)
        {
        }

        bool* AddTrigger()
        {
            m_triggers[m_numTriggers] = false;
            ++m_numTriggers;
            return &m_triggers[m_numTriggers - 1];
        }

        void Process()
        {
            for (size_t i = 0; i < m_numTriggers; i++)
            {
                if (m_triggers[i])
                {
                    *m_current = i;
                }
            }
        }
    };

    struct Param
    {
        int m_id;
        Value m_value;
        std::string m_name;
        Module* m_module;
        float m_min;
        float m_max;
        float m_default;

        float m_cvValue[16];
        float m_cvScaleValue;

        Input* m_cv;
        Param* m_cvScale;
        bool m_switch;

        enum class CVMode
        {
            Offset,
            Override
        };

        CVMode m_cvMode;

        Param(int id, std::string name, Module* mod, float min, float max, float def)
            : m_id(id)
            , m_name(name)
            , m_module(mod)
            , m_min(min)
            , m_max(max)
            , m_default(def)
            , m_cv(nullptr)
            , m_cvScale(nullptr)
            , m_switch(false)
            , m_cvMode(CVMode::Offset)
        {
        }

        void SetTarget(int id, float* target)
        {
            m_value.Set(id, target);
        }

        void SetTarget(int id, uint64_t* target)
        {
            m_value.Set(id, target);
        }

        void SetTarget(int id, uint32_t* target)
        {
            m_value.Set(id, target);
        }

        void SetTarget(int id, size_t* target)
        {
            m_value.Set(id, target);
        }

        void SetTarget(int id, bool* target)
        {
            m_value.Set(id, target);
        }

        void SetTarget(int id, uint8_t* target)
        {
            m_value.Set(id, target);
        }

        void SetCV(Input* cv)
        {
            m_cv = cv;
            for (int i = 0; i < 16; i++)
            {
                cv->SetTarget(i, &m_cvValue[i]);
            }
        }

        void SetCVScale(Param* cvScale)
        {
            m_cvScale = cvScale;
            m_cvScale->SetTarget(0, &m_cvScaleValue);
        };

        void SetParam()
        {
            float value = m_module->params[m_id].getValue();
            if (m_cv)
            {
                if (m_cvScale)
                {
                    m_cvScale->SetParam();
                }
                else
                {
                    m_cvScaleValue = 1;
                }
                
                m_cv->SetInput();
                m_value.m_channels = m_cv->m_value.m_channels;
                for (int i = 0; i < m_value.m_channels; i++)
                {
                    if (m_cvMode == CVMode::Offset)
                    {
                        m_value.FromFloat(i, Clamp(m_min, m_max, value + m_cvValue[i] * m_cvScaleValue));
                    }
                    else
                    {
                        m_value.FromFloat(i, Clamp(m_min, m_max, m_cvValue[i] * m_cvScaleValue));
                    }
                }
            }
            else
            {
                m_value.m_channels = 1;
                m_value.FromFloat(0, Clamp(m_min, m_max, value));
            }
        }

        void Config()
        {
            if (m_switch)
            {   
                m_module->configSwitch(m_id, m_min, m_max, m_default, m_name.c_str());
            }
            else
            {   
                m_module->configParam(m_id, m_min, m_max, m_default, m_name.c_str());
            }
        }

        void Widget(ModuleWidget* widget, int x, int y)
        {
            widget->addParam(createParamCentered<Trimpot>(Vec(25 * x, 25 * y), m_module, m_id));
        }
    };
                                    
    
    Module* m_module;
    std::vector<std::unique_ptr<Output>> m_outputs;
    std::vector<std::unique_ptr<Input>> m_inputs;
    std::vector<std::unique_ptr<Trigger>> m_triggers;
    std::vector<std::unique_ptr<RandomAccessSwitch>> m_switches;
    std::vector<std::unique_ptr<Param>> m_params;
    
    bool m_firstFrame;
    size_t m_frame;
    size_t m_controlFrames;

    std::vector<Output*> m_audioOutputs;
    std::vector<Output*> m_cvOutputs;
    std::vector<Input*> m_audioInputs;
    std::vector<Input*> m_cvInputs;
    std::vector<Trigger*> m_audioTriggers;
    std::vector<Trigger*> m_cvTriggers;
    std::vector<RandomAccessSwitch*> m_audioSwitches;
    std::vector<RandomAccessSwitch*> m_cvSwitches;
    std::vector<Param*> m_audioParams;
    std::vector<Param*> m_cvParams;
    

    IOMgr(Module* module)
        : m_module(module)
        , m_firstFrame(true)
        , m_frame(0)
        , m_controlFrames(64)
    {
    }

    Output* AddOutput(std::string name, bool isAudio)
    {
        m_outputs.push_back(std::unique_ptr<Output>(new Output(m_outputs.size(), m_module, name)));
        if (isAudio)
        {
            m_audioOutputs.push_back(m_outputs.back().get());
        }
        else
        {
            m_cvOutputs.push_back(m_outputs.back().get());
        }
        
        return m_outputs.back().get();
    }

    Input* AddInput(std::string name, bool isAudio)
    {
        m_inputs.push_back(std::unique_ptr<Input>(new Input(m_inputs.size(), m_module, name, isAudio)));
        if (isAudio)
        {
            m_audioInputs.push_back(m_inputs.back().get());
        }
        else
        {
            m_cvInputs.push_back(m_inputs.back().get());
        }
        
        return m_inputs.back().get();
    }

    Trigger* AddTrigger(std::string name, bool isAudio)
    {
        Input* input = AddInput(name, isAudio);
        m_triggers.push_back(std::unique_ptr<Trigger>(new Trigger(input)));
        if (isAudio)
        {
            m_audioTriggers.push_back(m_triggers.back().get());
        }
        else
        {
            m_cvTriggers.push_back(m_triggers.back().get());
        }
        
        return m_triggers.back().get();
    }

    RandomAccessSwitch* AddRandomAccessSwitch(size_t* current, bool isAudio)
    {
        m_switches.push_back(std::unique_ptr<RandomAccessSwitch>(new RandomAccessSwitch(current)));
        if (isAudio)
        {
            m_audioSwitches.push_back(m_switches.back().get());
        }
        else
        {
            m_cvSwitches.push_back(m_switches.back().get());
        }
        
        return m_switches.back().get();
    }

    Param* AddParam(std::string name, float min, float max, float def, bool isAudio)
    {
        m_params.push_back(std::unique_ptr<Param>(new Param(m_params.size(), name, m_module, min, max, def)));
        if (isAudio)
        {
            m_audioParams.push_back(m_params.back().get());
        }
        else
        {
            m_cvParams.push_back(m_params.back().get());
        }

        return m_params.back().get();
    }

    void Config()
    {
        m_module->config(m_params.size(), m_inputs.size(), m_outputs.size(), 0);
        for (auto& output : m_outputs)
        {
            output->Config();
        }

        for (auto& input : m_inputs)
        {
            input->Config();
        }

        for (auto& params : m_params)
        {
            params->Config();
        }
    }

    void SetAudioOutputs()
    {
        for (auto& output : m_audioOutputs)
        {
            output->SetOutput();
        }
    }

    void SetCVOutputs()
    {
        for (auto& output : m_cvOutputs)
        {
            output->SetOutput();
        }
    }

    void SetOutputs()
    {
        SetAudioOutputs();
        if (IsControlFrame())
        {
            SetCVOutputs();
        }
    }

    void SetAudioInputs()
    {
        for (auto& input : m_audioInputs)
        {
            input->SetInput();
        }

        for (auto& trigger : m_audioTriggers)
        {
            trigger->Process();
        }

        for (auto& sw : m_audioSwitches)
        {
            sw->Process();
        }
    }

    void SetCVInputs()
    {
        for (auto& input : m_cvInputs)
        {
            input->SetInput();
        }

        for (auto& trigger : m_cvTriggers)
        {
            trigger->Process();
        }

        for (auto& sw : m_cvSwitches)
        {
            sw->Process();
        }
    }

    void SetInputs()
    {
        SetAudioInputs();
        if (IsControlFrame())
        {
            SetCVInputs();
        }
    }

    void SetAudioParams()
    {
        for (auto& param : m_audioParams)
        {
            param->SetParam();
        }
    }

    void SetCVParams()
    {
        for (auto& param : m_cvParams)
        {
            param->SetParam();
        }
    }

    void SetParams()
    {
        SetAudioParams();
        if (IsControlFrame())
        {
            SetCVParams();
        }
    }

    bool IsControlFrame()
    {
        return m_frame % m_controlFrames == 0;
    }

    bool Flash()
    {
        return (m_frame / 4096) % 2 == 0;
    }
    
    void Process()
    {
        m_firstFrame = false;
        SetInputs();
        SetParams();
        ++m_frame;
    }    
};

#endif
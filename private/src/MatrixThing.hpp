#pragma once
#include "plugin.hpp"
#include <cstddef>
#include <cmath>

struct MatrixThing : Module
{

    static constexpr float x_blinkDelay = 0.01;
    static constexpr float x_blinkTime = 0.01;
    static constexpr float x_flashTime = 0.01;

    static constexpr size_t x_maxPoly = 16;
    static constexpr size_t x_numRows = 9;

    static float Flip(float in)
    {
        if (in > 0)
        {
            return 0;
        }
        else
        {
            return 10.0;
        }
    }

    static float CFlip(bool c, float in)
    {
        return c ? Flip(in) : in;
    }
    
    struct Block
    {
        bool m_wasUp;
        float m_value;
        float m_timeToBlink;
        float m_timeBlinking;
        bool m_blink;
        bool m_flash;

        Block()
            : m_wasUp(false)
            , m_value(0)
            , m_timeToBlink(0)
            , m_timeBlinking(0)
            , m_blink(false)
            , m_flash(false)
        {
        }
        
        void SetVal(float val)
        {
            if (val > 0)
            {
                m_timeToBlink = x_blinkDelay;
                
                if (!m_wasUp)
                {
                    if (m_value == val)
                    {
                        m_value = 0;
                    }
                    else
                    {
                        m_value = val;
                    }

                    m_wasUp = true;
                }
            }
            else
            {
                if (m_wasUp)
                {
                    m_timeToBlink = x_blinkDelay;
                    m_wasUp = false;
                }
            }
        }

        void Process(float dt, float val)
        {
            SetVal(val);
            
            if (m_timeToBlink > 0)
            {
                m_blink = false;
                m_timeToBlink -= dt;
                if (m_timeToBlink < 0)
                {
                    m_timeBlinking = x_blinkTime;
                }
            }
            else if (m_timeBlinking > 0)
            {
                m_blink = true;
                m_timeBlinking -= dt;
            }
            else
            {
                m_blink = false;
            }
        }

        float GetLight(bool flash)
        {
            if (!m_flash || m_value == 0)
            {
                return CFlip(m_blink, m_value);
            }
            else
            {
                return CFlip(flash, m_value);
            }
        }
    };

    struct Row
    {
        Block m_blocks[x_maxPoly];
        float m_vca[x_maxPoly];
        rack::dsp::TSchmittTrigger<float> m_blinkTrig[x_maxPoly];
        rack::dsp::TSchmittTrigger<float> m_shTrig;

        Row()
        {
            for (size_t i = 0; i < x_maxPoly; ++i)
            {
                m_vca[i] = 10;
            }
        }
        
        void ProcessBlink(size_t chan, float v)
        {
            m_blinkTrig[chan].process(v);
            if (m_blinkTrig[chan].isHigh())
            {
                m_blocks[chan].m_timeBlinking = x_blinkTime;
            }
        }
        
        void SetVCA(size_t chan, float v)
        {
            m_vca[chan] = v;
            m_blocks[chan].m_flash = v > 0;
        }
        
        bool ShouldSample(float v)
        {
            m_shTrig.process(v);
            return m_shTrig.isHigh();
        }

        float ComputeAvg(size_t chans)
        {
            float num = 0;
            float denom = 0;
            for (size_t chan = 0; chan < chans; ++chan)
            {
                num += m_blocks[chan].m_value * m_vca[chan];
                denom += m_blocks[chan].m_value;
            }

            return denom > 0 ? num / denom : 0;
        }
    };

    void ProcessRow(float dt, size_t row, size_t vcaRow, bool flash)
    {
        size_t chans = GetInputChannels(row);
        SetOutputChannels(row, chans);

        for (size_t chan = 0; chan < chans; ++chan)
        {
            m_rows[row].ProcessBlink(chan, GetBlinkVoltage(row, chan));
            m_rows[row].m_blocks[chan].Process(dt, GetInputVoltage(row, chan));
        }
        
        if (!IsSHConnected() || m_rows[row].ShouldSample(GetSHVoltage(row)))
        {
            for (size_t chan = 0; chan < chans; ++chan)
            {
                m_rows[row].SetVCA(chan, GetVCAVoltage(vcaRow, chan));
            }
        }

        float avg = m_rows[row].ComputeAvg(chans);

        for (size_t chan = 0; chan < chans; ++chan)
        {
            SetOutputVoltage(row, chan, m_rows[row].m_blocks[chan].m_value);
            SetLightVoltage(row, chan, m_rows[row].m_blocks[chan].GetLight(flash));
        }

        SetAvgVoltage(row, avg);
    }

    void ProcessRows(float dt, bool flash)
    {
        size_t vcaRow = 0;
        for (size_t row = 0; row < x_numRows; ++row)
        {
            if (GetVCAChannels(row) > 0)
            {
                vcaRow = row;
            }

            ProcessRow(dt, row, vcaRow, flash);
        }
    }

    void process(const ProcessArgs &args) override
    {
        m_totalTime += args.sampleTime;
        bool flash = static_cast<size_t>(m_totalTime / x_flashTime) % 4 == 0;
        if (m_totalTime > x_flashTime * 4)
        {
            m_totalTime -= 4 * x_flashTime;
        }
        
        ProcessRows(args.sampleTime, flash);
    }    

    size_t GetInputId(size_t row)
    {
        return row;
    }

    size_t GetVCAId(size_t row)
    {
        return 16 + row;
    }

    size_t GetBlinkId(size_t row)
    {
        return 32 + row;
    }

    size_t GetSHId()
    {
        return 48;
    }

    float GetInputVoltage(size_t row, size_t chan)
    {
        return inputs[GetInputId(row)].getVoltage(chan);
    }

    float GetVCAVoltage(size_t row, size_t chan)
    {
        size_t chans = GetVCAChannels(row);
        if (chans == 0)
        {
            return 0;
        }
        
        return inputs[GetVCAId(row)].getVoltage(chan % chans);
    }

    float GetBlinkVoltage(size_t row, size_t chan)
    {
        size_t chans = GetBlinkChannels(row);
        if (chans == 0)
        {
            return 0;
        }
        
        return inputs[GetBlinkId(row)].getVoltage(chan % chans);
    }

    float GetSHVoltage(size_t chan)
    {
        size_t chans = inputs[GetSHId()].getChannels();
        return inputs[GetSHId()].getVoltage(chan % chans);
    }

    size_t GetInputChannels(size_t row)
    {
        return inputs[GetInputId(row)].getChannels();
    }

    size_t GetVCAChannels(size_t row)
    {
        return inputs[GetVCAId(row)].getChannels();
    }

    size_t GetBlinkChannels(size_t row)
    {
        return inputs[GetBlinkId(row)].getChannels();
    }

    bool IsSHConnected()
    {
        return inputs[GetSHId()].isConnected();
    }

    size_t GetOutputId(size_t row)
    {
        return row;
    }

    size_t GetLightId(size_t row)
    {
        return 16 + row;
    }

    size_t GetAvgId(size_t row)
    {
        return 32 + row;
    }

    void SetOutputVoltage(size_t row, size_t chan, float v)
    {
        outputs[GetOutputId(row)].setVoltage(v, chan);
    }

    void SetLightVoltage(size_t row, size_t chan, float v)
    {
        outputs[GetLightId(row)].setVoltage(v, chan);
    }

    void SetAvgVoltage(size_t row, float v)
    {
        outputs[GetAvgId(row)].setVoltage(v);
    }

    void SetOutputChannels(size_t row, size_t num)
    {
        outputs[GetOutputId(row)].setChannels(num);
        outputs[GetLightId(row)].setChannels(num);
    }
    
    MatrixThing()
    {
        config(0, 49, 48, 0);

        for (size_t i = 0; i < x_numRows; ++i)
        {
            configInput(GetInputId(i), ("Momentary input " + std::to_string(i)).c_str());
            configInput(GetVCAId(i), ("VCA " + std::to_string(i)).c_str());
            configInput(GetBlinkId(i), ("Blink " + std::to_string(i)).c_str());

            configOutput(GetOutputId(i), ("Main Out " + std::to_string(i)).c_str());
            configOutput(GetLightId(i), ("Light Out " + std::to_string(i)).c_str());
            configOutput(GetAvgId(i), ("Avg Out " + std::to_string(i)).c_str());
        }

        configInput(GetSHId(), "S+H gate");
    }
    
    json_t* dataToJson() override
    {
        json_t* rootJ = json_object();
        for (size_t i = 0; i < x_numRows; ++i)
        {
            for (size_t j = 0; j < x_maxPoly; ++j)
            {
                json_object_set_new(rootJ, ("value" + std::to_string(i) + "_" + std::to_string(j)).c_str(), json_real(m_rows[i].m_blocks[j].m_value));
            }
        }
            
        return rootJ;
    }
    
    void dataFromJson(json_t* rootJ) override
    {
      	for (size_t i = 0; i < x_numRows; ++i)
        {
            for (size_t j = 0; j < x_maxPoly; ++j)
            {                
                json_t* val = json_object_get(rootJ, ("value" + std::to_string(i) + "_" + std::to_string(j)).c_str());
                if (val)
                {
                    m_rows[i].m_blocks[j].m_value = json_number_value(val);
                }
            }
        }
    }

    float m_totalTime;
    Row m_rows[x_numRows];
};

struct MatrixThingWidget : ModuleWidget
{
    MatrixThingWidget(MatrixThing* module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/MatrixThing.svg")));
        
        float rowOff = 25;
        float rowStart = 50;
        
        for (size_t i = 0; i < module->x_numRows; ++i)
        {
            float rowPos = 75 + i * 25;
            addInput(createInputCentered<PJ301MPort>(Vec(rowStart, rowPos), module, module->GetInputId(i)));
            addInput(createInputCentered<PJ301MPort>(Vec(rowStart + rowOff, rowPos), module, module->GetVCAId(i)));
            addInput(createInputCentered<PJ301MPort>(Vec(rowStart + 2 * rowOff, rowPos), module, module->GetBlinkId(i)));
            
            addOutput(createOutputCentered<PJ301MPort>(Vec(rowStart + 3 * rowOff, rowPos), module, module->GetOutputId(i)));
            addOutput(createOutputCentered<PJ301MPort>(Vec(rowStart + 4 * rowOff, rowPos), module, module->GetLightId(i)));
            addOutput(createOutputCentered<PJ301MPort>(Vec(rowStart + 5 * rowOff, rowPos), module, module->GetAvgId(i)));
        }
        
        addInput(createInputCentered<PJ301MPort>(Vec(rowStart + 3 * rowOff, 75 + module->x_numRows * 25), module, module->GetSHId()));
    }
};


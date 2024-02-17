#pragma once
#include <cstddef>
#include <cmath>
#include "plugin.hpp"

struct PhasorEnvelopeInternal
{
    struct Input
    {
        float m_gateFrac;
        float m_attackFrac;
        float m_shape;
        float m_in;

        Input()
            : m_gateFrac(0.5)
            , m_attackFrac(0.05)
            , m_shape(0)
        {
        }

        float ComputePhase()
        {
            if (m_gateFrac <= 0)
            {
                return 0;
            }
            
            float in = m_in / m_gateFrac;
            if (in >= 1 || in <= 0)
            {
                return 0;
            }

            if (in <= m_attackFrac)
            {
                return Shape(in / m_attackFrac);
            }
            else
            {
                float release = 1 - (in - m_attackFrac) / (1 - m_attackFrac);
                return Shape(release);
            }
        }

        float Shape(float in)
        {
            if (m_shape == 0)
            {
                return in;
            }
            else
            {
                return powf(in, powf(10, m_shape));
            }
        }
    };     
};

struct PhasorEnvelope : Module
{
    static constexpr size_t x_maxPoly = 16;
    static constexpr size_t x_numEnvs = 2;
    
    static constexpr size_t x_phaseInId = 0;
    static constexpr size_t x_gateFracInId = 1;
    static constexpr size_t x_attackFracInId = 3;
    static constexpr size_t x_shapeInId = 5;
    static constexpr size_t x_amplitudeInId = 7;
    static constexpr size_t x_numInputs = 9;

    static constexpr size_t x_gateFracKnob = 0;
    static constexpr size_t x_attackFracKnob = 4;
    static constexpr size_t x_shapeKnob = 8;
    static constexpr size_t x_amplitudeKnob = 12;
    static constexpr size_t x_numKnobs = 16;

    PhasorEnvelopeInternal::Input m_state[x_numEnvs][x_maxPoly];
    
    float GetValue(size_t cvId, size_t knobId, size_t chan, float denom=10)
    {
        float val = params[knobId].getValue();
        float cv = inputs[cvId].getVoltage(chan) * params[knobId + 1].getValue() / denom;
        return val + cv;
    }

    void Process()
    {
        size_t chans = inputs[x_phaseInId].getChannels();        
        for (size_t i = 0; i < x_numEnvs; ++i)
        {
            outputs[i].setChannels(chans);
            for (size_t j = 0; j < chans; ++j)
            {
                m_state[i][j].m_in = inputs[x_phaseInId].getVoltage(j) / 10;
                float voltage = m_state[i][j].ComputePhase();
                voltage *= GetValue(x_amplitudeInId + i, x_amplitudeKnob + 2 * i, j, 1);
                outputs[i].setVoltage(voltage, j);
            }
        }
    }

    void ReadParams()
    {
        size_t chans = inputs[x_phaseInId].getChannels();        
        for (size_t i = 0; i < x_numEnvs; ++i)
        {
            for (size_t j = 0; j < chans; ++j)
            {
                m_state[i][j].m_gateFrac = GetValue(x_gateFracInId + i, x_gateFracKnob + 2 * i, j);
                m_state[i][j].m_attackFrac = GetValue(x_attackFracInId + i, x_attackFracKnob + 2 * i, j);
                m_state[i][j].m_shape = GetValue(x_shapeInId + i, x_shapeKnob + 2 * i, j);
            }
        }
    }

    static constexpr float x_timeToCheck = 0.05;
    float m_timeToCheck;
    
    void process(const ProcessArgs &args) override
    {
        m_timeToCheck -= args.sampleTime;
        if (m_timeToCheck < 0)
        {
            ReadParams();
            m_timeToCheck = x_timeToCheck;
        }

        Process();
    }    

    PhasorEnvelope()
    {
        config(x_numKnobs, x_numInputs, x_numEnvs, 0);
        configInput(x_phaseInId, "Phasor");

        for (size_t i = 0; i < x_numEnvs; ++i)
        {
            configInput(x_gateFracInId + i, ("Env Length CV " + std::to_string(i)).c_str());
            configInput(x_attackFracInId + i, ("Attack Fraction CV " + std::to_string(i)).c_str());
            configInput(x_shapeInId + i, ("Shape CV " + std::to_string(i)).c_str());
            configInput(x_amplitudeInId + i, ("Amplitude CV " + std::to_string(i)).c_str());

            configParam(x_gateFracKnob + 2 * i, 0, 1, 0.5, ("Env Length " + std::to_string(i)).c_str());
            configParam(x_attackFracKnob + 2 * i, 0, 1, 0.05, ("Attack Fraction " + std::to_string(i)).c_str());
            configParam(x_shapeKnob + 2 * i, -1, 1, 0, ("Shape " + std::to_string(i)).c_str());
            configParam(x_amplitudeKnob + 2 * i, 0, 10, 10, ("Amplitude " + std::to_string(i)).c_str());

            configParam(x_gateFracKnob + 2 * i + 1, -1, 1, 0, ("Env Length Attn " + std::to_string(i)).c_str());
            configParam(x_attackFracKnob + 2 * i + 1, -1, 1, 0, ("Attack Fraction Attn " + std::to_string(i)).c_str());
            configParam(x_shapeKnob + 2 * i + 1, -1, 1, 0, ("Shape Attn " + std::to_string(i)).c_str());
            configParam(x_amplitudeKnob + 2 * i + 1, -1, 1, 0, ("Amplitude Attn " + std::to_string(i)).c_str());
        }
    }
};

struct PhasorEnvelopeWidget : ModuleWidget
{
    PhasorEnvelopeWidget(PhasorEnvelope* module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/PhasorEnvelope.svg")));
        
        addInput(createInputCentered<PJ301MPort>(Vec(50, 75), module, module->x_phaseInId));
        addOutput(createOutputCentered<PJ301MPort>(Vec(100, 75), module, 0));
        addOutput(createOutputCentered<PJ301MPort>(Vec(150, 75), module, 1));
        
        for (size_t i = 0; i < 2; ++i)
        {
            float yPos = 100 + 125 * i;
            float yOff = 25;
            float xPos = 50;
            float xOff = 25;
            
            addInput(createInputCentered<PJ301MPort>(Vec(xPos, yPos), module, module->x_gateFracInId + i));
            addParam(createParamCentered<Trimpot>(Vec(xPos + xOff, yPos), module, module->x_gateFracKnob + 1 + 2 * i));
            addParam(createParamCentered<RoundBlackKnob>(Vec(xPos + 2 * xOff, yPos), module, module->x_gateFracKnob + 2 * i));
            
            yPos += yOff;
            
            addInput(createInputCentered<PJ301MPort>(Vec(xPos, yPos), module, module->x_attackFracInId + i));
            addParam(createParamCentered<Trimpot>(Vec(xPos + xOff, yPos), module, module->x_attackFracKnob + 1 + 2 * i));
            addParam(createParamCentered<RoundBlackKnob>(Vec(xPos + 2 * xOff, yPos), module, module->x_attackFracKnob + 2 * i));
            
            yPos += yOff;
            
            addInput(createInputCentered<PJ301MPort>(Vec(xPos, yPos), module, module->x_shapeInId + i));
            addParam(createParamCentered<Trimpot>(Vec(xPos + xOff, yPos), module, module->x_shapeKnob + 1 + 2 * i));
            addParam(createParamCentered<RoundBlackKnob>(Vec(xPos + 2 * xOff, yPos), module, module->x_shapeKnob + 2 * i));
            
            yPos += yOff;
            
            addInput(createInputCentered<PJ301MPort>(Vec(xPos, yPos), module, module->x_amplitudeInId + i));
            addParam(createParamCentered<Trimpot>(Vec(xPos + xOff, yPos), module, module->x_amplitudeKnob + 1 + 2 * i));
            addParam(createParamCentered<RoundBlackKnob>(Vec(xPos + 2 * xOff, yPos), module, module->x_amplitudeKnob + 2 * i));
        }
    }
};

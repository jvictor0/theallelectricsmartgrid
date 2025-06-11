#pragma once
#include <cstddef>
#include <cmath>
#include "plugin.hpp"
#include "Slew.hpp"

struct PhasorEnvelopeInternal
{
    enum class State
    {
        Off,
        Phasor,
        Release
    };

    float m_preEnvelope;
    size_t m_phasorFrames;
    size_t m_lastPhasorFrames;
    float m_phasorEndValue;
    State m_state;
    bool m_inPhasor;
    FixedSlew m_slew;

    PhasorEnvelopeInternal()
        : m_preEnvelope(0)
        , m_phasorFrames(0)
        , m_lastPhasorFrames(0)
        , m_state(State::Off)
        , m_inPhasor(false)
        , m_slew(1.0/128)
    {
    }
    
    struct Input
    {
        float m_gateFrac;
        float m_attackFrac;
        float m_attackShape;
        float m_decayShape;
        float m_in;

        Input()
            : m_gateFrac(0.5)
            , m_attackFrac(0.05)
            , m_attackShape(0)
            , m_decayShape(0)
        {
        }

        float ComputePhase(bool* attack)
        {
            *attack = false;
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
                *attack = true;
                return in / m_attackFrac;
            }
            else
            {
                float release = 1 - (in - m_attackFrac) / (1 - m_attackFrac);
                return release;
            }
        }
    };

    float Shape(float shape, float in)
    {
        if (shape == 0)
        {
            return in;
        }
        else
        {
            return powf(in, powf(10, -shape));
        }
    }

    float UnShape(float shape, float in)
    {
        if (shape == 0)
        {
            return in;
        }
        else
        {
            return powf(in, 1 / powf(10, -shape));
        }
    }

    float ComputeRelease(Input& input)
    {
        if (m_state == State::Release && input.m_gateFrac > 1)
        {
            float delta = m_phasorEndValue / ((input.m_gateFrac - 1) * m_lastPhasorFrames);
            return std::max(0.0f, m_preEnvelope - delta);
        }

        return 0;
    }
    
    float Process(Input& input)
    {
        if (m_state == State::Phasor && (input.m_in <= 0 || input.m_in >= 1) && input.m_gateFrac > 1)
        {
            m_state = State::Release;
            if (input.m_gateFrac * input.m_attackFrac > 1)
            {
                m_preEnvelope = UnShape(input.m_decayShape, Shape(input.m_attackShape, m_preEnvelope));
            }

            m_phasorEndValue = m_preEnvelope;
            m_lastPhasorFrames = m_phasorFrames;
        }
        
        float releaseEnvelope = ComputeRelease(input);
        bool attack = false;
        if (input.m_in > 0 && input.m_in < 1)
        {
            if (!m_inPhasor)
            {
                m_inPhasor = true;
                m_phasorFrames = 0;
            }

            ++m_phasorFrames;
            
            float phase = input.ComputePhase(&attack);
            if (m_state == State::Release && attack)
            {
                float shapedRelease = Shape(input.m_decayShape, releaseEnvelope);
                float shapedPhase = Shape(input.m_attackShape, phase);
                if (shapedRelease <= shapedPhase)
                {
                    m_state = State::Phasor;
                    m_preEnvelope = phase;
                    return shapedPhase;
                }
                else
                {
                    m_preEnvelope = releaseEnvelope;
                    return shapedRelease;
                }
            }
            else if (releaseEnvelope <= phase)
            {
                if (m_state != State::Phasor)
                {
                    m_state = State::Phasor;
                }

                m_preEnvelope = phase;
            }
            else
            {
                m_preEnvelope = releaseEnvelope;
            }
        }
        else if (releaseEnvelope > 0)
        {
            m_inPhasor = false;
            m_preEnvelope = releaseEnvelope;
        }
        else
        {
            m_inPhasor = false;
            m_state = State::Off;
            m_preEnvelope = 0;
            return 0;
        }
            
        return m_slew.Process(Shape(attack ? input.m_attackShape : input.m_decayShape, m_preEnvelope));
    }
};

#ifndef IOS_BUILD
struct PhasorEnvelope : Module
{
    static constexpr size_t x_maxPoly = 16;
    static constexpr size_t x_numEnvs = 2;
    
    static constexpr size_t x_phaseInId = 0;
    static constexpr size_t x_gateFracInId = 1;
    static constexpr size_t x_attackFracInId = 3;
    static constexpr size_t x_attackShapeInId = 5;
    static constexpr size_t x_amplitudeInId = 7;
    static constexpr size_t x_decayShapeInId = 9;
    static constexpr size_t x_numInputs = 11;

    static constexpr size_t x_gateFracKnob = 0;
    static constexpr size_t x_attackFracKnob = 4;
    static constexpr size_t x_attackShapeKnob = 8;
    static constexpr size_t x_amplitudeKnob = 12;
    static constexpr size_t x_decayShapeKnob = 16;
    static constexpr size_t x_lfoKnob = 20;
    static constexpr size_t x_numKnobs = 21;

    PhasorEnvelopeInternal m_env[x_numEnvs][x_maxPoly];
    PhasorEnvelopeInternal::Input m_state[x_numEnvs][x_maxPoly];

    bool m_lfo;
    
    float GetValue(size_t cvId, size_t knobId, size_t chan, float denom=10)
    {
        float val = params[knobId].getValue();
        float cv = inputs[cvId].getVoltage(chan % inputs[cvId].getChannels()) * params[knobId + 1].getValue() / denom;
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
                if (m_lfo)
                {
                    m_state[i][j].m_in = fmod(m_state[i][j].m_in, m_state[i][j].m_gateFrac);
                }
                
                float voltage = m_env[i][j].Process(m_state[i][j]);
                voltage *= GetValue(x_amplitudeInId + i, x_amplitudeKnob + 2 * i, j, 1);
                outputs[i].setVoltage(voltage, j);
            }
        }
    }

    void ReadParams()
    {
        m_lfo = params[x_lfoKnob].getValue() > 0;
        size_t chans = inputs[x_phaseInId].getChannels();        
        for (size_t i = 0; i < x_numEnvs; ++i)
        {
            for (size_t j = 0; j < chans; ++j)
            {
                float gateFrac = GetValue(x_gateFracInId + i, x_gateFracKnob + 2 * i, j);
                float attacFrac = GetValue(x_attackFracInId + i, x_attackFracKnob + 2 * i, j);
                if (!m_lfo)
                {
                    gateFrac *= gateFrac;
                    gateFrac *= gateFrac;
                    gateFrac *= 16;
                    
                    attacFrac *= attacFrac;
                    attacFrac = std::min<float>(attacFrac / gateFrac, 1.0);
                }
                else
                {
                    gateFrac = std::max<float>(gateFrac, 0.25);
                }
                
                m_state[i][j].m_gateFrac = gateFrac;
                m_state[i][j].m_attackFrac = attacFrac;
                m_state[i][j].m_attackShape = GetValue(x_attackShapeInId + i, x_attackShapeKnob + 2 * i, j, 5);
                m_state[i][j].m_decayShape = GetValue(x_decayShapeInId + i, x_decayShapeKnob + 2 * i, j, 5);
            }
        }
    }

    static constexpr float x_timeToCheck = 0.005;
    float m_timeToCheck;
    
    void process(const ProcessArgs &args) override
    {
        m_timeToCheck -= args.sampleTime;
        ReadParams();
        if (m_timeToCheck < 0)
        {
            m_timeToCheck = x_timeToCheck;
        }

        Process();
    }    

    PhasorEnvelope()
        : m_lfo(false)
        , m_timeToCheck(0)
    {
        config(x_numKnobs, x_numInputs, x_numEnvs, 0);
        configInput(x_phaseInId, "Phasor");
        configSwitch(x_lfoKnob, 0, 1, 0, "LFO");

        for (size_t i = 0; i < x_numEnvs; ++i)
        {
            configInput(x_gateFracInId + i, ("Env Length CV " + std::to_string(i)).c_str());
            configInput(x_attackFracInId + i, ("Attack Fraction CV " + std::to_string(i)).c_str());
            configInput(x_attackShapeInId + i, ("AttackShape CV " + std::to_string(i)).c_str());
            configInput(x_amplitudeInId + i, ("Amplitude CV " + std::to_string(i)).c_str());
            configInput(x_decayShapeInId + i, ("ReleaseShape CV " + std::to_string(i)).c_str());

            configParam(x_gateFracKnob + 2 * i, 0, 1, 0.5, ("Env Length " + std::to_string(i)).c_str());
            configParam(x_attackFracKnob + 2 * i, 0, 1, 0.05, ("Attack Fraction " + std::to_string(i)).c_str());
            configParam(x_attackShapeKnob + 2 * i, -1, 1, 0, ("AttackShape " + std::to_string(i)).c_str());
            configParam(x_amplitudeKnob + 2 * i, 0, 10, 10, ("Amplitude " + std::to_string(i)).c_str());
            configParam(x_decayShapeKnob + 2 * i, -1, 1, 0, ("ReleaseShape " + std::to_string(i)).c_str());

            configParam(x_gateFracKnob + 2 * i + 1, -1, 1, 0, ("Env Length Attn " + std::to_string(i)).c_str());
            configParam(x_attackFracKnob + 2 * i + 1, -1, 1, 0, ("Attack Fraction Attn " + std::to_string(i)).c_str());
            configParam(x_attackShapeKnob + 2 * i + 1, -1, 1, 0, ("AttackShape Attn " + std::to_string(i)).c_str());
            configParam(x_amplitudeKnob + 2 * i + 1, -1, 1, 0, ("Amplitude Attn " + std::to_string(i)).c_str());
            configParam(x_decayShapeKnob + 2 * i + 1, -1, 1, 0, ("ReleaseShape Attn " + std::to_string(i)).c_str());
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
        addParam(createParamCentered<RoundBlackKnob>(Vec(200, 75), module, module->x_lfoKnob));
        
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
            
            addInput(createInputCentered<PJ301MPort>(Vec(xPos, yPos), module, module->x_attackShapeInId + i));
            addParam(createParamCentered<Trimpot>(Vec(xPos + xOff, yPos), module, module->x_attackShapeKnob + 1 + 2 * i));
            addParam(createParamCentered<RoundBlackKnob>(Vec(xPos + 2 * xOff, yPos), module, module->x_attackShapeKnob + 2 * i));
            
            yPos += yOff;
            
            addInput(createInputCentered<PJ301MPort>(Vec(xPos, yPos), module, module->x_amplitudeInId + i));
            addParam(createParamCentered<Trimpot>(Vec(xPos + xOff, yPos), module, module->x_amplitudeKnob + 1 + 2 * i));
            addParam(createParamCentered<RoundBlackKnob>(Vec(xPos + 2 * xOff, yPos), module, module->x_amplitudeKnob + 2 * i));

            yPos += yOff;
            
            addInput(createInputCentered<PJ301MPort>(Vec(xPos, yPos), module, module->x_decayShapeInId + i));
            addParam(createParamCentered<Trimpot>(Vec(xPos + xOff, yPos), module, module->x_decayShapeKnob + 1 + 2 * i));
            addParam(createParamCentered<RoundBlackKnob>(Vec(xPos + 2 * xOff, yPos), module, module->x_decayShapeKnob + 2 * i));
        }
    }
};
#endif

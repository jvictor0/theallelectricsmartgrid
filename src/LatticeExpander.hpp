#pragma once
#include "plugin.hpp"
#include <cstddef>
#include "LameJuisConstants.hpp"
#include "Lattice.hpp"
#include <cmath>

struct LatticeExpanderMessage
{
    int m_intervalSemitones[LameJuisConstants::x_numAccumulators];
    float m_intervalVoltages[LameJuisConstants::x_numAccumulators];
    bool m_is12EDOLike[LameJuisConstants::x_numAccumulators];
    int m_position[LameJuisConstants::x_numAccumulators][LameJuisConstants::x_numAccumulators];

    LatticeExpanderMessage()
    {
        memset(this, 0, sizeof(LatticeExpanderMessage));
    }
};

// Overkill?  Maybe.  But why not do it consistently.
//
namespace LatticeExpanderConstants
{
    static constexpr size_t x_gridSize = 6;

    enum class LightColor : int
    {
        Red = 0,
        Green = 1,
        Blue = 2,
        NumColors = 3
    };

    static constexpr size_t GetNumParams()
    {
        return 0;
    }

    static constexpr size_t GetNumInputs()
    {
        return 0;
    }

    static constexpr size_t GetNumOutputs()
    {
        return 0;
    }

    enum class LightType : int
    {
        LatticeLight = 0,
        NumLightTypes = 1
    };

    static constexpr size_t x_numLightsPerType[] = {
        x_gridSize * x_gridSize * static_cast<size_t>(LightColor::NumColors)
    };

    static constexpr size_t x_lightStartPerType[] = {
        0,
        x_numLightsPerType[0]
    };

    static constexpr size_t GetLightId(LightType lightType, size_t lightId)
    {
        return x_lightStartPerType[static_cast<int>(lightType)] + lightId;
    }

    static constexpr size_t GetLatticeLightId(size_t x, size_t y, LightColor color)
    {
        return static_cast<size_t>(LightColor::NumColors) * (x + x_gridSize * y)
            + static_cast<size_t>(color);
    }

    static constexpr size_t GetNumLights()
    {
        return x_lightStartPerType[static_cast<int>(LightType::NumLightTypes)];
    }
};

struct LatticeExpander : Module
{
	LatticeExpanderMessage m_leftMessages[2][1];
    LatticeExpanderMessage m_prevMessage;
    Lattice::NoteName m_noteNames[LatticeExpanderConstants::x_gridSize][LatticeExpanderConstants::x_gridSize];
    
    int m_prevZVal;
    int m_curZVal;

    bool m_is12EDOLike;
    bool m_prevWas12EDOLike;

	LatticeExpander()
    {
        using namespace LatticeExpanderConstants;
        
        config(GetNumParams(), GetNumInputs(), GetNumOutputs(), GetNumLights());

        m_prevZVal = 0;
        m_curZVal = 0;
        
		leftExpander.producerMessage = m_leftMessages[0];	
		leftExpander.consumerMessage = m_leftMessages[1];	
	}

    // Pick a Z-value for the grid (there can only be one per frame).  Use the one which displays the most
    // elements, and in the event of a tie, the first voice.
    //
    void PickCurZVal()
    {
        LatticeExpanderMessage* msg = static_cast<LatticeExpanderMessage*>(leftExpander.consumerMessage);

        // Ugly code, but just check for sets of 2-out-of-three.
        //
        if (msg->m_position[0][2] == msg->m_position[1][2] ||
            msg->m_position[0][2] == msg->m_position[2][2])
        {
            m_curZVal = msg->m_position[0][2];
        }
        else if (msg->m_position[1][2] == msg->m_position[2][2])
        {
            m_curZVal = msg->m_position[1][2];
        }
        else
        {
            // They are all three different, so pick the first voice.
            //
            m_curZVal = msg->m_position[0][2];
        }
    }

    void Pick12EDOLike()
    {
        using namespace LatticeExpanderConstants;
        LatticeExpanderMessage* msg = static_cast<LatticeExpanderMessage*>(leftExpander.consumerMessage);

        m_is12EDOLike = true;
        for (size_t i = 0; i < LameJuisConstants::x_numAccumulators; ++i)
        {
            if (!msg->m_is12EDOLike[i])
            {
                m_is12EDOLike = false;
            }
        }
    }

    void ProcessLights()
    {        
        using namespace LatticeExpanderConstants;
        LatticeExpanderMessage* msg = static_cast<LatticeExpanderMessage*>(leftExpander.consumerMessage);
        
        for (size_t i = 0; i < LameJuisConstants::x_numAccumulators; ++i)
        {
            if (msg->m_position[i][0] != m_prevMessage.m_position[i][0] ||
                msg->m_position[i][1] != m_prevMessage.m_position[i][1] ||
                (msg->m_position[i][2] == m_curZVal) != (m_prevMessage.m_position[i][2] == m_prevZVal))
            {
                SetLightFromArray(m_prevMessage.m_position[i], i, false);
                SetLightFromArray(msg->m_position[i], i, true);
            }
        }
    }

    void ProcessTextFields()
    {
        using namespace LatticeExpanderConstants;
        LatticeExpanderMessage* msg = static_cast<LatticeExpanderMessage*>(leftExpander.consumerMessage);

        if (m_is12EDOLike)
        {
            if (msg->m_intervalSemitones[0] != m_prevMessage.m_intervalSemitones[0] ||
                msg->m_intervalSemitones[1] != m_prevMessage.m_intervalSemitones[1] ||
                msg->m_intervalSemitones[2] != m_prevMessage.m_intervalSemitones[2] ||
                m_curZVal != m_prevZVal ||
                !m_prevWas12EDOLike)
            {
                int intervals[] = {msg->m_intervalSemitones[0], msg->m_intervalSemitones[1], msg->m_intervalSemitones[2]};

                for (size_t i = 0; i < x_gridSize; ++i)
                {
                    for (size_t j = 0; j < x_gridSize; ++j)
                    {
                        int pos[] = {static_cast<int>(i), static_cast<int>(j), static_cast<int>(m_curZVal)};
                        Lattice::Note note(pos, intervals);
                        m_noteNames[i][j] = note.ToNoteName();
                    }
                }
            }
        }
        else
        {
            if (msg->m_intervalVoltages[0] != m_prevMessage.m_intervalVoltages[0] ||
                msg->m_intervalVoltages[1] != m_prevMessage.m_intervalVoltages[1] ||
                msg->m_intervalVoltages[2] != m_prevMessage.m_intervalVoltages[2] ||
                m_curZVal != m_prevZVal ||
                m_prevWas12EDOLike)
            {
                for (size_t i = 0; i < x_gridSize; ++i)
                {
                    for (size_t j = 0; j < x_gridSize; ++j)
                    {
                        float voltage = i * msg->m_intervalVoltages[0] +
                                        j * msg->m_intervalVoltages[1] +
                                m_curZVal * msg->m_intervalVoltages[2];
                        uint32_t cents = static_cast<uint32_t>(1200 * voltage);
                        m_noteNames[i][j].WriteCentsToBuf(cents);
                    }
                }
            }
        }
    }

    void SetLightFromArray(int* values, size_t accumId, bool value)
    {
        using namespace LatticeExpanderConstants;

        LightColor color = static_cast<LightColor>(accumId);
        
        // Do nothing if the position is off the grid.
        //
        size_t expectedZVal = value ? m_curZVal : m_prevZVal;
        if (static_cast<size_t>(values[0]) < x_gridSize &&
            static_cast<size_t>(values[1]) < x_gridSize &&
            static_cast<size_t>(values[2]) == expectedZVal)
        {
            lights[GetLatticeLightId(values[0], values[1], color)].setBrightness(value ? 1.f : 0.f);
        }        
    }

	void process(const ProcessArgs &args) override
    {
		if (leftExpander.module &&
			leftExpander.module->model == modelLameJuis)
        {
            PickCurZVal();
            Pick12EDOLike();
            ProcessLights();
            ProcessTextFields();

            m_prevMessage = *static_cast<LatticeExpanderMessage*>(leftExpander.consumerMessage);
            m_prevZVal = m_curZVal;
            m_prevWas12EDOLike = m_is12EDOLike;
        }
	}
};

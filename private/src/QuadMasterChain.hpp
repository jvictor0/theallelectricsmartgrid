#pragma once

#include "QuadUtils.hpp"
#include "LinkwitzRileyCrossover.hpp"
#include "Filter.hpp"
#include "StereoUtils.hpp"
#include "MultibandSaturator.hpp"

struct QuadFloatWithStereoAndSub
{
    QuadFloat m_output;
    StereoFloat m_stereoOutput;
    float m_sub;

    QuadFloatWithStereoAndSub()
    {
        m_output = QuadFloat();
        m_stereoOutput = StereoFloat();
        m_sub = 0.0f;
    }

    QuadFloatWithStereoAndSub(const QuadFloat& output, const StereoFloat& stereoOutput, float sub)
    {
        m_output = output;
        m_stereoOutput = stereoOutput;
        m_sub = sub;
    }
};

template<size_t NumChannels>
struct MasteringChain
{
    MultibandSaturator<4, NumChannels> m_saturator;
    MultiChannelFloat<NumChannels> m_output;

    struct Input
    {
        typename MultibandSaturator<4, NumChannels>::Input m_saturatorInput;
    };

    MultiChannelFloat<NumChannels> Process(const Input& input, MultiChannelFloat<NumChannels> in)
    {
        m_output = m_saturator.Process(input.m_saturatorInput, in, true /* monoTheBass */);
        return m_output;
    }
};

struct DualMasteringChain
{
    MasteringChain<2> m_stereoMasteringChain;
    MasteringChain<4> m_quadMasteringChain;

    struct Input
    {
        typename MasteringChain<2>::Input m_stereoMasteringChainInput;
        typename MasteringChain<4>::Input m_quadMasteringChainInput;

        void SetGain(size_t band, float gain)
        {
            m_stereoMasteringChainInput.m_saturatorInput.m_gain[band].Update(gain);
            m_quadMasteringChainInput.m_saturatorInput.m_gain[band].Update(gain);
        }

        void SetMasterGain(float gain)
        {
            m_stereoMasteringChainInput.m_saturatorInput.m_masterGain.Update(gain);
            m_quadMasteringChainInput.m_saturatorInput.m_masterGain.Update(gain);
        }

        void SetBassFreq(float freq)
        {
            m_stereoMasteringChainInput.m_saturatorInput.m_bassFreq.Update(freq);
            m_quadMasteringChainInput.m_saturatorInput.m_bassFreq.Update(freq);
        }

        void SetCrossoverFreq(size_t band, float freq)
        {
            m_stereoMasteringChainInput.m_saturatorInput.m_crossoverFreqFactor[band].Update(freq);
            m_quadMasteringChainInput.m_saturatorInput.m_crossoverFreqFactor[band].Update(freq);
        }

        void PopulateUIState(MultibandSaturator<4, 2>::UIState* uiState)
        {
            m_stereoMasteringChainInput.m_saturatorInput.PopulateUIState(uiState);            
        }
    };

    DualMasteringChain()
    {
    }

    QuadFloatWithStereoAndSub Process(const Input& input, QuadFloat quadInput, StereoFloat stereoInput)
    {
        QuadFloat quadOutput = m_quadMasteringChain.Process(input.m_quadMasteringChainInput, quadInput);
        StereoFloat stereoOutput = m_stereoMasteringChain.Process(input.m_stereoMasteringChainInput, stereoInput);
        return QuadFloatWithStereoAndSub(quadOutput, stereoOutput, m_quadMasteringChain.m_saturator.m_sub);
    }
};

typedef MasteringChain<2> StereoMasteringChain;
typedef MasteringChain<4> QuadMasteringChain;
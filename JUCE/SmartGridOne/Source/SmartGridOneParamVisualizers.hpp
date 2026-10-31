#pragma once

#include "SmartGridOneLayout.hpp"
#include <cassert>

struct SmartGridOneParamVisualizers
{
    using Param = SmartGridOneLayout::Param;

    enum class Visualizer
    {
#define F(name, bank, slot, span, region, ctor, flags) name,
#include "ForEachSmartGridOneVisualizer.hpp"
#undef F
#define G(name, slot, ctor, mode, color, modulator) name,
#define H(modulator, ctor)
#include "ForEachModulationVisualizer.hpp"
#undef H
#undef G
        NumVisualizers
    };

    static constexpr size_t x_numVisualizers = static_cast<size_t>(Visualizer::NumVisualizers);
    static constexpr size_t x_numAssociations = 0
#define F(param, visualizer) + 1
#include "ForEachSmartGridOneParamVisualizer.hpp"
#undef F
        ;

    static_assert(x_numAssociations == SmartGridOneLayout::x_numParams,
        "Every parameter needs one visualizer association");

    static constexpr Visualizer Get(Param param)
    {
        // Duplicate associations fail to compile as duplicate case labels.
        //
        switch (param)
        {
#define F(name, visualizer) case Param::name: return Visualizer::visualizer;
#include "ForEachSmartGridOneParamVisualizer.hpp"
#undef F
            case Param::NumParams: break;
        }

        assert(false);
        return Visualizer::NumVisualizers;
    }

    static constexpr bool IsLFOParam(Param param)
    {
        switch (Get(param))
        {
            case Visualizer::LFO1Scope:
            case Visualizer::LFO2Scope:
            case Visualizer::QuadLFO1Scope:
            case Visualizer::QuadLFO2Scope:
            case Visualizer::DelayLFO:
            case Visualizer::ReverbLFO:
                return true;
            default: break;
        }

        // These LFO controls are associated with their audible result.
        //
        switch (param)
        {
            case Param::ReverbModDepth:
            case Param::TempoLFOSkew:
            case Param::TempoLFOMult:
            case Param::TempoLFOShape:
            case Param::TempoLFOCenter:
            case Param::TempoLFOSlope:
            case Param::TempoLFOIndex:
                return true;
            default: return false;
        }
    }

    static constexpr bool IsMainVisualizer(Visualizer visualizer)
    {
        switch (visualizer)
        {
#define F(name, bank, slot, span, region, ctor, flags) case Visualizer::name: return true;
#include "ForEachSmartGridOneVisualizer.hpp"
#undef F
            default: return false;
        }
    }

    static constexpr bool TargetsHaveMainVisualizers()
    {
        for (size_t i = 0; i < SmartGridOneLayout::x_numParams; ++i)
        {
            auto param = static_cast<Param>(i);
            if (!IsLFOParam(param) && !IsMainVisualizer(Get(param)))
            {
                return false;
            }
        }

        return true;
    }
};

static_assert(SmartGridOneParamVisualizers::TargetsHaveMainVisualizers(),
    "LFO targets must have a visualizer owned by the main visualizer page");

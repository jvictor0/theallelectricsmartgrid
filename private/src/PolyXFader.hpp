#include "ModuleUtils.hpp"
#include "Slew.hpp"
#include "WaveTable.hpp"

struct PolyXFaderInternal
{    
    struct Input
    {
        float* m_values;

        const WaveTable* m_waveTable;

        float m_attackFrac;
        float m_shape;
        float m_mult;

        float m_phaseShift;

        size_t m_size;
        float m_slope;
        float m_center;

        Input()
            : m_values(nullptr)
            , m_attackFrac(0.0f)
            , m_shape(0.0f)
            , m_mult(0.0f)
            , m_size(0)
            , m_slope(0.0f)
            , m_center(0.0f)
        {
            m_waveTable = &WaveTable::GetCosine();
        }

        float ComputePhase(size_t i)
        {
            float mult = m_mult;
            float amp = 1;
            float t = m_values[i] + m_phaseShift;
            t = (t - std::floorf(t)) * mult;
            float floorMult = std::floorf(mult);

            if (floorMult < t)
            {
                amp = mult - floorMult;
                t = (t - floorMult) / amp;
            }
            else
            {
                t = t - std::floorf(t);
            }
            
            if (t <= m_attackFrac)
            {
                return amp * Shape(m_shape, t / m_attackFrac);
            }
            else
            {
                float decay = 1 - (t - m_attackFrac) / (1 - m_attackFrac);
                return amp * Shape(m_shape, decay);
            }
        }
        
        float Shape(float shape, float in)
        {
            float fullShape = (- m_waveTable->Evaluate(in / 2) + 1) / 2;
            return in * shape + fullShape * (1 - shape);
        }
    };

    PolyXFaderInternal()
        : m_size(0)
        , m_slope(0.0f)
        , m_center(0.0f)
        , m_slew(1.0/128)
        , m_output(0.0f)
    {
    }
    
    float m_size;
    float m_slope;
    float m_center;
    float m_weights[16];
    float m_totalWeight;
    FixedSlew m_slew;
    float m_output;    

    float ComputeWeight(size_t index)
    {
        float center = m_center * m_size;
        float preSlope = m_slope * m_size;
        float slope = preSlope < 0.5 ? 2 : 1.0 / preSlope;
        if (center < 0.25 && index == 0)
        {
            return 1.0f;
        }
        else if (center > m_size - 0.25 && index == m_size - 1)
        {
            return 1.0f;
        }
        else if (index - 0.25 < center && center < index + 0.25)
        {
            return 1.0f;
        }
        else if (index < center)
        {
            return std::max<float>(0, 1.0f - (center - index - 0.25) * slope);
        }
        else
        {
            return std::max<float>(0, 1.0f - (index - 0.25 - center) * slope);
        }
    }

    void Process(Input& input)
    {
        if (m_size != input.m_size || m_slope != input.m_slope || m_center != input.m_center)
        {
            m_size = input.m_size;
            m_slope = input.m_slope;
            m_center = input.m_center;
            m_totalWeight = 0.0f;
            for (size_t i = 0; i < m_size; ++i)
            {
                m_weights[i] = ComputeWeight(i);
                m_totalWeight += m_weights[i];
            }
        }

        float output = 0.0f;
        if (m_size == 0)
        {
            return;
        }
        
        for (size_t i = 0; i < m_size; ++i)
        {
            if (m_weights[i] != 0)
            {
                output += input.ComputePhase(i) * m_weights[i];
            }
        }

        output /= m_totalWeight;

        m_output = m_slew.Process(output);
    }
};
            
struct PolyXFader : Module
{
    PolyXFaderInternal m_fader[16];
    PolyXFaderInternal::Input m_state[16];
    float m_values[16];

    IOMgr m_ioMgr;
    IOMgr::Input* m_input;
    IOMgr::Input* m_slope;
    IOMgr::Input* m_center;
    IOMgr::Input* m_mult;
    IOMgr::Input* m_attackFrac;
    IOMgr::Input* m_shape;
    IOMgr::Input* m_phaseShift;
    IOMgr::Output* m_output;

    PolyXFader()
        : m_ioMgr(this)
    {
        m_input = m_ioMgr.AddInput("Input", true);
        m_input->m_scale = 0.1;
        
        m_slope = m_ioMgr.AddInput("Slope", true);
        m_slope->m_scale = 0.1;
        m_center = m_ioMgr.AddInput("Center", true);
        m_center->m_scale = 0.1;

        m_mult = m_ioMgr.AddInput("Mult", true);
        m_mult->m_scale = 16.0 / 10.0;
        m_mult->m_offset = 1.0;

        m_attackFrac = m_ioMgr.AddInput("Skew", true);
        m_attackFrac->m_scale = 0.1;

        m_shape = m_ioMgr.AddInput("shape", true);
        m_shape->m_scale = 0.1;

        m_phaseShift = m_ioMgr.AddInput("Phase Shift", true);
        m_phaseShift->m_scale = 0.1;
        
        m_output = m_ioMgr.AddOutput("Output", true);
        m_output->m_scale = 10;

        for (size_t i = 0; i < 16; ++i)
        {
            m_input->SetTarget(i, &m_values[i]);
            m_slope->SetTarget(i, &m_state[i].m_slope);
            m_center->SetTarget(i, &m_state[i].m_center);

            m_mult->SetTarget(i, &m_state[i].m_mult);
            m_attackFrac->SetTarget(i, &m_state[i].m_attackFrac);
            m_shape->SetTarget(i, &m_state[i].m_shape);
            m_phaseShift->SetTarget(i, &m_state[i].m_phaseShift);
            
            m_output->SetSource(i, &m_fader[i].m_output);
            m_state[i].m_values = m_values;
        }

        m_ioMgr.Config();
    }

    void process(const ProcessArgs &args) override
    {
        m_ioMgr.Process();
        m_output->SetChannels(m_center->m_value.m_channels);
        for (int i = 0; i < m_center->m_value.m_channels; ++i)
        {
            m_state[i].m_size = m_input->m_value.m_channels;
            m_fader[i].Process(m_state[i]);
        }

        m_ioMgr.SetOutputs();
    }
};

struct PolyXFaderWidget : public ModuleWidget
{
    PolyXFaderWidget(PolyXFader* module)
    {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/PolyXFader.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        if (module)
        {
            module->m_input->Widget(this, 1, 1);
            module->m_slope->Widget(this, 2, 1);
            module->m_center->Widget(this, 3, 1);

            module->m_mult->Widget(this, 1, 2);
            module->m_attackFrac->Widget(this, 2, 2);
            module->m_shape->Widget(this, 3, 2);
            module->m_phaseShift->Widget(this, 5, 2);

            module->m_output->Widget(this, 1, 5);
        }
    }
};

#include "LatticeExpander.hpp"

struct LatticeExpanderWidget : ModuleWidget
{
    struct CustomTextFieldWidget : LedDisplayTextField
    {
        LatticeExpander* m_module;
        int m_xPos;
        int m_yPos;
        
        void Init(LatticeExpander* module, int xPos, int yPos)
        {
            m_module = module;
            m_xPos = xPos;
            m_yPos = yPos;
        }
        
        void step() override
        {
            if (m_module)
            {
                setText(m_module->m_noteNames[m_xPos][m_yPos].GetNoteString());
            }
            
            LedDisplayTextField::step();
        }
    };
    
    static constexpr float x_hp = 5.08;

    static constexpr float x_lightSpacingHP = 2.0;

    static constexpr float x_lightStartXHP = 2.0;
    static constexpr float x_lightStartYHP = 10.0;
    static constexpr float x_lightSpaceHP = 0.5;

    CustomTextFieldWidget* m_noteValues[LatticeExpanderConstants::x_gridSize][LatticeExpanderConstants::x_gridSize];	

    Vec GetLightMM(size_t x, size_t y, LatticeExpanderConstants::LightColor color)
    {
        using namespace LatticeExpanderConstants;

        float xOffset = color == LightColor::Green ? x_lightSpaceHP : 0;
        float yOffset = color == LightColor::Blue ? x_lightSpaceHP : 0;

        y = x_gridSize - y - 1;
        return Vec(x_hp * (x_lightStartXHP + x * x_lightSpacingHP + xOffset),
                   x_hp * (x_lightStartYHP + y * x_lightSpacingHP + yOffset));
    }

    LatticeExpanderWidget(LatticeExpander* module)
    {
        using namespace LatticeExpanderConstants;   

		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/LatticeExpander.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        
        for (size_t x = 0; x < x_gridSize; ++x)
        {
            for (size_t y = 0; y < x_gridSize; ++y)
            {
                addChild(createLightCentered<MediumLight<RedLight>>(
                             mm2px(GetLightMM(x, y, LightColor::Red)),
                             module,
                             GetLatticeLightId(x, y, LightColor::Red)));
                addChild(createLightCentered<MediumLight<GreenLight>>(
                             mm2px(GetLightMM(x, y, LightColor::Green)),
                             module,
                             GetLatticeLightId(x, y, LightColor::Green)));
                addChild(createLightCentered<MediumLight<BlueLight>>(
                             mm2px(GetLightMM(x, y, LightColor::Blue)),
                             module,
                             GetLatticeLightId(x, y, LightColor::Blue)));
            }
        }

        for (size_t x = 0; x < x_gridSize; ++x)
        {
            for (size_t y = 0; y < x_gridSize; ++y)
            {
                m_noteValues[x][y] = createWidget<CustomTextFieldWidget>(mm2px(GetLightMM(x, y, LightColor::Red)));
                m_noteValues[x][y]->box.size = mm2px(Vec(8 * x_hp * x_lightSpaceHP, 4 * x_hp * x_lightSpaceHP));
                m_noteValues[x][y]->Init(module, x, y);
                addChild(m_noteValues[x][y]);
            }
        }
    }
};

Model* modelLatticeExpander = createModel<LatticeExpander, LatticeExpanderWidget>("LatticeExpander");

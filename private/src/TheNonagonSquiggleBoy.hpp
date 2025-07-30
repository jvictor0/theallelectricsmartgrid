#pragma once

#include "SquiggleBoy.hpp"
#include "TheNonagon.hpp"
#include "TimerCell.hpp"

struct TheNonagonSquiggleBoyInternal
{
    struct SceneState
    {
        float m_blendFactor;
        bool m_shift;
        bool m_running;
        int m_leftScene;
        int m_rightScene;

        SceneState()
            : m_blendFactor(0)
            , m_shift(false)
            , m_running(false)
            , m_leftScene(0)
            , m_rightScene(1)
        {
        }

        void HandlePress(int scene, TheNonagonSquiggleBoyInternal* owner)
        {
            if (m_shift)
            {
                owner->CopyToScene(scene);  
            }
            else
            {
                if (m_blendFactor < 0.5)
                {
                    owner->SetRightScene(scene);
                }
                else
                {
                    owner->SetLeftScene(scene);
                }
            }
        }
    };

    SceneState m_sceneState;

    SquiggleBoyWithEncoderBank m_squiggleBoy;
    SquiggleBoyWithEncoderBank::Input m_squiggleBoyState;
    TheNonagonSmartGrid m_nonagon;

    TheNonagonSmartGrid::Trio m_activeTrio;

    QuadFloat m_output;

    double m_timer;

    void SaveJSON()
    {
        m_nonagon.SaveJSON();
        m_squiggleBoy.SaveJSON();
    }

    json_t* ToJSON()
    {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "nonagon", m_nonagon.ToJSON());
        json_object_set_new(rootJ, "squiggleBoy", m_squiggleBoy.ToJSON());
        return rootJ;
    }

    void LoadSavedJSON()
    {
        m_nonagon.LoadSavedJSON();
        m_squiggleBoy.LoadSavedJSON();
    }

    void FromJSON(json_t* rootJ)
    {
        json_t* nonagonJ = json_object_get(rootJ, "nonagon");
        if (nonagonJ)
        {
            m_nonagon.FromJSON(nonagonJ);
        }

        json_t* squiggleBoyJ = json_object_get(rootJ, "squiggleBoy");
        if (squiggleBoyJ)
        {
            m_squiggleBoy.FromJSON(squiggleBoyJ);
        }
    }

    void CopyToScene(int scene)
    {
        m_squiggleBoy.CopyToScene(scene);
        m_nonagon.CopyToScene(scene);
    }

    void SetRightScene(int scene)
    {
        m_sceneState.m_rightScene = scene;
        m_nonagon.SetRightScene(scene);
        m_squiggleBoyState.SetRightScene(scene);
    }

    void SetLeftScene(int scene)
    {
        m_sceneState.m_leftScene = scene;
        m_nonagon.SetLeftScene(scene);
        m_squiggleBoyState.SetLeftScene(scene);
    }

    void SetBlendFactor(float blendFactor)
    {
        m_sceneState.m_blendFactor = blendFactor;
    }

    void SetActiveTrio(TheNonagonSmartGrid::Trio trio)
    {
        m_activeTrio = trio;
        m_squiggleBoyState.SelectTrack(static_cast<int>(trio));
    }

    void SetSquiggleBoyInputs()
    {
        for (size_t i = 0; i < SquiggleBoy::x_numVoices; ++i)
        {
            if (m_nonagon.m_nonagon.m_multiPhasorGate.m_trig[i])            
            {
                m_squiggleBoyState.m_baseFreq[i] = PhaseUtils::VOctToNatural(m_nonagon.m_nonagon.m_output.m_voltPerOct[i], 1.0 / 48000.0);
            }

            m_squiggleBoy.m_state[i].m_gate = m_nonagon.m_nonagon.m_output.m_gate[i];
            m_squiggleBoy.m_state[i].m_trig = m_nonagon.m_nonagon.m_multiPhasorGate.m_trig[i];

            for (size_t j = 0; j < SquiggleBoyVoice::SquiggleLFO::x_numPhasors; ++j)
            {
                m_squiggleBoyState.m_totalPhasor[j] = m_nonagon.m_nonagon.m_output.m_totPhasors[j];
            }

            m_squiggleBoyState.m_sheafyModulators[i][0] = m_nonagon.m_nonagon.m_output.m_extraTimbre[i][0];
            m_squiggleBoyState.m_sheafyModulators[i][1] = m_nonagon.m_nonagon.m_output.m_extraTimbre[i][1];
            m_squiggleBoyState.m_sheafyModulators[i][2] = m_nonagon.m_nonagon.m_output.m_extraTimbre[i][2];
        }

        m_squiggleBoyState.m_shift = m_sceneState.m_shift;

        SetLeftScene(m_sceneState.m_leftScene);
        SetRightScene(m_sceneState.m_rightScene);
    }

    void SetNonagonInputs()
    {
        m_nonagon.m_state.m_shift = m_sceneState.m_shift;
        m_nonagon.m_state.m_running = m_sceneState.m_running;

        m_nonagon.m_state.m_theoryOfTimeInput.m_freq = m_squiggleBoyState.m_tempo.m_expParam;

        for (size_t i = 0; i < TheNonagonInternal::x_numVoices; ++i)
        {
            m_nonagon.m_state.m_multiPhasorGateInput.m_gateFrac[i] = m_squiggleBoy.m_voiceEncoderBank.GetValue(1, 3, 3, i);

            m_nonagon.m_state.m_arpInput.m_zoneHeight[i] = m_squiggleBoy.m_voiceEncoderBank.GetValue(2, 0, 2, i);
            m_nonagon.m_state.m_arpInput.m_zoneOverlap[i] = m_squiggleBoy.m_voiceEncoderBank.GetValue(2, 1, 2, i);
            m_nonagon.m_state.m_arpInput.m_offset[i] = m_squiggleBoy.m_voiceEncoderBank.GetValue(2, 0, 3, i);
            m_nonagon.m_state.m_arpInput.m_interval[i] = m_squiggleBoy.m_voiceEncoderBank.GetValue(2, 1, 3, i);
            m_nonagon.m_state.m_arpInput.m_pageInterval[i] = m_squiggleBoy.m_voiceEncoderBank.GetValue(2, 2, 3, i);
        }
    }

    void ConfigureEncoders()
    {
        m_squiggleBoy.m_voiceEncoderBank.Config(2, 0, 2, 0.25, "Sequencer Zone Height", m_squiggleBoyState.m_voiceEncoderBankInput);
        m_squiggleBoy.m_voiceEncoderBank.Config(2, 1, 2, 0.5, "Sequencer Zone Overlap", m_squiggleBoyState.m_voiceEncoderBankInput);
        m_squiggleBoy.m_voiceEncoderBank.Config(2, 0, 3, 0, "Sequencer Offset", m_squiggleBoyState.m_voiceEncoderBankInput);
        m_squiggleBoy.m_voiceEncoderBank.Config(2, 1, 3, 0, "Sequencer Interval", m_squiggleBoyState.m_voiceEncoderBankInput);
        m_squiggleBoy.m_voiceEncoderBank.Config(2, 2, 3, 0, "Sequencer Page Interval", m_squiggleBoyState.m_voiceEncoderBankInput);
    }

    void SetExternalInputs()
    {
        m_nonagon.SetBlendFactor(m_sceneState.m_blendFactor);
        m_squiggleBoyState.SetBlendFactor(m_sceneState.m_blendFactor);
    }

    void Process()
    {
        m_timer += 1.0 / 48000.0;

        SetExternalInputs();

        SetNonagonInputs();
        m_nonagon.Process(1.0 / 48000.0);
        
        SetSquiggleBoyInputs();
        m_squiggleBoy.Process(m_squiggleBoyState, 1.0 / 48000.0);

        m_output = m_squiggleBoy.m_output;
    }

    TheNonagonSquiggleBoyInternal()
        : m_nonagon(false)
        , m_activeTrio(TheNonagonSmartGrid::Trio::Fire)
        , m_timer(0)
    {
        m_nonagon.RemoveGridIds();
        m_squiggleBoy.Config(m_squiggleBoyState);
        ConfigureEncoders();
    }

    struct SaveLoadJSONCell : SmartGrid::Cell
    {
        TheNonagonSquiggleBoyInternal* m_owner;
        bool m_save;

        SaveLoadJSONCell(TheNonagonSquiggleBoyInternal* owner, bool save)
            : m_owner(owner)
            , m_save(save)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            SmartGrid::Color color = m_save ? SmartGrid::Color::Green : SmartGrid::Color::Red;
            return IsPressed() ? color : color.Dim();
        }

        virtual void OnPress(uint8_t) override
        {
            if (m_save)
            {
                m_owner->SaveJSON();
            }
            else
            {
                m_owner->LoadSavedJSON();
            }
        }
    };

    struct SceneSelectorCell : SmartGrid::Cell
    {
        TheNonagonSquiggleBoyInternal* m_owner;
        int m_scene;

        SceneSelectorCell(TheNonagonSquiggleBoyInternal* owner, int scene)
            : m_owner(owner)
            , m_scene(scene)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            if (m_owner->m_sceneState.m_leftScene == m_scene)
            {
                return SmartGrid::Color::Orange.AdjustBrightness(0.5 + 0.5 * (1.0 - m_owner->m_sceneState.m_blendFactor));
            }
            else if (m_owner->m_sceneState.m_rightScene == m_scene)
            {
                return SmartGrid::Color::SeaGreen.AdjustBrightness(0.5 + 0.5 * m_owner->m_sceneState.m_blendFactor);
            }

            return SmartGrid::Color::Grey;
        }
        
        virtual void OnPress(uint8_t velocity) override
        {
            m_owner->m_sceneState.HandlePress(m_scene, m_owner);
        }
    };

    SmartGrid::Cell* MakeShiftCell()
    {
        return new SmartGrid::StateCell<bool>(
                SmartGrid::Color::White.Dim() /*offColor*/,
                SmartGrid::Color::White /*onColor*/,
                &m_sceneState.m_shift,
                true,
                false,
                SmartGrid::StateCell<bool>::Mode::Momentary);
    }

    SmartGrid::Cell* MakeRunningCell()
    {
        return new SmartGrid::StateCell<bool>(
                SmartGrid::Color::Grey /*offColor*/,
                SmartGrid::Color::Green /*onColor*/,
                &m_sceneState.m_running,
                true,
                false,
                SmartGrid::StateCell<bool>::Mode::Toggle);
    }

    SmartGrid::Cell* MakeNoiseModeCell()
    {
        return new SmartGrid::StateCell<bool>(
                SmartGrid::Color::Grey /*offColor*/,
                SmartGrid::Color::White /*onColor*/,
                &m_squiggleBoy.m_mixerState.m_noiseMode,
                true,
                false,
                SmartGrid::StateCell<bool>::Mode::Toggle);
    }

    struct ActiveTrioIndicatorCell : SmartGrid::Cell
    {
        TheNonagonSquiggleBoyInternal* m_owner;

        ActiveTrioIndicatorCell(TheNonagonSquiggleBoyInternal* owner)
            : m_owner(owner)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            return TheNonagonSmartGrid::TrioColor(m_owner->m_activeTrio);
        }
    };

    struct RevertToDefaultCell : SmartGrid::Cell
    {
        TheNonagonSquiggleBoyInternal* m_owner;

        RevertToDefaultCell(TheNonagonSquiggleBoyInternal* owner)
            : m_owner(owner)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            return IsPressed() ? SmartGrid::Color::White : SmartGrid::Color::Grey;
        }

        virtual void OnPress(uint8_t velocity) override
        {
            m_owner->m_squiggleBoy.RevertToDefault();
        }
    };

    struct RecordCell : SmartGrid::Cell
    {
        TheNonagonSquiggleBoyInternal* m_owner;

        RecordCell(TheNonagonSquiggleBoyInternal* owner)
            : m_owner(owner)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            return m_owner->m_squiggleBoy.IsRecording() ? SmartGrid::Color::Red : SmartGrid::Color::Grey;
        }

        virtual void OnPress(uint8_t velocity) override
        {
            // m_owner->m_squiggleBoy.ToggleRecording();
            m_owner->m_timer = 0;
        }
    };
};

struct TheNonagonSquiggleBoyQuadLaunchpadTwister : TheNonagonSquiggleBoyInternal
{
    struct InteriorCell : SmartGrid::Cell
    {
        SmartGrid::Grid** m_grid;
        int m_x;
        int m_y;

        InteriorCell(SmartGrid::Grid** grid, int x, int y)
            : m_grid(grid)
            , m_x(x)
            , m_y(y)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            return (*m_grid)->GetColor(m_x, m_y);
        }

        virtual void OnPress(uint8_t velocity) override
        {
            (*m_grid)->OnPress(m_x, m_y, velocity);
        }

        virtual void OnRelease() override
        {
            (*m_grid)->OnRelease(m_x, m_y);
        }
    };

    struct NonagonGridBase : SmartGrid::Grid
    {
        TheNonagonSquiggleBoyQuadLaunchpadTwister* m_owner;
        SmartGrid::Grid* m_grid;

        NonagonGridBase(TheNonagonSquiggleBoyQuadLaunchpadTwister* owner)
            : m_owner(owner)
            , m_grid(nullptr)
        {
            for (size_t i = 0; i < SmartGrid::x_baseGridSize; ++i)
            {
                for (size_t j = 0; j < SmartGrid::x_baseGridSize; ++j)
                {
                    Put(i, j, new InteriorCell(&m_grid, i, j));
                }
            }
        }

        void SetGrid(SmartGrid::Grid* grid)
        {
            m_grid = grid;
        }
    };

    struct ToggleGridCell : SmartGrid::Cell
    {
        SmartGrid::Grid* m_grid;
        SmartGrid::Grid* m_replaceGrid;

        SmartGrid::Grid** m_target;

        ToggleGridCell(SmartGrid::Grid* grid, SmartGrid::Grid** target)
            : m_grid(grid)
            , m_target(target)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            return m_replaceGrid == nullptr ? m_grid->GetOffColor() : m_grid->GetOnColor();
        }

        virtual void OnPress(uint8_t velocity) override
        {
            m_replaceGrid = *m_target;
            *m_target = m_grid;
        }

        virtual void OnRelease() override
        {
            *m_target = m_replaceGrid;
            m_replaceGrid = nullptr;
        }
    };

    enum class TopGridsMode : uint8_t
    {
        Matrix,
        Water,
        Earth,
        Fire,
    };

    struct TopLeftGrid : NonagonGridBase
    {
        TopLeftGrid(TheNonagonSquiggleBoyQuadLaunchpadTwister* owner)
            : NonagonGridBase(owner)
        {
            SetGrid(m_owner->m_nonagon.m_lameJuisMatrixGrid);

            for (size_t i = 0; i < SmartGrid::x_baseGridSize; ++i)
            {
                Put(SmartGrid::x_baseGridSize, i, new ActiveTrioIndicatorCell(owner));
            }
        }
    };

    struct TopRightGrid : NonagonGridBase
    {
        TopRightGrid(TheNonagonSquiggleBoyQuadLaunchpadTwister* owner)
            : NonagonGridBase(owner)
        {
            SetGrid(m_owner->m_nonagon.m_lameJuisRHSGrid);
        }
    };

    struct BottomLeftGrid : NonagonGridBase
    {
        BottomLeftGrid(TheNonagonSquiggleBoyQuadLaunchpadTwister* owner)
            : NonagonGridBase(owner)
        {
            SetGrid(m_owner->m_nonagon.m_lameJuisCoMuteGrid);

            for (int i = 0; i < SmartGrid::x_baseGridSize; ++i)
            {
                Put(i, SmartGrid::x_baseGridSize + 1, new SceneSelectorCell(owner, i));
            }

            Put(-1, 5, owner->MakeNoiseModeCell());
            Put(-1, 6, owner->MakeRunningCell());
            Put(-1, 7, new RecordCell(owner));

            for (size_t i = 0; i < SmartGrid::x_baseGridSize; ++i)
            {
                Put(i, -1, new TimerCell(&owner->m_timer, i));
            }
        }
    };

    struct BottomRightGrid : NonagonGridBase
    {
        struct TopGridSelect : SmartGrid::Cell
        {
            TheNonagonSquiggleBoyQuadLaunchpadTwister* m_owner;
            TopGridsMode m_mode;
            SmartGrid::Color m_color;

            TopGridSelect(
                TheNonagonSquiggleBoyQuadLaunchpadTwister* owner, 
                TopGridsMode mode,
                SmartGrid::Color color)
                : m_owner(owner)
                , m_mode(mode)
                , m_color(color)
            {
            }

            virtual SmartGrid::Color GetColor() override
            {
                return m_owner->m_topGridsMode == m_mode ? m_color : m_color.Dim();
            }

            virtual void OnPress(uint8_t velocity) override
            {
                m_owner->SetTopGridsMode(m_mode);
            }
        };

        BottomRightGrid(TheNonagonSquiggleBoyQuadLaunchpadTwister* owner)
            : NonagonGridBase(owner)
        {
            SetGrid(m_owner->m_nonagon.m_theoryOfTimeTopologyGrid);
            Put(SmartGrid::x_baseGridSize, 4, new TopGridSelect(owner, TopGridsMode::Matrix, SmartGrid::Color::Indigo));
            Put(SmartGrid::x_baseGridSize, 5, new TopGridSelect(owner, TopGridsMode::Water, SmartGrid::Color::Blue));
            Put(SmartGrid::x_baseGridSize, 6, new TopGridSelect(owner, TopGridsMode::Earth, SmartGrid::Color::Green));
            Put(SmartGrid::x_baseGridSize, 7, new TopGridSelect(owner, TopGridsMode::Fire, SmartGrid::Color::Red));
            Put(SmartGrid::x_baseGridSize - 1, SmartGrid::x_baseGridSize, new ToggleGridCell(owner->m_nonagon.m_lameJuisIntervalGrid, &m_grid));

            Put(-1, 0, new SaveLoadJSONCell(owner, true));
            Put(-1, 1, new SaveLoadJSONCell(owner, false));

            Put(-1, 4, new RevertToDefaultCell(owner));

            for (size_t i = 0; i < SquiggleBoyWithEncoderBank::x_numVoiceBanks; ++i)
            {
                Put(i, SmartGrid::x_baseGridSize + 1, new SquiggleBoyWithEncoderBank::SelectorCell(&owner->m_squiggleBoy, i));
            }

            for (size_t i = 0; i < SquiggleBoyWithEncoderBank::x_numGlobalBanks; ++i)
            {
                Put(i, SmartGrid::x_baseGridSize , new SquiggleBoyWithEncoderBank::SelectorCell(&owner->m_squiggleBoy, i + SquiggleBoyWithEncoderBank::x_numVoiceBanks));
            }

            Put(SmartGrid::x_baseGridSize - 1, SmartGrid::x_baseGridSize + 1, owner->MakeShiftCell());

            for (size_t i = 0; i < SmartGrid::x_baseGridSize; ++i)
            {
                Put(i, -1, new TimerCell(&owner->m_timer, i + SmartGrid::x_baseGridSize));
            }
        }
    };

    TopLeftGrid* m_topLeftGrid;
    TopRightGrid* m_topRightGrid;
    BottomLeftGrid* m_bottomLeftGrid;
    BottomRightGrid* m_bottomRightGrid;

    TopGridsMode m_topGridsMode;
    SmartGrid::GridHolder m_gridHolder;

    void SetTopGridsMode(TopGridsMode mode)
    {
        m_topGridsMode = mode;

        switch (mode)
        {
            case TopGridsMode::Matrix:
            {
                m_topLeftGrid->SetGrid(m_nonagon.m_lameJuisMatrixGrid);
                m_topRightGrid->SetGrid(m_nonagon.m_lameJuisRHSGrid);
                break;
            }
            case TopGridsMode::Fire:
            {
                m_topLeftGrid->SetGrid(m_nonagon.m_sheafViewGridFireGrid);
                m_topRightGrid->SetGrid(m_nonagon.m_indexArpFireGrid);
                SetActiveTrio(TheNonagonSmartGrid::Trio::Fire);
                break;
            }
            case TopGridsMode::Earth:
            {
                m_topLeftGrid->SetGrid(m_nonagon.m_sheafViewGridEarthGrid);
                m_topRightGrid->SetGrid(m_nonagon.m_indexArpEarthGrid);
                SetActiveTrio(TheNonagonSmartGrid::Trio::Earth);
                break;
            }
            case TopGridsMode::Water:
            {
                m_topLeftGrid->SetGrid(m_nonagon.m_sheafViewGridWaterGrid);
                m_topRightGrid->SetGrid(m_nonagon.m_indexArpWaterGrid);
                SetActiveTrio(TheNonagonSmartGrid::Trio::Water);
                break;
            }
        }
    }

    TheNonagonSquiggleBoyQuadLaunchpadTwister()
        : m_topGridsMode(TopGridsMode::Matrix)
    {
        m_topLeftGrid = new TopLeftGrid(this);
        m_topRightGrid = new TopRightGrid(this);
        m_bottomLeftGrid = new BottomLeftGrid(this);
        m_bottomRightGrid = new BottomRightGrid(this);

        SetTopGridsMode(TopGridsMode::Matrix);

        m_gridHolder.AddGrid(m_topLeftGrid);
        m_gridHolder.AddGrid(m_topRightGrid);
        m_gridHolder.AddGrid(m_bottomLeftGrid);
        m_gridHolder.AddGrid(m_bottomRightGrid);
    }

    void Process()
    {
        m_gridHolder.Process(1.0 / 48000.0);
        TheNonagonSquiggleBoyInternal::Process();
    }
 };

#ifndef IOS_BUILD
struct TheNonagonSquiggleBoyQuadLaunchpadTwisterModule : Module
{
    IOMgr m_ioMgr;
    TheNonagonSquiggleBoyQuadLaunchpadTwister m_nonagonSquiggleBoy;

    // IOMgr Outputs
    //
    IOMgr::Output* m_topLeftGridIdOutput;
    IOMgr::Output* m_topRightGridIdOutput;
    IOMgr::Output* m_bottomLeftGridIdOutput;
    IOMgr::Output* m_bottomRightGridIdOutput;
    IOMgr::Output* m_squiggleBoyEncoderBankOutput;
    IOMgr::Output* m_mainOutput;

    // IOMgr Inputs
    //
    IOMgr::Input* m_blendFactorInput;
    IOMgr::Input* m_faderInput;

    TheNonagonSquiggleBoyQuadLaunchpadTwisterModule()
        : m_ioMgr(this)
    {
        // Add grid id outputs
        //
        m_topLeftGridIdOutput = m_ioMgr.AddOutput("Top Left Grid ID", false);
        m_topLeftGridIdOutput->SetSource(0, &m_nonagonSquiggleBoy.m_topLeftGrid->m_gridId);

        m_topRightGridIdOutput = m_ioMgr.AddOutput("Top Right Grid ID", false);
        m_topRightGridIdOutput->SetSource(0, &m_nonagonSquiggleBoy.m_topRightGrid->m_gridId);

        m_bottomLeftGridIdOutput = m_ioMgr.AddOutput("Bottom Left Grid ID", false);
        m_bottomLeftGridIdOutput->SetSource(0, &m_nonagonSquiggleBoy.m_bottomLeftGrid->m_gridId);

        m_bottomRightGridIdOutput = m_ioMgr.AddOutput("Bottom Right Grid ID", false);
        m_bottomRightGridIdOutput->SetSource(0, &m_nonagonSquiggleBoy.m_bottomRightGrid->m_gridId);

        // Add squiggle boy encoder bank output
        //
        m_squiggleBoyEncoderBankOutput = m_ioMgr.AddOutput("Squiggle Boy Encoder Bank", false);
        m_squiggleBoyEncoderBankOutput->SetSource(0, &m_nonagonSquiggleBoy.m_squiggleBoy.m_selectedGridId);

        // Add main output
        //
        m_mainOutput = m_ioMgr.AddOutput("Main", true);
        m_mainOutput->m_scale = 5;
        m_mainOutput->SetChannels(4);
        for (size_t i = 0; i < 4; ++i)
        {
            m_mainOutput->SetSource(i, &m_nonagonSquiggleBoy.m_output[i]);
        }

        // Add blend factor input
        //
        m_blendFactorInput = m_ioMgr.AddInput("Blend Factor", true);
        m_blendFactorInput->m_scale = 0.1;
        m_blendFactorInput->SetTarget(0, &m_nonagonSquiggleBoy.m_sceneState.m_blendFactor);

        // Add fader input
        //
        m_faderInput = m_ioMgr.AddInput("Fader", true);
        m_faderInput->m_scale = 0.1;
        m_faderInput->SetChannels(8);
        for (size_t i = 0; i < 8; ++i)
        {
            m_faderInput->SetTarget(i, &m_nonagonSquiggleBoy.m_squiggleBoyState.m_faders[i]);
        }

        m_ioMgr.Config();
    }

    virtual json_t* dataToJson() override
    {
        return m_nonagonSquiggleBoy.ToJSON();
    }

    virtual void dataFromJson(json_t* rootJ) override
    {
        m_nonagonSquiggleBoy.FromJSON(rootJ);
        m_nonagonSquiggleBoy.SaveJSON();
    }

    void process(const ProcessArgs &args) override
    {
        m_ioMgr.Process();
        
        m_nonagonSquiggleBoy.Process();
        
        m_ioMgr.SetOutputs();
    }
};

struct TheNonagonSquiggleBoyQuadLaunchpadTwisterWidget : public ModuleWidget
{
    TheNonagonSquiggleBoyQuadLaunchpadTwisterWidget(TheNonagonSquiggleBoyQuadLaunchpadTwisterModule* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TheNonagonSquiggleBoy.svg")));

        // Add widgets
        //
        if (module)
        {
            module->m_topLeftGridIdOutput->Widget(this, 1, 1);
            module->m_topRightGridIdOutput->Widget(this, 1, 2);
            module->m_bottomLeftGridIdOutput->Widget(this, 1, 3);
            module->m_bottomRightGridIdOutput->Widget(this, 1, 4);
            module->m_squiggleBoyEncoderBankOutput->Widget(this, 1, 5);
            module->m_mainOutput->Widget(this, 1, 6);
            module->m_blendFactorInput->Widget(this, 1, 7);
            module->m_faderInput->Widget(this, 1, 8);
        }
    }

    void appendContextMenu(Menu* menu) override
    {
        TheNonagonSquiggleBoyQuadLaunchpadTwisterModule* module = dynamic_cast<TheNonagonSquiggleBoyQuadLaunchpadTwisterModule*>(this->module);
        if (!module)
        {
            return;
        }

        menu->addChild(new MenuSeparator());

        // Add menu item to select recording directory
        //
        menu->addChild(createSubmenuItem("Recording Directory", "", [=](Menu* menu) {
            // Show current directory
            //
            std::string currentDir = module->m_nonagonSquiggleBoy.m_squiggleBoy.GetRecordingDirectory().empty() ? "Not set" : module->m_nonagonSquiggleBoy.m_squiggleBoy.GetRecordingDirectory();
            menu->addChild(createMenuLabel("Current: " + currentDir));
            
            menu->addChild(new MenuSeparator());
            
            // Add option to select directory
            //
            menu->addChild(createMenuItem("Select Directory", "", [=]() {
                const char* path = osdialog_file(OSDIALOG_OPEN_DIR, module->m_nonagonSquiggleBoy.m_squiggleBoy.GetRecordingDirectory().empty() ? nullptr : module->m_nonagonSquiggleBoy.m_squiggleBoy.GetRecordingDirectory().c_str(), nullptr, nullptr);
                if (path)
                {
                    module->m_nonagonSquiggleBoy.m_squiggleBoy.SetRecordingDirectory(path);
                    free((void*)path);
                }
            }));
        }));
    }
};
#endif
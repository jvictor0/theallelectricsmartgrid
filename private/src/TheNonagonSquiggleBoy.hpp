#pragma once

#include "QuadUtils.hpp"
#include "SquiggleBoy.hpp"
#include "TheNonagon.hpp"
#include "TimerCell.hpp"
#include "MessageInBus.hpp"
#include "PadUI.hpp"
#include "AnalogUIState.hpp"
#include "StateInterchange.hpp"

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
    struct UIState
    {
        SquiggleBoyWithEncoderBank::UIState m_squiggleBoyUIState;
        TheNonagonInternal::UIState m_nonagonUIState;
        AnalogUIState<1 + SquiggleBoyWithEncoderBank::x_numFaders> m_analogUIState;
    };

    SquiggleBoyWithEncoderBank::Input m_squiggleBoyState;
    TheNonagonSmartGrid m_nonagon;

    UIState m_uiState;

    TheNonagonSmartGrid::Trio m_activeTrio;

    QuadFloatWithStereoAndSub m_output;

    double m_timer;

    Blink m_blink;

    StateInterchange m_stateInterchange;

    void SetRecordingDirectory(const char* directory)
    {
        m_squiggleBoy.SetRecordingDirectory(directory);
    }

    JSON ToJSON()
    {
        JSON rootJ = JSON::Object();
        rootJ.SetNew("nonagon", m_nonagon.ToJSON());
        rootJ.SetNew("squiggleBoy", m_squiggleBoy.ToJSON());
        return rootJ;
    }

    void FromJSON(JSON rootJ)
    {
        JSON nonagonJ = rootJ.Get("nonagon");
        if (!nonagonJ.IsNull())
        {
            m_nonagon.FromJSON(nonagonJ);
        }

        JSON squiggleBoyJ = rootJ.Get("squiggleBoy");
        if (!squiggleBoyJ.IsNull())
        {
            m_squiggleBoy.FromJSON(squiggleBoyJ);
        }
    }

    void CopyToScene(int scene)
    {
        m_squiggleBoy.CopyToScene(scene);
        m_nonagon.CopyToScene(scene);
    }

    void ClearGesture(int gesture)
    {
        m_squiggleBoy.ClearGesture(gesture);
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

    void HandleParamSet(SmartGrid::MessageIn msg)
    {
        if (msg.m_x == 0)
        {
            m_sceneState.m_blendFactor = msg.AmountFloat();
        }
        else
        {
            m_squiggleBoyState.m_faders[msg.m_x - 1] = msg.AmountFloat();
        }
    }

    void Apply(SmartGrid::MessageIn msg)
    {
        if (msg.IsParamSet())
        {
            HandleParamSet(msg);
        }
        else
        {
            m_squiggleBoy.Apply(msg);
        }
    }

    void SetSquiggleBoyInputs()
    {
        for (size_t i = 0; i < SquiggleBoy::x_numVoices; ++i)
        {
            if (m_nonagon.m_nonagon.m_multiPhasorGate.m_adspControl[i].m_trig)            
            {
                m_squiggleBoyState.m_baseFreq[i] = PhaseUtils::VOctToNatural(m_nonagon.m_nonagon.m_output.m_voltPerOct[i], 1.0 / 48000.0);
                m_squiggleBoyState.m_ladderBaseFreq[i] = std::pow(m_squiggleBoyState.m_baseFreq[i], 0.7);
            }

            m_squiggleBoy.m_state[i].m_adspControl = m_nonagon.m_nonagon.m_multiPhasorGate.m_adspControl[i];

            for (size_t j = 0; j < SquiggleBoyVoice::SquiggleLFO::x_numPhasors; ++j)
            {
                m_squiggleBoyState.m_totalPhasor[j] = m_nonagon.m_nonagon.m_output.m_totPhasors[j];
                m_squiggleBoyState.m_totalTop[j] = m_nonagon.m_nonagon.m_output.m_totTop[j];
            }

            m_squiggleBoyState.m_sheafyModulators[i][0] = m_nonagon.m_nonagon.m_output.m_extraTimbre[i][0];
            m_squiggleBoyState.m_sheafyModulators[i][1] = m_nonagon.m_nonagon.m_output.m_extraTimbre[i][1];
            m_squiggleBoyState.m_sheafyModulators[i][2] = m_nonagon.m_nonagon.m_output.m_extraTimbre[i][2];
        }

        m_squiggleBoyState.m_top = m_nonagon.m_nonagon.m_theoryOfTime.m_loops[TheoryOfTimeBase::x_numLoops - 1].m_top;
        m_squiggleBoyState.m_shift = m_sceneState.m_shift;

        SetLeftScene(m_sceneState.m_leftScene);
        SetRightScene(m_sceneState.m_rightScene);
    }

    void SetNonagonInputs()
    {
        m_nonagon.m_state.m_shift = m_sceneState.m_shift;
        m_nonagon.m_state.m_running = m_sceneState.m_running;

        TheoryOfTime::Input& theoryOfTimeInput = m_nonagon.m_state.m_theoryOfTimeInput;
        
        theoryOfTimeInput.m_freq = m_squiggleBoyState.m_tempo.m_expParam;

        theoryOfTimeInput.m_phaseModLFOInput.m_attackFrac = theoryOfTimeInput.m_lfoSkewFilter.Process(m_squiggleBoy.m_globalEncoderBank.GetValue(0, 0, 2, 0));
        theoryOfTimeInput.m_lfoMult.Update(theoryOfTimeInput.m_lfoMultFilter.Process(m_squiggleBoy.m_globalEncoderBank.GetValue(0, 1, 2, 0)));
        theoryOfTimeInput.m_phaseModLFOInput.m_shape = theoryOfTimeInput.m_lfoShapeFilter.Process(m_squiggleBoy.m_globalEncoderBank.GetValue(0, 2, 2, 0));
        theoryOfTimeInput.m_phaseModLFOInput.m_center = 1 - theoryOfTimeInput.m_lfoCenterFilter.Process(m_squiggleBoy.m_globalEncoderBank.GetValue(0, 0, 3, 0));
        theoryOfTimeInput.m_phaseModLFOInput.m_slope = theoryOfTimeInput.m_lfoSlopeFilter.Process(m_squiggleBoy.m_globalEncoderBank.GetValue(0, 1, 3, 0));
        theoryOfTimeInput.m_modIndex.Update(theoryOfTimeInput.m_lfoIndexFilter.Process(m_squiggleBoy.m_globalEncoderBank.GetValue(0, 3, 3, 0)));

        for (size_t i = 0; i < TheNonagonInternal::x_numVoices; ++i)
        {
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

    void HandleStateInterchange()
    {
        if (m_stateInterchange.IsSaveRequested())
        {
            INFO("Save JSON request received");
            JSON toSave = ToJSON();
            INFO("JSON serialized");
            m_stateInterchange.AckSaveRequested(toSave);
        }

        if (m_stateInterchange.IsLoadRequested())
        {
            INFO("Load JSON request received");
            FromJSON(m_stateInterchange.GetToLoad());
            INFO("JSON deserialized");
            m_stateInterchange.AckLoadCompleted();
        }
    }

    QuadFloatWithStereoAndSub ProcessSample()
    {
        m_blink.Process();
        m_squiggleBoyState.SetBlink(m_blink.m_blink);

        m_timer += 1.0 / 48000.0;

        SetExternalInputs();

        SetNonagonInputs();
        m_nonagon.ProcessSample(1.0 / 48000.0);
        
        SetSquiggleBoyInputs();
        m_squiggleBoy.ProcessSample(m_squiggleBoyState, 1.0 / 48000.0);

        m_output = m_squiggleBoy.m_output;

        m_uiState.m_squiggleBoyUIState.AdvanceScopeIndices();

        return m_output;
    }

    void ProcessFrame()
    {
        m_squiggleBoy.ProcessFrame();
        m_nonagon.ProcessFrame(&m_uiState.m_nonagonUIState);
        PopulateUIState();
        HandleStateInterchange();
    }

    void PopulateUIState()
    {
        m_squiggleBoy.PopulateUIState(&m_uiState.m_squiggleBoyUIState);
        if (m_squiggleBoy.AreVoiceEncodersActive())
        {
            for (size_t i = 0; i < TheNonagonInternal::x_numVoices; ++i)
            {
                SmartGrid::Color color = TheNonagonSmartGrid::VoiceColor(i);
                m_uiState.m_squiggleBoyUIState.m_encoderBankUIState.SetIndicatorColor(i, color);
            }
        }
        else
        {
            for (size_t i = 0; i < 4; ++i)
            {                
                m_uiState.m_squiggleBoyUIState.m_encoderBankUIState.SetIndicatorColor(i, SmartGrid::Color::White);
            }
        }

        m_uiState.m_analogUIState.SetValue(0, m_sceneState.m_blendFactor);
        for (size_t i = 0; i < SquiggleBoyWithEncoderBank::x_numFaders; ++i)
        {
            m_uiState.m_analogUIState.SetValue(i + 1, m_squiggleBoyState.m_faders[i]);
        }

        for (size_t i = 0; i < TheNonagonInternal::x_numVoices; ++i)
        {
            m_uiState.m_squiggleBoyUIState.SetMuted(i, m_nonagon.m_state.m_trigLogic.m_mute[i]);
        }
    }

    TheNonagonSquiggleBoyInternal()
        : m_nonagon(false)
        , m_activeTrio(TheNonagonSmartGrid::Trio::Fire)
        , m_timer(0)
    {
        m_nonagon.RemoveGridIds();
        m_squiggleBoy.Config(m_squiggleBoyState);
        m_squiggleBoy.m_theoryOfTime = &m_nonagon.m_nonagon.m_theoryOfTime;
        ConfigureEncoders();
        m_squiggleBoy.SetupUIState(&m_uiState.m_squiggleBoyUIState);
        m_nonagon.SetupMonoScopeWriter(&m_uiState.m_squiggleBoyUIState.m_monoScopeWriter);
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
                bool success = m_owner->m_stateInterchange.RequestSave();
                if (success)
                {
                    INFO("Save requested");
                }
                else
                {
                    INFO("Save requested failed");
                }
            }
            else
            {
                INFO("Loading saved JSON");
                m_owner->FromJSON(m_owner->m_stateInterchange.m_lastSave);
                INFO("Loaded saved JSON");
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

    struct GestureSelectorCell : SmartGrid::Cell
    {
        TheNonagonSquiggleBoyInternal* m_owner;
        int m_gesture;

        GestureSelectorCell(TheNonagonSquiggleBoyInternal* owner, int gesture)
            : m_owner(owner)
            , m_gesture(gesture)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            if (m_owner->m_squiggleBoyState.m_selectedGesture == m_gesture)
            {
                return SmartGrid::Color::Red;
            }
            else
            {
                return SmartGrid::Color::Grey;
            }
        }

        virtual void OnPress(uint8_t velocity) override
        {
            if (m_owner->m_sceneState.m_shift)
            {
                m_owner->ClearGesture(m_gesture);
            }
            else if (m_owner->m_squiggleBoyState.m_selectedGesture != m_gesture)
            {
                m_owner->m_squiggleBoyState.SelectGesture(m_gesture);
            }
        }

        virtual void OnRelease() override
        {
            if (m_owner->m_squiggleBoyState.m_selectedGesture == m_gesture)
            {
                m_owner->m_squiggleBoyState.SelectGesture(-1);
            }
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
                SmartGrid::Color::Green.Dim() /*offColor*/,
                SmartGrid::Color::Green /*onColor*/,
                &m_sceneState.m_running,
                true,
                false,
                SmartGrid::StateCell<bool>::Mode::Toggle);
    }

    SmartGrid::Cell* MakeNoiseModeCell()
    {
        return new SmartGrid::StateCell<bool>(
                SmartGrid::Color::Pink /*offColor*/,
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
            return m_owner->m_squiggleBoy.IsRecording() ? SmartGrid::Color::Red : SmartGrid::Color::Red.Dim();
        }

        virtual void OnPress(uint8_t velocity) override
        {
            m_owner->m_squiggleBoy.ToggleRecording();
            m_owner->m_timer = 0;
        }
    };
};


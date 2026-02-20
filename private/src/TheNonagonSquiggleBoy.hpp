#pragma once

#include "QuadUtils.hpp"
#include "SquiggleBoy.hpp"
#include "TheNonagon.hpp"
#include "TimerCell.hpp"
#include "MessageInBus.hpp"
#include "PadUI.hpp"
#include "AnalogUIState.hpp"
#include "StateInterchange.hpp"
#include "MessageOut.hpp"
#include "StateSaver.hpp"
#include "SquiggleBoyConfig.hpp"

struct TheNonagonSquiggleBoyInternal
{
    // Centralized scene manager - source of truth for scene state and shift
    //
    SmartGrid::SceneManager m_sceneManager;

    // Running state (not part of scene management)
    //
    bool m_running;

    StateSaver m_stateSaver;

    SquiggleBoyConfigGrid m_configGrid;

    SquiggleBoyWithEncoderBank m_squiggleBoy;
    struct UIState
    {
        SquiggleBoyWithEncoderBank::UIState m_squiggleBoyUIState;
        TheNonagonInternal::UIState m_nonagonUIState;
        AnalogUIState<1 + SquiggleBoyWithEncoderBank::x_numFaders> m_analogUIState;
    };

    struct IOState
    {
        KMixMidi m_kMixMidi;
    };

    SquiggleBoyWithEncoderBank::Input m_squiggleBoyState;
    TheNonagonSmartGrid m_nonagon;

    UIState m_uiState;
    IOState m_ioState;

    TheNonagonSmartGrid::Trio m_activeTrio;

    QuadFloatWithStereoAndSub m_output;

    double m_timer;

    StateInterchange m_stateInterchange;

    SmartGrid::MessageOutBuffer m_messageOutBuffer;

    void SetRecordingDirectory(const char* directory)
    {
        m_squiggleBoy.SetRecordingDirectory(directory);
    }

    JSON ToJSON()
    {
        JSON rootJ = JSON::Object();
        rootJ.SetNew("nonagon", m_nonagon.ToJSON());
        rootJ.SetNew("squiggleBoy", m_squiggleBoy.ToJSON());
        rootJ.SetNew("stateSaver", m_stateSaver.ToJSON());
        return rootJ;
    }

    void FromJSON(JSON rootJ)
    {
        JSON nonagonJ = rootJ.Get("nonagon");
        if (!nonagonJ.IsNull())
        {
            m_nonagon.FromJSON(nonagonJ);
        }

        m_stateSaver.SetFromJSON(rootJ.Get("stateSaver"));

        JSON squiggleBoyJ = rootJ.Get("squiggleBoy");
        if (!squiggleBoyJ.IsNull())
        {
            m_squiggleBoy.FromJSON(squiggleBoyJ);
        }
        
        m_configGrid.PropagateSourceSelection();
        m_squiggleBoy.SelectGridId();
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

    void RevertToDefault(bool allScenes, bool allTracks)
    {
        m_squiggleBoy.RevertToDefault(allScenes, allTracks);
        m_nonagon.RevertToDefault(allScenes);
        if (allScenes)
        {
            m_stateSaver.RevertToDefaultAllScenes();
            m_configGrid.RevertToDefault();
        }
    }

    void SetRightScene(int scene)
    {
        m_sceneManager.m_scene2 = scene;
    }

    void SetLeftScene(int scene)
    {
        m_sceneManager.m_scene1 = scene;
    }

    void SetBlendFactor(float blendFactor)
    {
        m_sceneManager.m_blendFactor = blendFactor;
    }

    void SetActiveTrio(TheNonagonSmartGrid::Trio trio)
    {
        m_activeTrio = trio;
        m_squiggleBoy.SetTrack(static_cast<size_t>(trio));
    }

    void HandleParamSet(SmartGrid::MessageIn msg)
    {
        if (msg.m_x == 0)
        {
            m_sceneManager.m_blendFactor = msg.AmountFloat();
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
            if (m_nonagon.m_nonagon.m_multiPhasorGate.m_ahdControl[i].m_trig)            
            {
                float baseFreq = PhaseUtils::VOctToNatural(m_nonagon.m_nonagon.m_output.m_voltPerOct[i], 1.0 / 48000.0);
                m_squiggleBoy.m_state[i].m_ampInput.m_subTrig = m_nonagon.m_state.m_trigLogic.IsUnisonMaster(i);

                // UNDONE(DEEP_VOCODER)
                //
                float ratio = m_nonagon.m_state.m_trioOctaveSwitchesInput.OctavizedRatio(i);
                baseFreq = baseFreq / ratio;
                m_squiggleBoy.m_deepVocoderState.m_voiceInput[i].m_pitchCenter = baseFreq;
                m_squiggleBoy.m_deepVocoderState.m_voiceInput[i].m_pitchRatioPost = ratio;
                baseFreq = m_squiggleBoy.m_deepVocoder.TransformNote(i, &m_nonagon.m_nonagon.m_multiPhasorGate.m_ahdControl[i]);        
                if (m_nonagon.m_nonagon.m_multiPhasorGate.m_ahdControl[i].m_trig)                      
                {
                    m_squiggleBoyState.m_baseFreq[i] = baseFreq;
                }
            }

            m_squiggleBoy.m_state[i].m_ahdControl = m_nonagon.m_nonagon.m_multiPhasorGate.m_ahdControl[i];            

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
    }

    void SetNonagonInputs()
    {
        m_nonagon.m_state.m_shift = m_sceneManager.m_shift;
        m_nonagon.m_state.m_running = m_running;

        TheoryOfTime::Input& theoryOfTimeInput = m_nonagon.m_state.m_theoryOfTimeInput;
        
        theoryOfTimeInput.m_freq = m_squiggleBoyState.m_tempo.m_expParam;

        m_nonagon.m_state.m_theoryOfTimeInput.m_pllInput.m_phaseLearnRate.Update(m_squiggleBoy.m_globalEncoderBank.GetValue(0, 0, 1, 0));
        m_nonagon.m_state.m_theoryOfTimeInput.m_pllInput.m_freqLearnRate.Update(m_squiggleBoy.m_globalEncoderBank.GetValue(0, 1, 1, 0));
        m_nonagon.m_state.m_theoryOfTimeInput.m_pllInput.m_phaseLearnApplicationRate.Update(m_squiggleBoy.m_globalEncoderBank.GetValue(0, 2, 1, 0));
        m_nonagon.m_state.m_theoryOfTimeInput.m_pllInput.m_freqLearnApplicationRate.Update(m_squiggleBoy.m_globalEncoderBank.GetValue(0, 3, 1, 0));

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

        if (m_stateInterchange.IsNewRequested())
        {
            INFO("New patch request received");
            RevertToDefault(true, true);
            INFO("Reverted to default");
            m_stateInterchange.AckNewCompleted();
        }
    }

    QuadFloatWithStereoAndSub ProcessSample(const AudioInputBuffer& audioInputBuffer)
    {
        // Process scene manager first to set changed flags
        //
        m_sceneManager.Process();

        // Process state saver with scene manager state
        //
        m_stateSaver.Process();

        m_timer += 1.0 / 48000.0;        

        SetNonagonInputs();
        m_nonagon.ProcessSample(1.0 / 48000.0);
        
        SetSquiggleBoyInputs();
        m_squiggleBoy.ProcessSample(m_squiggleBoyState, 1.0 / 48000.0, audioInputBuffer);

        m_output = m_squiggleBoy.m_output;

        m_uiState.m_squiggleBoyUIState.AdvanceScopeIndices();

        return m_output;
    }

    void ProcessFrame()
    {
        m_squiggleBoy.ProcessFrame();
        m_nonagon.ProcessFrame(&m_uiState.m_nonagonUIState);
        PopulateUIState();
        PopulateIOState();
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

            m_uiState.m_squiggleBoyUIState.m_encoderBankUIState.SetMainIndicatorColor(TheNonagonSmartGrid::TrioColor(m_activeTrio));
        }
        else
        {
            for (size_t i = 0; i < 4; ++i)
            {                
                m_uiState.m_squiggleBoyUIState.m_encoderBankUIState.SetIndicatorColor(i, SmartGrid::Color::White);
            }

            m_uiState.m_squiggleBoyUIState.m_encoderBankUIState.SetMainIndicatorColor(SmartGrid::Color::White);
        }

        m_uiState.m_analogUIState.SetValue(0, m_sceneManager.m_blendFactor);
        for (size_t i = 0; i < SquiggleBoyWithEncoderBank::x_numFaders; ++i)
        {
            m_uiState.m_analogUIState.SetValue(i + 1, m_squiggleBoyState.m_faders[i]);
        }

    }

    void PopulateIOState()
    {
        m_squiggleBoy.WriteKMixMidi(&m_ioState.m_kMixMidi);
    }

    TheNonagonSquiggleBoyInternal()
        : m_running(false)
        , m_nonagon(false)
        , m_activeTrio(TheNonagonSmartGrid::Trio::Fire)
        , m_timer(0)
    {
        // Initialize components with scene manager pointer
        //
        m_squiggleBoy.Init(&m_sceneManager);        
        m_nonagon.SetSceneManager(&m_sceneManager);

        m_squiggleBoy.m_stateSaver = &m_stateSaver;
        m_configGrid.Init(&m_squiggleBoy);
        m_stateSaver.Insert("sceneStateLeft", &m_sceneManager.m_scene1);
        m_stateSaver.Insert("sceneStateRight", &m_sceneManager.m_scene2);
        m_stateSaver.Insert("activeTrio", &m_activeTrio);
        m_stateSaver.Insert("selectedAbsoluteEncoderBank", &m_squiggleBoy.m_selectedAbsoluteEncoderBank);
        m_nonagon.RemoveGridIds();
        m_squiggleBoy.Config(m_squiggleBoyState);
        m_squiggleBoy.m_theoryOfTime = &m_nonagon.m_nonagon.m_theoryOfTime;
        m_squiggleBoy.SetupUIState(&m_uiState.m_squiggleBoyUIState);
        m_nonagon.SetupMonoScopeWriter(&m_uiState.m_squiggleBoyUIState.m_monoScopeWriter);
        m_nonagon.SetupMessageOutBuffer(&m_messageOutBuffer);
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

    void HandleScenePress(int scene)
    {
        if (m_sceneManager.m_shift)
        {
            CopyToScene(scene);
        }
        else
        {
            if (m_sceneManager.m_blendFactor < 0.5)
            {
                SetRightScene(scene);
            }
            else
            {
                SetLeftScene(scene);
            }
        }
    }

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
            if (static_cast<int>(m_owner->m_sceneManager.m_scene1) == m_scene)
            {
                return SmartGrid::Color::Orange.AdjustBrightness(0.5 + 0.5 * (1.0 - m_owner->m_sceneManager.m_blendFactor));
            }
            else if (static_cast<int>(m_owner->m_sceneManager.m_scene2) == m_scene)
            {
                return SmartGrid::Color::SeaGreen.AdjustBrightness(0.5 + 0.5 * m_owner->m_sceneManager.m_blendFactor);
            }

            return SmartGrid::Color::Grey;
        }

        virtual void OnPress(uint8_t velocity) override
        {
            m_owner->HandleScenePress(m_scene);
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
            if (m_owner->m_squiggleBoy.m_selectedGesture.Get(m_gesture))
            {
                return SmartGrid::Color::White;
            }
            else
            {
                return SmartGrid::Color::Grey.Dim();
            }
        }

        virtual void OnPress(uint8_t velocity) override
        {
            if (m_owner->m_sceneManager.m_shift)
            {
                m_owner->ClearGesture(m_gesture);
            }
            
            m_owner->m_squiggleBoy.SelectGesture(m_gesture, true);
        }

        virtual void OnRelease() override
        {
            m_owner->m_squiggleBoy.SelectGesture(m_gesture, false);
        }
    };

    SmartGrid::Cell* MakeShiftCell()
    {
        return new SmartGrid::StateCell<bool>(
                SmartGrid::Color::White.Dim() /*offColor*/,
                SmartGrid::Color::White /*onColor*/,
                &m_sceneManager.m_shift,
                true,
                false,
                SmartGrid::StateCell<bool>::Mode::Momentary);
    }

    SmartGrid::Cell* MakeRunningCell()
    {
        return new SmartGrid::StateCell<bool>(
                SmartGrid::Color::Green.Dim() /*offColor*/,
                SmartGrid::Color::Green /*onColor*/,
                &m_running,
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


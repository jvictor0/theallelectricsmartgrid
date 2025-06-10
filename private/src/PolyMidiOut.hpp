#pragma once
#include "plugin.hpp"
#include "ModuleUtils.hpp"

struct PolyMidi : public Module
{
    midi::Output m_output;

    StateSaver m_stateSaver;

    float m_currentNote[16];
    bool m_isNoteOn[16];

    size_t m_numVoices;

    IOMgr m_ioMgr;
    IOMgr::Input* m_vPerOctInput;
    IOMgr::Input* m_gateInput;
    IOMgr::Input* m_velocityInput;
    IOMgr::Input* m_ccInput[2];

    PolyMidi()
        : m_ioMgr(this)
        , m_numVoices(16)
    {        
        for (size_t i = 0; i < 16; ++i)
        {
            m_isNoteOn[i] = false;
        }
    }

    void SendNote(size_t voice, float currentNote)
    {
        if (m_isNoteOn[voice])
        {
            StopNote(voice);
        }

        m_currentNote[voice] = currentNote;
        int midiNote = static_cast<int>(m_currentNote[voice]) * 12 + 60;
        float pitchBendFloat = m_currentNote[voice] * 12 + 60 - midiNote;
        int pitchBend = static_cast<int>(pitchBendFloat * 4096.0f) + 8192;

        m_output.setChannel(voice);
        m_output.setPitchWheel(pitchBend);
        m_output.setVelocity(static_cast<int>(m_velocityInput->Get(voice)));
        m_output.setNoteGate(midiNote, true, 0);
        m_isNotedOn[voice] = true;
    }

    void StopNote(size_t voice)
    {
        if (m_isNoteOn[voice])
        {
            m_output.setChannel(voice);
            int midiNote = static_cast<int>(m_currentNote[voice]) * 12 + 60;
            m_output.setNoteGate(midiNote, false, 0);
            m_isNoteOn[voice] = false;
        }
    }

    void process(const ProcessArgs& args) override
    {
        m_output.setFrame(args.frame);
    }

    json_t* dataToJson() override
    {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "midiOut", m_output.toJson());
        json_object_set_new(rootJ, "state", m_stateSaver.ToJSON());
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override
    {
        json_t* midiJ = json_object_get(rootJ, "midiOut");
		if (midiJ)
        {
			m_output.fromJson(midiJ);
        }

        midiJ = json_object_get(rootJ, "state");
        if (midiJ)
        {
            m_stateSaver.SetFromJSON(midiJ);
        }
	}
};

struct PolyMidiWidget : public ModuleWidget
{
    PolyMidiWidget(PolyMidi* module)
    {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/PolyCV_MIDICC.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		SmartGrid::MidiWidget* midiOutputWidget = createWidget<SmartGrid::MidiWidget>(Vec(10.0f, 107.4f));
		midiOutputWidget->box.size = Vec(130.0f, 67.0f);
		midiOutputWidget->SetMidiPort(module ? &module->m_output : NULL, true);
		addChild(midiOutputWidget);

        for (size_t i = 0; i < 4; ++i)
        {
            for (size_t j = 0; j < 4; ++j)
            {                        
                addInput(createInputCentered<ThemedPJ301MPort>(Vec(10 + 25 * i, 200 + 25 * j), module, i + 4 * j));
            }
        }
    }
};

#pragma once
#include "plugin.hpp"
#include "AudioFile.h"
#include "Trig.hpp"
#include <atomic>
#include <thread>
#include <unistd.h>

struct SamplePack;

struct Sample
{
    enum class Tag : size_t
    {
        None = 0,
        Dnb = 1,
        FourOnFloor = 2,
        EDM = 3,
        Reggae = 4
    };

    static char Tag2Char(Tag t)
    {
        switch (t)
        {
            case Tag::Dnb: return 'd';
            case Tag::FourOnFloor: return '4';
            case Tag::EDM: return 'e';
            case Tag::Reggae: return 'r';
            default: return '0';
        }
    }

    static Tag Char2Tag(char c)
    {
        switch (c)
        {
            case 'd': return Tag::Dnb;
            case '4': return Tag::FourOnFloor;
            case 'e': return Tag::EDM;
            case 'r': return Tag::Reggae;
            default: return Tag::None;
        }
    }

    Sample(
        std::string filename,
        size_t beats,
        float intensity,
        bool isFill,
        std::string tags,
        SamplePack* pack)
        : m_beats(beats)
        , m_filename(filename)
        , m_intensity(intensity)
        , m_isFill(isFill)
        , m_tags(0)
        , m_pack(pack)
    {
        for (char ch : tags)
        {
            Tag t = Char2Tag(ch);
            if (t != Tag::None)
            {
                SetTag(t);
            }
        }
    }

    bool HasTag(Tag t)
    {
        return m_tags & (1 << static_cast<size_t>(t));
    }

    void SetTag(Tag t)
    {
        m_tags |= 1 << static_cast<size_t>(t);
    }    
    
    size_t m_beats;
    std::string m_filename;
    float m_intensity;
    bool m_isFill;
    size_t m_tags;
    SamplePack* m_pack;
};

struct SamplePack
{
    std::string m_name;
    std::vector<std::unique_ptr<Sample>> m_loops;
    std::vector<std::unique_ptr<Sample>> m_fills;

    SamplePack(std::string name)
        : m_name(name)
    {
    }          
    
    Sample* Add(
        std::string filename,
        size_t beats,
        float intensity,
        bool isFill,
        std::string tags)
    {
        Sample* s = new Sample(filename, beats, intensity, isFill, tags, this);
        if (isFill)
        {
            m_fills.emplace_back(s);
            return m_fills.back().get();
        }
        else
        {
            m_loops.emplace_back(s);
            return m_loops.back().get();
        }
    }
};

struct SampleDb
{
    std::map<std::string, SamplePack> m_samples;
    std::vector<Sample*> m_allLoops;
    std::vector<Sample*> m_allFills;

    void Add(
        std::string packName,
        std::string filename,
        size_t beats,
        float intensity,
        bool isFill,
        std::string tags)
    {
        if (m_samples.count(packName) == 0)
        {
            INFO("Adding new pack '%s'", packName.c_str());
            m_samples.insert({std::string(packName), SamplePack(packName)});
        }

        INFO("Adding sample '%s' (%lu %f %d %s)", filename.c_str(), beats, intensity, isFill, tags.c_str());
        Sample* s = m_samples.find(std::string(packName))->second.Add(filename, beats, intensity, isFill, tags);
        if (isFill)
        {
            m_allFills.push_back(s);
        }
        else
        {
            m_allLoops.push_back(s);
        }

        for (Sample* s2 : m_allLoops)
        {
            INFO("%p Filename = %s", s2, s2->m_filename.c_str());
        }
    }

    void LoadDbFile(const char* dbFilename)
    {
        std::ifstream infile(dbFilename);
        std::string line;
        while (std::getline(infile, line))
        {
            std::stringstream ss(line);
            std::string tmp;
            std::stringstream tss;
            
            std::string packName;
            std::string filename;
            size_t beats;
            float intensity;
            bool isFill;
            std::string tags;

            std::getline(ss, packName, ',');
            std::getline(ss, filename, ',');

            filename.erase(std::remove(filename.begin(), filename.end(), '\\'), filename.end());
            
            std::getline(ss, tmp, ',');
            tss << tmp;
            tss >> beats;

            tss.clear();
            std::getline(ss, tmp, ',');
            tss << tmp;
            tss >> intensity;

            tss.clear();
            std::getline(ss, tmp, ',');
            tss << tmp;
            tss >> isFill;

            std::getline(ss, tags);

            Add(
                packName.c_str(),
                filename.c_str(),
                beats,
                intensity,
                isFill,
                tags.c_str());
        }
    }

    Sample* RandomSample(RGen& gen)
    {
        size_t ix = gen.RangeGen(m_allLoops.size());
        return m_allLoops[ix];
    }
};

struct MxDJDeck
{
    struct Input
    {
        float m_t;
        size_t m_beats;

        Input()
            : m_t(0)
            , m_beats(16)
        {
        }
    };

    struct Output
    {
        float m_out[2];

        Output()
        {
            m_out[0] = 0;
            m_out[1] = 0;
        }
    };

    MxDJDeck()
        : m_sample(nullptr)
    {
    }

    void LoadSample(Sample* s)
    {
        bool tst = false;
        if (m_loading.compare_exchange_strong(tst, true))
        {
            m_sample = s;
            LoadFile(s->m_filename.c_str());
            m_loading.store(false);
        }
    }
    
    void LoadFile(const char* fname)
    {
        bool ok = m_audioFile.load(fname);
        if (!ok)
        {
            WARN("Could not load %s", fname);
        }
    }

    void Process(Input& input)
    {
        if (!m_sample ||
            m_loading.load() ||
            m_audioFile.samples.size() == 0 ||
            m_audioFile.samples[0].size() == 0)
        {
            return;
        }

        float pos = input.m_t * input.m_beats / m_sample->m_beats;
        pos = pos - floor(pos);
        size_t ix = pos * static_cast<float>(m_audioFile.samples[0].size());
        ix = std::min<size_t>(ix, m_audioFile.samples[0].size() - 1);
            
        for (size_t i = 0; i < 2 && i < m_audioFile.samples.size(); ++i)
        {
            m_output.m_out[i] = m_audioFile.samples[i][ix];
        }
    }
    
    AudioFile<float> m_audioFile;
    Sample* m_sample;
    std::atomic<bool> m_loading;
    Output m_output;
};

struct MxDJInternal
{
    enum class State
    {
        Unloaded,
        Running,
        Switching
    };
    
    struct Input
    {
        MxDJDeck::Input m_input;
        float m_fadeBeats;
        bool m_requestNewSample;
        RGen m_gen;

        Input()
            : m_fadeBeats(1.0 / 32.0)
        {
        }
    };

    struct Output
    {
        float m_out[2];

        Output()
        {
            m_out[0] = 0;
            m_out[1] = 0;
        }
    };

    MxDJInternal()
        : m_state(State::Unloaded)
        , m_active(&m_decks[0])
        , m_waiting(&m_decks[1])
        , m_switchBeat(-1)
        , m_done(false)
        , m_loadRequested(nullptr)
        , m_loadThread(&MxDJInternal::LoadThread, this)
    {
        m_samples.LoadDbFile("/Users/joyo/Downloads/LoopDb.csv");
    }

    ~MxDJInternal()
    {
        m_done.store(true);
        m_loadThread.join();
    }
    
    void Process(Input& input)
    {
        if (input.m_requestNewSample || m_state == State::Unloaded)
        {
            LoadRandomSample(input);
        }
        
        m_active->Process(input.m_input);

        for (size_t i = 0; i < 2; ++i)
        {
            m_output.m_out[i] = m_active->m_output.m_out[i];
        }
        
        if (m_state == State::Switching)
        {
            m_waiting->Process(input.m_input);

            float beatPos = input.m_input.m_t * input.m_input.m_beats;
            if (m_switchBeat == -1)
            {
                m_switchBeat = floor(beatPos);
            }
            
            beatPos = beatPos - m_switchBeat;
            float fadePos;
            if (beatPos > 0)
            {
                fadePos = beatPos / input.m_fadeBeats + 1.0 - 1.0 / input.m_fadeBeats;
                fadePos = std::max<float>(0, std::min<float>(1, fadePos));
            }
            else
            {
                fadePos = 1;
            }
                
            for (size_t i = 0; i < 2; ++i)
            {
                m_output.m_out[i] *= 1 - fadePos;
                m_output.m_out[i] += fadePos * m_waiting->m_output.m_out[i];
            }
            
            if (fadePos == 1)
            {
                Switch();
            }
        }
    }

    void Switch()
    {
        INFO("Switching");
        std::swap(m_active, m_waiting);
        m_state = State::Running;
        m_switchBeat = -1;
    }

    void LoadSample(Sample* s)
    {
        INFO("Loading %s", s->m_filename.c_str());
        if (m_state == State::Unloaded)
        {
            m_active->LoadSample(s);
            m_state = State::Running;
        }
        else if (m_state == State::Running)
        {
            m_waiting->LoadSample(s);
            m_state = State::Switching;
        }
    }

    void RequestLoadSample(Sample* s)
    {
        Sample* tst = nullptr;
        m_loadRequested.compare_exchange_strong(tst, s);
    }

    void LoadRandomSample(Input& input)
    {
        RequestLoadSample(m_samples.RandomSample(input.m_gen));
    }

    void LoadThread()
    {
        while (!m_done.load())
        {
            Sample* s = m_loadRequested.load();
            if (s)
            {
                LoadSample(s);
                m_loadRequested.store(nullptr);
            }
            else
            {
                usleep(1000);
            }
        }
    }

    SampleDb m_samples;
    State m_state;
    MxDJDeck m_decks[2];
    MxDJDeck* m_active;
    MxDJDeck* m_waiting;
    float m_switchBeat;
    Output m_output;
    std::atomic<bool> m_done;
    std::atomic<Sample*> m_loadRequested;
    std::thread m_loadThread;
};

#ifndef IOS_BUILD
struct MxDJ : Module
{
    MxDJInternal m_dj;
    MxDJInternal::Input m_state;

    Trig m_newSampSame;
    Trig m_newSampDiff;

    MxDJ()
    {
        config(0, 3, 2, 0);
        configInput(0, "Time in");
        configInput(1, "New Samp Same");
        configInput(2, "New Samp Diff");
        configOutput(0, "Left Out");
        configOutput(1, "Right Out");
    }

    void process(const ProcessArgs &args) override
    {
        m_state.m_input.m_t = inputs[0].getVoltage() / 10;
        m_state.m_requestNewSample = m_newSampDiff.Process(inputs[2].getVoltage());
        m_dj.Process(m_state);
        for (size_t i = 0; i < 2; ++i)
        {
            outputs[i].setVoltage(m_dj.m_output.m_out[i] * 10);
        }
    }
};

struct MxDJWidget : ModuleWidget
{
    MxDJWidget(MxDJ* module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/MxDJ.svg")));

        addOutput(createOutputCentered<PJ301MPort>(Vec(175, 100), module, 0));
        addOutput(createOutputCentered<PJ301MPort>(Vec(125, 100), module, 1));        
        addInput(createInputCentered<PJ301MPort>(Vec(125, 200), module, 0));
        addInput(createInputCentered<PJ301MPort>(Vec(150, 200), module, 1));
        addInput(createInputCentered<PJ301MPort>(Vec(175, 200), module, 2));
    }
};
#endif


#include "Bench.hpp"
#include "support/SynthRig.hpp"
#include <map>
#include <iterator>
#include <iomanip>

// Sample-addressed control replay through SynthRig. No audio state is inferred
// from the recorded output stems. Keep the limitations in README.md explicit.
//
struct ControlReplay
{
    using Cell = SmartGrid::BankedEncoderCell;

    struct Event
    {
        size_t m_sample;
        int m_type;
        std::string m_name;
        int m_scene;
        size_t m_gesture;
        std::vector<size_t> m_path;
        JSON m_value;
        uint32_t m_order;
    };

    synthrig::SynthRig m_rig;
    std::map<std::string, Cell*> m_encoders;
    std::map<std::string, State*> m_states;
    std::vector<Event> m_events;
    size_t m_nextEvent = 0;
    size_t m_hostFrame = 0;
    size_t m_frames = 0;

    explicit ControlReplay(JSON root)
    {
        Require(root.Get("replay_version").IntegerValue() == 1, "Unsupported replay version");
        Require(root.Get("sample_rate").IntegerValue() == SampleTimer::x_hostSampleRate,
            "Replay sample rate does not match this engine");
        const auto frames = root.Get("frames").IntegerValue();
        Require(frames > 0 && frames <= 86400LL * SampleTimer::x_hostSampleRate, "Invalid frame count");
        m_frames = static_cast<size_t>(frames);
        Require(!root.Get("initial_phase").IsNull() && std::isfinite(root.Get("initial_phase").NumberValue()),
            "Missing or nonfinite initial clock phase");
        Require(m_rig.LoadPatchJSON(root.Get("initial_patch")), "Cannot load initial patch");
        auto& internal = m_rig.Internal();
        auto& synth = internal.m_squiggleBoy;
        uint32_t seed = 1000;
        for (auto& track : synth.m_tracks)
        {
            for (auto& random : track.m_gangedRandomLFO)
            {
                random.m_draws = GangedRandomLFODrawSource(seed++);
            }

            for (auto& random : track.m_source.m_dualWaveShapingVCO.m_morphRandom)
            {
                random.m_draws = GangedRandomLFODrawSource(seed++);
            }
        }

        for (auto& random : synth.m_quadGangedRandomLFO)
        {
            random.m_draws = GangedRandomLFODrawSource(seed++);
        }

        for (auto& random : synth.m_globalGangedRandomLFO)
        {
            random.m_draws = GangedRandomLFODrawSource(seed++);
        }

        synth.m_encoders.m_encoderBankBank.ForEachNamedEncoder(
            [this](size_t, Cell* cell)
            {
                m_encoders.emplace(cell->m_name, cell);
            });
        for (auto* state : internal.m_stateSaver.m_state)
        {
            m_states.emplace(state->m_name, state);
        }

        for (auto* state : internal.m_nonagon.m_stateSaver.m_state)
        {
            m_states.emplace(state->m_name, state);
        }

        // Settle patch-load smoothing before recording frame zero. This cannot
        // restore pre-recording oscillator, envelope, delay, or random history.
        //
        m_rig.RunSeconds(0.5);
        m_rig.ClearOutput();
        internal.m_nonagon.m_state.m_theoryOfTimeInput.m_unmodulatedPhase = root.Get("initial_phase").NumberValue();
        auto events = root.Get("events");
        for (size_t i = 0; i < events.Size(); ++i)
        {
            auto row = events.GetAt(i);
            Require(row.Size() == 8, "Invalid event row");
            Event event;
            const auto sample = row.GetAt(0).IntegerValue();
            Require(sample >= 0 && static_cast<size_t>(sample) < m_frames, "Event outside recording");
            event.m_sample = static_cast<size_t>(sample);
            event.m_type = static_cast<int>(row.GetAt(1).IntegerValue());
            event.m_name = row.GetAt(2).StringValue();
            event.m_scene = static_cast<int>(row.GetAt(3).IntegerValue());
            event.m_gesture = static_cast<size_t>(row.GetAt(4).IntegerValue());
            event.m_value = row.GetAt(6);
            event.m_order = static_cast<uint32_t>(row.GetAt(7).IntegerValue());
            Require(event.m_scene >= 0 && event.m_scene < 8 && event.m_gesture < 16, "Invalid scene or gesture");
            auto path = row.GetAt(5);
            Require(path.Size() <= ParamEvent::x_maxEncoderPath, "Encoder path too deep");
            for (size_t hop = 0; hop < path.Size(); ++hop)
            {
                const auto index = path.GetAt(hop).IntegerValue();
                Require((index >= 0 && index < 15) || (index >= 128 && index < 144), "Invalid encoder hop");
                event.m_path.push_back(static_cast<size_t>(index));
            }

            Require((event.m_type >= 1 && event.m_type <= 5) || event.m_type == 8 || event.m_type == 9,
                "Unsupported event type");
            if (!m_events.empty())
            {
                const auto& previous = m_events.back();
                Require(event.m_sample > previous.m_sample
                    || (event.m_sample == previous.m_sample && event.m_order > previous.m_order),
                    "Events must preserve capture order");
            }

            if (event.m_type == 1)
            {
                auto found = m_states.find(event.m_name);
                Require(found != m_states.end(), "Unknown state name");
                Require(event.m_value.Size() == found->second->m_len
                    && event.m_scene < found->second->m_numScenes, "State shape mismatch");
            }

            if (event.m_type == 4 || event.m_type == 5)
            {
                Require(m_encoders.count(event.m_name) == 1, "Unknown encoder name");
                Require(event.m_type != 5 || (!event.m_path.empty() && event.m_path.back() >= 128),
                    "Activation requires a gesture path");
            }

            m_events.push_back(event);
        }

        Require(!m_events.empty() && m_events[0].m_sample == 0
            && (m_events[0].m_type == 8 || m_events[0].m_type == 9), "Missing initial transport");
    }

    Cell* Resolve(const Event& event)
    {
        Cell* cell = m_encoders.at(event.m_name);
        cell->SynchronizePreparedState();
        for (size_t hop : event.m_path)
        {
            if (hop < 128)
            {
                if (!cell->m_modulators.m_modulators[hop])
                {
                    cell->FillModulators(cell->m_context);
                }

                cell = cell->m_modulators.m_modulators[hop].get();
            }
            else
            {
                cell->m_modulators.AddGesture(cell, hop - 128);
                cell = cell->m_modulators.m_gestures[hop - 128].get();
            }
        }

        return cell;
    }

    void Apply(const Event& event)
    {
        auto& internal = m_rig.Internal();
        if (event.m_type == 1)
        {
            State* state = m_states.at(event.m_name);
            char bytes[State::x_maxValueLen]{};
            for (size_t i = 0; i < state->m_len; ++i)
            {
                const auto byte = event.m_value.GetAt(i).IntegerValue();
                Require(byte >= 0 && byte <= 255, "Invalid state byte");
                bytes[i] = static_cast<char>(byte);
            }

            state->SetBytes(bytes, event.m_scene);
            if (state->m_curScene == event.m_scene)
            {
                std::memcpy(state->m_ptr, bytes, state->m_len);
            }
        }
        else if (event.m_type == 2 || event.m_type == 3)
        {
            float value = static_cast<float>(event.m_value.NumberValue());
            Require(std::isfinite(value) && value >= 0 && value <= 1, "Invalid fader/blend value");
            if (event.m_type == 2)
            {
                internal.m_faderSlew[event.m_gesture].SetTarget(
                    internal.m_squiggleBoyState.m_faders[event.m_gesture], value);
            }
            else
            {
                internal.m_blendSlew.SetTarget(internal.m_context.m_sceneManager.m_blendFactor, value);
            }
        }
        else if (event.m_type == 4 || event.m_type == 5)
        {
            Cell* cell = Resolve(event);
            if (event.m_type == 4)
            {
                float value = static_cast<float>(event.m_value.NumberValue());
                Require(std::isfinite(value) && value >= (cell->m_bipolar ? -1 : 0) && value <= 1,
                    "Invalid encoder value");
                cell->SetAndRecordValue(cell->ToNormalized(value), event.m_scene);
                cell->SetState();
            }
            else
            {
                cell->SetActive(event.m_value.BooleanValue(), event.m_scene);
            }

            cell->SetForceUpdateRecursive();
            cell->SetModulatorsAffectingRecursive();
        }
        else
        {
            internal.m_running = event.m_type == 8;
        }
    }

    void RunTo(size_t end)
    {
        while (m_hostFrame < end)
        {
            while (m_nextEvent < m_events.size() && m_events[m_nextEvent].m_sample == m_hostFrame)
            {
                Apply(m_events[m_nextEvent++]);
            }

            size_t next = std::min(end, m_hostFrame + 512);
            if (m_nextEvent < m_events.size())
            {
                next = std::min(next, m_events[m_nextEvent].m_sample);
            }

            Require(next > m_hostFrame, "Replay timeline failed to advance");
            m_rig.RunHostFrames(next - m_hostFrame);
            m_hostFrame = next;
            Require(!m_rig.SawNaN(), "Nonfinite replay output");
            m_rig.ClearOutput();
        }
    }
};

int main(int argc, char** argv)
{
    try
    {
        Require(argc == 4 || argc == 5, "Usage: perf_sgrec PREPARED.json SECONDS OUTPUT.jsonl [FINAL_PATCH.json]");
        std::ifstream input(argv[1]);
        Require(input.good(), "Cannot open prepared replay");
        std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        Require(text.size() < 64 * 1024 * 1024, "Replay JSON too large");
        JsonArena arena(text.size() * 16 + 1024 * 1024);
        JSON root = arena.Loads(text.c_str());
        Require(!root.IsNull() && !arena.Failed(), "Invalid replay JSON");
        PrepareCPU();
        ControlReplay replay(root);
        const size_t end = std::min(replay.m_frames,
            static_cast<size_t>(PositiveSeconds(argv[2]) * SampleTimer::x_hostSampleRate));
        std::ofstream output(argv[3]);
        Require(output.good(), "Cannot open result output");
        output << std::setprecision(10);
        while (replay.m_hostFrame < end)
        {
            const size_t begin = replay.m_hostFrame;
            const size_t next = std::min(end, begin + SampleTimer::x_hostSampleRate);
            const size_t eventsBefore = replay.m_nextEvent;
            const double cpu = CPUSeconds();
            const auto wall = std::chrono::steady_clock::now();
            replay.RunTo(next);
            const double elapsed = CPUSeconds() - cpu;
            const double wallSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - wall).count();
            auto& synth = replay.m_rig.Internal().m_squiggleBoy;
            size_t activeBins = 0;
            for (const auto& pvdr : synth.m_delay.m_grainManager.m_resynthesizer.m_pvdr)
            {
                for (size_t i = 0; i < pvdr.m_size; ++i)
                {
                    activeBins += !pvdr.m_results[i].m_small;
                }
            }

            output << "{\"second\":" << static_cast<double>(begin) / SampleTimer::x_hostSampleRate
                << ",\"frames\":" << next - begin << ",\"cpu_seconds\":" << elapsed
                << ",\"wall_seconds\":" << wallSeconds << ",\"events\":" << replay.m_nextEvent - eventsBefore
                << ",\"phase\":" << replay.m_rig.Internal().m_nonagon.m_nonagon.m_theoryOfTime.m_sample.m_unmodulatedPhase
                << ",\"pvdr_active_bins_last_grain\":" << activeBins << "}\n";
            if (next % (60 * SampleTimer::x_hostSampleRate) == 0 || next == end)
            {
                output.flush();
                std::cerr << "Replayed " << next / SampleTimer::x_hostSampleRate << " s\n";
            }
        }

        Require(output.good(), "Cannot write results");
        if (argc == 5)
        {
            JsonArena snapshotArena(16 * 1024 * 1024);
            auto snapshot = replay.m_rig.Internal().ToJSON(snapshotArena);
            Require(!snapshotArena.Failed() && !snapshot.IsNull(), "Cannot snapshot replay state");
            char* dumped = snapshot.Dumps(0);
            Require(dumped != nullptr, "Cannot serialize replay state");
            std::ofstream saved(argv[4]);
            saved << dumped << '\n';
            std::free(dumped);
            Require(saved.good(), "Cannot write replay state");
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}

#include "Bench.hpp"
#include "support/SynthRig.hpp"
#include <cstring>
#include <iomanip>
#include <iterator>

using Cell = SmartGrid::BankedEncoderCell;

// Native little-endian IEEE float payload; explicit dimensions reject stale captures.
// No SIMD object layout or pointer is serialized.
//
struct ModeInput
{
    float m_values[Cell::x_numModulators][4];
    float m_amplitudes[Cell::x_numModulators][4];
    float m_gestures[Cell::x_numGestureParams];
};

static_assert(sizeof(ModeInput) == sizeof(float) * (8 * Cell::x_numModulators + Cell::x_numGestureParams));

struct ReplayHeader
{
    char m_magic[8] = {'S', 'G', 'P', 'E', 'R', 'F', '1', '\0'};
    uint64_t m_engineRate = SampleTimer::x_sampleRate;
    uint64_t m_hostRate = SampleTimer::x_hostSampleRate;
    uint64_t m_modes = 0;
    uint64_t m_modulators = Cell::x_numModulators;
    uint64_t m_gestures = Cell::x_numGestureParams;
    uint64_t m_frameBytes = sizeof(ModeInput);
    uint64_t m_frames = 0;
};

static_assert(sizeof(ReplayHeader) == 64);

void Load(synthrig::SynthRig& rig, const char* path)
{
    std::ifstream input(path);
    Require(input.good(), "Cannot open patch");
    std::string patch((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    auto& synth = rig.Internal().m_squiggleBoy;
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

    Require(rig.LoadPatch(patch), "Patch load failed");
    rig.StartSequencer();
    rig.ClearOutput();
}

float Blend(const std::string& mode, size_t hostFrame)
{
    float phase =
        static_cast<float>(hostFrame % SampleTimer::x_hostSampleRate) / SampleTimer::x_hostSampleRate;
    return mode == "fixed" || mode == "knobs"
               ? 0
               : mode == "right" ? 1 : mode == "mid" ? 0.5f : phase < 0.5f ? phase * 2 : 2 - phase * 2;
}

float GestureWeight(size_t gesture, size_t hostFrame)
{
    const size_t period = SampleTimer::x_hostSampleRate * (2 + gesture % 3);
    const size_t position = (hostFrame + gesture * period / Cell::x_numGestureParams) % period;
    float phase = static_cast<float>(position) / period;
    return phase < 0.5f ? phase * 2.0f : 2.0f - phase * 2.0f;
}

BitSet16 GestureFaders(const Cell& cell)
{
    BitSet16 faders;
    for (size_t gesture = 0; gesture < Cell::x_numGestureParams; ++gesture)
    {
        const auto& stored = cell.m_modulators.m_gestures[gesture];
        if (stored && std::any_of(std::begin(stored->m_isActive), std::end(stored->m_isActive),
            [](bool active)
            {
                return active;
            }))
        {
            faders.Set(static_cast<int>(gesture), true);
        }
    }

    for (const auto& amount : cell.m_modulators.m_modulators)
    {
        if (amount)
        {
            faders = faders.Union(GestureFaders(*amount));
        }
    }

    return faders;
}

void RunSynth(synthrig::SynthRig& rig, size_t samples, const std::string& mode, size_t offset,
              std::ofstream* dump, BitSet16 gestureFaders)
{
    constexpr size_t x_chunk = SampleTimer::x_sampleRate / 1000;
    for (size_t done = 0; done < samples;)
    {
        size_t position = offset + done;
        if (mode == "sweep" || mode == "scenes")
        {
            rig.SetBlend(Blend(mode, position / SampleTimer::x_oversample));
        }

        if (mode == "scenes" && position % SampleTimer::x_sampleRate == 0)
        {
            rig.SetLeftScene(static_cast<int>((position / SampleTimer::x_sampleRate) % 8));
            rig.SetRightScene(static_cast<int>((position / SampleTimer::x_sampleRate + 1) % 8));
        }

        if (!gestureFaders.IsZero())
        {
            for (size_t gesture = 0; gesture < Cell::x_numGestureParams; ++gesture)
            {
                if (gestureFaders.Get(static_cast<int>(gesture)))
                {
                    rig.SetFader(static_cast<int>(gesture), GestureWeight(gesture, position / SampleTimer::x_oversample));
                }
            }
        }

        size_t chunk = std::min(x_chunk, samples - done);
        rig.RunSamples(chunk);
        Require(!rig.SawNaN(), "Nonfinite synth output");
        if (dump)
        {
            const auto& output = rig.Output();
            dump->write(reinterpret_cast<const char*>(output.data()),
                        output.size() * sizeof(synthrig::OutputSample));
        }

        rig.ClearOutput();
        done += chunk;
    }
}

void Record(synthrig::SynthRig& rig, const std::string& mode, double seconds, const char* path)
{
    Require(seconds <= 10, "Capture duration must be at most 10 seconds");
    RunSynth(rig, SampleTimer::x_sampleRate / 2, mode, 0, nullptr, BitSet16());
    auto& bank = rig.Internal().m_squiggleBoy.m_encoders.m_encoderBankBank;
    ReplayHeader header;
    header.m_modes = bank.m_modulatorManager.m_groups.size();
    header.m_frames = static_cast<size_t>(seconds * SampleTimer::x_hostSampleRate);
    Require(header.m_frames > 0, "Capture must include a host frame");
    std::ofstream output(path, std::ios::binary);
    Require(output.good(), "Cannot open replay output");
    output.write(reinterpret_cast<const char*>(&header), sizeof(header));
    for (size_t frame = 0; frame < header.m_frames; ++frame)
    {
        if (frame % (SampleTimer::x_hostSampleRate / 1000) == 0)
        {
            rig.SetBlend(Blend(mode, frame));
        }

        rig.RunSamples(SampleTimer::x_oversample);
        Require(!rig.SawNaN(), "Nonfinite capture output");
        rig.ClearOutput();
        for (size_t modeIndex = 0; modeIndex < bank.m_modulatorManager.m_groups.size(); ++modeIndex)
        {
            ModeInput input{};
            const auto& source = bank.GetModulatorValues(modeIndex);
            for (size_t index = 0; index < Cell::x_numModulators; ++index)
            {
                for (size_t lane = 0; lane < 4; ++lane)
                {
                    input.m_values[index][lane] = source.m_hostValue[index][lane];
                    input.m_amplitudes[index][lane] = source.m_amplitude[index][lane];
                }
            }

            const auto& gestures = bank.GetGestureValues();
            std::copy(std::begin(gestures.m_gestureWeights), std::end(gestures.m_gestureWeights),
                      input.m_gestures);
            output.write(reinterpret_cast<const char*>(&input), sizeof(input));
        }
    }

    output.close();
    Require(output.good(), "Cannot write replay output");
    std::cout << "{\"kind\":\"record\",\"frames\":" << header.m_frames << ",\"modes\":" << header.m_modes
              << "}\n";
}

std::vector<ModeInput> ReadReplay(const char* path, size_t modes)
{
    ReplayHeader header;
    ReplayHeader expected;
    std::ifstream input(path, std::ios::binary);
    Require(static_cast<bool>(input.read(reinterpret_cast<char*>(&header), sizeof(header))),
            "Cannot read replay header");
    Require(std::memcmp(header.m_magic, expected.m_magic, sizeof(header.m_magic)) == 0 &&
                header.m_engineRate == expected.m_engineRate && header.m_hostRate == expected.m_hostRate &&
                header.m_modes == modes && header.m_modulators == expected.m_modulators &&
                header.m_gestures == expected.m_gestures && header.m_frameBytes == sizeof(ModeInput) &&
                header.m_frames > 0 && header.m_frames <= 10 * SampleTimer::x_hostSampleRate,
            "Incompatible replay header; regenerate this capture");
    std::vector<ModeInput> frames(header.m_frames * modes);
    Require(static_cast<bool>(
                input.read(reinterpret_cast<char*>(frames.data()), frames.size() * sizeof(ModeInput))),
            "Truncated replay payload");
    Require(input.peek() == std::char_traits<char>::eof(), "Trailing replay data");
    for (const auto& frame : frames)
    {
        for (size_t index = 0; index < Cell::x_numModulators; ++index)
        {
            for (size_t lane = 0; lane < 4; ++lane)
            {
                Require(std::isfinite(frame.m_values[index][lane]) &&
                            std::isfinite(frame.m_amplitudes[index][lane]),
                        "Nonfinite replay input");
            }
        }

        for (float value : frame.m_gestures)
        {
            Require(std::isfinite(value), "Nonfinite gesture input");
        }
    }

    for (size_t frame = 0; frame < frames.size(); frame += modes)
    {
        for (size_t group = 1; group < modes; ++group)
        {
            Require(std::equal(std::begin(frames[frame].m_gestures), std::end(frames[frame].m_gestures),
                               std::begin(frames[frame + group].m_gestures)),
                    "Replay groups must share gesture weights");
        }
    }

    return frames;
}

void Matrix(synthrig::SynthRig& rig, const std::string& mode, double seconds, const char* path,
            std::ofstream* dump, bool gestures, bool consumer)
{
    RunSynth(rig, SampleTimer::x_sampleRate / 2, "fixed", 0, nullptr, BitSet16());
    auto& bank = rig.Internal().m_squiggleBoy.m_encoders.m_encoderBankBank;
    auto& scene = rig.Internal().m_context.m_sceneManager;
    auto inputs = ReadReplay(path, bank.m_modulatorManager.m_groups.size());
    size_t inputFrames = inputs.size() / bank.m_modulatorManager.m_groups.size();
    size_t frames = static_cast<size_t>(seconds * SampleTimer::x_hostSampleRate);
    Require(frames > 0, "Duration must include a host frame");
    // Synthetic gesture weights are precomputed outside timing, as recorded sources are.
    //
    if (gestures)
    {
        for (size_t frame = 0; frame < inputFrames; ++frame)
        {
            for (size_t modeIndex = 0; modeIndex < bank.m_modulatorManager.m_groups.size(); ++modeIndex)
            {
                for (size_t gesture = 0; gesture < Cell::x_numGestureParams; ++gesture)
                {
                    inputs[frame * bank.m_modulatorManager.m_groups.size() + modeIndex].m_gestures[gesture] =
                        0.5f + 0.45f * std::sin(frame * (0.001 + gesture * 0.00017));
                }
            }
        }
    }

    const size_t firstSample = (SampleTimer::GetHostSample() + 1) * SampleTimer::x_oversample;
    double start = CPUSeconds();
    for (size_t frame = 0; frame < frames; ++frame)
    {
        for (size_t modeIndex = 0; modeIndex < bank.m_modulatorManager.m_groups.size(); ++modeIndex)
        {
            const auto& input = inputs[(frame % inputFrames) * bank.m_modulatorManager.m_groups.size() + modeIndex];
            auto& source = bank.GetModulatorValues(modeIndex);
            for (size_t index = 0; index < Cell::x_numModulators; ++index)
            {
                source.m_hostValue[index] = QuadFloat(input.m_values[index]);
                source.m_amplitude[index] = QuadFloat(input.m_amplitudes[index]);
            }
        }

        bank.m_modulatorManager.SetGestureWeights(inputs[(frame % inputFrames) * bank.m_modulatorManager.m_groups.size()].m_gestures);
        scene.m_blendFactor = Blend(mode, frame);
        scene.Process();
        SampleTimer::s_instance->m_sample = firstSample + frame * SampleTimer::x_oversample;
        if (mode == "knobs" && (frame % 1600 == 0 || frame % 1600 == 48))
        {
            bank.ForEachNamedEncoder([&](size_t index, Cell* cell)
            {
                if (index % 16 == 0)
                {
                    float target = (frame / 1600) % 2 ? 0.3f : 0.7f;
                    target += frame % 1600 == 48 ? 0.02f : 0.0f;
                    cell->SetAndRecordValue(target, scene.m_scene1);
                    cell->SetForceUpdateRecursive();
                }
            });
        }

        bank.ProcessHostFrame();
        if (consumer)
        {
            auto& synth = rig.Internal().m_squiggleBoy;
            for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
            {
                SampleTimer::s_instance->m_sample = firstSample + frame * SampleTimer::x_oversample + sample;
                synth.m_encoders.m_values = bank.m_valueCaches[SampleTimer::GetSubSampleIndex()].data();
                synth.SetEncoderParameters(rig.Internal().m_squiggleBoyState);
                asm volatile("" : : "r"(&synth) : "memory");
            }
        }

        asm volatile("" : : "r"(&bank) : "memory");
        if (dump && frame % (SampleTimer::x_hostSampleRate / 1000) == 0)
        {
            bank.ForEachNamedEncoder([&](size_t, Cell* cell)
            {
                for (size_t lane = 0; lane < 4; ++lane)
                {
                    float value = cell->m_output[lane];
                    dump->write(reinterpret_cast<const char*>(&value), sizeof(value));
                }
            });
        }
    }

    double cpu = CPUSeconds() - start;
    double checksum = 0;
    bank.ForEachNamedEncoder([&](size_t, Cell* cell)
    {
        for (size_t lane = 0; lane < 4; ++lane)
        {
            Require(std::isfinite(cell->m_output[lane]), "Nonfinite matrix output");
            checksum += cell->m_output[lane];
        }
    });
    double audio = static_cast<double>(frames) / SampleTimer::x_hostSampleRate;
    std::cout << std::setprecision(12) << "{\"kind\":\"" << (consumer ? "consumer" : "matrix")
              << "\",\"mode\":\"" << mode
              << "\",\"audio_seconds\":" << audio << ",\"cpu_seconds\":" << cpu
              << ",\"ms_per_audio_second\":" << cpu * 1000 / audio << ",\"checksum\":" << checksum
              << ",\"parameter_updates\":" << (consumer ? frames * SampleTimer::x_oversample : 0)
              << ",\"finite\":true}\n";
}

int main(int argc, char** argv)
{
    try
    {
        Require(argc >= 5, "Usage: perf_encoders full|full_gestures|full_saved_gestures|record|matrix|matrix_gestures|consumer|consumer_gestures PATCH "
                           "hold|fixed|right|mid|sweep|scenes|knobs SECONDS [REPLAY] [DUMP]");
        std::string kind(argv[1]);
        std::string mode(argv[3]);
        bool full = kind == "full" || kind == "full_gestures" || kind == "full_saved_gestures";
        bool gestureSweep = kind == "full_gestures" || kind == "full_saved_gestures";
        bool consumer = kind == "consumer" || kind == "consumer_gestures";
        Require(full || consumer || kind == "record" || kind == "matrix" || kind == "matrix_gestures",
                "Unknown encoder workload");
        Require(mode == "fixed" || mode == "right" || mode == "mid" || mode == "sweep" ||
                    (mode == "knobs" && !full && kind != "record") ||
                    ((mode == "scenes" || mode == "hold") && full),
                "Unknown scene mode");
        Require(full ? (argc == 5 || argc == 6)
                               : kind == "record" ? argc == 6 : (argc == 6 || argc == 7),
                "Wrong argument count");
        double seconds = PositiveSeconds(argv[4]);
        PrepareCPU();
        synthrig::SynthRig rig;
        Load(rig, argv[2]);
        if (mode != "hold")
        {
            rig.SetBlend(Blend(mode, 0));
        }
        if (kind == "record")
        {
            Record(rig, mode, seconds, argv[5]);
            return 0;
        }

        std::ofstream dump;
        if (argc == (full ? 6 : 7))
        {
            dump.open(argv[argc - 1], std::ios::binary);
            Require(dump.good(), "Cannot open output dump");
        }

        auto* output = dump.is_open() ? &dump : nullptr;
        if (full)
        {
            BitSet16 gestureFaders;
            if (kind == "full_saved_gestures")
            {
                rig.Internal().m_squiggleBoy.m_encoders.m_encoderBankBank.ForEachNamedEncoder(
                    [&](size_t, Cell* cell)
                    {
                        gestureFaders = gestureFaders.Union(GestureFaders(*cell));
                    });
            }
            else if (gestureSweep)
            {
                gestureFaders = BitSet16(static_cast<uint16_t>(0xffff));
            }

            float initialBlend = rig.Blend();
            size_t frames = static_cast<size_t>(seconds * SampleTimer::x_sampleRate);
            Require(frames > 0, "Duration must include an engine sample");
            RunSynth(rig, SampleTimer::x_sampleRate / 2, mode, 0, nullptr, gestureFaders);
            double start = CPUSeconds();
            RunSynth(rig, frames, mode, SampleTimer::x_sampleRate / 2, output, gestureFaders);
            double cpu = CPUSeconds() - start;
            double audio = static_cast<double>(frames) / SampleTimer::x_sampleRate;
            size_t roots = 0;
            size_t gestureRoots = 0;
            size_t modulatedRoots = 0;
            double gestureChecksum = 0;
            auto& bank = rig.Internal().m_squiggleBoy.m_encoders.m_encoderBankBank;
            bank.ForEachNamedEncoder([&](size_t index, Cell* cell)
            {
                ++roots;
                bool hasModulation = false;
                for (const auto& amount : cell->m_modulators.m_modulators)
                {
                    hasModulation = hasModulation || static_cast<bool>(amount);
                }

                modulatedRoots += hasModulation;
                if (cell->m_modulators.m_numActiveGestures != 0)
                {
                    ++gestureRoots;
                    for (size_t lane = 0; lane < 4; ++lane)
                    {
                        gestureChecksum += (index + 1) * static_cast<double>(cell->m_output[lane]);
                    }
                }
            });
            Require(std::isfinite(gestureChecksum), "Nonfinite gesture output");
            std::cout << std::setprecision(12) << "{\"kind\":\"" << kind << "\",\"mode\":\"" << mode
                      << "\",\"audio_seconds\":" << audio << ",\"cpu_seconds\":" << cpu
                      << ",\"ms_per_audio_second\":" << cpu * 1000 / audio
                      << ",\"roots\":" << roots << ",\"gesture_roots\":" << gestureRoots
                      << ",\"modulated_roots\":" << modulatedRoots
                      << ",\"gesture_checksum\":" << gestureChecksum
                      << ",\"gesture_sweep\":" << (gestureSweep ? "true" : "false")
                      << ",\"gesture_fader_mask\":" << gestureFaders.m_bits
                      << ",\"initial_blend\":" << initialBlend
                      << ",\"final_blend\":" << rig.Blend()
                      << ",\"finite\":true}\n";
        }
        else
        {
            Matrix(rig, mode, seconds, argv[5], output,
                kind == "matrix_gestures" || kind == "consumer_gestures", consumer);
        }

        if (output)
        {
            dump.close();
            Require(dump.good(), "Cannot write output dump");
        }

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 2;
    }
}

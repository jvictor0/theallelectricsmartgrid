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
    return mode == "fixed"
               ? 0
               : mode == "right" ? 1 : mode == "mid" ? 0.5f : phase < 0.5f ? phase * 2 : 2 - phase * 2;
}

void RunSynth(synthrig::SynthRig& rig, size_t samples, const std::string& mode, size_t offset,
              std::ofstream* dump)
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
    RunSynth(rig, SampleTimer::x_sampleRate / 2, mode, 0, nullptr);
    auto& bank = rig.Internal().m_squiggleBoy.m_encoders.m_encoderBankBank;
    ReplayHeader header;
    header.m_modes = bank.m_numModes;
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
        for (size_t modeIndex = 0; modeIndex < bank.m_numModes; ++modeIndex)
        {
            ModeInput input{};
            const auto& source = bank.m_bankModes[modeIndex].m_modulatorValues;
            for (size_t index = 0; index < Cell::x_numModulators; ++index)
            {
                for (size_t lane = 0; lane < 4; ++lane)
                {
                    input.m_values[index][lane] = source.m_hostValue[index][lane];
                    input.m_amplitudes[index][lane] = source.m_amplitude[index][lane];
                }
            }

            std::copy(std::begin(source.m_gestureWeights), std::end(source.m_gestureWeights),
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

    return frames;
}

void Matrix(synthrig::SynthRig& rig, const std::string& mode, double seconds, const char* path,
            std::ofstream* dump, bool gestures)
{
    RunSynth(rig, SampleTimer::x_sampleRate / 2, "fixed", 0, nullptr);
    auto& bank = rig.Internal().m_squiggleBoy.m_encoders.m_encoderBankBank;
    auto& scene = rig.Internal().m_context.m_sceneManager;
    auto inputs = ReadReplay(path, bank.m_numModes);
    size_t inputFrames = inputs.size() / bank.m_numModes;
    size_t frames = static_cast<size_t>(seconds * SampleTimer::x_hostSampleRate);
    Require(frames > 0, "Duration must include a host frame");
    // Synthetic gesture weights are precomputed outside timing, as recorded sources are.
    //
    if (gestures)
    {
        for (size_t frame = 0; frame < inputFrames; ++frame)
        {
            for (size_t modeIndex = 0; modeIndex < bank.m_numModes; ++modeIndex)
            {
                for (size_t gesture = 0; gesture < Cell::x_numGestureParams; ++gesture)
                {
                    inputs[frame * bank.m_numModes + modeIndex].m_gestures[gesture] =
                        0.5f + 0.45f * std::sin(frame * (0.001 + gesture * 0.00017));
                }
            }
        }
    }

    double start = CPUSeconds();
    for (size_t frame = 0; frame < frames; ++frame)
    {
        for (size_t modeIndex = 0; modeIndex < bank.m_numModes; ++modeIndex)
        {
            const auto& input = inputs[(frame % inputFrames) * bank.m_numModes + modeIndex];
            auto& source = bank.m_bankModes[modeIndex].m_modulatorValues;
            for (size_t index = 0; index < Cell::x_numModulators; ++index)
            {
                source.m_hostValue[index] = QuadFloat(input.m_values[index]);
                source.m_amplitude[index] = QuadFloat(input.m_amplitudes[index]);
            }

            std::copy(std::begin(input.m_gestures), std::end(input.m_gestures), source.m_gestureWeights);
        }

        scene.m_blendFactor = Blend(mode, frame);
        scene.Process();
        for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
        {
            SampleTimer::IncrementSample();
        }

        bank.ProcessHostFrame();
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
    std::cout << std::setprecision(12) << "{\"kind\":\"matrix\",\"mode\":\"" << mode
              << "\",\"audio_seconds\":" << audio << ",\"cpu_seconds\":" << cpu
              << ",\"ms_per_audio_second\":" << cpu * 1000 / audio << ",\"checksum\":" << checksum
              << ",\"finite\":true}\n";
}

int main(int argc, char** argv)
{
    try
    {
        Require(argc >= 5, "Usage: perf_encoders full|record|matrix|matrix_gestures PATCH "
                           "fixed|right|mid|sweep|scenes SECONDS [REPLAY] [DUMP]");
        std::string kind(argv[1]);
        std::string mode(argv[3]);
        Require(kind == "full" || kind == "record" || kind == "matrix" || kind == "matrix_gestures",
                "Unknown encoder workload");
        Require(mode == "fixed" || mode == "right" || mode == "mid" || mode == "sweep" ||
                    (mode == "scenes" && kind == "full"),
                "Unknown scene mode");
        Require(kind == "full" ? (argc == 5 || argc == 6)
                               : kind == "record" ? argc == 6 : (argc == 6 || argc == 7),
                "Wrong argument count");
        double seconds = PositiveSeconds(argv[4]);
        PrepareCPU();
        synthrig::SynthRig rig;
        Load(rig, argv[2]);
        rig.SetBlend(Blend(mode, 0));
        if (kind == "record")
        {
            Record(rig, mode, seconds, argv[5]);
            return 0;
        }

        std::ofstream dump;
        if (argc == (kind == "full" ? 6 : 7))
        {
            dump.open(argv[argc - 1], std::ios::binary);
            Require(dump.good(), "Cannot open output dump");
        }

        auto* output = dump.is_open() ? &dump : nullptr;
        if (kind == "full")
        {
            size_t frames = static_cast<size_t>(seconds * SampleTimer::x_sampleRate);
            Require(frames > 0, "Duration must include an engine sample");
            RunSynth(rig, SampleTimer::x_sampleRate / 2, mode, 0, nullptr);
            double start = CPUSeconds();
            RunSynth(rig, frames, mode, SampleTimer::x_sampleRate / 2, output);
            double cpu = CPUSeconds() - start;
            double audio = static_cast<double>(frames) / SampleTimer::x_sampleRate;
            std::cout << std::setprecision(12) << "{\"kind\":\"full\",\"mode\":\"" << mode
                      << "\",\"audio_seconds\":" << audio << ",\"cpu_seconds\":" << cpu
                      << ",\"ms_per_audio_second\":" << cpu * 1000 / audio << ",\"finite\":true}\n";
        }
        else
        {
            Matrix(rig, mode, seconds, argv[5], output, kind == "matrix_gestures");
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

import DSP.Codegen.Cpp
import DSP.SmoothRandomLFO.Stream

namespace DSP.Codegen
open Lean Elab Command

/-- Lower the stream's clock and RNG state around the generated spline arithmetic. -/
def generateSmoothRandomLFOStream (name : String) : MetaM String := do
  unless validIdentifier name do throwError "C++ codegen: invalid spline stream name"
  let alpha ← hexLiteral SmoothRandomLFO.speedAlpha.toBits.toNat 52 11 1023 ""
  return String.intercalate "\n" (#[
    "#pragma once", "", "// Generated from DSP.SmoothRandomLFO.Stream. Do not edit.", "//", "",
    "#include \"SmoothRandomLFOCubicGeneric.hpp\"", "#include \"SmoothRandomLFOPrepareGeneric.hpp\"",
    "#include <algorithm>", "#include <cmath>", "#include <cstdint>", "",
    "template<typename Number>", s!"struct {name}", "{",
    s!"    {name}(double lambdaSamples, std::uint64_t seed)",
    "        : m_lambdaSamples(std::max(1.0, lambdaSamples)), m_phaseIncrement(1 / m_lambdaSamples),",
    s!"          m_seed(seed), m_speedSeed(seed ^ {SmoothRandomLFO.speedSeedMask.toNat}ULL)", "    {",
    "        m_segment = {DrawPoint(), DrawPoint(), DrawPoint(), DrawPoint(), 1, 1, 1, 1, 1};",
    "        Prepare();", "    }", "",
    "    Number Value() const", "    {",
    "        const Number phase = static_cast<Number>(m_phase);",
    "        return Cubic::Evaluate(m_cubic, Cubic::Vector::Splat(phase))[0];",
    "    }", "",
    "    void AdvanceSamples(unsigned samples)", "    {",
    "        while (samples != 0)", "        {",
    "            if (m_frameRemaining == 0)", "            {", "                PrepareSpeed();", "            }", "",
    "            const auto count = std::min(samples, m_frameRemaining);",
    "            m_phase += m_phaseIncrement * count;",
    "            m_frameRemaining -= count;",
    "            samples -= count;",
    "            if (m_phase >= 1)", "            {", "                AdvanceWrapped();", "            }",
    "        }",
    "    }", "",
    "    Number Process()", "    {",
    "        const Number output = Value();",
    "        AdvanceSamples(1);",
    "        return output;", "    }", "",
    "private:",
    "    using Cubic = SmoothRandomLFOCubicGeneric<Number, 1>;",
    "    using Preparer = SmoothRandomLFOPrepareGeneric<double, 1>;", "",
    s!"    static constexpr unsigned x_frameSamples = {SmoothRandomLFO.frameSamples};",
    s!"    static constexpr double x_speedAlpha = {alpha};", "",
    "    typename Cubic::Snapshot m_cubic;",
    "    double m_phase = 0;",
    "    double m_lambdaSamples;",
    "    double m_phaseIncrement;",
    "    unsigned m_frameRemaining = 0;",
    "    Preparer::Input m_segment;",
    "    std::uint64_t m_seed;",
    "    std::uint64_t m_speedSeed;", "",
    "    static double DrawUnit(std::uint64_t& seed)", "    {",
    s!"        seed += {SmoothRandomLFO.randomStep.toNat}ULL;",
    s!"        auto bits = (seed ^ (seed >> 30)) * {SmoothRandomLFO.randomMix1.toNat}ULL;",
    s!"        bits = (bits ^ (bits >> 27)) * {SmoothRandomLFO.randomMix2.toNat}ULL;",
    "        bits ^= bits >> 31;",
    "        return (static_cast<double>(bits >> 12) + 0.5) / 4503599627370496.0;",
    "    }", "",
    "    double DrawPoint()", "    {", "        return 2 * DrawUnit(m_seed) - 1;", "    }", "",
    "    void PrepareSpeed()", "    {",
    "        const double draw = std::min(1.0, -std::log(DrawUnit(m_speedSeed)) / m_lambdaSamples);",
    "        m_phaseIncrement = std::fma(x_speedAlpha, m_phaseIncrement, (1 - x_speedAlpha) * draw);",
    "        m_frameRemaining = x_frameSamples;", "    }", "",
    "    void Prepare()", "    {",
    "        const auto p = Preparer::Prepare(m_segment);",
    "        m_cubic = {static_cast<Number>(p.m_a[0]), static_cast<Number>(p.m_b[0]),",
    "            static_cast<Number>(p.m_c[0]), static_cast<Number>(p.m_d[0])};",
    "    }", "",
    "    [[gnu::noinline, gnu::cold]] void AdvanceWrapped()", "    {",
    "        do", "        {",
    "            m_phase -= 1;",
    "            m_segment = {m_segment.m_p1, m_segment.m_p2, m_segment.m_p3, DrawPoint(), 1, 1, 1, 1, 1};",
    "            Prepare();", "        }",
    "        while (m_phase >= 1);", "    }", "", "};", "",
    "template<typename Number>",
    "struct SmoothRandomLFOFramedGeneric", "{",
    "    SmoothRandomLFOFramedGeneric(double lambdaSamples, std::uint64_t seed)",
    "        : m_endpoint(lambdaSamples, seed), m_right(m_endpoint.Value())",
    "    {", "        PrepareFrame();", "    }", "",
    "    Number Process()", "    {",
    "        if (m_position == x_frameSamples)", "        {", "            PrepareFrame();", "        }", "",
    "        const Number output = std::fma(m_slope, static_cast<Number>(m_position), m_left);",
    "        ++m_position;",
    "        return output;", "    }", "",
    "private:",
    s!"    static constexpr unsigned x_frameSamples = {SmoothRandomLFO.frameSamples};",
    s!"    {name}<Number> m_endpoint;",
    "    Number m_left;",
    "    Number m_right;",
    "    Number m_slope;",
    "    unsigned m_position;", "",
    "    [[gnu::noinline]] void PrepareFrame()", "    {",
    "        m_left = m_right;",
    "        m_endpoint.AdvanceSamples(x_frameSamples);",
    "        m_right = m_endpoint.Value();",
    "        m_slope = (m_right - m_left) / x_frameSamples;",
    "        m_position = 0;", "    }", "", "};", ""]).toList

elab "#generate_cpp_smooth_random_lfo_stream " name:ident " to " path:str : command => do
  let code ← liftTermElabM <| generateSmoothRandomLFOStream name.getId.toString
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} from DSP.SmoothRandomLFO.Stream"

end DSP.Codegen

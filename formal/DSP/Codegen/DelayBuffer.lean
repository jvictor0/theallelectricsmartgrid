import DSP.Codegen.Cpp

namespace DSP.Codegen
open Lean Meta Elab Command

/-- Storage lowering for the fixed-capacity buffer interface. The general
arithmetic is read from the elaborated Lean definitions. The power-of-two
path uses `address_mod`, `advance_mod` and `modulo_intermediates_bounded`.
Native round-trip tests check this translation and the array storage boundary. -/
def generateDelayBuffer (cppName : String) : MetaM String := do
  unless validIdentifier cppName do throwError "C++ codegen: invalid buffer name"
  withLocalDeclD `capacity (mkConst ``Nat) fun capacity => do
    let fin := mkApp (mkConst ``Fin) capacity
    withLocalDeclD `head fin fun head => do
      withLocalDeclD `age fin fun age => do
        let locals := [(capacity.fvarId!, Value.scalar "size_t" "Capacity"),
          (head.fvarId!, Value.scalar "size_t" "m_head"),
          (age.fvarId!, Value.scalar "size_t" "age")]
        let (_, read) ← (do
          let (_, index) ← scalar (← evaluate (mkAppN (mkConst ``DelayBuffer.address) #[capacity, head, age]) locals)
          emit s!"return {index};").run { boundedIndices := true }
        let (_, push) ← (do
          let (_, index) ← scalar (← evaluate (mkAppN (mkConst ``DelayBuffer.advance) #[capacity, head]) locals)
          emit "m_data[m_head] = value;"
          emit s!"m_head = {index};").run { boundedIndices := true }
        let lines := #["#pragma once", "", "// Generated from DSP.DelayBuffer. Do not edit.",
          "// Storage lowering is tested against native Lean, not formally verified.", "//", "",
          "#include \"MultiChannelArray.hpp\"", "#include <array>", "#include <cassert>", "#include <cstddef>",
          "#include <cstdint>", "#include <limits>", "",
          "template<typename Number, size_t Size, size_t Capacity>", s!"struct {cppName}", "{",
          "    static_assert(Capacity > 0);", "    using Vector = MultiChannelNumber<Number, Size>;",
          "    static constexpr size_t x_capacity = Capacity;",
          "    static constexpr bool x_modularWrap = (Capacity & (Capacity - 1)) == 0 &&",
          "        Capacity <= std::numeric_limits<size_t>::max() / 2;",
          "    MultiChannelArray<Number, Size, Capacity> m_data{};", "    size_t m_head = 0;", "",
          "    size_t Address(size_t age) const", "    {", "        assert(age < Capacity);",
          "        assert(m_head < Capacity);", "        if constexpr (x_modularWrap)", "        {",
          "            return (m_head + Capacity - (age + 1)) % Capacity;", "        }",
          "        else", "        {"] ++
          read.lines.map (fun line => if line.isEmpty then "" else "            " ++ line) ++
          #["        }", "    }", "", "    Vector ReadAge(size_t age) const", "    {",
          "        return m_data[Address(age)];", "    }", "",
          "    Vector ReadWrappedAge(std::uint64_t age) const", "    {",
          "        return ReadAge(age % Capacity);", "    }", "",
          "    template<typename Index, size_t Count>",
          "    std::array<Vector, Count> ReadAgeOffsets(const MultiChannelNumber<Index, Size>& ages,",
          "        const std::array<std::uint64_t, Count>& offsets) const",
          "    {",
          "        return m_data.GatherWrappedOffsets([&](size_t lane)",
          "        {",
          "            return Address(ages[lane] % Capacity);",
          "        }, offsets);",
          "    }", "", "    Vector ReadDelay(size_t delay) const", "    {",
          "        assert(delay > 0 && delay <= Capacity);", "        return ReadAge(delay - 1);", "    }", "",
          "    Vector ReadDelays(const MultiChannelNumber<size_t, Size>& delays) const", "    {",
          "        Vector result;", "        for (size_t lane = 0; lane < Size; ++lane)", "        {",
          "            result[lane] = ReadDelay(delays[lane])[lane];", "        }", "",
          "        return result;", "    }", "", "    void Push(const Vector& value)", "    {",
          "        assert(m_head < Capacity);", "        if constexpr (x_modularWrap)", "        {",
          "            m_data[m_head] = value;", "            m_head = (m_head + 1) % Capacity;", "        }",
          "        else", "        {"] ++
          push.lines.map (fun line => if line.isEmpty then "" else "            " ++ line) ++
          #["        }", "    }", "", "    void Clear()", "    {", "        m_data.fill(Vector::Splat(0));",
          "        m_head = 0;", "    }", "", "};", ""]
        return String.intercalate "\n" lines.toList

elab "#generate_cpp_delay_buffer " cppName:ident " to " path:str : command => do
  let code ← liftTermElabM <| generateDelayBuffer cppName.getId.toString
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} from DSP.DelayBuffer"

end DSP.Codegen

"""Generate narrowly edited experimental headers; production stays unchanged."""
import pathlib
import sys


def replace_once(text, old, new):
    assert text.count(old) == 1, f"Source contract changed: {old!r}"
    return text.replace(old, new)


source, destination, variant = sys.argv[1:]
text = pathlib.Path(source).read_text()
if variant == "floor60":
    text = replace_once(text, "Number tolerance = maxMag * 0.00000001;",
                        "Number tolerance = maxMag * 0.001;")
elif variant == "schmitt":
    text = replace_once(text, "        struct Entry\n", """        enum struct GateState
        {
            Low,
            Triggered,
            High
        };

        GateState m_gate[x_maxComponents]{};

        struct Entry
""")
    text = replace_once(text, "Number tolerance = maxMag * 0.00000001;", """Number tolerance = maxMag * 0.001;
            Number lowTolerance = maxMag * 0.000251188643150958;""")
    text = replace_once(text, """                if (mag < tolerance)
""", """                GateState& state = m_gate[i];
                if (state == GateState::Triggered)
                {
                    state = GateState::High;
                }

                if (mag <= 0 || mag < lowTolerance)
                {
                    state = GateState::Low;
                }
                else if (state == GateState::Low && mag > tolerance)
                {
                    state = GateState::Triggered;
                }

                if (state == GateState::Low)
""")
    text = replace_once(text, """                    m_synthesisPhase[result.m_bin][channel] = m_synthesisPhase[result.m_parent][channel] + delta;""", """                    if (result.m_bin == result.m_parent &&
                        pvdr[channel].m_gate[result.m_bin] == PVDR::GateState::Triggered)
                    {
                        m_synthesisPhase[result.m_bin][channel] = m_owner->m_analysisPhase[result.m_bin][channel];
                    }
                    else
                    {
                        m_synthesisPhase[result.m_bin][channel] = m_synthesisPhase[result.m_parent][channel] + delta;
                    }
""")
else:
    assert variant == "current"
pathlib.Path(destination).write_text(text)

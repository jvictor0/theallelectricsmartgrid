"""Use production Schmitt code and reconstruct the historical comparison gates."""
import pathlib
import sys


def replace_once(text, old, new):
    assert text.count(old) == 1, f"Source contract changed: {old!r}"
    return text.replace(old, new)


source, destination, variant = sys.argv[1:]
text = pathlib.Path(source).read_text()
if variant in ("current", "floor60"):
    ratio = "0.00000001" if variant == "current" else "0.001"
    text = replace_once(text, """            Number triggerMagnitude = maxMag * x_triggerMagnitudeRatio;
            Number lowMagnitude = maxMag * x_lowMagnitudeRatio;""",
                        f"            Number tolerance = maxMag * {ratio};")
    text = replace_once(text, """                GateState& state = m_gate[i];
                if (state == GateState::Triggered)
                {
                    state = GateState::High;
                }

                if (mag <= 0 || mag < lowMagnitude)
                {
                    state = GateState::Low;
                }
                else if (state == GateState::Low && mag > triggerMagnitude)
                {
                    state = GateState::Triggered;
                }

                if (state == GateState::Low)""", "                if (mag < tolerance)")
    text = replace_once(text, """                    if (result.m_bin == result.m_parent &&
                        pvdr[channel].m_gate[result.m_bin] == PVDR::GateState::Triggered)
                    {
                        m_synthesisPhase[result.m_bin][channel] = m_owner->m_analysisPhase[result.m_bin][channel];
                    }
                    else
                    {
                        m_synthesisPhase[result.m_bin][channel] = m_synthesisPhase[result.m_parent][channel] + delta;
                    }""", """                    m_synthesisPhase[result.m_bin][channel] = m_synthesisPhase[result.m_parent][channel] + delta;""")
else:
    assert variant == "schmitt"
pathlib.Path(destination).write_text(text)

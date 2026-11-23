#!/usr/bin/env python3
"""Summarize the numerical ladder sweep and make its 4D table browsable offline."""

import argparse
import csv
import json
from pathlib import Path


PAGE = """<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>Ladder periodic-response sweep</title>
<style>
body {font:15px system-ui,sans-serif;background:#101722;color:#e1e8f0;margin:28px;max-width:1350px}
h1 {font-size:26px} p {max-width:1050px;line-height:1.5} a {color:#8ecfff}
label {display:inline-block;margin:10px 22px 18px 0} select {font:inherit;padding:5px}
table {border-collapse:collapse;font-variant-numeric:tabular-nums} th,td {padding:9px;border:1px solid #384455;text-align:right}
th {background:#1b2634} td {min-width:67px} button {font:inherit;color:inherit;background:transparent;border:0;cursor:pointer;width:100%;padding:0}
.timeout {outline:2px solid #e8a34f;outline-offset:-2px} small {display:block;font-size:10px;margin-top:3px}
pre {white-space:pre-wrap;background:#1b2634;padding:15px;line-height:1.4}.scroll {overflow-x:auto}
.note {color:#b2c0d1} .summary {display:flex;gap:22px;flex-wrap:wrap} .summary b {font-size:25px;display:block}
</style>
<h1>Ladder periodic-response sweep</h1>
<div class="summary" id="summary"></div>
<p>A = 0.25; 192 kHz engine; real tanh; excess-energy detector off; existing kSafe cap retained.
Both frequency axes span 20 Hz–20 kHz. Drive and resonance use ten equally spaced knob positions.
Input frequencies are rounded within 0.1% to repeating sample sequences.</p>
<p class="note">“Periodic” means all four sampled state histories passed a binary64 repeat test,
eight consecutive times, followed by a separate verification window. This is numerical evidence,
not a proof or an exact fixed-point certificate. Orange outlines indicate timeouts or failed verification:
their power values are finite-window observations, not established periodic responses.</p>
<p><a href="responses.csv">Response CSV table</a> · <a href="table.csv">Original q-period tests</a> · <a href="summary.json">Summary JSON</a> · <a href="metadata.json">Method and provenance</a> · <a href="multiple-period-checks.json">Longer-period checks</a></p>
<label>Drive <select id="drive"></select></label>
<label>Resonance <select id="resonance"></select></label>
<label>Cell value <select id="metric"><option value="total">Total power gain (dB)</option><option value="coherent">Fundamental power gain (dB)</option><option value="seconds">Audio seconds to settle / timeout</option></select></label>
<p id="parameters"></p><div class="scroll"><table id="grid"></table></div>
<p class="note">Rows: cutoff. Columns: actual input frequency. Click a cell for its complete record.
Power gain is relative to input mean-square A²/2. Settling time includes the minimum observation period;
it is not an estimate of the earliest mathematically possible convergence time.</p>
<pre id="detail">Select a cell.</pre>
<script>
const columns = __COLUMNS__;
const data = __DATA__.map(values=>Object.fromEntries(columns.map((key,index)=>[key,values[index]])));
const summary = __SUMMARY__;
const count = Math.max(...data.map(r => r.drive_index)) + 1;
const drive = document.getElementById('drive'), resonance = document.getElementById('resonance');
for (let j = 0; j < count; ++j) {
  for (const element of [drive, resonance]) {
    const option = document.createElement('option'); option.value=j;
    option.textContent=(100*j/(count-1)).toFixed(1)+'%'; element.appendChild(option);
  }
}
document.getElementById('summary').innerHTML =
  `<div><b>${summary.rows.toLocaleString()}</b>settings</div>` +
  `<div><b>${summary.counts.periodic.toLocaleString()}</b>q-periodic</div>` +
  `<div><b>${summary.longer_periodic}</b>longer-period responses</div>` +
  `<div><b>${(summary.rows-summary.counts.periodic-summary.longer_periodic).toLocaleString()}</b>unresolved</div>` +
  `<div><b>${summary.elapsed_wall_seconds.toFixed(1)} s</b>whole sweep wall time</div>`;
function render() {
  const selected = data.filter(r => r.drive_index === +drive.value && r.resonance_index === +resonance.value);
  const metric = document.getElementById('metric').value;
  const table = document.getElementById('grid'); table.replaceChildren();
  const header = table.insertRow(); header.appendChild(document.createElement('th')).textContent='Cutoff / input Hz';
  for (let f=0; f<count; ++f) {
    const row=selected.find(r => r.frequency_index===f);
    header.appendChild(document.createElement('th')).textContent=row.frequency_hz.toFixed(1);
  }
  for (let c=0; c<count; ++c) {
    const tr=table.insertRow(); tr.appendChild(document.createElement('th')).textContent=selected.find(r=>r.cutoff_index===c).cutoff_hz.toFixed(1);
    for (let f=0; f<count; ++f) {
      const row=selected.find(r=>r.cutoff_index===c && r.frequency_index===f);
      const td=tr.insertCell(); const button=document.createElement('button'); td.appendChild(button);
      const power=metric==='coherent' ? row.response_coherent_power : row.response_total_power;
      const value=metric==='seconds' ? row.response_search_audio_seconds : 10*Math.log10(Math.max(1e-30,power)/(row.amplitude**2/2));
      button.textContent=value.toFixed(metric==='seconds'?3:1);
      const label=document.createElement('small'); label.textContent=row.response_status==='periodic' ? `periodic ×${row.response_period_multiplier}` : 'unresolved'; button.appendChild(label);
      if(row.response_status!=='periodic') td.className='timeout';
      const level=metric==='seconds' ? Math.min(1,Math.log10(1+value)/Math.log10(31)) : Math.max(0,Math.min(1,(value+80)/100));
      td.style.background=`hsl(${220-190*level} 45% ${18+12*level}%)`;
      button.onclick=()=>{document.getElementById('detail').textContent=JSON.stringify(row,null,2)};
    }
  }
  const first=selected[0];
  document.getElementById('parameters').textContent=`g = ${first.gain.toFixed(5)}; s = ${first.slope.toFixed(5)}; requested k = ${first.requested_feedback.toFixed(5)}. Actual k may be capped at high cutoff; inspect each cell.`;
}
for(const id of ['drive','resonance','metric']) document.getElementById(id).onchange=render;
render();
</script></html>
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    directory = args.directory
    summary = json.loads((directory / "summary.json").read_text())
    rows = [json.loads(line) for line in (directory / "table.jsonl").read_text().splitlines()]
    metadata = json.loads((directory / "metadata.json").read_text())
    multiples_path = directory / "multiple-period-checks.json"
    multiples = json.loads(multiples_path.read_text()) if multiples_path.exists() else {"rows": [], "periodic": 0}
    longer = {row["id"]: row for row in multiples["rows"] if row["status"] == "periodic"}
    summary["longer_periodic"] = len(longer)
    size = metadata["arguments"]["size"]
    assert len(rows) == size ** 4 and len({row["id"] for row in rows}) == len(rows)
    assert all(row["verification_defect_score"] <= 1 for row in rows if row["status"] == "periodic")
    assert all(abs(row["frequency_error_relative"]) <= metadata["arguments"]["frequency_tolerance"] for row in rows)
    assert all(row["feedback"] <= row["requested_feedback"] + 1e-14 and
               row["feedback"] <= row["k_safe"] + 1e-14 for row in rows)
    for row in rows:
        response = longer.get(row["id"], row)
        assert response["status"] != "periodic" or response["verification_defect_score"] <= 1
        row.update(response_status="periodic" if response["status"] == "periodic" else "unresolved",
                   response_period_multiplier=response.get("multiplier", 1) if response["status"] == "periodic" else 0,
                   response_total_power=response["total_power"], response_coherent_power=response["coherent_power"],
                   response_coherent_real=response["coherent_real"], response_coherent_imag=response["coherent_imag"],
                   response_rms=response["rms"], response_defect_score=response["verification_defect_score"],
                   response_defect_absolute=response["verification_defect_absolute"],
                   response_period_samples=row["period_samples"] * response.get("multiplier", 1)
                       if response["status"] == "periodic" else 0,
                   response_compute_cpu_seconds=row["compute_cpu_seconds"] +
                       (response["compute_cpu_seconds"] if row["id"] in longer else 0),
                   response_search_audio_seconds=row["settling_audio_seconds"] +
                       (row["measurement_audio_seconds"] + response["settling_audio_seconds"] if row["id"] in longer else 0))
        for stage in range(1, 5):
            row[f"response_state_{stage}"] = response[f"state_{stage}"]
    with (directory / "responses.csv").open("w") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(sorted(rows, key=lambda row: row["id"]))
    columns = list(rows[0])
    values = [[row[column] for column in columns] for row in rows]
    output = PAGE.replace("__DATA__", json.dumps(values, separators=(",", ":"), allow_nan=False))
    output = output.replace("__COLUMNS__", json.dumps(columns))
    output = output.replace("__SUMMARY__", json.dumps(summary, allow_nan=False))
    (directory / "index.html").write_text(output)
    lines = ["# Periodic-response grid", "",
             f"{len(rows):,} settings; {summary['counts'].get('periodic', 0):,} passed the q-period repeat test; "
             f"{len(longer):,} additional responses passed a longer-period test; "
             f"{len(rows)-summary['counts'].get('periodic', 0)-len(longer):,} unresolved.", "",
             "Numerical binary64 evidence only. An unresolved row does not imply that no periodic solution exists.", "",
             "| Drive knob | Gain g | Periodic / settings |", "| --- | ---: | ---: |"]
    for group in summary["by_drive"]:
        gain = 0.25 * 20 ** group["drive_knob"]
        lines.append(f"| {group['drive_knob']:.1%} | {gain:.6g} | {group['periodic']} / {group['total']} |")
    lines += ["", "Settling audio seconds for accepted rows (includes the minimum observation period):", ""]
    for percentile, value in summary["settling_audio_seconds_percentiles"].items():
        lines.append(f"- p{percentile}: {value:.6g} s")
    lines += ["", f"Sweep wall time: {summary['elapsed_wall_seconds']:.3f} s with "
              f"{metadata['arguments']['workers']} workers. Native thread CPU sum: "
              f"{summary['total_native_thread_cpu_seconds']:.3f} s.", "",
              "## Interpretation", "",
              "The repeat test checks the maximum difference between consecutive whole-period state waveforms, "
              "stage by stage, against absolute tolerance 1e-12 plus relative tolerance 1e-9. "
              "It must pass eight consecutive periods, following at least 16 input periods and 20 standalone "
              "stage time constants. A further measurement window of at least eight periods and 0.1 seconds "
              "must also pass. The default settling budget is 30 seconds of audio per point.", "",
              "Warm starts use only previously accepted rows at the same frequency and cutoff. They interpolate "
              "(x1, x2/s, x3/s², x4/s³) at matching k*s³ when bracketed by a previous drive slice, otherwise "
              "use the nearest available effective resonance. Matching is a linearized seed heuristic, "
              "not an exact nonlinear equivalence. No repelling-root solver or extrapolation is used.", "",
              "Total power and coherent power are averaged over the separate measurement window. For timeouts "
              "these are finite-window observations only. The endpoint lag diagnostic can suggest a multiple "
              "of the forcing period but does not establish one. CPU/wall timings include measurement; "
              "settling audio samples and periods do not.", "",
              "The equation is the exact-real-tanh research model evaluated in binary64, with the Lean setter's "
              "kSafe cap and the excess detector disabled. The runtime rational saturator is not being tested. "
              "This table does not enumerate all solutions, establish uniqueness, or prove attraction.", "",
              "Longer-period results are supplementary checks of the 2–8-period candidates suggested by "
              "the endpoint diagnostic. They pass the full waveform test; the search is not exhaustive. "
              "responses.csv merges those measurements into response_* columns, keeping the original "
              "q-period results and timings. The browser displays the merged responses. Timing percentiles "
              "above cover q-periodic rows only.", "",
              "## Reproduce", "", "```sh", "make ladder-periodic-grid",
              "python3 scripts/analyze_ladder_periodic_grid.py --refine-rejections",
              "python3 scripts/analyze_ladder_periodic_grid.py --check-multiples",
              "python3 scripts/report_ladder_periodic_grid.py build/results/ladder-periodic-grid", "```", ""]
    (directory / "report.md").write_text("\n".join(lines))
    print(json.dumps(dict(rows=len(rows), report=str(directory / "index.html"), checks="passed")))


if __name__ == "__main__":
    main()

"""One row per requested point, retaining gaps and numerical disagreement."""
import argparse
import csv
import json
from collections import Counter
from pathlib import Path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("data", type=Path)
    ap.add_argument("table", type=Path)
    ap.add_argument("summary", type=Path)
    args = ap.parse_args()
    logs = [json.loads(line) for line in args.data.open()]
    targets = sorted({r["r"] for r in logs if r["kind"] == "target"})
    curves = sorted({(r["actual_hz"], r["cutoff_hz"], r["drive"]) for r in logs})
    key = lambda r: (r["actual_hz"], r["cutoff_hz"], r["drive"], r["r"])
    samples = {(key(r),r["spacing"]):r for r in logs if r["kind"] == "target"}
    meshes = {key(r):r["state_scaled_inf"] for r in logs if r["kind"] == "mesh_check"}
    all_rows = []
    for f, fc, drive in curves:
        for r in targets:
            k = f, fc, drive, r
            both = [samples[k,s] for s in [.01,.02] if (k,s) in samples]
            gap = meshes.get(k)
            row = dict(input_hz=f, cutoff_hz=fc, drive=drive, resonance=r, meshes=len(both), mesh_gap=gap)
            if both:
                b = both[0]
                for field in ["base_feedback","total_power","fundamental_power","fundamental_real","fundamental_imag",
                              "rho_wet","sigma_min","cond","full_return_residual","metal_jac_relative_inf"]:
                    row[field] = b[field]
            if len(both) < 2:
                status = "unresolved"
            elif gap is None or gap > 1e-5:
                status = "mesh_disagreement"
            elif any(t["full_return_residual"] > 1e-8 or t["metal_jac_relative_inf"] > .01 for t in both):
                status = "check_derivative_or_return"
            else:
                status = "consistent"
            row["status"] = status
            all_rows.append(row)
    counts = Counter(x["status"] for x in all_rows)
    good = [x for x in all_rows if x["status"] == "consistent"]
    result = dict(points=len(all_rows), paths=len(curves), counts=counts,
                  by_drive={str(d):dict(Counter(r["status"] for r in all_rows if r["drive"] == d))
                           for d in sorted({row["drive"] for row in all_rows})},
                  at_least_one_unstable_wet_multiplier=sum(row["rho_wet"] > 1.0001 for row in good),
                  max_accepted_abs_residual=max(r["full_return_residual"] for r in good),
                  max_accepted_mesh_gap=max(r["mesh_gap"] for r in good))
    with args.table.open("x",newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(dict.fromkeys(k for r in all_rows for k in r)))
        writer.writeheader()
        writer.writerows(all_rows)
    with args.summary.open("x") as f:
        json.dump(result,f,indent=2)
    print(json.dumps(result,indent=2))


if __name__ == "__main__":
    main()

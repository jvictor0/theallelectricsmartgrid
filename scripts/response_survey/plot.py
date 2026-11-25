"""Plot the finite trial with the largest visible error in a completed run."""
import argparse
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from experiment import Backend, Design, db
from run import digest, read


def plot(out, destination):
    out = out.resolve()
    hashes = read(out/"hashes.json")

    def checked(name):
        path = out/name
        if digest(path) != hashes[name]:
            raise ValueError(f"Archived file changed: {name}")
        return path

    report = read(checked("report.json"))
    scores = ((score, row["cell"], i) for row in report["cells"]
              for i, score in enumerate(row["scores"]) if score is not None)
    worst = max(scores, default=None)
    if worst is None:
        raise ValueError("No finite trial to plot")
    score, cell, index = worst
    with np.load(checked(f"audit/{cell}.npz")) as archive:
        config = archive["config"][index]
        measured = db(archive["powers"][index])
    checked("generated/recipe.json")
    design = Design(out/"generated")
    backend = Backend(out/"work", design, False)
    try:
        fitted = backend.knots(checked("table.bin").read_bytes(),config,len(design.freq))
    finally:
        backend.close()
    edges = design.dual_rate.edges if design.dual_rate else design.freq
    f = np.unique(np.r_[design.freq,np.geomspace(edges[0],edges[-1],1200)])
    fig, ax = plt.subplots(figsize=(8,4.5), layout="constrained")
    for knots, label in [(measured,"Held-out measurement"), (fitted,"Packed table")]:
        y = np.maximum(design.r["floorDb"],np.interp(np.log(f),np.log(design.freq),knots))
        ax.semilogx(f,y,label=label)
    knobs = ", ".join(f"{a['name']} = {x:.4g}" for a,x in zip(design.axes,config))
    ax.set(title=f"Largest measured RMS error · {score:.3f} dB\n{knobs}",
           xlabel="Frequency (Hz)",ylabel="Wet / dry power (dB)")
    ax.grid(alpha=0.25)
    ax.legend()
    fig.savefig(destination, dpi=160)
    plt.close(fig)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run",type=Path)
    parser.add_argument("destination",type=Path)
    args = parser.parse_args()
    plot(args.run,args.destination)

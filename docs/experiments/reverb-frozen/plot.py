"""Render the frozen Quad Reverb magnitude and return-series convergence plots."""
import json
import sys

from evaluate import OUT

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import ticker
import numpy as np

BG = "#f7f8fa"
FG = "#182538"
QUIET = "#4d596b"
GRID = "#dae0e8"
PALETTE = ("#186b99", "#c35b2b", "#087d70", "#8655a9")
TICKS = [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000]

plt.rcParams.update({
    "font.family": "DejaVu Sans",
    "figure.facecolor": BG,
    "axes.facecolor": "#ffffff",
    "axes.labelcolor": FG,
    "axes.edgecolor": GRID,
    "text.color": FG,
    "xtick.color": QUIET,
    "ytick.color": QUIET,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "axes.axisbelow": True,
    "savefig.facecolor": BG,
})

label = sys.argv[1] if len(sys.argv) > 1 else "curves"
summary = json.loads((OUT / f"summary-{label}.json").read_text())
data = np.load(OUT / f"data-{label}.npz")
settings = summary["settings"]
gain = 10*np.log10(data["full_power"])
frequency = data["frequency"]


def freq_tick(value, _=None):
    return f"{value/1000:g}k" if value >= 1000 else f"{value:g}"


fig, axes = plt.subplots(2, 2, figsize=(13.7, 9.5), sharex=True, sharey=True)
fig.subplots_adjust(left=.085, right=.965, top=.78, bottom=.105, hspace=.34, wspace=.12)
fig.text(.085, .943, "Quad reverb wet magnitude", fontsize=26, weight="bold")
fig.text(.085, .900, "Frozen small-signal RMS gain per output, averaging equal power from the four inputs",
         fontsize=13, color=QUIET)
for idx, (ax, info) in enumerate(zip(axes.flat, settings)):
    for lane, color in enumerate(PALETTE):
        ax.semilogx(frequency, gain[idx, :, lane], label=f"Output {lane+1}",
                    color=color, lw=1.10, alpha=.90)
    title = f"{info['name']}   ·   {info['ms']} ms   ·   {info['feedback_knob']:.0%} feedback knob"
    detail = f"{freq_tick(info['high_cut_hz'])}–{freq_tick(info['low_cut_hz'])} Hz damping passband"
    ax.text(0, 1.115, title, transform=ax.transAxes, fontsize=12.5, weight="bold")
    ax.text(0, 1.045, detail, transform=ax.transAxes, fontsize=9.5, color=QUIET)
    ax.set_xlim(20, 20000)
    ax.set_ylim(-34, 8)
    ax.set_yticks([-30, -20, -10, 0, 5])
    ax.set_xticks(TICKS, labels=[freq_tick(x) for x in TICKS])
    ax.xaxis.set_minor_locator(ticker.NullLocator())
    ax.grid(color=GRID, alpha=.8, lw=.65)
    ax.axhline(0, color="#a4aebe", lw=.85, zorder=.6)
    ax.tick_params(length=0, labelsize=9, pad=6)
    if idx % 2 == 0:
        ax.set_ylabel("Wet RMS gain (dB)", fontsize=10)
    if idx >= 2:
        ax.set_xlabel("Frequency (Hz)", fontsize=10)

handles, labels = axes[0, 0].get_legend_handles_labels()
fig.legend(handles, labels, frameon=False, ncol=4, fontsize=10, handlelength=2.5,
           bbox_to_anchor=(.084, .866), loc="upper left", borderaxespad=0, columnspacing=1.55)
fig.text(.085, .034, "192 kHz engine   ·   SAT gain 0.5   ·   Wet output at the reverb core",
         fontsize=10, color=QUIET)
fig.savefig(OUT / "reverb-magnitude.png", dpi=145)
plt.close(fig)

fig, ax = plt.subplots(figsize=(10.5, 5.8))
fig.subplots_adjust(left=.105, right=.955, top=.72, bottom=.15)
fig.text(.103, .918, "How many feedback terms?", fontsize=23, weight="bold")
fig.text(.103, .845, "Worst deviation from the solved loop across the four outputs and all plotted frequencies",
         fontsize=11, color=QUIET)
for info, color in zip(settings, PALETTE):
    delta = info["max_truncation_delta_db"]
    n = [int(key) for key in delta]
    err = [max(delta[key], 1e-3) for key in delta]
    ax.loglog(n, err, marker="o", markersize=4, lw=1.65, color=color, label=info["name"])
ax.axhline(.5, color="#485467", lw=1, ls=(0, (4, 3)), zorder=.6)
ax.annotate("0.5 dB target", (1.08, .53), fontsize=9, color=QUIET, ha="left", va="bottom")
ax.set_xlim(.95, 132)
ax.set_ylim(.0009, 28)
ax.set_xticks([1, 2, 4, 8, 16, 32, 64, 128], labels=["1", "2", "4", "8", "16", "32", "64", "128"])
ax.set_yticks([.001, .01, .1, .5, 1, 10], labels=["≤0.001", "0.01", "0.1", "0.5", "1", "10"])
ax.xaxis.set_minor_locator(ticker.NullLocator())
ax.yaxis.set_minor_locator(ticker.NullLocator())
ax.grid(color=GRID, alpha=.8, lw=.65)
ax.tick_params(length=0, labelsize=9, pad=6)
ax.set_xlabel("Terms retained, including the direct pass", fontsize=10)
ax.set_ylabel("Maximum gain error (dB)", fontsize=10)
ax.legend(frameon=False, ncol=4, fontsize=9, handlelength=2.15, loc="lower left",
          bbox_to_anchor=(0, 1.04), borderaxespad=0, columnspacing=1.0)
fig.savefig(OUT / "reverb-convergence.png", dpi=145)
plt.close(fig)

if label == "curves":
    ripple = np.load(OUT / "ripple-check.npz")
    fig, axes = plt.subplots(1, 2, figsize=(11.6, 4.5))
    fig.subplots_adjust(left=.09, right=.97, top=.70, bottom=.16, wspace=.235)
    fig.text(.09, .92, "The upper band still has ripple", fontsize=22, weight="bold")
    fig.text(.09, .846, "40 ms delay, 80% feedback knob, output 1", fontsize=11, color=QUIET)
    for ax, band in zip(axes, (0, 4)):
        f = ripple["freq"][band]
        averaged, fixed, original = ripple["curves"][0, band, :, :, 0]
        ax.plot(f, fixed, color="#b77d59", lw=1.3, label="Wander disabled")
        ax.plot(f, averaged, color=PALETTE[0], lw=1.8, label="Wander averaged, 0.5 Hz grid")
        ax.plot(f, original, color="#536176", lw=1.3, ls="--", label="Original plot")
        shown = (frequency >= f[0]) & (frequency <= f[-1])
        ax.scatter(frequency[shown], gain[1, shown, 0], color="#536176", s=12, zorder=4)
        ax.set_xlim(f[0], f[-1])
        ax.set_xlabel("Frequency (Hz)")
        ax.set_title(f"{f[0]:,.0f}–{f[-1]:,.0f} Hz", loc="left", fontsize=11, weight="bold", pad=8)
        ax.grid(color=GRID, linewidth=.6)
        ax.tick_params(length=0, labelsize=9)
    axes[0].set_ylabel("Wet RMS gain (dB)")
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, ncol=3, frameon=False, loc="upper left",
               bbox_to_anchor=(.08, .815), fontsize=9)
    fig.savefig(OUT / "reverb-ripple-zoom.png", dpi=150)
    plt.close(fig)

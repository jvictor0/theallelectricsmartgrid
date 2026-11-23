"""Plot a stationary finite-bank response and empirical fresh-record meeting times."""
import argparse
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np


def main(root):
    with np.load(root/'physical-stress/curves.npz') as data:
        centers = data['centers']
        fresh, fit = data['measured'][1], data['fitted'][1]
    with np.load(root/'record-banks/four/setting-1.npz') as data:
        bank = (data['lower_db']+data['upper_db'])/2
    with np.load(root/'lagged-fresh-extended/pairs.npz') as data:
        times = data['met'].T
    with np.load(root/'lagged-range/range.npz') as data:
        box = data['rows'][:, -1]
    fig = plt.figure(figsize=(12, 5.5), constrained_layout=True)
    g = fig.add_gridspec(2, 2, height_ratios=[2, 1])
    top = fig.add_subplot(g[0, 0])
    bottom = fig.add_subplot(g[1, 0], sharex=top)
    side = fig.add_subplot(g[:, 1])
    hz = np.geomspace(20, 20000, 2001)
    def displayed(db):
        return np.maximum(-60., np.interp(np.log(hz), np.log(centers), db))
    ref = displayed(fresh)
    for raw, color, style, name in [(fresh, '#252c39', '-', '64 independent long records'),
                                      (bank, '#13817e', '--', '4-record bank, stationary average'),
                                      (fit, '#b05b26', '-', 'Frozen UI fit')]:
        v = displayed(raw)
        top.semilogx(hz, v, style, color=color, lw=1.6, label=name)
        if raw is not fresh:
            bottom.semilogx(hz, v-ref, style, color=color, lw=1.4)
    top.set_title('515 Hz cutoff · 71% drive · 93% resonance', loc='left')
    top.set_ylabel('Displayed power ratio (dB)')
    top.legend(frameon=False, fontsize=8, loc='lower left')
    bottom.axhline(0, color='#414141', alpha=.35, lw=.8)
    bottom.set_ylabel('Difference from\n64 records (dB)')
    bottom.set_xlabel('Output frequency (Hz)')
    bottom.set_xlim(20, 20000)
    bottom.set_xticks([20, 100, 1000, 10000, 20000], ['20', '100', '1k', '10k', '20k'])
    top.tick_params(labelbottom=False)
    steps = np.arange(19)
    for a, name, col, style in [(times[0], '250 Hz · 24 pairs', '#af5a26', '-'),
                                (times[1], '515 Hz · 24 pairs', '#186a85', '-'),
                                (times[2], '359 Hz · 24 pairs', '#806f9d', '-'),
                                (box, 'Random controls · 96 pairs', '#292f39', '--')]:
        side.step(steps, [100*np.mean((a == 0) | (a > n)) for n in steps], where='post',
                  label=name, color=col, ls=style, lw=1.7)
    side.set_title('Fresh noise shared after different histories', loc='left')
    side.set_ylabel('Pairs still differing (%)')
    side.set_xlabel('Shared records (9.56 seconds each)')
    side.set_xlim(0, 18)
    side.set_xticks(np.arange(0, 19, 3))
    side.set_ylim(-2, 102)
    side.legend(frameon=False, fontsize=9)
    for ax in (top, bottom, side):
        ax.grid(color='#548096', alpha=.16)
        ax.spines[['top', 'right']].set_visible(False)
    fig.savefig(root/'lagged-fresh-extended/whole-spectrum.png', dpi=160)
    plt.close(fig)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('screen_results', type=Path)
    main(parser.parse_args().screen_results)

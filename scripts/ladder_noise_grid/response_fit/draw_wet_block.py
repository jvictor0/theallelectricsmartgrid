"""Compare the independent four-record confirmation and its frozen reference."""
import argparse
import json
from pathlib import Path

import matplotlib
import numpy as np

matplotlib.use('Agg')
import matplotlib.pyplot as plt

from record_bank import EDGES


def draw(root):
    reference = root.parent / 'survivor-replay-250'
    with np.load(reference / 'fresh-parents.npz') as data:
        wet = (data['base'] + data['delta'].sum(1)).mean(0)[0]
    dry = json.loads((reference / 'dry-power.json').read_text())
    denom = np.sqrt(np.array(dry['dry_power_lower']) * dry['dry_power_upper'])
    limits = json.loads((root / 'confirmation-bounds.json').read_text())
    centers = np.sqrt(EDGES[:-1] * EDGES[1:])
    freq = np.unique(np.r_[np.geomspace(EDGES[0], EDGES[-1], 1600), centers])

    def curve(power):
        db = 10 * np.log10(power / denom)
        return np.maximum(-60, np.interp(np.log(freq), np.log(centers), db))

    ref = curve(wet)
    fig, (top, bottom) = plt.subplots(2, 1, figsize=(9, 6.2), sharex=True,
        height_ratios=[1.25, 1], layout='constrained')
    fig.suptitle('Naive reupping ladder at 249.70 Hz cutoff', fontweight='bold')
    top.set_title('60.33% drive  ·  97.73% resonance  ·  white noise')
    top.semilogx(freq, ref, color='#243652', lw=3, label='Frozen reference')
    for file, label, color in [('verified', 'First 1,024 histories', '#766194'),
                                ('confirmation', 'Fresh 1,024 histories', '#ab4a22')]:
        with np.load(root / (file + '.npz')) as data:
            measured = curve(data['powers'].mean(axis=(0, 1)))
        bottom.plot(freq, measured-ref, color=color, lw=1.4, label=label)
        if file == 'confirmation':
            top.plot(freq, measured, color=color, ls='--', lw=1.7, label=label)
    lo = curve(wet * np.array(limits['band_mean_lower_multiple'])) - ref
    hi = curve(wet * np.array(limits['band_capped_mean_upper_multiple'])) - ref
    bottom.fill_between(freq, lo, hi, color='#8ba4ab', alpha=.38, zorder=0,
        label='99% interval for capped mean')
    peak = limits['peak_band_center_hz']
    low, high = limits['raw_peak_bounds_db']
    bottom.plot([peak, peak], [low, high], color='#23364b', marker='_', lw=2,
        label='98.75% uncapped interval at 261.43 Hz')
    bottom.axhline(0, color='.4', lw=.7, zorder=0)
    for ax in (top, bottom):
        ax.grid(axis='y', alpha=.2)
        ax.spines[['right','top']].set_visible(False)
        ax.legend(fontsize=8, loc='best', frameon=False)
    top.set_ylabel('Wet / dry power (dB)')
    bottom.set_ylabel('Difference from reference (dB)')
    bottom.set_xlabel('Frequency (Hz)')
    bottom.set_xlim(EDGES[0], EDGES[-1])
    bottom.set_xticks([20, 50, 100, 250, 500, 1000, 2500, 5000, 10000, 20000],
        labels=['20', '50', '100', '250', '500', '1k', '2.5k', '5k', '10k', '20k'])
    fig.savefig(root / 'finite-wet-response.png', dpi=160)
    plt.close(fig)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    draw(parser.parse_args().root)

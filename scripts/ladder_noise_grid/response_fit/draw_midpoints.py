"""Draw the measured spectra at the worst whole-curve and worst single-band midpoints."""
import argparse
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

from peak_width import summarize


def main(root):
    data = json.loads((root/'midpoint-examples.json').read_text())
    audit = json.loads((root/'midpoint-audit.json').read_text())
    assert data['points'] == audit['points'] and data['model'] == audit['model']
    edges = np.array(data['edges'])
    centers = np.sqrt(edges[:-1]*edges[1:])
    hz = np.unique(np.r_[np.geomspace(20, 20000, 2000), centers])
    def displayed(raw):
        return np.maximum(-60, np.interp(np.log2(hz), np.log2(centers), raw))

    fig, axes = plt.subplots(2, 2, figsize=(11, 6), sharex=True, sharey='row',
        gridspec_kw={'height_ratios':[2, 1]}, constrained_layout=True)
    colors = ['#704291', '#d46b20']
    widths = {}
    for column, (key, title, score) in enumerate([
        ('whole_curve', 'Largest whole-curve error', audit['worst']),
        ('single_band', 'Largest band-center error', audit['max_center_db']),
    ]):
        ex = data['examples'][key]
        assert abs(score-ex['score']) < 1e-10
        d,r,c = ex['ids']
        fit, measurement = [np.array(ex[k]) for k in ('fitted','measured')]
        stats = summarize(centers, measurement, fit)
        widths[key] = stats
        ax, residual = axes[:,column]
        ax.set_title(f'{title}\n{20*1000**(c/2880):.1f} Hz cutoff · {100*d/72:.1f}% drive · {100*r/72:.1f}% resonance',
                     fontsize=11, loc='left')
        ax.semilogx(hz, displayed(measurement), color=colors[0], lw=2, label='16 noise records')
        ax.semilogx(hz, displayed(fit), color=colors[1], lw=1.6, ls='--', label='Frozen fit')
        ax.legend(frameon=False, loc='lower left', fontsize=9)
        residual.axhspan(-3, 3, color='#dcdeda', alpha=.55)
        residual.axhline(0, color='#879087', lw=.7)
        for low, high in stats['widths']['3']['hz_intervals']:
            residual.axvspan(low, high, color=colors[0], alpha=.16)
        residual.semilogx(hz, displayed(measurement)-displayed(fit), color=colors[0], lw=1.5)
        residual.set_title(f"{stats['rms_db']:.2f} dB RMS · above 3 dB on {stats['widths']['3']['percent']:.2f}% of axis",
                           fontsize=10, loc='left')
        residual.set_xlabel('Output frequency (Hz)')
        residual.set_xlim(20, 20000)
        residual.set_xticks([20, 100, 1000, 10000, 20000], ['20','100','1k','10k','20k'])
    axes[0,0].set_ylabel('Displayed wet / dry power (dB)')
    axes[1,0].set_ylabel('Measured − fit (dB)')
    for ax in axes.flat:
        ax.spines[['right','top']].set_visible(False)
        ax.grid(alpha=.16)
    fig.savefig(root/'midpoint-worst.png', dpi=160)
    plt.close(fig)
    (root/'midpoint-widths.json').write_text(json.dumps(widths, indent=2, allow_nan=False)+'\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('screen_results', type=Path)
    main(parser.parse_args().screen_results)

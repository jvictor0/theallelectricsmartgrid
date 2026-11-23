"""Check every pair of boundary states and evaluate a synchronized bank with exact weights."""
import argparse
from fractions import Fraction
from pathlib import Path

import numpy as np


def check(edges, reward, lo, hi):
    n, size = edges.shape
    if (reward.shape != (n, size, 240) or not np.issubdtype(edges.dtype, np.integer)
            or edges.min() < 0 or edges.max() >= n):
        raise ValueError('Invalid transition or reward array')
    unseen = {0}
    reached = set(unseen)
    while unseen:
        unseen = {int(j) for i in unseen for j in edges[i]} - reached
        reached |= unseen
    if len(reached) != n:
        raise ValueError('Graph contains unreachable states')
    pairs = {(i, j) for i in range(n) for j in range(i)}
    counts = [len(pairs)]
    while pairs:
        next_pairs = {(max(int(u), int(v)), min(int(u), int(v))) for i, j in pairs
                      for u, v in zip(edges[i], edges[j]) if u != v}
        if next_pairs == pairs:
            return dict(pair_counts=counts, guaranteed_meeting_records=None)
        if not next_pairs < pairs:
            raise ArithmeticError('Pair image grew')
        counts.append(len(next_pairs))
        pairs = next_pairs
    depth = len(counts)-1
    # Every length-depth word now gives the same end state from every origin.
    # Count iid equally likely words from reset using Python's unbounded integers.
    def advance(weights):
        out = [0]*n
        for i, weight in enumerate(weights):
            for j in edges[i]:
                out[j] += weight
        return out
    weights = [1]+[0]*(n-1)
    for _ in range(depth):
        weights = advance(weights)
    if advance(weights) != [v*size for v in weights] or sum(weights) != size**depth:
        raise ArithmeticError('Word counts do not give a stationary distribution')
    for b in range(reward.shape[-1]):
        power = sum(w*Fraction(float(v)) for w, row in zip(weights, reward[:, :, b]) for v in row)
        mean = power/(size**(depth+1))
        if not Fraction(float(lo[b])) <= mean <= Fraction(float(hi[b])):
            raise ArithmeticError(f'Exact stationary readout outside enclosure at column {b}')
    return dict(pair_counts=counts, guaranteed_meeting_records=depth,
                exact_weights=weights, weight_denominator=size**depth)


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('banks', type=Path, nargs='+')
    for path in ap.parse_args().banks:
        with np.load(path) as data:
            print(path, check(*(data[k] for k in ('edges', 'rewards', 'lo', 'hi'))))

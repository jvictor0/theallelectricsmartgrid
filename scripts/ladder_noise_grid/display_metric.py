"""The sampled-response UI displays wet/dry power down to −60 dB."""
import numpy as np

DISPLAY_FLOOR_DB = -60.0

def displayed_db(db):
    return np.maximum(db, DISPLAY_FLOOR_DB)

def display_delta(a, b):
    return np.abs(displayed_db(a) - displayed_db(b))

def curve_mse(a, b, edges):
    """Mean squared displayed dB error, integrated uniformly in log frequency."""
    widths = np.diff(np.r_[np.log2(edges[0]), np.log2(np.sqrt(edges[:-1] * edges[1:])), np.log2(edges[-1])])
    a, b = (np.pad(v, ((0, 0), (1, 1)), mode='edge') for v in (a, b))
    visible = ((a[:, :-1] > DISPLAY_FLOOR_DB) | (a[:, 1:] > DISPLAY_FLOOR_DB) |
               (b[:, :-1] > DISPLAY_FLOOR_DB) | (b[:, 1:] > DISPLAY_FLOOR_DB))
    rows, segments = np.nonzero(visible)
    area = np.zeros(len(a))
    if not len(rows):
        return area
    x, y = a[rows, segments], b[rows, segments]
    dx, dy = a[rows, segments + 1] - x, b[rows, segments + 1] - y

    def crossing(v, slope):
        t = np.divide(DISPLAY_FLOOR_DB - v, slope, out=np.zeros_like(v), where=slope != 0)
        return np.clip(t, 0, 1)

    tx, ty = crossing(x, dx), crossing(y, dy)
    points = [np.zeros_like(x), np.minimum(tx, ty), np.maximum(tx, ty), np.ones_like(x)]
    errors = [displayed_db(x + t * dx) - displayed_db(y + t * dy) for t in points]
    for start, end, left, right in zip(points, points[1:], errors, errors[1:]):
        contributing = (end > start) & ((left != 0) | (right != 0))
        dt = (end - start)[contributing]
        l, r = left[contributing], right[contributing]
        integrals = dt * (l*l + l*r + r*r) * widths[segments[contributing]]
        area += np.bincount(rows[contributing], weights=integrals, minlength=len(a))
    return area / (3 * widths.sum())

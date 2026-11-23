#!/usr/bin/env python3
"""Validate binary-to-SQL fidelity, checksum rejection, resume, and grid geometry."""

import argparse
import ctypes
import json
from pathlib import Path
import tempfile

import numpy as np

from import_ladder_survey_sql import DTYPE, ROOT, columns, connect, ingest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('fixture', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='check-', dir=args.output) as directory:
        db = Path(directory)/'fixture.duckdb'
        ingest(args.fixture, db, 32)
        ingest(args.fixture, db, 32)
        con = connect(db)
        mapped = np.memmap(args.fixture/'worker-000.bin', dtype=DTYPE, mode='r', offset=24)
        order = np.argsort(mapped['job'])
        expected = mapped['points'][order].reshape(-1,216)
        actual = con.execute('SELECT '+','.join(columns())+' FROM points ORDER BY point_id').fetchnumpy()
        for i,name in enumerate(columns()):
            values = actual[name]
            if np.ma.isMaskedArray(values):
                values = values.filled(np.nan)
            assert np.array_equal(values, expected[:,i], equal_nan=True), name
        n = json.loads((args.fixture/'manifest.json').read_text())['grid']
        for axis in ('frequency','drive','resonance','cutoff'):
            assert con.execute(f'SELECT count(*) FROM neighbors_{axis}').fetchone()[0] == n**3*(n-1)
            assert con.execute(f'SELECT count(*) FROM interpolation_{axis}').fetchone()[0] == n**3*(n-2)
        con.execute('UPDATE points SET late_total_mean=1+c_i,late_total_sd=0')
        assert con.execute('SELECT max(total_interpolation_error_input_power) FROM interpolation_cutoff').fetchone()[0] < 1e-12
        con.execute('DELETE FROM points WHERE c_i=1')
        assert con.execute('SELECT count(*) FROM neighbors_cutoff').fetchone()[0] == n**3*(n-3)
        assert con.execute('SELECT count(*) FROM interpolation_cutoff').fetchone()[0] == n**3*max(n-4,0)
        con.close()
        library = ctypes.CDLL(str(ROOT/'build/tools/ladder-survey-sql/libladder_survey_verify.so'))
        library.LadderVerifyRecords.argtypes = [ctypes.c_void_p,ctypes.c_uint64,ctypes.c_uint64]
        library.LadderVerifyRecords.restype = ctypes.c_int64
        record = bytearray(mapped[:1].tobytes())
        buffer = (ctypes.c_ubyte*len(record)).from_buffer(record)
        assert library.LadderVerifyRecords(buffer,1,DTYPE.itemsize) == -1
        record[72] ^= 1
        assert library.LadderVerifyRecords(buffer,1,DTYPE.itemsize) == 0
    result = dict(status='passed',roundtrip_fields=len(expected)*216,resume_no_duplicate=True,
                  neighbor_no_wrap_or_gap_bridge=True,linear_interpolation_exact=True,
                  checksum_corruption_rejected=True)
    (args.output/'validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()

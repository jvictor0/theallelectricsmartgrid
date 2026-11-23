#!/usr/bin/env python3
"""Import all saved spectral summaries into a persistent, validated DuckDB."""

import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import struct
import time

import duckdb
import numpy as np
import pyarrow as pa

ROOT = Path(__file__).resolve().parents[1]
DTYPE = np.dtype([('job', '<u8'), ('points', '<f4', (4, 216)), ('checksum', '<u8')])
assert DTYPE.itemsize == 3472
STATS = ('mean', 'sd', 'min', 'max')
SCOPES = ('quarter', 'half', 'full', 'late')
METRICS = ('total', 'fundamental', 'real', 'imaginary', 'dc', 'residual')


def columns():
    aggregates = ('raw_full_total', 'raw_late_total', 'temporal_total_cv',
                  'temporal_fundamental_cv', 'fundamental_coherence', 'late_peak',
                  'late_crest', 'total_prefix_drift', 'fundamental_prefix_drift',
                  'coherent_prefix_drift')
    result = [f'{name}_{stat}' for name in aggregates for stat in STATS]
    result += [f'{scope}_{metric}_{stat}' for scope in SCOPES for metric in METRICS for stat in STATS]
    result += [f'window_{w:02}_{kind}' for kind in ('total_mean', 'total_sd', 'fundamental_mean', 'fundamental_sd')
               for w in range(20)]
    assert len(result) == 216
    return result


def connect(path):
    connection = duckdb.connect(str(path))
    connection.execute("SET memory_limit='96GB'")
    connection.execute("SET threads=16")
    connection.execute("SET max_temp_directory_size='24GB'")
    connection.execute("SET preserve_insertion_order=false")
    return connection


def metadata(connection, manifest, source, fingerprint):
    connection.execute('CREATE TABLE IF NOT EXISTS dataset (fingerprint VARCHAR, source VARCHAR, manifest JSON, status VARCHAR)')
    previous = connection.execute('SELECT fingerprint FROM dataset').fetchall()
    if previous:
        assert previous == [(fingerprint,)], 'Database belongs to a different source'
    else:
        connection.execute('INSERT INTO dataset VALUES (?,?,?,?)',
                           [fingerprint, str(source), json.dumps(manifest), 'importing'])
    connection.execute('CREATE TABLE IF NOT EXISTS ingest_chunks (file_name VARCHAR, first_record UBIGINT, records UINTEGER, PRIMARY KEY(file_name, first_record))')
    if not connection.execute("SELECT count(*) FROM information_schema.tables WHERE table_name='axes'").fetchone()[0]:
        n = manifest['grid']
        rows = []
        for i, f in enumerate(manifest['frequencies']):
            cutoff = 20*1000**(i/(n-1))
            rows.append(dict(index=i, target_hz=f['target_hz'], frequency_hz=f['actual_hz'],
                             frequency_octaves=float(np.log2(f['actual_hz'])),
                             cutoff_hz=cutoff, cutoff_octaves=float(np.log2(cutoff)),
                             knob=i/(n-1), gain=.25*20**(i/(n-1)),
                             requested_resonance=3.9*np.expm1(np.log(49)*i/(n-1))/48,
                             period_samples=f['period_samples'], window_samples=f['window_samples'],
                             complete_windows=f['complete_windows']))
        connection.register('axis_input', pa.Table.from_pylist(rows))
        connection.execute('CREATE TABLE axes AS SELECT * FROM axis_input')
        connection.unregister('axis_input')


def views(connection, manifest):
    n = manifest['grid']
    power = manifest['amplitude']**2/2
    starts = manifest['starts']
    connection.execute(f"""
      CREATE OR REPLACE VIEW responses AS
      SELECT p.*,
        f.frequency_hz, f.frequency_octaves, c.cutoff_hz, c.cutoff_octaves,
        d.knob AS drive_knob, d.gain AS drive_gain, r.knob AS resonance_knob,
        r.requested_resonance,
        late_total_mean::DOUBLE/{power} AS total_gain_squared,
        late_fundamental_mean::DOUBLE/{power} AS fundamental_gain_squared,
        10*log10(nullif(late_total_mean::DOUBLE/{power},0)) AS total_db,
        10*log10(nullif(late_fundamental_mean::DOUBLE/{power},0)) AS fundamental_db,
        late_total_sd::DOUBLE/nullif(late_total_mean,0) AS total_trial_cv,
        late_fundamental_sd::DOUBLE/nullif(late_fundamental_mean,0) AS fundamental_trial_cv,
        (late_total_max::DOUBLE-late_total_min)/{power} AS total_trial_range_input_power,
        (late_fundamental_max::DOUBLE-late_fundamental_min)/{power} AS fundamental_trial_range_input_power,
        late_total_sd::DOUBLE/sqrt({starts-1}) AS total_mc_standard_error,
        late_fundamental_sd::DOUBLE/sqrt({starts-1}) AS fundamental_mc_standard_error
      FROM points p JOIN axes f ON f_i=f.index JOIN axes c ON c_i=c.index
      JOIN axes d ON d_i=d.index JOIN axes r ON r_i=r.index
    """)
    connection.execute("""
      CREATE OR REPLACE VIEW measures AS
      SELECT point_id,f_i,d_i,r_i,c_i,late_total_mean::DOUBLE AS total,
        late_total_sd::DOUBLE AS total_sd,late_fundamental_mean::DOUBLE AS fundamental,
        late_fundamental_sd::DOUBLE AS fundamental_sd,
        temporal_total_cv_mean::DOUBLE AS total_temporal_cv,
        temporal_fundamental_cv_mean::DOUBLE AS fundamental_temporal_cv,
        total_prefix_drift_mean::DOUBLE AS total_prefix_drift,
        fundamental_prefix_drift_mean::DOUBLE AS fundamental_prefix_drift
      FROM points
    """)
    for axis, stride in [('frequency', n**3), ('drive', n**2), ('resonance', n), ('cutoff', 1)]:
        index = {'frequency':'f_i', 'drive':'d_i', 'resonance':'r_i', 'cutoff':'c_i'}[axis]
        coord = {'frequency':'frequency_octaves', 'cutoff':'cutoff_octaves',
                 'drive':'knob', 'resonance':'knob'}[axis]
        fields = []
        for metric in ('total', 'fundamental'):
            fields += [
                f'a.{metric} AS {metric}_a', f'b.{metric} AS {metric}_b',
                f'abs(b.{metric}-a.{metric})/{power} AS {metric}_change_input_power',
                f"CASE WHEN least(a.{metric},b.{metric})>={power*1e-4} THEN abs(10*log10(b.{metric}/a.{metric})) END AS {metric}_change_db",
                f'sqrt((a.{metric}_sd*a.{metric}_sd+b.{metric}_sd*b.{metric}_sd)/{starts-1})/{power} AS {metric}_change_mc_se',
                f'greatest(a.{metric}_sd/nullif(a.{metric},0),b.{metric}_sd/nullif(b.{metric},0)) AS {metric}_max_trial_cv',
                f'greatest(a.{metric}_temporal_cv,b.{metric}_temporal_cv) AS {metric}_max_temporal_cv',
            ]
        connection.execute(f"""
          CREATE OR REPLACE VIEW neighbors_{axis} AS
          SELECT a.point_id AS point_a,b.point_id AS point_b,a.f_i,a.d_i,a.r_i,a.c_i,
            xb.{coord}-xa.{coord} AS coordinate_step,{','.join(fields)}
          FROM measures a JOIN measures b ON b.point_id=a.point_id+{stride} AND b.{index}=a.{index}+1
          JOIN axes xa ON xa.index=a.{index} JOIN axes xb ON xb.index=b.{index}
        """)
        fields = []
        for metric in ('total', 'fundamental'):
            prediction = f'(1-w)*a.{metric}+w*b.{metric}'
            fields += [
                f'c.{metric} AS {metric}_center',
                f'abs(c.{metric}-({prediction}))/{power} AS {metric}_interpolation_error_input_power',
                f"""CASE WHEN least(a.{metric},b.{metric},c.{metric})>={power*1e-4}
                  THEN abs(10*log10(c.{metric})-((1-w)*10*log10(a.{metric})+w*10*log10(b.{metric})))
                  END AS {metric}_interpolation_error_db""",
                f'sqrt((c.{metric}_sd*c.{metric}_sd+(1-w)*(1-w)*a.{metric}_sd*a.{metric}_sd+w*w*b.{metric}_sd*b.{metric}_sd)/{starts-1})/{power} AS {metric}_interpolation_mc_se',
                f'greatest(a.{metric}_sd/nullif(a.{metric},0),b.{metric}_sd/nullif(b.{metric},0),c.{metric}_sd/nullif(c.{metric},0)) AS {metric}_max_trial_cv',
            ]
        connection.execute(f"""
          CREATE OR REPLACE VIEW interpolation_{axis} AS
          SELECT c.point_id,c.f_i,c.d_i,c.r_i,c.c_i,w,{','.join(fields)}
          FROM measures c
          JOIN measures a ON a.point_id+{stride}=c.point_id AND a.{index}+1=c.{index}
          JOIN measures b ON b.point_id=c.point_id+{stride} AND b.{index}=c.{index}+1
          JOIN axes xa ON xa.index=a.{index} JOIN axes xb ON xb.index=b.{index}
          JOIN axes xc ON xc.index=c.{index},
          LATERAL (SELECT (xc.{coord}-xa.{coord})/(xb.{coord}-xa.{coord}) AS w)
        """)


def ingest(source, database, chunk_size):
    manifest = json.loads((source/'manifest.json').read_text())
    assert manifest['format'] == 3
    assert manifest['record_bytes'] == DTYPE.itemsize and manifest['point_float_count'] == 216
    assert json.loads((source/'progress.json').read_text())['status'] == 'complete'
    files = sorted(source.glob('worker-*.bin'))
    info = [(p.name, p.stat().st_size) for p in files]
    fingerprint = hashlib.sha256(((source/'manifest.json').read_text()+json.dumps(info)+str(chunk_size)).encode()).hexdigest()
    expected_identity = 14695981039346656037
    for byte in manifest['identity'].encode():
        expected_identity = ((expected_identity ^ byte)*1099511628211) % 2**64
    library = ctypes.CDLL(str(ROOT/'build/tools/ladder-survey-sql/libladder_survey_verify.so'))
    library.LadderVerifyRecords.argtypes = [ctypes.c_void_p, ctypes.c_uint64, ctypes.c_uint64]
    library.LadderVerifyRecords.restype = ctypes.c_int64
    names = columns()
    database.parent.mkdir(parents=True, exist_ok=True)
    connection = connect(database)
    metadata(connection, manifest, source, fingerprint)
    n = manifest['grid']
    seen = np.zeros(n**4//4, dtype=bool)
    complete_windows = np.array([f['complete_windows'] for f in manifest['frequencies']])
    chunks_done = {(name, first): count for name, first, count in connection.execute('SELECT * FROM ingest_chunks').fetchall()}
    total_records = 0
    started = time.monotonic()
    for path in files:
        with path.open('rb') as stream:
            magic, identity, record_size = struct.unpack('<QQQ',stream.read(24))
        assert magic == 0x334752454444414c and record_size == DTYPE.itemsize
        assert identity == expected_identity, 'Header does not match the manifest'
        assert (path.stat().st_size-24) % record_size == 0
        count = (path.stat().st_size-24)//record_size
        mapped = np.memmap(path, dtype=DTYPE, mode='r', offset=24, shape=(count,))
        for first in range(0, count, chunk_size):
            block = mapped[first:first+chunk_size]
            size = len(block)
            assert library.LadderVerifyRecords(block.ctypes.data, size, record_size) == -1, (path, first, 'checksum failure')
            jobs = np.array(block['job'], copy=True)
            assert np.all(jobs < len(seen)) and len(np.unique(jobs)) == size and not np.any(seen[jobs])
            seen[jobs] = True
            total_records += size
            if (path.name, first) in chunks_done:
                assert chunks_done[path.name, first] == size
                continue
            point = (np.repeat(jobs*4, 4)+np.tile(np.arange(4,dtype='uint64'), size)).astype('uint32')
            f = (point//n**3).astype('uint16')
            d = ((point//n**2)%n).astype('uint16')
            r = ((point//n)%n).astype('uint16')
            c = (point%n).astype('uint16')
            values = np.array(block['points']).reshape(-1,216)
            assert np.all(np.isfinite(values[:, :136]))
            groups = values[:, :136].reshape(-1,34,4)
            assert np.all(groups[:,:,1]>=0)
            assert np.all(groups[:,:,2]<=groups[:,:,0]) and np.all(groups[:,:,0]<=groups[:,:,3])
            for j in range(80):
                assert np.array_equal(np.isnan(values[:,136+j]), (j%20)>=complete_windows[f])
            arrays = [pa.array(x) for x in [point,f,d,r,c]]
            arrays += [pa.array(values[:,j], mask=np.isnan(values[:,j])) for j in range(216)]
            incoming = pa.Table.from_arrays(arrays, names=['point_id','f_i','d_i','r_i','c_i']+names)
            connection.register('incoming', incoming)
            connection.execute('BEGIN')
            connection.execute('CREATE TABLE IF NOT EXISTS points AS SELECT * FROM incoming LIMIT 0')
            connection.execute('INSERT INTO points SELECT * FROM incoming')
            connection.execute('INSERT INTO ingest_chunks VALUES (?,?,?)', [path.name,first,size])
            connection.execute('COMMIT')
            connection.unregister('incoming')
            if total_records//chunk_size % 8 == 0:
                connection.execute('CHECKPOINT')
            elapsed = time.monotonic()-started
            print(json.dumps(dict(stage='import', records=total_records, points=total_records*4,
                                  elapsed_seconds=elapsed, records_per_second=total_records/elapsed)), flush=True)
    assert total_records == manifest['jobs']
    row_count = connection.execute('SELECT count(*) FROM points').fetchone()[0]
    assert row_count == manifest['selected_points']
    views(connection, manifest)
    connection.execute("UPDATE dataset SET status='complete'")
    connection.execute('CHECKPOINT')
    report = dict(status='complete', rows=row_count, records_checked=total_records,
                  all_checksums_verified=True, unique_in_range_job_ids=True, finite_fields_validated=True,
                  source_fingerprint=fingerprint, database=str(database), duckdb_version=duckdb.__version__,
                  elapsed_seconds=time.monotonic()-started)
    connection.close()
    (database.parent/'import-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('database', type=Path)
    parser.add_argument('--chunk-records', type=int, default=65536)
    args = parser.parse_args()
    assert args.chunk_records > 0
    ingest(args.source.resolve(), args.database.resolve(), args.chunk_records)


if __name__ == '__main__':
    main()

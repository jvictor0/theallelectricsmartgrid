#!/usr/bin/env python3
"""Inspect committed continuation records and optionally load a completed run into DuckDB."""
import argparse
import json
from pathlib import Path
import struct
import numpy as np

DTYPE=np.dtype([('point','<u8'),('values','<f4',(16,)),('checksum','<u8')])


def hash_bytes(data):
    value=14695981039346656037
    for byte in data:
        value=((value^byte)*1099511628211)&((1<<64)-1)
    return value


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root',type=Path);parser.add_argument('--import-sql',action='store_true')
    args=parser.parse_args();root=args.root
    manifest=json.loads((root/'manifest.json').read_text())
    checkpoint=(root/'checkpoint.bin').read_bytes()
    assert hash_bytes(checkpoint[:-8])==struct.unpack_from('<Q',checkpoint,len(checkpoint)-8)[0]
    header=struct.unpack_from('<9Q',checkpoint)
    magic,identity,cursor,next_level,points,completed,updates,active,output_bytes=header
    assert output_bytes==24+points*80
    with (root/'points.bin').open('rb') as f:
        file_magic,file_identity,stride=struct.unpack('<3Q',f.read(24))
    assert file_magic==0x314e54434c44414c and identity==file_identity and stride==80
    result={'committed_points':points,'completed_curves':completed,'active_curves':active,
            'next_fine_position':next_level,'complete':completed==manifest['curves'],
            'input_amplitude':manifest['amplitude'],'input_peak_dbfs':float(20*np.log10(manifest['amplitude']))}
    if points:
        records=np.memmap(root/'points.bin',mode='r',dtype=DTYPE,offset=24,shape=(points,))
        indices=np.linspace(0,points-1,min(points,200000),dtype=np.int64)
        sample=records['values'][indices]
        result['sampled_record_count']=len(indices)
        result['sample_cycle_repeat_rms_quantiles']=[float(x) for x in np.quantile(sample[:,6],[0,.5,.9,.99,1])]
        result['sample_buffer_repeat_rms_quantiles']=[float(x) for x in np.quantile(sample[:,7],[0,.5,.9,.99,1])]
        result['sample_fundamental_power_quantiles']=[float(x) for x in np.quantile(sample[:,1],[0,.5,.9,.99,1])]
        if args.import_sql:
            assert result['complete'] and points==manifest['points'],'SQL import requires a completed run'
            import duckdb
            import pyarrow as pa
            database=root/'measurements.duckdb'
            assert not database.exists(),'Refusing to overwrite existing database'
            connection=duckdb.connect(str(database))
            connection.execute("SET threads=8")
            fields=manifest['fields']
            columns=', '.join(['point_id UBIGINT']+[f'{name} FLOAT' for name in fields])
            connection.execute(f'CREATE TABLE measurements ({columns})')
            for first in range(0,points,262144):
                block=records[first:first+262144]
                raw=block.view(np.uint8).reshape(-1,80)
                checksum=np.full(len(block),14695981039346656037,dtype=np.uint64)
                for k in range(72):
                    checksum=(checksum^raw[:,k].astype(np.uint64))*np.uint64(1099511628211)
                assert np.array_equal(checksum,block['checksum']),f'Checksum failure at {first}'
                assert np.all(np.isfinite(block['values']))
                arrays={'point_id':pa.array(block['point'])}
                arrays.update({name:pa.array(block['values'][:,i]) for i,name in enumerate(fields)})
                table=pa.table(arrays);connection.register('incoming',table)
                connection.execute('INSERT INTO measurements SELECT * FROM incoming')
                connection.unregister('incoming')
            table=pa.Table.from_pylist(manifest['frequencies']);connection.register('tones',table)
            connection.execute('CREATE TABLE frequencies AS SELECT * FROM tones')
            n=manifest['grid'];pin=manifest['amplitude']**2/2
            connection.execute(f'''CREATE VIEW responses AS
                SELECT m.*, (point_id//{n**3})::INTEGER AS frequency_index,
                  ((point_id//{n**2})%{n})::INTEGER AS drive_index,
                  ((point_id//{n})%{n})::INTEGER AS resonance_index,
                  (point_id%{n})::INTEGER AS cutoff_index,
                  actual_hz AS input_hz,
                  20*pow(1000.0,(point_id%{n})::DOUBLE/{n-1}) AS cutoff_hz,
                  10*log10(greatest(fundamental_power,{1e-38})/{pin}) AS fundamental_gain_db,
                  10*log10(greatest(total_power,{1e-38})/{pin}) AS total_gain_db
                FROM measurements m JOIN frequencies f ON f."index"=point_id//{n**3}''')
            result['full_grid_counts']=connection.execute('''SELECT count(*),
                count(*) FILTER (WHERE cycle_repeat_rms<0.001),
                count(*) FILTER (WHERE cycle_repeat_rms<0.01),
                count(*) FILTER (WHERE buffer_repeat_rms<0.0001),
                count(*) FILTER (WHERE cycle_repeat_rms>=0.01 AND buffer_repeat_rms<0.0001)
                FROM measurements''').fetchone()
            result['full_grid_count_fields']=['points','cycle_error_below_0p1_percent','cycle_error_below_1_percent',
                'buffer_error_below_0p01_percent','buffer_repeats_but_cycle_error_at_least_1_percent']
            connection.execute('CHECKPOINT');connection.close()
            result['database']=str(database)
    name='dataset-summary.json' if result['complete'] else 'partial-summary.json'
    (root/name).write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()

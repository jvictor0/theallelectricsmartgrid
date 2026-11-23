#!/usr/bin/env python3
"""Import derived per-trial checkpoint measurements into a separate DuckDB."""
import argparse
import json
from pathlib import Path
import duckdb
import numpy as np
import pyarrow as pa


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('database',type=Path)
    parser.add_argument('runs',nargs='+',type=Path)
    args=parser.parse_args()
    con=duckdb.connect(str(args.database))
    con.execute('BEGIN')
    con.execute('DROP TABLE IF EXISTS measurements')
    for ri,root in enumerate(args.runs):
        report=json.loads((root/'report.json').read_text())
        data=np.load(root/'checkpoint-trials.npz')
        values=data['values']; nc,nt,nm,ns,_=values.shape
        assert ns==101
        shape=(nc,nt,nm,ns)
        ids=np.broadcast_to(data['point_ids'][:,None,None,None],shape).ravel()
        times=np.broadcast_to(data['times'][None,:,None,None],shape).ravel()
        modes=np.broadcast_to(data['modes'][None,None,:,None],shape).ravel()
        starts=np.broadcast_to(np.arange(ns)[None,None,None,:],shape).ravel()
        bounds=np.array([[[[cp[str(mode)]['begin_seconds'],cp[str(mode)]['end_seconds']] for mode in data['modes']]
                          for cp in case['checkpoints']] for case in report['cases']])
        arrays={'run_seconds':np.full(len(ids),data['times'][-1]),'point_id':ids,'horizon_seconds':times,
                'mode':modes,'start_index':starts,'is_reset':starts==100,
                'begin_seconds':np.broadcast_to(bounds[...,0,None],shape).ravel(),
                'end_seconds':np.broadcast_to(bounds[...,1,None],shape).ravel()}
        arrays.update({str(name):values[...,i].ravel() for i,name in enumerate(data['metrics'])})
        con.register('incoming',pa.table(arrays))
        con.execute('CREATE TABLE IF NOT EXISTS measurements AS SELECT * FROM incoming LIMIT 0')
        con.execute('INSERT INTO measurements SELECT * FROM incoming')
        if ri==0:
            con.register('case_input',pa.Table.from_pylist([{k:v for k,v in c.items() if k!='checkpoints'} for c in report['cases']]))
            con.execute('CREATE OR REPLACE TABLE cases AS SELECT * FROM case_input')
    con.execute('COMMIT')
    con.execute('CHECKPOINT')
    print(con.execute('SELECT run_seconds,count(*) AS measurements FROM measurements GROUP BY run_seconds ORDER BY run_seconds').fetchall())


if __name__=='__main__':
    main()

#!/usr/bin/env python3
"""Choose deterministic, stratified representatives; not a population estimate."""
import argparse
import json
from pathlib import Path
import duckdb


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('database',type=Path)
    parser.add_argument('output',type=Path)
    args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    con=duckdb.connect(str(args.database),read_only=True)
    manifest,status=con.execute('SELECT manifest,status FROM dataset').fetchone()
    assert status=='complete'
    common='fundamental_trial_cv>.05 AND total_trial_cv<.05'
    groups=[
      ('bass_loud',f'{common} AND fundamental_db>=0 AND cutoff_hz<80',4),
      ('lowmid_loud',f'{common} AND fundamental_db>=0 AND cutoff_hz BETWEEN 80 AND 320',4),
      ('mid_loud',f'{common} AND fundamental_db>=0 AND cutoff_hz BETWEEN 320 AND 1280',4),
      ('upper_loud',f'{common} AND fundamental_db>=0 AND cutoff_hz>=1280',4),
      ('weak_tail',f'{common} AND fundamental_db BETWEEN -40 AND -20 AND fundamental_trial_cv>.25',4),
      ('onset', 'total_trial_cv>.05 AND fundamental_trial_cv>.05 AND fundamental_db>=-20 AND cutoff_hz<80',4),
      ('stable_control','fundamental_db>=0 AND fundamental_trial_cv<.001 AND cutoff_hz BETWEEN 80 AND 3000',2)]
    cases=[]
    used=set()
    for group,condition,count in groups:
        result=con.execute(f'SELECT * FROM responses WHERE {condition} ORDER BY hash(point_id,20261121) LIMIT {count}')
        names=[c[0] for c in result.description]
        values=[dict(zip(names,r)) for r in result.fetchall()]
        assert len(values)==count
        for row in values:
            assert row['point_id'] not in used
            used.add(row['point_id'])
            cases.append({'group':group,'baseline':row})
    for group,point in [('weak_extreme',61887140),('loud_extreme',75993866),('two_levels',89931773),('decaying_control',37537001)]:
        result=con.execute('SELECT * FROM responses WHERE point_id=?',[point])
        row=dict(zip([c[0] for c in result.description],result.fetchone()))
        assert point not in used
        used.add(point)
        cases.append({'group':group,'baseline':row})
    (args.output/'selection.json').write_text(json.dumps({'manifest':json.loads(manifest),'cases':cases,'selection':'Four hash-ordered cases per variable group, two stable controls, four named outliers; not population-representative.'},indent=2)+'\n')
    (args.output/'point-ids.txt').write_text(''.join(str(c['baseline']['point_id'])+'\n' for c in cases))
    print('Selected',len(cases),'configurations')


if __name__=='__main__':
    main()

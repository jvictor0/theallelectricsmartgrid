#!/usr/bin/env python3
"""Locate high finite-time variance in the completed ladder survey (no new runs)."""

import argparse
import json
from pathlib import Path

import duckdb


def rows(con, sql):
    result=con.execute(sql)
    names=[c[0] for c in result.description]
    return [dict(zip(names,row)) for row in result.fetchall()]


def save(path,value):
    path.write_text(json.dumps(value,indent=2,allow_nan=False)+'\n')


def bins(expression,edges):
    return 'CASE '+''.join(f'WHEN {expression}<{edge} THEN {i} ' for i,edge in enumerate(edges))+f'ELSE {len(edges)} END'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('database',type=Path)
    parser.add_argument('output',type=Path)
    args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    con=duckdb.connect(str(args.database),read_only=True)
    con.execute("SET memory_limit='96GB'")
    con.execute('SET threads=16')
    manifest,status=con.execute('SELECT manifest,status FROM dataset').fetchone()
    assert status=='complete'
    manifest=json.loads(manifest)
    assert manifest['sample_rate']==192000 and manifest['grid']==100
    power=manifest['amplitude']**2/2
    # Diagnostic coordinates use binary64 reconstructions of coefficient formulas.
    # Existing measurements/selection are untouched; these are not bit-exact GPU coefficients.
    con.execute("""CREATE TEMP TABLE parameter_coordinates AS
      WITH a AS (
        SELECT d.index AS d_i,r.index AS r_i,c.index AS c_i,d.gain,
          r.requested_resonance,
          2*sin(pi()*c.cutoff_hz/192000)*(sqrt(1+pow(sin(pi()*c.cutoff_hz/192000),2))-sin(pi()*c.cutoff_hz/192000)) AS alpha,
          d.gain/least(1,d.gain*(d.gain*d.gain+27)/(9*d.gain*d.gain+27)) AS slope
        FROM axes d CROSS JOIN axes r CROSS JOIN axes c
      ), b AS (
        SELECT *,alpha/(1-alpha) AS pole_ratio,
          least(requested_resonance,.9/(pow(alpha/(2-alpha)*slope,4)+1e-8)) AS k
        FROM a
      )
      SELECT d_i,r_i,c_i,alpha,slope,k,k*pow(slope,3) AS kappa,
        pow(pole_ratio+2,2) AS critical,
        k*pow(slope,3)/pow(pole_ratio+2,2) AS eta,
        192000/pi()*asin(pole_ratio/2) AS nominal_hz FROM b
    """)
    # Numerically check the reused nominal crossing formula against the delayed loop.
    for alpha,critical,hz in con.execute('SELECT DISTINCT alpha,critical,nominal_hz FROM parameter_coordinates').fetchall():
        import cmath
        z=cmath.exp(-2j*cmath.pi*hz/192000)
        loop=critical*z*(alpha/(1-(1-alpha)*z))**4
        assert abs(loop+1)<1e-10
    con.execute("""CREATE TEMP VIEW features AS
      SELECT r.*,p.kappa,p.eta,p.nominal_hz,p.critical,
        frequency_hz/p.nominal_hz AS tone_ratio,
        log2(frequency_hz/p.nominal_hz) AS octave_gap
      FROM responses r JOIN parameter_coordinates p USING(d_i,r_i,c_i)
    """)
    fields=['count(*) AS points']
    for metric in ('total','fundamental'):
        eligible=f'{metric}_db>=-40'
        high=f'{metric}_trial_cv>.05 AND {eligible}'
        temporal=f'temporal_{metric}_cv_mean'
        fields += [f'count(*) FILTER (WHERE {eligible}) AS {metric}_eligible',
                   f'count(*) FILTER (WHERE {high}) AS {metric}_high_trial',
                   f'count(*) FILTER (WHERE {temporal}>.05 AND {eligible}) AS {metric}_high_temporal',
                   f'count(*) FILTER (WHERE {high} AND {temporal}>.05) AS {metric}_both_high',
                   f'count(*) FILTER (WHERE {high} AND {temporal}<=.01) AS {metric}_high_trial_low_temporal',
                   f'approx_quantile({metric}_trial_cv,[.5,.95]) FILTER (WHERE {eligible}) AS {metric}_trial_cv_quantiles',
                   f'approx_quantile({metric}_db,[.1,.5,.9]) FILTER (WHERE {high}) AS {metric}_high_trial_db_quantiles',
                   f'approx_quantile(late_{metric}_sd/{power},[.5,.95]) FILTER (WHERE {high}) AS {metric}_high_trial_absolute_sd_quantiles']
    fields += ['count(*) FILTER (WHERE fundamental_db>=-40 AND fundamental_trial_cv>.05 AND total_trial_cv<=.05) AS high_fundamental_stable_total']
    aggregate=','.join(fields)
    definitions={
        'eta':('eta',[.9,1,1.05,1.1,1.25,1.5,2,4,8,16]),
        'tone_ratio':('tone_ratio',[.125,.25,.5,.8,1.25,2,4,8,16,32]),
        'cutoff':('cutoff_hz',[80,320,1280,5120]),
        'drive':('drive_knob',[.25,.5,.75,.9]),
        'resonance':('resonance_knob',[.25,.5,.75,.9]),
        'fundamental_db':('fundamental_db',[-40,-20,-10,0,10,20]),
        'total_db':('total_db',[-40,-20,-10,0,10,20]),
    }
    report={'threshold_cv':.05,'minimum_gain_db':-40,'coordinates':'Binary64 reconstructed diagnostic coefficients; eta=k*s^3/kcritical; tone_ratio=f/f_nominal.',
            'overall':rows(con,'SELECT '+aggregate+' FROM features')[0], 'groups':{}}
    for name,(expression,edges) in definitions.items():
        report['groups'][name]={'upper_edges':edges,'rows':rows(con,f'SELECT {bins(expression,edges)} AS bin,{aggregate} FROM features GROUP BY bin ORDER BY bin')}
        print(name,flush=True)
    save(args.output/'regions.json',report)
    eta_expr=bins('eta',definitions['eta'][1])
    map_fields=['count(*) AS points']
    for metric in ('total','fundamental'):
        map_fields += [f'count(*) FILTER (WHERE {metric}_db>=-40) AS {metric}_eligible',
                       f'count(*) FILTER (WHERE {metric}_db>=-40 AND {metric}_trial_cv>.05) AS {metric}_high_trial',
                       f'count(*) FILTER (WHERE {metric}_db>=-40 AND temporal_{metric}_cv_mean>.05) AS {metric}_high_temporal']
    save(args.output/'ratio-eta-map.json',rows(con,f'SELECT {eta_expr} AS eta_bin,floor(octave_gap*2)::INTEGER AS ratio_bin,{",".join(map_fields)} FROM features GROUP BY eta_bin,ratio_bin ORDER BY eta_bin,ratio_bin'))
    save(args.output/'frequency-cutoff-map.json',rows(con,f'SELECT f_i,c_i,{",".join(map_fields)} FROM features GROUP BY f_i,c_i ORDER BY f_i,c_i'))
    detail_fields='point_id,f_i,d_i,r_i,c_i,frequency_hz,cutoff_hz,drive_knob,resonance_knob,kappa,eta,nominal_hz,tone_ratio,total_db,fundamental_db,total_trial_cv,fundamental_trial_cv,temporal_total_cv_mean,temporal_fundamental_cv_mean,fundamental_coherence_mean,total_prefix_drift_mean,fundamental_prefix_drift_mean,late_total_mean,late_total_min,late_total_max,late_fundamental_mean,late_fundamental_min,late_fundamental_max'
    conditions={
        'total':('total_db>=-40 AND total_trial_cv>.05','total_trial_cv'),
        'fundamental':('fundamental_db>=-40 AND fundamental_trial_cv>.05','fundamental_trial_cv'),
        'total_quiet_in_time':('total_db>=-10 AND total_trial_cv>.1 AND temporal_total_cv_mean<.01','total_trial_cv'),
        'fundamental_loud_stable_total':('fundamental_db>=0 AND fundamental_trial_cv>.25 AND total_trial_cv<.01 AND cutoff_hz BETWEEN 80 AND 2000','fundamental_trial_cv')}
    examples={}
    for name,(condition,order) in conditions.items():
        examples[name]=rows(con,f'SELECT {detail_fields} FROM features WHERE {condition} ORDER BY {order} DESC LIMIT 20')
    save(args.output/'examples.json',examples)
    ids=[str(values[0]['point_id']) for values in examples.values() if values]
    window_fields=','.join(f'window_{i:02}_{kind}' for i in range(20) for kind in ('total_mean','total_sd','fundamental_mean','fundamental_sd'))
    save(args.output/'example-windows.json',rows(con,f'SELECT {detail_fields},{window_fields} FROM features WHERE point_id IN ({",".join(ids)})'))
    # Conditional distributions distinguish where bad points live from the risk in a region.
    subsets={
        'total_high':'total_db>=-40 AND total_trial_cv>.05',
        'fundamental_high':'fundamental_db>=-40 AND fundamental_trial_cv>.05',
        'total_high_loud':'total_db>=0 AND total_trial_cv>.05',
        'fundamental_high_loud':'fundamental_db>=0 AND fundamental_trial_cv>.05',
        'total_stationary_spread':'total_db>=-40 AND total_trial_cv>.05 AND temporal_total_cv_mean<=.01'}
    distributions={}
    for name,condition in subsets.items():
        distributions[name]=rows(con,'SELECT count(*) AS points,'+','.join(f'approx_quantile({field},[.05,.25,.5,.75,.95]) AS {field}' for field in ('eta','tone_ratio','cutoff_hz','frequency_hz','drive_knob','resonance_knob','total_db','fundamental_db','total_trial_cv','fundamental_trial_cv','temporal_total_cv_mean','temporal_fundamental_cv_mean','fundamental_coherence_mean'))+' FROM features WHERE '+condition)[0]
    save(args.output/'bad-point-distributions.json',distributions)
    stationary=subsets['total_stationary_spread']
    diagnostics=rows(con,f"""SELECT
      count(*) FILTER (WHERE {stationary}) AS stationary_spread_points,
      count(*) FILTER (WHERE {stationary} AND tone_ratio BETWEEN .8 AND 1.2) AS stationary_near_nominal,
      count(*) FILTER (WHERE {stationary} AND tone_ratio BETWEEN 2.5 AND 3.5) AS stationary_near_third,
      approx_quantile(pow(late_total_sd::DOUBLE,2)/nullif((late_total_max::DOUBLE-late_total_mean)*(late_total_mean::DOUBLE-late_total_min),0),[.05,.5,.95])
        FILTER (WHERE {stationary}) AS stationary_two_endpoint_variance_ratio,
      approx_quantile(late_total_mean::DOUBLE/nullif(full_total_mean,0),[.05,.5,.95])
        FILTER (WHERE total_db>=-40 AND total_trial_cv>.05) AS total_high_late_to_full_power,
      count(*) FILTER (WHERE total_db>=-40 AND total_trial_cv>.05 AND late_total_mean < .1*full_total_mean) AS total_high_late_below_tenth_full
      FROM features""")[0]
    save(args.output/'diagnostics.json',diagnostics)
    print('complete',flush=True)


if __name__=='__main__':
    main()

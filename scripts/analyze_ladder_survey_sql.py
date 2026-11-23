#!/usr/bin/env python3
"""Full-grid consistency and local interpolation diagnostics, without resimulation."""

import argparse
import json
from pathlib import Path
import time

from import_ladder_survey_sql import connect


def rows(connection, query):
    result = connection.execute(query)
    names = [c[0] for c in result.description]
    return [dict(zip(names,row)) for row in result.fetchall()]


def save(path, value):
    path.write_text(json.dumps(value,indent=2,allow_nan=False)+'\n')


def run(database, output):
    output.mkdir(parents=True, exist_ok=True)
    con = connect(database)
    manifest, status = con.execute('SELECT manifest,status FROM dataset').fetchone()
    assert status == 'complete'
    manifest = json.loads(manifest)
    power = manifest['amplitude']**2/2
    started = time.monotonic()
    report = dict(input_power=power,quantiles=[.5,.9,.95,.99,.999],
                  db_eligibility='All involved mean powers >= input power * 1e-4 (-40 dB gain)',
                  note='Finite-time, finite-grid evidence; per-start SD is population SD. Quantiles are approximate.')
    # Keep reusable summary tables small; raw points and the neighbor views stay unchanged.
    fields = ['count(*) AS points']
    for metric in ('total','fundamental'):
        eligible = f'late_{metric}_mean >= {power*1e-4}'
        fields += [f'count(*) FILTER (WHERE {eligible}) AS {metric}_eligible_points']
        for short,expression in [('trial_cv',f'{metric}_trial_cv'),
                                 ('temporal_cv',f'temporal_{metric}_cv_mean'),
                                 ('trial_range_db',f'10*log10(late_{metric}_max/nullif(late_{metric}_min,0))')]:
            fields += [f'approx_quantile({expression},[.5,.9,.95,.99,.999]) FILTER (WHERE {eligible}) AS {metric}_{short}_quantiles']
        for threshold in (.01,.05,.1):
            fields += [f'avg(({metric}_trial_cv<={threshold})::INTEGER) FILTER (WHERE {eligible}) AS {metric}_trial_cv_le_{str(threshold).replace(".","_")}']
        fields += [f'approx_quantile(late_{metric}_sd/{power},[.5,.9,.95,.99,.999]) AS {metric}_absolute_sd_quantiles',
                   f'avg((temporal_{metric}_cv_mean<=.05)::INTEGER) FILTER (WHERE {eligible}) AS {metric}_temporal_cv_le_0_05',
                   f'approx_quantile({metric}_prefix_drift_mean,[.5,.9,.95,.99,.999]) AS {metric}_prefix_drift_input_power_quantiles']
    fields += [f'approx_quantile(fundamental_coherence_mean,[.001,.01,.05,.1,.5]) FILTER (WHERE late_fundamental_mean>={power*1e-4}) AS coherence_lower_quantiles']
    summary = []
    for region, condition in [('all','true'),('drive_and_resonance_at_most_80_percent','drive_knob<=.8 AND resonance_knob<=.8')]:
        result = rows(con, 'SELECT '+','.join(fields)+' FROM responses WHERE '+condition)[0]
        result['region'] = region
        summary.append(result)
        print(json.dumps(dict(stage='consistency',region=region,elapsed=time.monotonic()-started)),flush=True)
    report['consistency'] = summary
    save(output/'summary.json',report)
    con.execute(f"""CREATE OR REPLACE TABLE consistency_map AS
      SELECT d_i,r_i,count(*) AS points,
        count(*) FILTER (WHERE late_total_mean>={power*1e-4}) AS total_eligible,
        count(*) FILTER (WHERE late_fundamental_mean>={power*1e-4}) AS fundamental_eligible,
        avg((total_trial_cv>.05)::INTEGER) FILTER (WHERE late_total_mean>={power*1e-4}) AS total_fraction_trial_cv_gt_5pct,
        avg((fundamental_trial_cv>.05)::INTEGER) FILTER (WHERE late_fundamental_mean>={power*1e-4}) AS fundamental_fraction_trial_cv_gt_5pct,
        avg((temporal_total_cv_mean>.05)::INTEGER) FILTER (WHERE late_total_mean>={power*1e-4}) AS total_fraction_temporal_cv_gt_5pct,
        avg((temporal_fundamental_cv_mean>.05)::INTEGER) FILTER (WHERE late_fundamental_mean>={power*1e-4}) AS fundamental_fraction_temporal_cv_gt_5pct
      FROM responses GROUP BY d_i,r_i""")
    save(output/'consistency-map.json', rows(con,'SELECT * FROM consistency_map ORDER BY d_i,r_i'))
    continuity = []
    for axis in ('frequency','drive','resonance','cutoff'):
        for kind in ('neighbors','interpolation'):
            measure = 'change' if kind=='neighbors' else 'interpolation_error'
            fields = ['count(*) AS points']
            for metric in ('total','fundamental'):
                db = f'{metric}_{measure}_db'
                se = f'{metric}_'+('change_mc_se' if kind=='neighbors' else 'interpolation_mc_se')
                fields += [f'count({db}) AS {metric}_eligible',
                           f'approx_quantile({db},[.5,.9,.95,.99,.999]) AS {metric}_db_quantiles',
                           f'avg(({db}<=.1)::INTEGER) AS {metric}_fraction_le_0_1db',
                           f'avg(({db}<=1)::INTEGER) AS {metric}_fraction_le_1db',
                           f'avg((CASE WHEN {db} IS NOT NULL THEN {db}>1 AND {metric}_{measure}_input_power>3*{se} END)::INTEGER) AS {metric}_fraction_gt_1db_and_3se',
                           f'approx_quantile({se},[.5,.9,.95,.99,.999]) AS {metric}_mc_se_input_power_quantiles',
                           f'approx_quantile({metric}_{measure}_input_power,[.5,.9,.95,.99,.999]) AS {metric}_input_power_quantiles',
                           f'count({db}) FILTER (WHERE {metric}_max_trial_cv<=.05) AS {metric}_consistent_eligible',
                           f'approx_quantile({db},[.5,.9,.95,.99,.999]) FILTER (WHERE {metric}_max_trial_cv<=.05) AS {metric}_consistent_db_quantiles']
            result = rows(con,'SELECT '+','.join(fields)+f' FROM {kind}_{axis}')[0]
            result.update(axis=axis,kind=kind)
            continuity.append(result)
            report['continuity'] = continuity
            save(output/'summary.json',report)
            print(json.dumps(dict(stage=kind,axis=axis,elapsed=time.monotonic()-started)),flush=True)
        for metric in ('total','fundamental'):
            outliers = rows(con,f'SELECT * FROM interpolation_{axis} WHERE {metric}_max_trial_cv<=.05 ORDER BY {metric}_interpolation_error_db DESC NULLS LAST LIMIT 20')
            save(output/f'outliers-{axis}-{metric}.json',outliers)
    # Save representative frequency sweeps with all cross-start spread and temporal information.
    triples = [(60,80,50),(80,40,50),(80,80,50),(99,99,50)]
    for metric in ('total','fundamental'):
        outliers = json.loads((output/f'outliers-frequency-{metric}.json').read_text())
        if outliers:
            row=outliers[0]
            triples.append((row['d_i'],row['r_i'],row['c_i']))
    sweeps = []
    for d,r,c in sorted(set(triples)):
        result = rows(con,f"""SELECT point_id,frequency_hz,cutoff_hz,drive_knob,resonance_knob,drive_gain,
          total_db,fundamental_db,total_trial_cv,fundamental_trial_cv,
          10*log10(nullif(late_total_min/{power},0)) AS total_min_db,
          10*log10(nullif(late_total_max/{power},0)) AS total_max_db,
          10*log10(nullif(late_fundamental_min/{power},0)) AS fundamental_min_db,
          10*log10(nullif(late_fundamental_max/{power},0)) AS fundamental_max_db,
          temporal_total_cv_mean,temporal_fundamental_cv_mean,
          total_prefix_drift_mean,fundamental_prefix_drift_mean
          FROM responses WHERE d_i={d} AND r_i={r} AND c_i={c} ORDER BY f_i""")
        if result:
            sweeps.append(dict(indices=[d,r,c],points=result))
    save(output/'sweeps.json',sweeps)
    report['elapsed_seconds']=time.monotonic()-started
    save(output/'summary.json',report)
    con.execute('CHECKPOINT')
    con.close()
    print(json.dumps(dict(stage='complete',elapsed=report['elapsed_seconds'])),flush=True)


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('database',type=Path)
    parser.add_argument('output',type=Path)
    args=parser.parse_args()
    run(args.database,args.output)

"""Report exact-cycle scores with unresolved records explicitly separated."""
from fit_display import here,np,json,score,curve_mse,reference
z=np.load(here/'extended.npz');m=dict(np.load(here/'display-model.npz'))
_,edges=reference()
rows=[]
for i,p in enumerate(z['controls']):
    selected=z['k']==i;ok=selected&(z['period']>0)
    def curve(k,d):return 10*np.log10(z[k][ok].mean(0)/z[d][ok].mean(0))
    v=curve('phase','dry');late=curve('late','drylate');cold=curve('cold','drylate')
    rows.append(dict(knobs=p.tolist(),complete=int(ok.sum()),total=int(selected.sum()),
        longest=int(z['period'][ok].max()),max_entry=int(z['start'][ok].max()),rms_complete_only=float(np.sqrt(score(m,(p[None],v[None]),edges)[0])),
        startup_same_phases=float(np.sqrt(curve_mse(late[None],cold[None],edges)[0]))))
(here/'cycle-summary.json').write_text(json.dumps(rows,indent=2))
print(rows[:6])

"""Run and audit an exported Lean mono processor. See README.md."""
import argparse
from contextlib import contextmanager
import fcntl
from fractions import Fraction
import hashlib
import itertools
import json
import math
import os
import platform
from pathlib import Path
import secrets
import shutil
import struct
import subprocess
import sys
from tempfile import TemporaryDirectory

import numpy as np

from experiment import Backend, Design, HERE, ROOT, db
import spectral_fit


def digest(path):
    with path.open("rb") as file:
        return hashlib.file_digest(file, "sha256").hexdigest()


def atomic(path, data):
    tmp = path.with_name(path.name + ".tmp")
    with tmp.open("wb") as file:
        file.write(data)
        file.flush()
        os.fsync(file.fileno())
    tmp.replace(path)


def write_json(path, data):
    atomic(path, (json.dumps(data, sort_keys=True, indent=2, allow_nan=False)+"\n").encode())


def read(path):
    return json.loads(path.read_text())


@contextmanager
def lock(out):
    out.mkdir(parents=True,exist_ok=True)
    with (out/".lock").open("a") as f:
        fcntl.flock(f,fcntl.LOCK_EX | fcntl.LOCK_NB)
        yield


def describe(d):
    risk = Fraction(d.r["riskPPM"],1000000*d.tests)
    minimum = {}
    for q in d.r["coveragePPM"]:
        minimum[str(q)] = math.ceil(-math.log(float(risk))/math.log((1000000+q)/(2*q)))
    return dict(trainingTrials=math.prod(d.knots)*len(d.stimuli)*d.r["training"],
                auditTrials=len(d.active)*d.r["audit"], auditCells=len(d.active),
                frequencyRelativeTo=d.r.get("frequencyRelativeTo"),
                recordsPerTrial=d.r["records"], packedBytes=d.nbytes, packedKnots=d.storage_shape,
                perClaimRisk=str(risk), trialsPerCellIfAllPass=minimum)


GENERATED = ("recipe.json", "processor.json", "processor.cpp", "processor.metal")
LEAN_SCRATCH = ROOT/"build/lean/scratch"
CHECKER = ROOT/"build/lean/artifacts/bin/survey_check"


def studio_lake(arguments, outputs):
    command = [sys.executable, "-B", str(ROOT/"scripts/lean_studio.py")]
    for path in outputs:
        command.extend(("--return-output", str(path)))
    subprocess.run([*command, "lake", "--", *map(str, arguments)], check=True, stdout=sys.stderr)


def export(source, directory):
    # Work is disposable. Never let an unrelated Lean file pick up the last
    # processor that happened to export into this directory.
    directory.mkdir(parents=True, exist_ok=True)
    for name in GENERATED:
        (directory / name).unlink(missing_ok=True)
    LEAN_SCRATCH.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(prefix="survey-export-", dir=LEAN_SCRATCH) as scratch:
        staged = Path(scratch)
        outputs = [staged/name for name in GENERATED]
        studio_lake(["lean", source.resolve(), "--", f"-Ddsp.survey.exportDir={staged.relative_to(ROOT)}"], outputs)
        if not all(path.is_file() for path in outputs):
            raise ValueError("The Lean source must export a processor and recipe with #survey_processor")
        for path in outputs:
            shutil.copyfile(path, directory/path.name)


def load_prepared(plan, directory):
    record = read(plan/"export.json")
    directory.mkdir(parents=True, exist_ok=True)
    for name in GENERATED:
        source = plan/"work"/name
        if digest(source) != record["files"][name]:
            raise ValueError(f"Prepared Lean export differs from plan: {name}")
        if source.resolve() != (directory/name).resolve():
            shutil.copyfile(source, directory/name)
        if digest(directory/name) != record["files"][name]:
            raise ValueError(f"Prepared Lean export changed while copying: {name}")
    return record


def signature(directory, device):
    files = [directory/n for n in GENERATED]
    files += [HERE/n for n in ("experiment.py","run.py","Table.cpp","Metal.mm")]
    files += [ROOT/"private/src/SurveyResponseTable.hpp"]
    files += [HERE/n for n in ("Spectrum.metal","Spectrum.mm","spectrum.py","spectral_fit.py")]
    files += [ROOT/"formal/DSP/Survey"/n for n in ("Bernoulli.lean","Certificate.lean","Regional.lean","Audit.lean")]
    files += [ROOT/"formal/SurveyCheck.lean"]
    return dict(files={p.name:digest(p) for p in files}, device=device, numpy=np.__version__)


def packed_table(d, curves):
    if d.r.get("spectralMesh") is not None:
        return spectral_fit.fit(d, curves)
    # Resample the complete training lattice at the affordable knot counts.
    table = curves
    for axis,size in enumerate(d.storage_shape):
        if size == d.knots[axis]:
            continue
        values = np.moveaxis(table,axis,0)
        indices = np.linspace(0,len(values)-1,size)
        lo = np.minimum(indices.astype(int),len(values)-2)
        if axis == d.relative_axis:
            a = d.axes[axis]
            spacing = np.geomspace if a["logScale"] else np.linspace
            source = np.log(spacing(a["low"], a["high"], len(values)))
            target = np.log(spacing(a["low"], a["high"], size))
            log_hz = np.log(d.freq)
            bands = np.arange(len(log_hz))
            def shifted(index, center):
                position = np.interp(log_hz + (source[index] - center), log_hz, bands)
                lower = np.minimum(position.astype(int), len(bands) - 2)
                left = values[index][..., lower]
                return left + (position - lower) * (values[index][..., lower+1] - left)
            samples = []
            for i, left, center in zip(indices, lo, target):
                lower = shifted(left, center)
                upper = shifted(left+1, center)
                samples.append(lower + (i-left)*(upper-lower))
            table = np.moveaxis(np.stack(samples), 0, axis)
        else:
            mix = (indices-lo).reshape((-1,)+(1,)*(values.ndim-1))
            table = np.moveaxis(values[lo]*(1-mix)+values[lo+1]*mix,0,axis)
    quant = np.rint(table*64)
    if not np.isfinite(quant).all() or np.any(quant < -32768) or np.any(quant > 32767):
        raise ValueError("Response exceeds the table's quantization range")
    version = b"DSR1" if d.relative_axis is None else b"DSR2"
    header = struct.pack("<4sIId",version,len(d.axes),len(d.freq),d.r["floorDb"])
    axes = b"".join(struct.pack("<IIdd",k,int(a["logScale"]) | (2 if i == d.relative_axis else 0),
                                a["low"],a["high"])
                    for i,(k,a) in enumerate(zip(d.storage_shape,d.axes)))
    result = header+axes+d.freq.astype("<f8").tobytes()+quant.astype("<i2").tobytes()
    if len(result) != d.nbytes:
        raise ValueError("Packed artifact size mismatch")
    return result


def save_npz(path, **arrays):
    tmp = path.with_suffix(".tmp")
    with tmp.open("wb") as f:
        np.savez_compressed(f,**arrays)
        f.flush()
        os.fsync(f.fileno())
    tmp.replace(path)


def train_tiles(d, backend, data, seed, replay):
    n = math.prod(d.knots)
    repeat = d.r["training"]
    curves = np.empty((n,len(d.freq)),dtype=np.float32)
    # Spread the common-noise training banks across the control box.
    order = np.random.default_rng(np.random.SeedSequence([seed, n, 1])).permutation(n)
    for start in range(0,n,d.dual_rate.tile):
        stop = min(n,start+d.dual_rate.tile)
        ix = order[start:stop]
        u = np.array(np.unravel_index(ix,d.knots)).T / (np.array(d.knots)-1)
        configs = [d.config(point) for point in u]
        path = data/f"tile-{start}.npz"
        if not path.exists():
            if replay:
                raise ValueError(f"Missing training tile {start}")
            powers = np.empty((len(ix),len(d.stimuli),repeat,2,len(d.freq)))
            for s in range(len(d.stimuli)):
                for rep in range(repeat):
                    rng = np.random.default_rng(np.random.SeedSequence([seed,start,s,rep]))
                    powers[:,s,rep] = d.measure_batch(backend,configs,s,[rng])
            if not np.isfinite(powers).all():
                raise ValueError(f"Invalid training measurement in tile {start}")
            save_npz(path,vertices=ix,powers=powers)
        with np.load(path) as z:
            if not np.array_equal(z["vertices"],ix):
                raise ValueError(f"Unexpected training vertices in tile {start}")
            values = z["powers"]
        if values.shape != (len(ix),len(d.stimuli),repeat,2,len(d.freq)):
            raise ValueError(f"Invalid training tile {start}")
        if len(d.stimuli) == 1:
            wet, dry = values[:,0].mean(axis=1).transpose(1,0,2)
            curves[ix] = db((wet,dry))
        else:
            for j,point in enumerate(u):
                weights = stimulus_weights(d,point)
                curves[ix[j]] = db(np.average(values[j].mean(1),axis=0,weights=weights))
    return packed_table(d,curves.reshape(d.knots+(len(d.freq),)))


def stimulus_weights(d,u):
    cond = tuple(min(a["cells"]-1,round(x*(a["cells"]-1)) if a["points"] else int(x*a["cells"]))
                 for x,a in zip(u,d.axes))
    first = np.ravel_multi_index(cond+(0,),d.shape)
    weights = d.weights[first:first+len(d.stimuli)]
    return weights if sum(weights) else [s["mass"] for s in d.stimuli]


def train(d, backend, out, seed, replay):
    data = out/"training"
    data.mkdir(exist_ok=True)
    if d.dual_rate:
        return train_tiles(d,backend,data,seed,replay)
    curves = np.empty(d.knots+(len(d.freq),))
    for vertex,indices in enumerate(np.ndindex(d.knots)):
        path = data/f"{vertex}.npz"
        u = [j/(n-1) for j,n in zip(indices,d.knots)]
        weights = stimulus_weights(d,u)
        if not path.exists():
            if replay:
                raise ValueError(f"Missing training vertex {vertex}")
            observations = []
            for s in range(len(d.stimuli)):
                for rep in range(d.r["training"]):
                    rng = np.random.default_rng(np.random.SeedSequence([seed,vertex,s,rep]))
                    p, = d.measure_batch(backend, [d.config(u)], s, [rng])
                    if not np.isfinite(p).all():
                        raise ValueError(f"Invalid training observation at vertex {vertex}")
                    observations.append(p)
            save_npz(path,powers=np.array(observations))
        with np.load(path) as z:
            p = z["powers"].reshape(len(d.stimuli),d.r["training"],2,len(d.freq))
            curves[indices] = db(np.average(p.mean(1),axis=0,weights=weights))
    return packed_table(d,curves)


def audit(d, backend, out, packed, replay):
    data = out/"audit"
    data.mkdir(exist_ok=True)
    seeds_path = out/"audit-seeds.bin"
    size = len(d.active)*d.r["audit"]*16
    if not seeds_path.exists():
        if replay or any(data.glob("*.npz")):
            raise ValueError("Missing audit seed schedule")
        atomic(seeds_path,secrets.token_bytes(size))
    seeds = seeds_path.read_bytes()
    if len(seeds) != size:
        raise ValueError("Invalid seed schedule")
    rows = []
    for slot,cell in enumerate(d.active):
        path = data/f"{cell}.npz"
        if not path.exists():
            if replay:
                raise ValueError(f"Missing audit cell {cell}")
            configurations, powers = [], []
            stimulus = int(np.unravel_index(cell,d.shape)[-1])
            batch = d.batch_size(stimulus)
            for begin in range(0, d.r["audit"], batch):
                generators, configs = [], []
                for i in range(begin, min(begin + batch, d.r["audit"])):
                    offset = 16 * (slot * d.r["audit"] + i)
                    rng = np.random.default_rng(int.from_bytes(seeds[offset:offset+16], "little"))
                    config, _ = d.draw(cell, rng)
                    configs.append(config)
                    generators.append(rng)
                powers.extend(d.measure_batch(backend, configs, stimulus, generators))
                configurations.extend(configs)
            save_npz(path,config=np.array(configurations),powers=np.array(powers))
        with np.load(path) as z:
            scores = []
            if z["powers"].shape != (d.r["audit"],2,len(d.freq)) or z["config"].shape != (d.r["audit"],len(d.axes)):
                raise ValueError(f"Bad cell archive {cell}")
            for config,p in zip(z["config"],z["powers"]):
                score = d.score(backend.knots(packed,config,len(d.freq)),db(p)) if np.isfinite(p).all() else None
                scores.append(score)
            rows.append(dict(cell=cell,indices=list(map(int,np.unravel_index(cell,d.shape))),
                             mass=d.weights[cell],scores=scores))
    return rows


def certificate_decisions(checks, prepared):
    if prepared:
        result = subprocess.run([CHECKER], input=json.dumps(checks), capture_output=True, text=True, check=True)
        return json.loads(result.stdout)

    LEAN_SCRATCH.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(prefix="survey-certify-", dir=LEAN_SCRATCH) as scratch:
        request = Path(scratch)/"checks.json"
        result = Path(scratch)/"decisions.json"
        write_json(request, checks)
        studio_lake(["exe", "survey_check", request, result], [result])
        return read(result)


def certify(d, rows, prepared):
    risk = Fraction(d.r["riskPPM"],1000000*d.tests)
    checks = []
    for row in rows:
        row["hits"] = [sum(x is not None and x <= t for x in row["scores"]) for t in d.r["thresholdsDb"]]
        for hits,q in itertools.product(row["hits"],d.r["coveragePPM"]):
            checks.append([q,1000000,risk.numerator,risk.denominator,d.r["audit"],hits])
    decisions = certificate_decisions(checks, prepared)
    if not isinstance(decisions, list) or len(decisions) != len(checks) or any(type(x) is not bool for x in decisions):
        raise ValueError("Lean survey checker returned invalid decisions")
    flags = iter(decisions)
    for row in rows:
        row["lowerPPM"] = [max([0]+[q for q in d.r["coveragePPM"] if next(flags)]) for _ in row["hits"]]
    return dict(thresholdsDb=d.r["thresholdsDb"],riskPPM=d.r["riskPPM"],cells=rows)


def inventory(out):
    files = [out/n for n in ("manifest.json","runtime.json","table.bin","audit-seeds.bin","report.json")]
    files += sorted((out/"generated").glob("*"))
    files += sorted((out/"training").glob("*.npz"))+sorted((out/"audit").glob("*.npz"))
    return {str(p.relative_to(out)):digest(p) for p in files}


def execute(args):
    out = args.out.resolve()
    with lock(out):
        generated = out/"work"
        if args.command == "run-prepared":
            export_record = load_prepared(args.source.resolve(), generated)
        else:
            export(args.source, generated)
            export_record = dict(sourceSHA256=digest(args.source),
                                 files={n:digest(generated/n) for n in GENERATED})
        d = Design(generated)
        plan = describe(d)
        print(json.dumps(plan,indent=2),flush=True)
        if args.command == "plan":
            write_json(out/"export.json", export_record)
            return
        replay = args.command == "verify" or (out/"hashes.json").exists()
        device = args.device or (read(out/"manifest.json")["inputs"]["device"] if (out/"manifest.json").exists() else "metal")
        inputs = signature(generated,device)
        manifest = out/"manifest.json"
        if manifest.exists():
            frozen = read(manifest)
            if args.command == "run-prepared" and "export" in frozen and frozen["export"] != export_record:
                raise ValueError("Lean source or prepared export differs from the frozen experiment")
            if replay:
                # Replay consumes archived powers and independently checks every
                # packed byte and reported score. It can use a newer driver.
                if any(frozen["inputs"]["files"][name] != inputs["files"][name] for name in GENERATED):
                    raise ValueError("Current Lean export differs from the frozen experiment")
            elif frozen["inputs"] != inputs:
                raise ValueError("Recipe, implementation, or backend changed; use a distinct run directory")
        elif replay:
            raise ValueError("Missing run manifest")
        else:
            frozen = dict(inputs=inputs,trainingSeed=secrets.randbits(128),plan=plan,export=export_record)
            shutil.copytree(generated, out/"generated", dirs_exist_ok=True, ignore=shutil.ignore_patterns("*.dylib"))
            write_json(manifest,frozen)
        if replay and inventory(out) != read(out/"hashes.json"):
            raise ValueError("Artifact digest mismatch")
        for name in GENERATED:
            if digest(out/"generated"/name) != inputs["files"][name]:
                raise ValueError(f"Frozen source mismatch: {name}")
        backend = Backend(generated, d, device == "metal" and not replay)
        try:
            if not replay:
                runtime = out/"runtime.json"
                identity = dict(hostLibrary=digest(generated/"processor.dylib"), system=platform.platform(),
                                machine=platform.machine(), device=device)
                if runtime.exists() and read(runtime) != identity:
                    raise ValueError("Runtime binary or platform changed during the run")
                if not runtime.exists():
                    write_json(runtime, identity)
            table = train(d,backend,out,frozen["trainingSeed"],replay)
            artifact = out/"table.bin"
            if artifact.exists():
                if artifact.read_bytes() != table:
                    raise ValueError("Rebuilt table differs from the frozen bytes")
            else:
                if replay or (out/"audit-seeds.bin").exists():
                    raise ValueError("Missing frozen artifact")
                atomic(artifact,table)
            report = certify(d, audit(d, backend, out, table, replay), args.command == "run-prepared")
            if replay:
                if read(out/"report.json") != report:
                    raise ValueError("Audit replay differs")
                print("Verified packed table, scores, and regional endpoints.")
            else:
                write_json(out/"report.json",report)
                write_json(out/"hashes.json",inventory(out))
        finally:
            backend.close()


def query(out,cells,axis_regions,stimulus):
    hashes = read(out/"hashes.json")
    for name in ("report.json", "generated/recipe.json", "table.bin"):
        if digest(out/name) != hashes[name]:
            raise ValueError(f"Published artifact hash differs: {name}")
    report = read(out/"report.json")
    region = set(cells) if cells else {r["cell"] for r in report["cells"]}
    rows = [r for r in report["cells"] if r["cell"] in region]
    if len(rows) != len(region) or not rows:
        raise ValueError("Region must be a nonempty union of positive-mass audit cells")
    recipe = read(out/"generated"/"recipe.json")
    axes = {a["name"]: (i,a["cells"]) for i,a in enumerate(recipe["axes"])}
    for spec in axis_regions:
        name, value = spec.split("=", 1)
        i, n = axes[name]
        left, right = map(int,value.split(":"))
        if not 0 <= left < right <= n:
            raise ValueError(f"Invalid cell interval {spec}")
        rows = [r for r in rows if left <= r["indices"][i] < right]
    if stimulus is not None:
        names = [s["name"] for s in recipe["stimuli"]]
        position = names.index(stimulus)
        rows = [r for r in rows if r["indices"][-1] == position]
    if not rows:
        raise ValueError("Region has zero mass")
    mass = sum(r["mass"] for r in rows)
    bounds = []
    for j,t in enumerate(report["thresholdsDb"]):
        estimate = sum(Fraction(r["mass"]*r["hits"][j],len(r["scores"])) for r in rows)/mass
        lower = Fraction(sum(r["mass"]*r["lowerPPM"][j] for r in rows),mass*1000000)
        bounds.append(dict(errorDb=t,estimatedCoverage=float(estimate),lowerCoverage=str(lower)))
    quantiles = {str(q): min((b["errorDb"] for b in bounds if Fraction(b["lowerCoverage"]) >= Fraction(q,1000000)),
                             default=None) for q in recipe["coveragePPM"]}
    return dict(cells=[r["cell"] for r in rows], quantileUpperDbByPPM=quantiles, trials=sum(len(r["scores"]) for r in rows),
                confidence=1-report["riskPPM"]/1000000,bounds=bounds,tableSHA256=hashes["table.bin"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command",required=True)
    for name in ("plan","run","run-prepared","verify"):
        p = sub.add_parser(name)
        p.add_argument("source",type=Path)
        p.add_argument("out",type=Path)
        p.add_argument("--device",choices=("cpu","metal"))
    p = sub.add_parser("query")
    p.add_argument("out",type=Path)
    p.add_argument("--cells",type=int,nargs="+")
    p.add_argument("--axis", action="append", default=[], metavar="NAME=START:STOP")
    p.add_argument("--stimulus")
    args = parser.parse_args()
    if args.command == "query":
        print(json.dumps(query(args.out,args.cells,args.axis,args.stimulus),indent=2))
    else:
        execute(args)


if __name__ == "__main__":
    main()

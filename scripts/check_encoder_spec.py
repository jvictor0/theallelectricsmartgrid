#!/usr/bin/env python3
"""Check source correspondence and render the encoder contract index.

Hashes deliberately require review after any covered C++ file changes. They are
a drift alarm, not a proof that Lean definitions refine C++ semantics.
"""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SPEC = ROOT / "formal/Encoder"


def source_locations(manifest, sources):
    locations = {}
    for key, anchors in manifest["anchors"].items():
        if not anchors:
            raise ValueError(f"empty source anchor: {key}")
        locations[key] = []
        for anchor in anchors:
            source = sources[anchor["path"]]
            needle = anchor["symbol"]
            if source.count(needle) != 1:
                raise ValueError(f"source anchor is missing or ambiguous: {key}: {needle}")
            line = source[:source.index(needle)].count("\n") + 1
            locations[key].append((anchor["path"], line, needle.strip()))
    return locations


def check_references(tag, locations):
    references = set()
    for path in SPEC.glob("*.lean"):
        for key in re.findall(r"@" + tag + r"\s+([a-z_]+):", path.read_text()):
            if key not in locations:
                raise ValueError(f"unknown {tag} source anchor {key} in {path.name}")
            references.add(key)
    unused = locations.keys() - references
    if unused:
        raise ValueError(f"{tag} source anchors have no model reference: {sorted(unused)}")


def body(source, symbol):
    """Read the anchored C++ body, skipping braces in comments/string literals."""
    if source.count(symbol) != 1:
        raise ValueError(f"shared body is missing or ambiguous: {symbol}")
    start = source.index(symbol)
    tokens = re.finditer(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]',
                         source[start:], re.DOTALL)
    depth = 0
    for token in tokens:
        if token[0] == "{":
            depth += 1
        elif token[0] == "}":
            depth -= 1
            if depth == 0:
                return source[start:start + token.end()]
    raise ValueError(f"unterminated shared C++ body: {symbol}")


def check_head(contracts, current_sources):
    head = json.loads((SPEC / "head.json").read_text())
    revision = head["revision"]
    if not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise ValueError("HEAD model must pin a complete Git commit hash")
    if f'"{revision}"' not in (SPEC / "Head.lean").read_text():
        raise ValueError("HEAD revision differs between Lean and source manifest")
    sources = {}
    for path, expected in head["files"].items():
        data = subprocess.check_output(["git", "show", f"{revision}:{path}"], cwd=ROOT)
        if hashlib.sha256(data).hexdigest() != expected:
            raise ValueError(f"pinned HEAD source hash mismatch: {path}")
        sources[path] = data.decode()
        snapshot = ROOT / "build/lean/head-source" / path
        snapshot.parent.mkdir(parents=True, exist_ok=True)
        snapshot.write_bytes(data)
    locations = source_locations(head, sources)
    check_references("head", locations)
    for shared in head["shared_bodies"]:
        path, symbol = shared["path"], shared["symbol"]
        if body(sources[path], symbol) != body(current_sources[path], symbol):
            raise ValueError(f"shared C++ operation changed between HEAD and working tree: {path}: {symbol}")
    ids = [c["id"] for c in head["contracts"]]
    if len(ids) != len(set(ids)) or set(ids) != {c["id"] for c in contracts}:
        raise ValueError("HEAD comparison must classify every EARS contract exactly once")
    report = ["# Pinned HEAD comparison", "", f"Revision: `{revision}`.", "",
              "Shared proofs refer to common Lean definitions used by both models, with reviewed C++ correspondence.", "",
              "| Contract | HEAD status | Reason |", "| --- | --- | --- |"]
    for row in head["contracts"]:
        if row["status"] not in {"shared", "proved", "comparison", "analogue", "not-applicable"}:
            raise ValueError(f"unknown HEAD status: {row['id']}")
        if not row["reason"] or (row["status"] != "not-applicable" and (not row["theorems"] or not row["anchors"])):
            raise ValueError(f"incomplete HEAD contract: {row['id']}")
        for key in row["anchors"]:
            if key not in locations:
                raise ValueError(f"unknown HEAD anchor: {row['id']}: {key}")
        report.append(f"| {row['id']} | {row['status']} | {row['reason']} |")
    report.extend(["", "## Source references", ""])
    for key, anchors in locations.items():
        for path, line, symbol in anchors:
            report.append(f"- `{key}`: [{path}:{line}](head-source/{path}#L{line}) — `{symbol}`")
    (ROOT / "build/lean/head-comparison.md").write_text("\n".join(report) + "\n")
    print(f"HEAD audit: {revision[:12]}, {len(sources)} files, {len(head['shared_bodies'])} identical C++ bodies, {len(ids)} classified contracts.")


def check():
    manifest = json.loads((SPEC / "traceability.json").read_text())
    contracts = json.loads((SPEC / "requirements.json").read_text())
    sources = {}
    for path, expected in manifest["files"].items():
        data = (ROOT / path).read_bytes()
        actual = hashlib.sha256(data).hexdigest()
        if actual != expected:
            raise ValueError(f"C++ drift: {path}; review the model and update its recorded SHA-256")
        sources[path] = data.decode()

    locations = source_locations(manifest, sources)
    check_references("cpp", locations)

    ids = set()
    report = ["# Encoder EARS contracts", "", "Generated by `make lean-test` from `formal/Encoder/requirements.json`.", "",
              "Proofs apply to the model and its stated assumptions. Source links are audited correspondence, not C++ verification.", ""]
    for contract in contracts:
        if contract["id"] in ids:
            raise ValueError(f"duplicate EARS id: {contract['id']}")
        ids.add(contract["id"])
        if not contract["theorems"] or not contract["anchors"]:
            raise ValueError(f"contract lacks proof or source: {contract['id']}")
        report.extend([f"## {contract['id']} — {contract['status']}", "", contract["requirement"], "",
                       "Lean: " + ", ".join(f"`{t}`" for t in contract["theorems"]) + ".", ""])
        for key in contract["anchors"]:
            for path, line, symbol in locations[key]:
                report.append(f"- [{path}:{line}](../../{path}#L{line}) — `{symbol}`")
        report.append("")
    output = ROOT / "build/lean/contracts.md"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(report))
    check_head(contracts, sources)
    print(f"Encoder source audit: {len(sources)} files, {len(locations)} anchors, {len(contracts)} EARS contracts.")
    print(f"Contract index: {output}")


if __name__ == "__main__":
    try:
        check()
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as error:
        print(f"Encoder specification audit failed: {error}", file=sys.stderr)
        sys.exit(1)

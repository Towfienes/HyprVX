#!/usr/bin/env python3
"""Form P / form S capture parity for Feature 002 (V-23, T054).

  python3 -I tests/native/compare_forms.py --suite-p DIR --suite-s DIR --out JSON

Each suite archive (run_feature002.sh) lists its per-test runs in results.txt ("TEST form=F scale=S RESULT run=ID").
For every (test, scale) present in both suites, every capture (*.png) that both runs wrote under the same name must be
byte-identical (sha256). Excluded, with the reason recorded in the output:
  - t_ws_anim control-early.png: a frame in the middle of a workspace animation with Drift OFF, so its content
    depends on the capture time, not on the delivery form.
A capture present in only one of the two runs, or a (test, scale) whose run is missing or not PASS in either suite, is
listed as a gap. Exit 0 when nothing differs and there are no gaps, 1 otherwise, 2 on bad input.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

EXCLUDED = {("t_ws_anim", "control-early.png"): "mid-animation frame with Drift OFF (timing-dependent)"}
EVIDENCE = Path.home() / ".local/share/hyprvx/evidence/claude"


def runs(suite):
    out = {}
    results = Path(suite) / "results.txt"
    if not results.is_file():
        print(f"compare_forms: no results.txt in {suite}", file=sys.stderr)
        sys.exit(2)
    for line in results.read_text().splitlines():
        words = line.split()
        if len(words) < 5:
            continue
        fields = dict(w.split("=", 1) for w in words[1:] if "=" in w)
        out[(words[0], fields.get("scale"))] = {"result": words[3], "run": fields.get("run")}
    return out


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--suite-p", required=True)
    parser.add_argument("--suite-s", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    p, s = runs(args.suite_p), runs(args.suite_s)
    report = {"suite_p": args.suite_p, "suite_s": args.suite_s, "compared": 0, "identical": 0, "differ": [], "gaps": [],
              "excluded": [], "pairs": []}
    for key in sorted(set(p) | set(s)):
        test, scale = key
        if key not in p or key not in s or p[key]["result"] != "PASS" or s[key]["result"] != "PASS":
            report["gaps"].append({"test": test, "scale": scale, "p": p.get(key), "s": s.get(key)})
            continue
        dp, ds = EVIDENCE / p[key]["run"], EVIDENCE / s[key]["run"]
        names_p = {f.name for f in dp.glob("*.png")}
        names_s = {f.name for f in ds.glob("*.png")}
        pair = {"test": test, "scale": scale, "run_p": p[key]["run"], "run_s": s[key]["run"], "images": 0}
        for name in sorted(names_p | names_s):
            if (test, name) in EXCLUDED:
                report["excluded"].append({"test": test, "scale": scale, "image": name, "reason": EXCLUDED[(test, name)]})
                continue
            if name not in names_p or name not in names_s:
                report["gaps"].append({"test": test, "scale": scale, "image": name, "only_in": "P" if name in names_p else "S"})
                continue
            report["compared"] += 1
            pair["images"] += 1
            hp, hs = sha(dp / name), sha(ds / name)
            if hp == hs:
                report["identical"] += 1
            else:
                report["differ"].append({"test": test, "scale": scale, "image": name, "sha_p": hp, "sha_s": hs})
        report["pairs"].append(pair)
    report["verdict"] = "PASS" if not report["differ"] and not report["gaps"] and report["compared"] else "FAIL"
    Path(args.out).write_text(json.dumps(report, indent=1))
    print(f"forms compare: {report['identical']}/{report['compared']} identical, {len(report['differ'])} differ, "
          f"{len(report['gaps'])} gaps, {len(report['excluded'])} excluded -> {report['verdict']}")
    return 0 if report["verdict"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())

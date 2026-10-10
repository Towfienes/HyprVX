#!/usr/bin/env bash
# GATE-N (V-24, T034): proves the analyzer detects the defect class Sprint 1 must catch, on V2-as-is.
#   tests/native/gate_n.sh V2_RUN_DIR GATE_A_RUN_DIR
# Evaluates S1, S2, P and N of contracts/test-tooling.md section 6.1 from archived runs and classifies the outcome with
# the decision table. H (visual confirmation of the reported artifacts) is done by Claude on the archived PNGs and
# recorded separately; this script therefore never reports PASS: its best result is "PENDING-H".
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
V2_RUN=${1:?V2 run dir (from v2_capture.sh)}
GATE_A_RUN=${2:?GATE-A run dir}

hvx_init_run v2 gate_n
cp "$V2_RUN/summary.json" "$HVX_EVID/v2_run_summary.json"
cp "$GATE_A_RUN/summary.json" "$HVX_EVID/gate_a_run_summary.json"
set +e
python3 -I - "$HVX_REPO" "$V2_RUN" "$GATE_A_RUN" "$HVX_EVID" << 'PY'
import importlib.util, json, subprocess, sys
from pathlib import Path
from PIL import Image

repo, v2, gate_a, out = map(Path, sys.argv[1:5])
analyzer = [sys.executable, "-I", str(repo / "tools/analyze_frame.py")]
spec = importlib.util.spec_from_file_location("b", repo / "tools/hyprvx_build.py")
build = importlib.util.module_from_spec(spec); spec.loader.exec_module(build)

def analyze(name, *args):
    subprocess.run(analyzer + list(args) + ["--out", str(out / f"{name}.json")], capture_output=True, text=True)
    return json.loads((out / f"{name}.json").read_text())

report = {}
# ---- S1: setup validity
info = json.loads((v2 / "build-info-v2.json").read_text())
_, v2_patchset, _ = build.variant_identity(repo, "v2")
v2_summary = json.loads((v2 / "summary.json").read_text())
s1 = (info.get("variant") == "v2" and info.get("patchset_id") == v2_patchset
      and v2_summary["result"] == "CAPTURED" and not v2_summary["not_run"])
report["S1"] = {"holds": s1, "variant": info.get("variant"), "patchset_id": info.get("patchset_id"),
                "expected_patchset_id": v2_patchset, "v2_run_result": v2_summary["result"], "timeouts": v2_summary["not_run"]}

# ---- V2 camera model (research R2): pan in logical px, render z*(W - pan); zoom is centre-anchored with
#      pan += c*(1/old - 1/new), c = monitor size / 2; zoom clamped to [0.2, 1.5]. Replays v2_capture.sh.
c = (640.0, 400.0)
B = {"x": 140, "y": 100, "w": 800, "h": 600}   # identity surface box (window rule), checked by P on stock

def v2_box(zoom, pan):
    return (round(zoom * (B["x"] - pan[0])), round(zoom * (B["y"] - pan[1])), round(zoom * B["w"]), round(zoom * B["h"]))

cams = {}
state = {"pan": [0.0, 0.0], "z": 1.0}
def zoom_to(new):
    new = min(max(new, 0.2), 1.5)
    for i in (0, 1):
        state["pan"][i] += c[i] * (1 / state["z"] - 1 / new)
    state["z"] = new
def snap(name):
    cams[name] = (state["z"], tuple(state["pan"]))
zoom_to(0.5); snap("v2_z05")
zoom_to(0.25); snap("v2_z025")
zoom_to(1.5); snap("v2_z15")
state.update(pan=[0.0, 0.0], z=1.0)
state["pan"][0] += -300; snap("v2_pan_right")
state["pan"][0] += 300; state["pan"][1] += 300; snap("v2_pan_up")

# ---- S2: camera applied, judged on captures: every non-identity V2 capture differs from the V2 identity capture, and
#      in every capture where the window border is visible its box is where the V2 camera predicts (at least one such
#      capture is required). A capture without a visible border is recorded as an observation (V2 drops the border when
#      zooming out), not used as evidence either way. (Revised 2026-10-11 after the first run picked z=0.5 only, where
#      V2 draws no border, and border edges outside the output are not compared; see integration-log.md.)
def border_box(path):
    im = Image.open(path).convert("RGB"); w, h = im.size; px = im.load()
    pts = [(x, y) for y in range(h) for x in range(w) if all(abs(a - 0x88) <= 24 for a in px[x, y])]
    return (min(p[0] for p in pts), min(p[1] for p in pts), max(p[0] for p in pts), max(p[1] for p in pts)) if pts else None
def edges_match(measured, predicted, tol=3):
    """Compares only the predicted border edges that lie on the 1280x800 output; off-screen edges cannot be seen."""
    x0, y0, x1, y1 = predicted
    checks = []
    if 0 <= x0 <= 1279: checks.append(abs(measured[0] - x0) <= tol)
    if 0 <= y0 <= 799: checks.append(abs(measured[1] - y0) <= tol)
    if 0 <= x1 <= 1279: checks.append(abs(measured[2] - x1) <= tol)
    if 0 <= y1 <= 799: checks.append(abs(measured[3] - y1) <= tol)
    return bool(checks) and all(checks)
s2_details = {}
for name, (zz, pp) in cams.items():
    d = analyze(f"s2_diff_{name}", "diff", str(v2 / f"{name}.png"), "--reference", str(v2 / "v2_identity.png"))
    x, y, w, h = v2_box(zz, pp)
    border = max(1, round(2 * zz))
    predicted = (x - border, y - border, x + w + border - 1, y + h + border - 1)
    measured = border_box(v2 / f"{name}.png")
    match = None if measured is None else edges_match(measured, predicted)
    s2_details[name] = {"diff_pixels": (d["diff"] or {}).get("pixels"), "border_measured": measured,
                        "border_predicted": predicted, "border_match": match, "zoom": zz, "pan": pp}
all_differ = all((v["diff_pixels"] or 0) > 0 for v in s2_details.values())
visible = [v for v in s2_details.values() if v["border_match"] is not None]
s2 = all_differ and bool(visible) and all(v["border_match"] for v in visible)
report["S2"] = {"holds": s2, "all_differ_from_identity": all_differ, "captures_with_border": len(visible),
                "observations": sorted(k for k, v in s2_details.items() if v["border_match"] is None),
                "details": s2_details}

# ---- P: positive control (stock identity from GATE-A, synthetic known-good fixtures)
p_stock = analyze("p_stock_identity", "bbox", str(gate_a / "stock1.png"), "--expect-box", "140,100,800,600", "--tolerance", "2")
def fixture(path, x, y, zoom):
    tc = Image.new("RGB", (800, 600), (32, 32, 32)); q = tc.load()
    for yy in range(600):
        for xx in range(800):
            if xx % 50 == 0 or yy % 50 == 0:
                q[xx, yy] = (64, 64, 64)
    for (x0, y0), col in (((0, 0), (255, 0, 0)), ((784, 0), (0, 255, 0)), ((0, 584), (0, 0, 255)), ((784, 584), (255, 0, 255))):
        for yy in range(y0, y0 + 16):
            for xx in range(x0, x0 + 16):
                q[xx, yy] = col
    if zoom != 1:
        tc = tc.resize((round(800 * zoom), round(600 * zoom)), Image.BILINEAR)
    canvas = Image.new("RGB", (1280, 800), (17, 17, 17)); canvas.paste(tc, (x, y)); canvas.save(path)
    return "%d,%d,%d,%d" % (x, y, round(800 * zoom), round(600 * zoom))
p_fix = {}
for name, (x, y, zz) in {"fixture_id": (140, 100, 1.0), "fixture_z05": (390, 250, 0.5), "fixture_z025": (300, 200, 0.25)}.items():
    box = fixture(out / f"{name}.png", x, y, zz)
    p_fix[name] = analyze(f"p_{name}", "bbox", str(out / f"{name}.png"), "--expect-box", box, "--tolerance", "2")["verdict"]
p = p_stock["verdict"] == "PASS" and all(v == "PASS" for v in p_fix.values())
report["P"] = {"holds": p, "stock_identity": p_stock["verdict"], "fixtures": p_fix}

# ---- N: detection on V2 non-identity captures (RC1 geometry/clipping; RC2 stale regions vs full damage)
expected = ("marker_missing", "bbox_mismatch", "scale_mismatch", "pixels_differ")
n_results = {}
for name, (zz, pp) in cams.items():
    x, y, w, h = v2_box(zz, pp)
    r = analyze(f"n_{name}", "bbox", str(v2 / f"{name}.png"), "--expect-box", f"{x},{y},{w},{h}", "--tolerance", "2",
                "--allow-clip", "0,0,1280,800")
    n_results[name] = {"expected_box": [x, y, w, h], "verdict": r["verdict"], "reasons": r["reasons"], "bbox": r["bbox"]}
for k in range(4):
    r = analyze(f"n_v2_fill{k}", "diff", str(v2 / f"v2_fill{k}.png"), "--reference", str(v2 / f"v2_fill{k}-full.png"))
    n_results[f"v2_fill{k}"] = {"verdict": r["verdict"], "reasons": r["reasons"], "diff": r["diff"]}
detected = sorted(k for k, v in n_results.items()
                  if v["verdict"] == "FAIL" and any(reason.startswith(e) for reason in v["reasons"] for e in expected))
n = bool(detected)
report["N"] = {"holds": n, "results": n_results, "detected_in": detected}

# ---- Decision table (H is pending here, so this script never reports PASS)
if not s1 or not s2:
    outcome, classification = "BLOCKED", "Invalid test setup (S1 or S2 failed); the analyzer is not judged"
elif not p:
    outcome, classification = "FAIL", "Defective analyzer (over-reports on good images)"
elif n:
    outcome, classification = "PENDING-H", "S1, S2, P, N hold; PASS needs H (visual confirmation at the reported places)"
else:
    outcome, classification = ("PENDING-H", "S1, S2, P hold and the analyzer passes every V2 capture: H decides between "
                               "'defective analyzer (misses defects)' (FAIL) and 'V2 defect not reproduced' (BLOCKED)")
report["outcome"], report["classification"] = outcome, classification
(out / "gate_n_report.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps({"outcome": outcome, "classification": classification, "S1": s1, "S2": s2, "P": p, "N": n,
                  "detected_in": detected}, indent=1))
sys.exit(0 if outcome == "PENDING-H" else 1)
PY
code=$?
set -e
outcome=$(python3 -I -c "import json,sys;print(json.load(open(sys.argv[1]))['outcome'])" "$HVX_EVID/gate_n_report.json")
hvx_finish "$outcome" "GATE-N S1/S2/P/N on V2-as-is (V-24); H pending" "v2 run $(basename "$V2_RUN"); gate-a run $(basename "$GATE_A_RUN")"
exit $code

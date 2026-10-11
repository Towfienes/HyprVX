#!/usr/bin/env bash
# M-COST (T045): informational compositor cost measurement, NOT a gate (plan: "Drift frame cost is measured, not
# optimized"; Constitution VI). Measures the nested compositor's utime+stime (/proc/<pid>/stat, all threads) over
# fixed windows, for stock, patched identity and patched non-identity, HVX_COST_RUNS (>= 5) sessions each:
#   redraw  10 s, 50 full-surface client redraws every 200 ms (camera fixed: identity, or z = 0.5 for non-identity)
#   camera  50 dev-control camera sets, paced at most every 200 ms: identity re-sets (patched identity) or the P-zoom
#           sweep 1.0 -> 0.1 -> 2.0 -> 1.0 anchored at the output centre (patched non-identity; 50 samples, z in
#           [0.136, 1.962] and back to exactly 1.0); stock has no dev control. Each scripts/nested.sh call re-verifies
#           the session identity (~0.44 s), so this window runs ~22 s instead of 10 s; wall time is recorded per window
# CPU resolution is one clock tick (getconf CLK_TCK; 10 ms at 100 Hz), so small windows sit at that resolution.
#   idle    60 s, no input (camera fixed as in redraw)
# Frames presented are NOT MEASURED: stock Hyprland 0.56.2 exposes no presented-frame counter, a client that keeps a
# frame callback pending forces renders (src/render/Renderer.cpp "hack: schedule frame events"), and the scheduler
# trace lines exist only with render:new_render_scheduling, which is off by default.
# Conditions follow master plan section E1 (read only; nothing on the host is changed).
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
RUNS="${HVX_COST_RUNS:-5}"
SIZE=1280x800
((RUNS >= 5)) || { echo "HVX_COST_RUNS must be >= 5" >&2; exit 2; }

hvx_init_run mixed m_cost
hvx_copy_build_info stock
hvx_copy_build_info patched
TICK=$(getconf CLK_TCK)

python3 -I - "$HVX_EVID/conditions.json" "$SIZE" "$RUNS" << 'PY'
import glob, json, sys
from pathlib import Path
def read(path):
    try:
        return Path(path).read_text().strip()
    except OSError:
        return None
out, size, runs = sys.argv[1:4]
conditions = {
    "output": {"name": "HVX-1 (headless, nested)", "mode": f"{size}@60", "scale": 1, "hdr": False},
    "power_online": {p: read(p) for p in sorted(glob.glob("/sys/class/power_supply/*/online"))},
    "cpu_governor": sorted({read(p) for p in glob.glob("/sys/devices/system/cpu/cpu*/cpufreq/scaling_governor")} - {None}),
    "cpu_epp": sorted({read(p) for p in glob.glob("/sys/devices/system/cpu/cpu*/cpufreq/energy_performance_preference")} - {None}),
    "initial_temperature_millicelsius": {p: read(p) for p in sorted(glob.glob("/sys/class/thermal/thermal_zone*/temp"))},
    "clients": "1 hyprvx-testclient 800x600 (window rule 140,100)",
    "runs_per_variant": int(runs),
    "build": "build-info-stock.json, build-info-patched.json (binary sha256, patchset_id)",
}
Path(out).write_text(json.dumps(conditions, indent=2) + "\n")
PY

# cpu_ticks PID: utime + stime of the whole process (fields 14 and 15 after the comm field).
cpu_ticks() {
    awk '{ sub(/^.*\) /, ""); print $12 + $13 }' "/proc/$1/stat"
}

# measure LABEL PID SECONDS [STEP-FUNCTION]: CPU ms and wall ms over a window; 50 steps when a step function is given
# (deadline-paced every SECONDS/50 s), otherwise idle.
measure() {
    local label=$1 pid=$2 seconds=$3 step=${4:-} t0 c0 c1 t1 i target now
    c0=$(cpu_ticks "$pid")
    t0=$(date +%s%N)
    if [[ -n $step ]]; then
        for ((i = 0; i < 50; i++)); do
            "$step" "$i"
            target=$((t0 + (i + 1) * seconds * 1000000000 / 50))
            now=$(date +%s%N)
            if ((now < target)); then
                sleep "$(awk -v d=$((target - now)) 'BEGIN { printf "%.3f", d / 1e9 }')"
            fi
        done
    else
        sleep "$seconds"
    fi
    c1=$(cpu_ticks "$pid")
    t1=$(date +%s%N)
    printf '%s %d %d\n' "$label" $(((c1 - c0) * 1000 / TICK)) $(((t1 - t0) / 1000000)) >> "$HVX_EVID/samples.txt"
}

step_redraw() { hvx_client_cmd client redraw; }
step_identity() { hvx_nested dev camera 0 0 1 > /dev/null || hvx_die_blocked "identity camera set rejected"; }
# P-zoom: z piecewise linear over steps 0..49 through 1.0 -> 0.1 -> 2.0 -> 1.0; P keeps world (640,400) at the centre.
step_pzoom() {
    local p
    p=$(awk -v i="$1" 'BEGIN {
        t = (i + 1) / 50
        if (t <= 1 / 3) z = 1 - 0.9 * t * 3; else if (t <= 2 / 3) z = 0.1 + 1.9 * (t - 1 / 3) * 3; else z = 2 - (t - 2 / 3) * 3
        if (i == 49) z = 1
        printf "%.9f %.9f %.9f", 640 - 640 / z, 400 - 400 / z, z }')
    # shellcheck disable=SC2086
    hvx_nested dev camera $p > /dev/null || hvx_die_blocked "P-zoom camera set rejected ($p)"
}

: > "$HVX_EVID/samples.txt"
for ((run = 1; run <= RUNS; run++)); do
    for variant in stock patched-identity patched-nonidentity; do
        bin=${variant%%-*}
        hvx_start --variant "$bin" --size "$SIZE" --scale 1 >> "$HVX_EVID/session.log" 2>&1 \
            || { tail -20 "$HVX_EVID/session.log" >&2; hvx_die_blocked "nested $bin session did not start (run $run)"; }
        pid=$(hvx_nested status | sed -n 's/.*pid=\([0-9]*\);.*/\1/p')
        [[ -n $pid && -r /proc/$pid/stat ]] || hvx_die_blocked "no compositor pid (run $run $variant)"
        rm -f "$HVX_EVID/client.jsonl"
        hvx_client_start client 800x600
        if [[ $variant == patched-nonidentity ]]; then
            hvx_nested dev camera -500 -300 0.5 > /dev/null || hvx_die_blocked "static camera rejected"
        fi
        hvx_ready client
        sleep 1
        measure "run$run $variant redraw" "$pid" 10 step_redraw
        hvx_ready client
        case $variant in
            patched-identity) measure "run$run $variant camera" "$pid" 10 step_identity ;;
            patched-nonidentity)
                measure "run$run $variant camera" "$pid" 10 step_pzoom
                hvx_nested dev camera -500 -300 0.5 > /dev/null || hvx_die_blocked "static camera rejected"
                ;;
        esac
        hvx_ready client
        measure "run$run $variant idle" "$pid" 60
        if [[ $bin == patched ]]; then
            hvx_nested dev panic > /dev/null
        fi
        hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop (run $run $variant)"
    done
done

python3 -I - "$HVX_EVID" << 'PY'
import json, statistics, sys
from collections import defaultdict
from pathlib import Path
evid = Path(sys.argv[1])
cpu, wall = defaultdict(list), defaultdict(list)
for line in (evid / "samples.txt").read_text().splitlines():
    _, variant, window, ms, wall_ms = line.split()
    cpu[(variant, window)].append(int(ms)); wall[(variant, window)].append(int(wall_ms))
report = {"metric": "compositor utime+stime ms per window (all threads)", "frames_presented": "NOT MEASURED",
          "windows": {}}
rows = ["| variant | window | runs | CPU ms median [min, max] | wall ms median |", "|---|---|---|---|---|"]
for variant in ("stock", "patched-identity", "patched-nonidentity"):
    for window in ("redraw", "camera", "idle"):
        values = cpu.get((variant, window))
        if not values:
            report["windows"][f"{variant}/{window}"] = "NOT RUN (stock has no dev control)" if window == "camera" else "missing"
            rows.append(f"| {variant} | {window} | 0 | NOT RUN (no dev control in stock) | - |")
            continue
        entry = {"runs": len(values), "median": statistics.median(values), "min": min(values), "max": max(values),
                 "values": values, "wall_ms_median": statistics.median(wall[(variant, window)])}
        report["windows"][f"{variant}/{window}"] = entry
        rows.append(f"| {variant} | {window} | {len(values)} | {entry['median']:g} [{entry['min']}, {entry['max']}] | "
                    f"{entry['wall_ms_median']:g} |")
(evid / "cost_report.json").write_text(json.dumps(report, indent=2) + "\n")
(evid / "cost_table.md").write_text("\n".join(rows) + "\n")
print("\n".join(rows))
PY
hvx_finish MEASURED "M-COST informational compositor CPU cost (T045, not a gate)" "runs $RUNS per variant; frames presented NOT MEASURED"

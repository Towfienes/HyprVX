# Shared helpers for HyprVX native tests (contracts/test-tooling.md sections 5 and 8). Source it from a test:
#   source "$(dirname "$0")/lib.sh"; hvx_init_run stock gate_a
# Raw evidence goes to ~/.cache/hyprvx/$HYPRVX_AGENT/evidence/<run-id>/; hvx_finish writes summary.json and a final
# SHA256SUMS there, copies both to the repo index evidence/<run-id>/ and archives the run read-only under
# ~/.local/share/hyprvx/evidence/<agent>/<run-id>/ after `sha256sum -c`. A missing ready signal is BLOCKED, never PASS.

set -euo pipefail

HVX_REPO="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
: "${HYPRVX_AGENT:?set HYPRVX_AGENT}"
HVX_READY_TIMEOUT="${HVX_READY_TIMEOUT:-5}"
HVX_NOT_RUN=()

hvx_die_blocked() {
    echo "BLOCKED: $*" >&2
    if [[ -n ${HVX_EVID:-} ]]; then
        HVX_NOT_RUN+=("$*")
        hvx_finish BLOCKED "${HVX_TEST:-unknown}" "$*" || true
    fi
    exit 2
}

# hvx_init_run VARIANT TEST: sets HVX_RUN_ID, HVX_EVID, HVX_TEST.
hvx_init_run() {
    local sha
    sha=$(git -C "$HVX_REPO" rev-parse --short=7 HEAD)
    HVX_TEST="$2"
    # The test name keeps run-ids unique when tests start within the same second (e.g. run_sprint1).
    local variant=$1
    [[ -n ${HVX_SPIKE_PLUGIN:-} && $variant == patched ]] && variant=spike
    HVX_RUN_ID="$(date +%Y%m%d-%H%M%S)-$variant-$sha-$2${HVX_PATCHED_PREFIX:+-negctl-$HVX_PATCHED_PREFIX}"
    HVX_EVID="$HOME/.cache/hyprvx/$HYPRVX_AGENT/evidence/$HVX_RUN_ID"
    mkdir -p "$HVX_EVID"
    { date -Is; git -C "$HVX_REPO" rev-parse HEAD; grep MemAvailable /proc/meminfo; cat /proc/pressure/memory; } \
        > "$HVX_EVID/preflight.log"
    echo "run $HVX_RUN_ID -> $HVX_EVID"
}

hvx_nested() {
    "$HVX_REPO/scripts/nested.sh" "$@"
}

# Sessions this test started are stopped on any exit (PASS, FAIL or BLOCKED); stop re-verifies identity first.
HVX_OWN_SESSION=0
# HVX_PATCHED_PREFIX=<prefix id> (negative controls only): the patched variant runs that archived agent prefix, e.g.
# the 0001-0005 build, to show that a test fails without the patches it covers. The run-id gets "-negctl-<id>" and the
# evidence records that prefix's build-info; such a run never counts as the test's PASS evidence.
# HVX_SPIKE_PLUGIN=<path to hyprvx-plugin-spike.so> (delivery spike T067): the patched variant runs the stock prefix with
# the spike plugin, loaded through `nested.sh run` into the verified nested session. The run-id variant is "spike" and
# the evidence records the stock build-info and the plugin's sha256.
hvx_start() {
    local extra=() args=("$@") spike=0
    [[ -n ${HVX_PATCHED_PREFIX:-} && " $* " == *" --variant patched "* ]] \
        && extra=(--binary "$HOME/.cache/hyprvx/$HYPRVX_AGENT/prefix/$HVX_PATCHED_PREFIX/bin/Hyprland")
    if [[ -n ${HVX_SPIKE_PLUGIN:-} && " $* " == *" --variant patched "* ]]; then
        spike=1
        args=("${args[@]/#patched/stock}")
    fi
    hvx_nested start "${args[@]}" "${extra[@]}" && HVX_OWN_SESSION=1 || return 1
    if ((spike)); then
        sha256sum "$HVX_SPIKE_PLUGIN" > "$HVX_EVID/plugin.sha256"
        local reply
        reply=$(hvx_nested run -- "$HOME/.cache/hyprvx/$HYPRVX_AGENT/prefix/stock/bin/hyprctl" plugin load "$HVX_SPIKE_PLUGIN")
        [[ $reply == ok ]] || { echo "spike plugin load failed: $reply" >&2; return 1; }
    fi
}
hvx_stop() {
    hvx_nested stop && HVX_OWN_SESSION=0
}
hvx_cleanup() {
    if [[ $HVX_OWN_SESSION == 1 ]]; then
        hvx_nested stop > /dev/null 2>&1 || echo "WARNING: nested session left running; use scripts/nested.sh stop" >&2
    fi
}
trap hvx_cleanup EXIT

# Builds (incrementally, so a source change is never tested with a stale binary) hyprvx-testclient and
# hyprvx-vpointer in the agent cache; prints the build directory.
hvx_tools_build() {
    local build="$HOME/.cache/hyprvx/$HYPRVX_AGENT/build/testclient"
    if [[ ! -f $build/build.ninja ]]; then
        cmake -S "$HVX_REPO" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHYPRVX_WITH_WAYLAND_PLATFORM=ON > "$build.configure.log" 2>&1
    fi
    cmake --build "$build" --target hyprvx_testclient hyprvx_vpointer -j2 > "$build.build.log" 2>&1
    echo "$build"
}
hvx_testclient() {
    echo "$(hvx_tools_build)/hyprvx-testclient"
}

# hvx_client_start NAME SIZE [testclient args...]: starts a nested client with a control FIFO and a JSONL log.
hvx_client_start() {
    local name=$1 size=$2
    shift 2
    local fifo="$HVX_EVID/$name.fifo" log="$HVX_EVID/$name.jsonl" client
    client=$(hvx_testclient)
    rm -f "$fifo"
    mkfifo -m 600 "$fifo"
    : > "$log"
    hvx_nested run --background -- "$client" --size "$size" --title "$name" --log "$log" --control "$fifo" "$@" \
        > /dev/null
    hvx_wait_for "$log" '"ev": "frame_done"' || hvx_die_blocked "client $name never presented a frame"
}

# hvx_vpointer_start NAME: starts hyprvx-vpointer on the nested display (logical layout from HVX_LAYOUT, default
# 1280x800) with control FIFO NAME.fifo and log NAME.jsonl; waits for its ready line.
hvx_vpointer_start() {
    local name=$1 fifo="$HVX_EVID/$1.fifo" log="$HVX_EVID/$1.jsonl" vp
    vp="$(hvx_tools_build)/hyprvx-vpointer"
    rm -f "$fifo"
    mkfifo -m 600 "$fifo"
    : > "$log"
    hvx_nested run --background -- "$vp" --layout "${HVX_LAYOUT:-1280x800}" --log "$log" --control "$fifo" > /dev/null
    hvx_wait_for "$log" '"ev": "ready"' || hvx_die_blocked "vpointer $name never became ready"
}

# hvx_pointer_run OUT VPOINTER COMMANDS_FILE CLIENT...: runs the commands through pointer_driver.py and writes
# OUT.json in the evidence dir; a timeout is BLOCKED.
hvx_pointer_run() {
    local out=$1 vp=$2 commands=$3 client
    shift 3
    local args=()
    for client in "$@"; do
        args+=(--client "$client:$HVX_EVID/$client.fifo:$HVX_EVID/$client.jsonl")
    done
    python3 -I "$HVX_REPO/tests/native/pointer_driver.py" --vpointer "$HVX_EVID/$vp.fifo:$HVX_EVID/$vp.jsonl" \
        "${args[@]}" --commands "$commands" --out "$HVX_EVID/$out.json" \
        || hvx_die_blocked "pointer driver $out did not complete"
}

hvx_client_cmd() {
    printf '%s\n' "$2" > "$HVX_EVID/$1.fifo"
}

# hvx_wait_for FILE PATTERN [TIMEOUT]: waits until a line containing PATTERN appears.
hvx_wait_for() {
    local file=$1 pattern=$2 timeout=${3:-$HVX_READY_TIMEOUT} deadline
    deadline=$(($(date +%s%N) + timeout * 1000000000))
    while (($(date +%s%N) < deadline)); do
        grep -qF -- "$pattern" "$file" 2> /dev/null && return 0
        sleep 0.02
    done
    return 1
}

hvx_last_seq() {
    grep -o '"seq": [0-9]*, "ev": "commit"' "$1" | tail -1 | grep -o '[0-9]\+' || echo 0
}

# Ready rule (test-tooling section 5): after any state change, sync and wait for frame_done of that seq, twice.
hvx_ready() {
    local name=$1 log="$HVX_EVID/$1.jsonl" round before seq deadline
    for round in 1 2; do
        before=$(hvx_last_seq "$log")
        hvx_client_cmd "$name" sync
        deadline=$(($(date +%s%N) + HVX_READY_TIMEOUT * 1000000000))
        seq=$before
        while ((seq <= before)) && (($(date +%s%N) < deadline)); do
            seq=$(hvx_last_seq "$log")
            sleep 0.01
        done
        ((seq > before)) || hvx_die_blocked "client $name did not commit sync round $round"
        hvx_wait_for "$log" "\"seq\": $seq, \"ev\": \"frame_done\"" \
            || hvx_die_blocked "no frame_done for seq $seq of $name (round $round)"
    done
}

# hvx_capture NAME CLIENT: ready rule on CLIENT, then a capture plus the camera state for bookkeeping.
hvx_capture() {
    local name=$1 client=$2
    hvx_ready "$client"
    hvx_nested capture "$HVX_EVID/$name.png" || hvx_die_blocked "capture $name failed"
    hvx_nested dev state > "$HVX_EVID/$name.state.json" 2> /dev/null || echo "n/a (no dev control)" > "$HVX_EVID/$name.state.json"
}

# hvx_analyze NAME MODE ARGS...: runs the analyzer into NAME.json and returns its exit code (0 PASS, 1 FAIL, 2 BLOCKED).
hvx_analyze() {
    local name=$1
    shift
    python3 -I "$HVX_REPO/tools/analyze_frame.py" "$@" --out "$HVX_EVID/$name.json" > /dev/null
}

# hvx_phys_box X,Y,W,H: a logical box in physical output pixels (the capture is in physical pixels).
hvx_phys_box() {
    python3 -I -c "
import sys
s = float(sys.argv[2]); x, y, w, h = map(float, sys.argv[1].split(','))
print('%d,%d,%d,%d' % (round(x * s), round(y * s), round(w * s), round(h * s)))" "$1" "${HVX_OUTPUT_SCALE:-1}"
}

# hvx_prefix_id VARIANT: the prefix id (patchset id or stock id) of the variant built from this checkout.
hvx_prefix_id() {
    [[ $1 == patched && -n ${HVX_PATCHED_PREFIX:-} ]] && { echo "$HVX_PATCHED_PREFIX"; return; }
    [[ $1 == patched && -n ${HVX_SPIKE_PLUGIN:-} ]] && { echo stock; return; }
    python3 -I -c "
import importlib.util, sys; from pathlib import Path
spec = importlib.util.spec_from_file_location('b', '$HVX_REPO/tools/hyprvx_build.py'); b = importlib.util.module_from_spec(spec)
spec.loader.exec_module(b); print(b.variant_identity(Path('$HVX_REPO'), '$1')[0])"
}

hvx_copy_build_info() {
    local variant=$1
    local label=$variant
    [[ $variant == patched && -n ${HVX_SPIKE_PLUGIN:-} ]] && label=spike-stock
    cp "$HOME/.cache/hyprvx/$HYPRVX_AGENT/prefix/$(hvx_prefix_id "$variant")/build-info.json" "$HVX_EVID/build-info-$label.json"
}

# hvx_hyprctl VARIANT ARGS...: read-only queries (e.g. -j clients, -j activewindow) with the nested prefix's hyprctl,
# through `nested.sh run`, which re-verifies the session and points HYPRLAND_INSTANCE_SIGNATURE at it. Never the host.
hvx_hyprctl() {
    local variant=$1
    shift
    hvx_nested run -- "$HOME/.cache/hyprvx/$HYPRVX_AGENT/prefix/$(hvx_prefix_id "$variant")/bin/hyprctl" "$@"
}

# hvx_session_dir: the verified session's directory (nested.sh status); `stop` copies the compositor log there.
hvx_session_dir() {
    hvx_nested status | sed -n 's/.*; session=\(.*\)$/\1/p'
}

# hvx_finish RESULT TEST [NOTE]: summary.json, SHA256SUMS, repo index and read-only archive.
hvx_finish() {
    local result=$1 test=$2 note=${3:-}
    local archive="$HOME/.local/share/hyprvx/evidence/$HYPRVX_AGENT/$HVX_RUN_ID"
    rm -f "$HVX_EVID"/*.fifo
    python3 -I - "$HVX_EVID" "$HVX_RUN_ID" "$test" "$result" "$note" "$(git -C "$HVX_REPO" rev-parse HEAD)" \
        "${HVX_NOT_RUN[@]+"${HVX_NOT_RUN[@]}"}" << 'PY'
import json, sys
from pathlib import Path
evid, run_id, test, result, note, sha, *not_run = sys.argv[1:]
artifacts = sorted(p.name for p in Path(evid).iterdir() if p.is_file() and p.name not in ("summary.json", "SHA256SUMS"))
summary = {"run_id": run_id, "test": test, "result": result, "agent": "claude", "git_sha": sha,
           "note": note, "artifacts": artifacts, "not_run": not_run}
Path(evid, "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
PY
    (cd "$HVX_EVID" && find . -maxdepth 1 -type f ! -name SHA256SUMS -printf '%P\n' | sort | xargs sha256sum > SHA256SUMS)
    mkdir -p "$HVX_REPO/evidence/$HVX_RUN_ID"
    cp "$HVX_EVID/summary.json" "$HVX_EVID/SHA256SUMS" "$HVX_REPO/evidence/$HVX_RUN_ID/"
    cp -a "$HVX_EVID" "$archive"
    chmod -R a-w "$archive"
    (cd "$archive" && sha256sum -c --quiet SHA256SUMS)
    echo "$result $test run=$HVX_RUN_ID archive=$archive"
}

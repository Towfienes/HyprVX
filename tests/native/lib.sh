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
    HVX_RUN_ID="$(date +%Y%m%d-%H%M%S)-$1-$sha"
    HVX_EVID="$HOME/.cache/hyprvx/$HYPRVX_AGENT/evidence/$HVX_RUN_ID"
    mkdir -p "$HVX_EVID"
    { date -Is; git -C "$HVX_REPO" rev-parse HEAD; grep MemAvailable /proc/meminfo; cat /proc/pressure/memory; } \
        > "$HVX_EVID/preflight.log"
    echo "run $HVX_RUN_ID -> $HVX_EVID"
}

hvx_nested() {
    "$HVX_REPO/scripts/nested.sh" "$@"
}

# Builds hyprvx-testclient in the agent cache when missing and prints its path.
hvx_testclient() {
    local build="$HOME/.cache/hyprvx/$HYPRVX_AGENT/build/testclient"
    if [[ ! -x $build/hyprvx-testclient ]]; then
        cmake -S "$HVX_REPO" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHYPRVX_WITH_WAYLAND_PLATFORM=ON > "$build.configure.log" 2>&1
        cmake --build "$build" --target hyprvx_testclient -j2 > "$build.build.log" 2>&1
    fi
    echo "$build/hyprvx-testclient"
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

hvx_copy_build_info() {
    local variant=$1 id
    id=$(python3 -I -c "
import importlib.util, sys; from pathlib import Path
spec = importlib.util.spec_from_file_location('b', '$HVX_REPO/tools/hyprvx_build.py'); b = importlib.util.module_from_spec(spec)
spec.loader.exec_module(b); print(b.variant_identity(Path('$HVX_REPO'), '$variant')[0])")
    cp "$HOME/.cache/hyprvx/$HYPRVX_AGENT/prefix/$id/build-info.json" "$HVX_EVID/build-info-$variant.json"
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

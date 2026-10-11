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
        [[ -f $(dirname -- "$HVX_SPIKE_PLUGIN")/build-info.json ]] \
            && cp "$(dirname -- "$HVX_SPIKE_PLUGIN")/build-info.json" "$HVX_EVID/build-info-spike-plugin.json"
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
    cmake --build "$build" --target hyprvx_testclient hyprvx_vpointer hyprvx_vkeyboard hyprvx_x11client -j2 > "$build.build.log" 2>&1
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

# ---- Feature 002 (test-tooling 1.4) -------------------------------------------------------------------------------
# HVX_FORM=P|S selects the delivery form (default P): form P runs the patched build (mechanism in the binary, dev control
# from -DHYPRVX_DEV=ON) and loads plugin/P/hyprvx-drift-P.so for policy tests; form S runs the stock build and always
# loads plugin/S-dev/hyprvx-drift-S.so (policy + mechanism hooks + dev control; scripts/build-plugin.sh --form S --dev).
HVX_FORM="${HVX_FORM:-P}"
case $HVX_FORM in P|S) ;; *) echo "HVX_FORM must be P or S" >&2; exit 2 ;; esac
hvx_form_variant() { [[ $HVX_FORM == P ]] && echo patched || echo stock; }
hvx_form_label() { echo "form$HVX_FORM"; }
hvx_plugin_path() {
    [[ $HVX_FORM == P ]] && echo "$HOME/.cache/hyprvx/$HYPRVX_AGENT/plugin/P/hyprvx-drift-P.so" \
        || echo "$HOME/.cache/hyprvx/$HYPRVX_AGENT/plugin/S-dev/hyprvx-drift-S.so"
}

# hvx_plugin_load [PATH]: loads the form's plugin (or PATH) into the verified session; its sha256 and build-info go to
# the evidence. A refused load is returned to the caller (tests decide whether that is BLOCKED or the result).
hvx_plugin_load() {
    local so=${1:-$(hvx_plugin_path)} reply
    [[ -f $so ]] || { echo "no plugin at $so; run scripts/build-plugin.sh" >&2; return 1; }
    reply=$(hvx_nested plugin load "$so" 2>&1) || { printf '%s\n' "$reply" >> "$HVX_EVID/plugin-load.log"; return 1; }
    printf '%s\n' "$reply" >> "$HVX_EVID/plugin-load.log"
    sha256sum "$so" > "$HVX_EVID/plugin.sha256"
    [[ -f $(dirname -- "$so")/build-info.json ]] && cp "$(dirname -- "$so")/build-info.json" "$HVX_EVID/build-info-plugin.json"
    return 0
}
hvx_plugin_unload() {
    hvx_nested plugin unload "${1:-$(hvx_plugin_path)}"
}

# hvx_start_form [--policy] START_ARGS...: starts the form's compositor (patched or stock). Form S always loads its
# plugin (the mechanism lives there); form P loads the policy plugin only with --policy. Build info of the prefix is
# copied to the evidence.
hvx_start_form() {
    local policy=0
    [[ ${1:-} == --policy ]] && { policy=1; shift; }
    local variant
    variant=$(hvx_form_variant)
    hvx_copy_build_info "$variant"
    hvx_nested start --variant "$variant" "$@" && HVX_OWN_SESSION=1 || return 1
    if [[ $HVX_FORM == S || $policy == 1 ]]; then
        hvx_plugin_load || return 1
    fi
}

# hvx_hyprctl_form ARGS...: hyprctl of the form's prefix through `nested.sh run` (dispatchers, keyword, -j queries).
hvx_hyprctl_form() {
    hvx_hyprctl "$(hvx_form_variant)" "$@"
}
# hvx_dispatch NAME [ARGS...]: a dispatcher; the reply is printed and never evidence (Constitution II).
hvx_dispatch() {
    hvx_hyprctl_form dispatch "$@"
}

# hvx_vkeyboard_start NAME: hyprvx-vkeyboard on the nested display with control FIFO NAME.fifo and log NAME.jsonl.
hvx_vkeyboard_start() {
    local name=$1 fifo="$HVX_EVID/$1.fifo" log="$HVX_EVID/$1.jsonl" vk
    vk="$(hvx_tools_build)/hyprvx-vkeyboard"
    rm -f "$fifo"
    mkfifo -m 600 "$fifo"
    : > "$log"
    hvx_nested run --background -- "$vk" --log "$log" --control "$fifo" > /dev/null
    hvx_wait_for "$log" '"ev": "ready"' || hvx_die_blocked "vkeyboard $name never became ready"
}
# hvx_key NAME COMMAND KEY: one vkeyboard command, waiting for its done line.
hvx_key() {
    local name=$1 log="$HVX_EVID/$1.jsonl" before
    before=$(grep -c '"ev": "done"' "$log" || true)
    printf '%s %s\n' "$2" "$3" > "$HVX_EVID/$name.fifo"
    local deadline=$(($(date +%s%N) + HVX_READY_TIMEOUT * 1000000000))
    while (($(grep -c '"ev": "done"' "$log" || true) <= before)); do
        (($(date +%s%N) < deadline)) || hvx_die_blocked "vkeyboard $name did not complete '$2 $3'"
        sleep 0.01
    done
}

# hvx_x11client_start NAME SIZE [x11client args...]: hyprvx-x11client through `nested.sh run --x11` (DISPLAY resolved
# only through the verified compositor PID); waits for its first drawn line.
hvx_x11client_start() {
    local name=$1 size=$2
    shift 2
    local fifo="$HVX_EVID/$name.fifo" log="$HVX_EVID/$name.jsonl" x11
    x11="$(hvx_tools_build)/hyprvx-x11client"
    rm -f "$fifo"
    mkfifo -m 600 "$fifo"
    : > "$log"
    hvx_nested run --background --x11 -- "$x11" --size "$size" --title "$name" --log "$log" --control "$fifo" "$@" \
        > /dev/null || hvx_die_blocked "x11client $name: nested XWayland display not resolvable"
    hvx_wait_for "$log" '"ev": "drawn"' 10 || hvx_die_blocked "x11client $name never drew"
}
# X11 ready rule (test-tooling 1.4 4c): drawn for a new redraw, then the section 5 ready rule on a Wayland client twice.
hvx_x11_ready() {
    local name=$1 wayland=$2 log="$HVX_EVID/$1.jsonl" before
    before=$(grep -c '"ev": "drawn"' "$log" || true)
    hvx_client_cmd "$name" redraw
    local deadline=$(($(date +%s%N) + HVX_READY_TIMEOUT * 1000000000))
    while (($(grep -c '"ev": "drawn"' "$log" || true) <= before)); do
        (($(date +%s%N) < deadline)) || hvx_die_blocked "x11client $name did not redraw"
        sleep 0.01
    done
    hvx_ready "$wayland"
}

# hvx_super_escape KB: Super+Escape through vkeyboard KB (LEFTMETA down, ESC tap, LEFTMETA up).
hvx_super_escape() {
    hvx_key "$1" down KEY_LEFTMETA
    hvx_key "$1" tap KEY_ESC
    hvx_key "$1" up KEY_LEFTMETA
}
# hvx_escape_count CLIENT: Escape presses (evdev 1) the client received, from its keyboard log.
hvx_escape_count() {
    grep -c '"ev": "key", "key": 1, "state": "pressed"' "$HVX_EVID/$1.jsonl" || true
}
# hvx_drift_off_probe KB CLIENT: evidence that Drift is OFF on the focused monitor. Super+Escape is consumed only in
# DRIFT (drift-control 1.0), so with Drift OFF the keyboard-focused CLIENT receives the Escape press. Returns 1 if the
# press did not arrive (consumed: Drift was on). Side effect in DRIFT: Drift turns off.
hvx_drift_off_probe() {
    local before
    before=$(hvx_escape_count "$2")
    hvx_super_escape "$1"
    hvx_ready "$2"
    (($(hvx_escape_count "$2") == before + 1))
}
# hvx_differs NAME A B: setup validity, image A differs from image B (else a comparison with B is vacuous). Returns 0
# only for an analyzer FAIL of the 0-pixel diff; PASS (identical) and BLOCKED return 1.
hvx_differs() {
    local rc=0
    hvx_analyze "$1" diff "$2" --reference "$3" --max-pixels 0 || rc=$?
    ((rc == 1))
}

# hvx_events_start: socket2 event log of the verified session in events.log (tests/native/events_log.py).
hvx_events_start() {
    : > "$HVX_EVID/events.log"
    hvx_nested run --background -- python3 -I "$HVX_REPO/tests/native/events_log.py" "$HVX_EVID/events.log" > /dev/null
    hvx_wait_for "$HVX_EVID/events.log" 'hyprvx-events-log>>ready' || hvx_die_blocked "socket2 event logger did not start"
}

# hvx_clients_snapshot NAME: the window model Drift must not change (hyprctl -j clients: address, workspace, at, size,
# floating, fullscreen), sorted, in NAME.clients.json.
hvx_clients_snapshot() {
    hvx_hyprctl_form -j clients | python3 -I -c "
import json, sys
clients = json.load(sys.stdin)
keep = [{k: c.get(k) for k in ('address', 'class', 'workspace', 'at', 'size', 'floating', 'fullscreen', 'mapped')} for c in clients]
print(json.dumps(sorted(keep, key=lambda c: c['address']), indent=1, sort_keys=True))" > "$HVX_EVID/$1.clients.json"
}

# hvx_window_address CLASS: the address of the (first) mapped client of that class.
hvx_window_address() {
    hvx_hyprctl_form -j clients | python3 -I -c "
import json, sys
print(next(c['address'] for c in json.load(sys.stdin) if c.get('class') == sys.argv[1]))" "$1"
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

#!/usr/bin/env bash
# Phase 2 US3 native suite (V-16..V-19, T065): t_pointer, t_focus, t_popup and t_drag_scope at output scale 1 and
# 1.25. Each test writes and archives its own evidence run; this aggregate run records each test's run-id and result.
# The Sprint 1 regression is tests/native/run_sprint1.sh (run both for T065). Any FAIL makes the suite FAIL; otherwise
# any BLOCKED makes it BLOCKED.
set -uo pipefail
here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source "$here/lib.sh"
set +e  # lib.sh enables errexit; a failing test must be recorded, not end the suite
TESTS=(${HVX_TESTS:-t_pointer t_focus t_popup t_drag_scope})
SCALES=(${HVX_SCALES:-1 1.25})

hvx_init_run patched phase2
failed=0 blocked=0
for scale in "${SCALES[@]}"; do
    for t in "${TESTS[@]}"; do
        log="$HVX_EVID/${t}_scale${scale}.log"
        # Same logical output (1280x800) at every scale: physical size = logical size * scale.
        size=$(python3 -I -c "import sys; s = float(sys.argv[1]); print('%dx%d' % (round(1280 * s), round(800 * s)))" "$scale")
        HVX_OUTPUT_SCALE="$scale" HVX_OUTPUT_SIZE="$size" "$here/$t.sh" > "$log" 2>&1
        code=$?
        run=$(grep -o 'run=[0-9A-Za-z_.-]*' "$log" | tail -1 | cut -d= -f2)
        case $code in 0) r=PASS ;; 2) r=BLOCKED; blocked=1 ;; *) r=FAIL; failed=1 ;; esac
        printf '%s scale=%s %s run=%s\n' "$t" "$scale" "$r" "${run:-none}" | tee -a "$HVX_EVID/results.txt"
    done
done
overall=$( ((failed)) && echo FAIL || { ((blocked)) && echo BLOCKED || echo PASS; })
hvx_finish "$overall" "Phase 2 US3 native suite at scales ${SCALES[*]} (V-16..V-19)" "per-test runs listed in results.txt"
[[ $overall == PASS ]]

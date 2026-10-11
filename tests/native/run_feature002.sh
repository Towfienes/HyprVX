#!/usr/bin/env bash
# Feature 002 native suite (test-tooling 1.4 section 9; T025, T056, T057). Runs each test in each delivery form at
# each output scale (same logical 1280x800 output: physical size = logical size * scale). Each test writes and archives
# its own evidence run; this aggregate run records every test's run-id and result in results.txt. Any FAIL makes the
# suite FAIL; otherwise any BLOCKED makes it BLOCKED.
#   HVX_TESTS="t_multi ..."  (default: every Feature 002 test)   HVX_FORMS="P S"   HVX_SCALES="1 1.25"
# From fish: env HYPRVX_AGENT=claude bash tests/native/run_feature002.sh
set -uo pipefail
here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source "$here/lib.sh"
set +e  # lib.sh enables errexit; a failing test must be recorded, not end the suite
DEFAULT_TESTS="t_plugin_inert t_multi t_input_mw t_stale_mw t_x11 t_resize t_drift_off t_panic_drift t_idemp t_monitor t_anchor t_pan_drift
t_ws_camera t_ws_anim t_keys t_scroll t_fullscreen t_move t_drag_abort t_spawn"
TESTS=(${HVX_TESTS:-$DEFAULT_TESTS})
FORMS=(${HVX_FORMS:-P S})
SCALES=(${HVX_SCALES:-1 1.25})

hvx_init_run "suite" feature002
failed=0 blocked=0
for form in "${FORMS[@]}"; do
    for scale in "${SCALES[@]}"; do
        for t in "${TESTS[@]}"; do
            log="$HVX_EVID/${t}_form${form}_scale${scale}.log"
            size=$(python3 -I -c "import sys; s = float(sys.argv[1]); print('%dx%d' % (round(1280 * s), round(800 * s)))" "$scale")
            HVX_FORM="$form" HVX_OUTPUT_SCALE="$scale" HVX_OUTPUT_SIZE="$size" "$here/$t.sh" > "$log" 2>&1
            code=$?
            run=$(grep -o 'run=[0-9A-Za-z_.-]*' "$log" | tail -1 | cut -d= -f2)
            case $code in 0) r=PASS ;; 2) r=BLOCKED; blocked=1 ;; *) r=FAIL; failed=1 ;; esac
            printf '%s form=%s scale=%s %s run=%s\n' "$t" "$form" "$scale" "$r" "${run:-none}" | tee -a "$HVX_EVID/results.txt"
        done
    done
done
overall=$( ((failed)) && echo FAIL || { ((blocked)) && echo BLOCKED || echo PASS; })
hvx_finish "$overall" "Feature 002 native suite: ${TESTS[*]} in forms ${FORMS[*]} at scales ${SCALES[*]}" "per-test runs listed in results.txt"
[[ $overall == PASS ]]

#!/usr/bin/env bash
# Feature 003 native suite (test-tooling 1.5 section 9; T023, T027, T031, T038, T039, T041, T043). Runs each test in each delivery form at
# each output scale (same logical 1280x800 output: physical size = logical size * scale). Each test writes and archives
# its own evidence run; this aggregate run records every test's run-id and result in results.txt. Any FAIL makes the
# suite FAIL; otherwise any BLOCKED makes it BLOCKED.
#   HVX_TESTS="t_tp_pan ..."  (default: every Feature 003 test written so far)   HVX_FORMS="P S"   HVX_SCALES="1 1.25"
# From fish: env HYPRVX_AGENT=claude bash tests/native/run_feature003.sh
set -uo pipefail
here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source "$here/lib.sh"
set +e  # lib.sh enables errexit; a failing test must be recorded, not end the suite
DEFAULT_TESTS="t_plugin_inert t_tp_pan t_tp_window t_tp_super t_tp_lock t_tp_surfaces t_tp_off"
TESTS=(${HVX_TESTS:-$DEFAULT_TESTS})
FORMS=(${HVX_FORMS:-P S})
SCALES=(${HVX_SCALES:-1 1.25})

hvx_init_run "suite" feature003
failed=0 blocked=0
for form in "${FORMS[@]}"; do
    for scale in "${SCALES[@]}"; do
        for t in "${TESTS[@]}"; do
            log="$HVX_EVID/${t}_form${form}_scale${scale}.log"
            size=$(python3 -I -c "import sys; s = float(sys.argv[1]); print('%dx%d' % (round(1280 * s), round(800 * s)))" "$scale")
            script=$t
            env HVX_FORM="$form" HVX_OUTPUT_SCALE="$scale" HVX_OUTPUT_SIZE="$size" "$here/$script.sh" > "$log" 2>&1
            code=$?
            run=$(grep -o 'run=[0-9A-Za-z_.-]*' "$log" | tail -1 | cut -d= -f2)
            case $code in 0) r=PASS ;; 2) r=BLOCKED; blocked=1 ;; *) r=FAIL; failed=1 ;; esac
            printf '%s form=%s scale=%s %s run=%s\n' "$t" "$form" "$scale" "$r" "${run:-none}" | tee -a "$HVX_EVID/results.txt"
        done
    done
done
overall=$( ((failed)) && echo FAIL || { ((blocked)) && echo BLOCKED || echo PASS; })
hvx_finish "$overall" "Feature 003 native suite: ${TESTS[*]} in forms ${FORMS[*]} at scales ${SCALES[*]}" "per-test runs listed in results.txt"
[[ $overall == PASS ]]

#!/usr/bin/env bash
# Sprint 1 native suite (V-15, T032): T-ID, US2 (zoom, pan, stale, stale-static, layer) and US4 (off, invalid) at
# output scale 1 and 1.25. Each test writes and archives its own evidence run; this script records which run-id
# each test produced and its result in an aggregate run. Requires GATE-A (T022) and, for US2 judging, GATE-N (T034).
set -uo pipefail
here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source "$here/lib.sh"
TESTS=(t_id t_zoom t_pan t_stale t_stale_static t_layer t_off t_invalid)
SCALES=(${HVX_SCALES:-1 1.25})

hvx_init_run patched sprint1
overall=PASS
for scale in "${SCALES[@]}"; do
    for t in "${TESTS[@]}"; do
        log="$HVX_EVID/${t}_scale${scale}.log"
        HVX_OUTPUT_SCALE="$scale" "$here/$t.sh" > "$log" 2>&1
        code=$?
        run=$(grep -o 'run=[0-9A-Za-z-]*' "$log" | tail -1 | cut -d= -f2)
        case $code in 0) r=PASS ;; 2) r=BLOCKED ;; *) r=FAIL ;; esac
        printf '%s scale=%s %s run=%s\n' "$t" "$scale" "$r" "${run:-none}" | tee -a "$HVX_EVID/results.txt"
        [[ $r == PASS ]] || overall=$([[ $overall == BLOCKED || $r == BLOCKED ]] && echo BLOCKED || echo FAIL)
    done
done
hvx_finish "$overall" "Sprint 1 native suite at scales ${SCALES[*]} (V-15)" "per-test runs listed in results.txt"
[[ $overall == PASS ]]

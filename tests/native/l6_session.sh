#!/usr/bin/env bash
# L6 session helper (Feature 003 T045, checklists/l6-physical.md, V-24). A visible nested session (patched, form P
# plugin, interactive: no headless output, captures refused) with the three scene clients a, b, c and Drift on. A
# human performs the checklist with the real trackpad; Claude drives bookkeeping through the control FIFO
# <evidence>/l6.ctl (one command per line; reads block, no polling):
#   mark NAME        record `hyprctl hyprvx state` and the line count of each client log (axis events for H2/H9)
#   glide VALUE      keyword plugin:hyprvx:glide VALUE
#   off | on         dispatch hyprvx:off / hyprvx:on
#   done RESULT      stop the session and archive the run with RESULT (the human's checklist decides it)
# Nothing is sent to the host. The run records no screen image (captures are refused in interactive mode).
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
HVX_FORM=P

hvx_init_run patched l6_session
log() { printf '%s %s\n' "$(date +%T.%3N)" "$*" >> "$HVX_EVID/steps.log"; }
hvx_start_form --policy --interactive --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "interactive session did not start"; }
hvx_client_start a 300x200
hvx_client_start b 300x200 --app-id hyprvx-testclient-b
hvx_client_start c 300x200 --app-id hyprvx-testclient-c
hvx_dispatch hyprvx:on > "$HVX_EVID/on.reply" 2>&1 || true
hvx_hyprctl_form hyprvx state > "$HVX_EVID/start.state.json" 2>&1 || true
grep -q '"state":"DRIFT"' "$HVX_EVID/start.state.json" || hvx_die_blocked "Drift did not turn on"
log "ready: Drift on, clients a b c"
CTL="$HVX_EVID/l6.ctl"
rm -f "$CTL"; mkfifo -m 600 "$CTL"
echo "L6 ready; control FIFO $CTL"
result=""
while [[ -z $result ]]; do
    while read -r cmd arg; do
        case $cmd in
            mark)
                hvx_hyprctl_form hyprvx state > "$HVX_EVID/$arg.state.json" 2>&1 || true
                for w in a b c; do echo "$w $(grep -c '"ev": "axis"' "$HVX_EVID/$w.jsonl" || true)"; done > "$HVX_EVID/$arg.axis-counts"
                log "mark $arg: $(tr '\n' ' ' < "$HVX_EVID/$arg.axis-counts")" ;;
            glide)
                log "glide $arg -> $(hvx_hyprctl_form keyword plugin:hyprvx:glide "$arg" 2>&1)" ;;
            off|on)
                log "$cmd -> $(hvx_dispatch "hyprvx:$cmd" 2>&1)" ;;
            done)
                result=$arg; log "done $arg"; break ;;
            *) log "unknown command: $cmd $arg" ;;
        esac
    done < "$CTL"
done
hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "L6 physical trackpad session (bookkeeping only; the signed checklist is the result)" \
    "form P; interactive; clients a b c; output $SIZE scale $SCALE"

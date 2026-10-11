#!/usr/bin/env bash
# Archived L1/L4 evidence for Feature 002 checks that start no compositor (T060):
#  V-03  identity refusal of the new tools and harness commands: tests/native/harness/test_identity_refusal.sh with
#        hyprvx-vkeyboard and hyprvx-x11client (xdisplay, plugin load, run --x11 included), and
#        test_vpointer_refusal.sh (hyprvx-vpointer gained `scroll` in this feature).
#  V-28  scripts/check-single-transform.sh: self-test; patches/hyprland/0001–0013; the policy and the form S mechanism
#        sources (platform/plugin/src/{policy,mechanism}, not the generated copied/ bodies, which are the patched
#        bodies already linted as patches) fed as all-added diffs; and a planted `x * camera.zoom` line that must be
#        reported, so the lint is not vacuous.
# From fish: env HYPRVX_AGENT=claude bash tests/native/static_evidence.sh
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"

hvx_init_run static f002_static
result=PASS
fail() { echo "FAIL $*" >> "$HVX_EVID/checks.txt"; result=FAIL; }
ok() { echo "ok   $*" >> "$HVX_EVID/checks.txt"; }
: > "$HVX_EVID/checks.txt"
git -C "$HVX_REPO" status --porcelain -- scripts tools tests platform patches > "$HVX_EVID/source-status.txt"

# V-03
tools=$(hvx_tools_build)
bash "$HVX_REPO/tests/native/harness/test_identity_refusal.sh" "$tools/hyprvx-vkeyboard" "$tools/hyprvx-x11client" \
    > "$HVX_EVID/v03-identity-refusal.log" 2>&1 && ok "V-03 test_identity_refusal.sh" || fail "V-03 test_identity_refusal.sh"
bash "$HVX_REPO/tests/native/harness/test_vpointer_refusal.sh" "$tools/hyprvx-vpointer" \
    > "$HVX_EVID/v03-vpointer-refusal.log" 2>&1 && ok "V-03 test_vpointer_refusal.sh" || fail "V-03 test_vpointer_refusal.sh"

# V-28
lint="$HVX_REPO/scripts/check-single-transform.sh"
bash "$lint" --self-test > "$HVX_EVID/v28-self-test.log" 2>&1 && ok "V-28 lint self-test" || fail "V-28 lint self-test"
bash "$lint" > "$HVX_EVID/v28-patches.log" 2>&1 && ok "V-28 lint 0001-0013" || fail "V-28 lint 0001-0013"
diffs=$(mktemp -d)
trap 'rm -rf "$diffs"' EXIT
for f in "$HVX_REPO"/platform/plugin/src/policy/*.[ch]pp "$HVX_REPO"/platform/plugin/src/mechanism/*.[ch]pp; do
    rel=${f#"$HVX_REPO"/}
    (cd "$HVX_REPO" && git diff --no-index /dev/null "$rel" > "$diffs/${rel//\//_}.patch") || true
done
ls "$diffs" > "$HVX_EVID/v28-plugin-files.txt"
bash "$lint" "$diffs"/*.patch > "$HVX_EVID/v28-plugin.log" 2>&1 \
    && ok "V-28 lint policy + form S mechanism ($(wc -l < "$HVX_EVID/v28-plugin-files.txt") files)" || fail "V-28 lint policy + form S mechanism"
printf 'diff --git a/src/planted.cpp b/src/planted.cpp\n--- /dev/null\n+++ b/src/planted.cpp\n@@ -0,0 +1 @@\n+    const double x = p.x * camera.zoom;\n' \
    > "$diffs/planted.patch"
if bash "$lint" "$diffs/planted.patch" > "$HVX_EVID/v28-planted.log" 2>&1; then
    fail "V-28 planted violation not reported (lint vacuous)"
else
    grep -q 'camera arithmetic outside' "$HVX_EVID/v28-planted.log" && ok "V-28 planted violation reported" \
        || fail "V-28 planted lint run failed without a finding"
fi

cat "$HVX_EVID/checks.txt"
hvx_finish "$result" "Feature 002 V-03 identity refusal and V-28 single-transform lint" "$(grep -c '^ok' "$HVX_EVID/checks.txt") checks ok"
[[ $result == PASS ]]

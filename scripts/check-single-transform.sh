#!/usr/bin/env bash
# Constitution IV lint: the camera transform lives only in the vendored CameraMath.hpp.
# Fails when an added line of any patch, outside src/hyprvx/CameraMath.hpp, does camera arithmetic
# (multiplies or divides by a zoom, or adds/subtracts a camera origin) or spells a second forward/inverse formula.
#   scripts/check-single-transform.sh [PATCH...]      default: patches/hyprland/*.patch
#   scripts/check-single-transform.sh --self-test
set -euo pipefail
export LC_ALL=C

readonly repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
readonly vendored='src/hyprvx/CameraMath.hpp'
# Camera arithmetic on camera fields: "* zoom", "/ z", "zoom *", "origin.x -", "- c.origin.y", "* camera.zoom" ...
# gawk ERE (\y = word boundary); passed through ENVIRON so awk does not process escapes.
readonly pattern='([*/]=?[[:space:]]*([A-Za-z_][A-Za-z0-9_]*(\.|->))*(zoom|z)\y)|(\y(zoom|z)[[:space:]]*[*/])|(\yorigin\.[xy][[:space:]]*[-+*/])|([-+*/][[:space:]]*([A-Za-z_][A-Za-z0-9_]*(\.|->))*origin\.[xy]\y)'

lint() {
    local status=0 patch
    for patch in "$@"; do
        [[ -f "$patch" ]] || { echo "BLOCKED: no such patch: $patch" >&2; return 2; }
        PATTERN="$pattern" gawk -v vendored="$vendored" -v name="$patch" '
            /^diff --git / { file = $4; sub(/^b\//, "", file); next }
            /^\+\+\+ / { next }
            /^\+/ {
                line = substr($0, 2)
                code = line
                sub(/\/\/.*/, "", code)
                if (file != vendored && code ~ ENVIRON["PATTERN"]) {
                    printf "%s: %s: camera arithmetic outside %s: %s\n", name, file, vendored, line
                    bad = 1
                }
            }
            END { exit bad ? 1 : 0 }
        ' "$patch" || status=1
    done
    return "$status"
}

self_test() {
    local dir
    dir=$(mktemp -d)
    trap 'rm -rf -- "$dir"' RETURN
    cat > "$dir/bad.patch" <<'EOF'
diff --git a/src/render/Renderer.cpp b/src/render/Renderer.cpp
--- a/src/render/Renderer.cpp
+++ b/src/render/Renderer.cpp
@@ -1,1 +1,2 @@
+    const double sx = M.x + (W.x - c.origin.x) * c.zoom;
EOF
    cat > "$dir/bad-divide.patch" <<'EOF'
diff --git a/src/managers/input/InputManager.cpp b/src/managers/input/InputManager.cpp
--- a/src/managers/input/InputManager.cpp
+++ b/src/managers/input/InputManager.cpp
@@ -1,1 +1,2 @@
+    local = (pos - M) / camera.zoom;
EOF
    cat > "$dir/good.patch" <<'EOF'
diff --git a/src/hyprvx/CameraMath.hpp b/src/hyprvx/CameraMath.hpp
--- a/src/hyprvx/CameraMath.hpp
+++ b/src/hyprvx/CameraMath.hpp
@@ -1,1 +1,2 @@
+    return {M.x + (W.x - c.origin.x) * c.zoom, M.y + (W.y - c.origin.y) * c.zoom};
diff --git a/src/render/Renderer.cpp b/src/render/Renderer.cpp
--- a/src/render/Renderer.cpp
+++ b/src/render/Renderer.cpp
@@ -1,1 +1,3 @@
+    const auto S = hyprvx::cammath::forward(camera, M, W); // forward is M + (W - P)*z
+    modif.emplace_back(RMOD_TYPE_SCALE, sc<float>(camera.zoom));
EOF
    lint "$dir/bad.patch" > /dev/null && { echo 'SELF-TEST FAIL: bad.patch accepted' >&2; return 1; }
    lint "$dir/bad-divide.patch" > /dev/null && { echo 'SELF-TEST FAIL: bad-divide.patch accepted' >&2; return 1; }
    lint "$dir/good.patch" || { echo 'SELF-TEST FAIL: good.patch rejected' >&2; return 1; }
    echo 'self-test PASS'
}

if [[ ${1:-} == --self-test ]]; then
    self_test
    exit
fi
if [[ $# -eq 0 ]]; then
    shopt -s nullglob
    set -- "$repo"/patches/hyprland/*.patch
    [[ $# -gt 0 ]] || { echo 'BLOCKED: no patches under patches/hyprland' >&2; exit 2; }
fi
lint "$@" && echo "single-transform lint PASS ($# patches)"

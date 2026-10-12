#!/usr/bin/env bash
# Unit-test evidence run. Default suite f002: Feature 002 V-01 (transform 1.2) and V-02 (DriftModel), reusable for the R1
# re-run (T057). Suite f003 (first argument): Feature 003 V-01 (GestureModel, contracts/gesture-model.md 1.0).
# No compositor. A fresh headless build of a copy of this checkout (mutants never touch the working tree): ctest;
# CameraMath and DriftModel tests with the default seed and seed 987654321; and mutation checks (each mutant must be
# applied and make the suite fail). Evidence: ~/.cache/hyprvx/<agent>/evidence/<run-id>/, archived by hvx_finish.
# From fish: env HYPRVX_AGENT=claude bash tests/unit/run_unit_evidence.sh [f002|f003]
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/../native/lib.sh"

suite=${1:-f002}
case $suite in f002|f003) ;; *) echo "usage: run_unit_evidence.sh [f002|f003]" >&2; exit 2 ;; esac
hvx_init_run unit "${suite}_unit"
cache="$HOME/.cache/hyprvx/$HYPRVX_AGENT"
mkdir -p "$cache/build"
work=$(mktemp -d "$cache/build/unit-XXXXXXXX")
rsync -a --exclude .git --exclude node_modules --exclude 'build*' --exclude evidence --exclude research "$HVX_REPO/" "$work/src/"
git -C "$HVX_REPO" status --porcelain -- core tests CMakeLists.txt > "$HVX_EVID/source-status.txt"

configure() { cmake -S "$1" -B "$2" -G Ninja -DHYPRVX_WITH_SDL=OFF -DHYPRVX_WITH_WAYLAND_PLATFORM=OFF -DHYPRVX_WITH_HYPRLAND_PLUGIN=OFF; }
configure "$work/src" "$work/build" > "$HVX_EVID/configure.log" 2>&1 || hvx_die_blocked "configure failed"
cmake --build "$work/build" -j4 > "$HVX_EVID/build.log" 2>&1 || hvx_die_blocked "build failed"
result=PASS
ctest --test-dir "$work/build" --output-on-failure > "$HVX_EVID/ctest.log" 2>&1 || result=FAIL
if [[ $suite == f003 ]]; then
    filter='GestureTarget.*:ScrollTracker.*:ReleaseVelocity.*:GlideDamping.*:ExpectedGlide.*:Glide.*:PinchCamera.*'
else
    filter='CameraMathTest.*:DriftModelTest.*'
fi
"$work/build/hyprvx_core_tests" --gtest_filter="$filter" > "$HVX_EVID/core_tests_seed_default.log" 2>&1 || result=FAIL
HYPRVX_TEST_SEED=987654321 "$work/build/hyprvx_core_tests" --gtest_filter="$filter" > "$HVX_EVID/core_tests_seed_987654321.log" 2>&1 || result=FAIL

# file ;; sed expression ;; what the mutant breaks
if [[ $suite == f003 ]]; then
mutants=(
    'core/src/GestureModel.cpp;;s/if (ctx.layerUnderPointer || ctx.fullscreenScene || ctx.dragActive)/if (ctx.fullscreenScene || ctx.dragActive)/;;3-finger gesture over a layer pans instead of being ignored'
    'core/src/GestureModel.cpp;;s/elapsed(timeMs, last_) > kIdleMs/elapsed(timeMs, last_) >= kIdleMs/;;idle boundary off by one'
    'core/src/GestureModel.cpp;;s/if (!moving_\[0\] \&\& !moving_\[1\]) {/if (!moving_[0] || !moving_[1]) {/;;sequence ends when one of two moving axes stops'
    'core/src/GestureModel.cpp;;s/if (newest.timeMs == timeMs) {/if (false) {/;;equal timestamps not merged'
    'core/src/GestureModel.cpp;;s/> kMaxRestMs)/> kMaxRestMs + 10)/;;rest limit 60 ms'
    'core/src/GestureModel.cpp;;s/<= kWindowMs)/<= kWindowMs + 30)/;;window 130 ms'
    'core/src/GestureModel.cpp;;s/2.0 + 10.0 \* (1.0 - std::min(strength, 1.0))/2.0 + 10.0 * (1.0 - strength)/;;strength above 1 not capped'
    'core/src/GestureModel.cpp;;s/const double targetZoom = start.zoom \* scale;/const double targetZoom = scale;/;;pinch zoom not relative to the start zoom'
    'core/src/GestureModel.cpp;;s/motion.stopSpeed = cfg.stopSpeed;/motion.stopSpeed = 50.0;/;;glide stop speed ignored'
)
else
mutants=(
    'core/include/hyprvx/core/CameraMath.hpp;;s/{{c.origin.x - O.x, c.origin.y - O.y}, c.zoom}/{{c.origin.x + O.x, c.origin.y + O.y}, c.zoom}/;;offsetCamera adds the offset'
    'core/include/hyprvx/core/CameraMath.hpp;;s/return {d.x \/ c.zoom, d.y \/ c.zoom};/return {d.x * c.zoom, d.y * c.zoom};/;;worldDelta multiplies by the zoom'
    'core/src/DriftModel.cpp;;s/^void DriftModel::panic() {$/void DriftModel::panic() {\n    offsets_.clear();/;;panic clears stored offsets'
    'core/src/DriftModel.cpp;;s/return mapped \&\& scene \&\& \*scene == workspace/return scene \&\& *scene == workspace/;;mechanismOffset ignores unmapped windows'
    'core/src/DriftModel.cpp;;s/    if (s.mode == Mode::Entering || s.mode == Mode::Drift)/    if (s.mode == Mode::Entering)/;;enable in DRIFT is not a no-op'
    'core/src/DriftModel.cpp;;s/return setOffset(drag_->window, {drag_->startOffset.x + world.x, drag_->startOffset.y + world.y});/return setOffset(drag_->window, {drag_->startOffset.x + delta.x, drag_->startOffset.y + delta.y});/;;move drag ignores the zoom'
)
fi
killed=0
: > "$HVX_EVID/mutation.log"
for entry in "${mutants[@]}"; do
    file=${entry%%;;*}
    rest=${entry#*;;}
    expr=${rest%;;*}
    what=${rest##*;;}
    mdir=$(mktemp -d "$work/mutant-XXXX")
    cp -a "$work/src" "$mdir/src"
    sed -i "$expr" "$mdir/src/$file"
    if cmp -s "$work/src/$file" "$mdir/src/$file"; then
        echo "NOT APPLIED: $what" >> "$HVX_EVID/mutation.log"
        result=FAIL
        continue
    fi
    if configure "$mdir/src" "$mdir/build" > "$mdir/configure.log" 2>&1 \
        && cmake --build "$mdir/build" -j4 --target hyprvx_core_tests > "$mdir/build.log" 2>&1 \
        && "$mdir/build/hyprvx_core_tests" --gtest_filter="$filter" > "$mdir/test.log" 2>&1; then
        echo "SURVIVED: $what" >> "$HVX_EVID/mutation.log"
        result=FAIL
    else
        killed=$((killed + 1))
        echo "killed: $what ($(grep -h -m1 -E 'FAILED  \]|error:' "$mdir/test.log" "$mdir/build.log" 2> /dev/null | head -1))" >> "$HVX_EVID/mutation.log"
    fi
done
echo "mutants killed: $killed/${#mutants[@]}" >> "$HVX_EVID/mutation.log"
if [[ $suite == f003 ]]; then label="Feature 003 V-01 GestureModel unit tests"; else label="Feature 002 V-01 transform 1.2 and V-02 DriftModel unit tests"; fi
hvx_finish "$result" "$label" \
    "$(grep -h 'tests passed' "$HVX_EVID/ctest.log"); seeds default and 987654321; mutants $killed/${#mutants[@]}"
[[ $result == PASS ]]

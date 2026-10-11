#!/usr/bin/env bash
# Unit-test evidence run for Feature 002 V-01 (transform 1.2) and V-02 (DriftModel), reusable for the R1 re-run (T057).
# No compositor. A fresh headless build of a copy of this checkout (mutants never touch the working tree): ctest;
# CameraMath and DriftModel tests with the default seed and seed 987654321; and mutation checks (each mutant must be
# applied and make the suite fail). Evidence: ~/.cache/hyprvx/<agent>/evidence/<run-id>/, archived by hvx_finish.
# From fish: env HYPRVX_AGENT=claude bash tests/unit/run_unit_evidence.sh
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/../native/lib.sh"

hvx_init_run unit f002_unit
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
filter='CameraMathTest.*:DriftModelTest.*'
"$work/build/hyprvx_core_tests" --gtest_filter="$filter" > "$HVX_EVID/core_tests_seed_default.log" 2>&1 || result=FAIL
HYPRVX_TEST_SEED=987654321 "$work/build/hyprvx_core_tests" --gtest_filter="$filter" > "$HVX_EVID/core_tests_seed_987654321.log" 2>&1 || result=FAIL

# file ;; sed expression ;; what the mutant breaks
mutants=(
    'core/include/hyprvx/core/CameraMath.hpp;;s/{{c.origin.x - O.x, c.origin.y - O.y}, c.zoom}/{{c.origin.x + O.x, c.origin.y + O.y}, c.zoom}/;;offsetCamera adds the offset'
    'core/include/hyprvx/core/CameraMath.hpp;;s/return {d.x \/ c.zoom, d.y \/ c.zoom};/return {d.x * c.zoom, d.y * c.zoom};/;;worldDelta multiplies by the zoom'
    'core/src/DriftModel.cpp;;s/^void DriftModel::panic() {$/void DriftModel::panic() {\n    offsets_.clear();/;;panic clears stored offsets'
    'core/src/DriftModel.cpp;;s/return mapped \&\& scene \&\& \*scene == workspace/return scene \&\& *scene == workspace/;;mechanismOffset ignores unmapped windows'
    'core/src/DriftModel.cpp;;s/    if (s.mode == Mode::Entering || s.mode == Mode::Drift)/    if (s.mode == Mode::Entering)/;;enable in DRIFT is not a no-op'
    'core/src/DriftModel.cpp;;s/return setOffset(drag_->window, {drag_->startOffset.x + world.x, drag_->startOffset.y + world.y});/return setOffset(drag_->window, {drag_->startOffset.x + delta.x, drag_->startOffset.y + delta.y});/;;move drag ignores the zoom'
)
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
hvx_finish "$result" "Feature 002 V-01 transform 1.2 and V-02 DriftModel unit tests" \
    "$(grep -h 'tests passed' "$HVX_EVID/ctest.log"); seeds default and 987654321; mutants $killed/${#mutants[@]}"
[[ $result == PASS ]]

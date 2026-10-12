# Quickstart: Trackpad-Native Drift UX (Feature 003)

Validation guide; commands are run from the worktree root in the user's fish shell unless noted. Native steps need
the host GPU profile Integrated and follow the heavy-lock rules (master plan §O5). `HYPRVX_AGENT=claude`.

## 1. Unit tests (no compositor)

```fish
cmake -S . -B build-v3 -G Ninja -DHYPRVX_WITH_SDL=OFF -DHYPRVX_WITH_WAYLAND_PLATFORM=OFF -DHYPRVX_WITH_HYPRLAND_PLUGIN=OFF
cmake --build build-v3 -j4; and ctest --test-dir build-v3 --output-on-failure
```

Expected: all tests pass, including `test_gesture_model` (target table, scroll tracker, release velocity, glide
invariants, pinch camera; `contracts/gesture-model.md`).

## 2. Build

```fish
env HYPRVX_AGENT=claude bash scripts/build-plugin.sh --form P
env HYPRVX_AGENT=claude bash scripts/build-plugin.sh --form S
env HYPRVX_AGENT=claude bash scripts/build-plugin.sh --form S --dev
env HYPRVX_AGENT=claude bash scripts/build-plugin.sh --testinput --form P
env HYPRVX_AGENT=claude bash scripts/build-plugin.sh --testinput --form S
```

The patched build (`patched`, `patched-nodev`) is unchanged from Feature 002 (patch series 0001–0013); rebuild it
only if the prefix is missing (`python3 -I tools/hyprvx_build.py build --variant patched`).

## 3. Static checks and tool tests

```fish
bash scripts/check-single-transform.sh --self-test; and bash scripts/check-single-transform.sh
python3 -I tools/tests/test_analyze_frame.py
for s in fetch-hyprland.sh build-hyprland.sh build-plugin.sh apply-patches.sh; env HYPRVX_AGENT=claude python3 -I tools/tests/test_build_scripts.py $s; end
bash tests/native/static_evidence.sh
```

## 4. Native suite

```fish
env HYPRVX_AGENT=claude bash tests/native/gate_a_stock_determinism.sh    # after the §5 config change
env HYPRVX_AGENT=claude bash tests/native/run_feature003.sh              # T-TP-* × forms P, S × scales 1, 1.25
env HYPRVX_AGENT=claude bash tests/native/run_feature002.sh              # regression
for F in P S; env HYPRVX_AGENT=claude HVX_REGRESSION_FORM=$F bash tests/native/run_sprint1.sh; env HYPRVX_AGENT=claude HVX_REGRESSION_FORM=$F bash tests/native/run_phase2.sh; end
python3 -I tests/native/compare_forms.py --form-p <P suite> --form-s <S suite>
```

Every run prints its run-id; `sha256sum -c SHA256SUMS` in each archive before citing it.

## 5. Try it by hand (nested, physical L6)

```fish
env HYPRVX_AGENT=claude bash scripts/nested.sh start --variant patched --interactive   # visible nested window
env HYPRVX_AGENT=claude bash scripts/nested.sh plugin load ~/.cache/hyprvx/claude/plugin/P/hyprvx-drift-P.so
env HYPRVX_AGENT=claude bash scripts/nested.sh dispatch hyprvx:on
```

Then follow `checklists/l6-physical.md`: two-finger pan on empty canvas, two-finger scroll over a window, Super +
two-finger pan, fast release glides and stops, touching again stops it. **Never make 3- or 4-finger gestures over the
nested window** (the host closes windows, switches workspaces or suspends on them). Stop with
`scripts/nested.sh stop`.

# Quickstart: DriftWM Window Behavior (Feature 004)

Validation guide. All compositor work runs in nested sessions through the harness (`scripts/nested.sh`), never on the
host. Details: `contracts/`, `data-model.md`.

## 1. Unit tests (no compositor)

```fish
cmake -S . -B build-v3 -G Ninja -DHYPRVX_WITH_SDL=OFF -DHYPRVX_WITH_WAYLAND_PLATFORM=OFF -DHYPRVX_WITH_HYPRLAND_PLUGIN=OFF
cmake --build build-v3 -j4; and ctest --test-dir build-v3 --output-on-failure
env HYPRVX_AGENT=claude bash tests/unit/run_unit_evidence.sh f004      # two seeds + mutation check, archived
```

Expected: every test passes, including `test_window_behavior` (snap attach/hold/release on slow paths, groups,
navigation, framing, history, bookmarks).

## 2. Build (patch 0014 added to the series)

```fish
env HYPRVX_AGENT=claude bash scripts/apply-patches.sh --variant patched --reset
env HYPRVX_AGENT=claude bash scripts/build-hyprland.sh --variant patched        # new patched prefix id
env HYPRVX_AGENT=claude bash scripts/build-plugin.sh --form P
env HYPRVX_AGENT=claude bash scripts/build-plugin.sh --form S
env HYPRVX_AGENT=claude bash scripts/build-plugin.sh --form S --dev
```

## 3. Static checks

```fish
env HYPRVX_AGENT=claude bash tests/native/static_evidence.sh   # identity refusal (new vkeyboard keys), lint incl. 0014 and WindowBehavior
```

## 4. Native suite

```fish
env HYPRVX_AGENT=claude bash tests/native/gate_a_stock_determinism.sh
env HYPRVX_AGENT=claude bash tests/native/run_feature004.sh     # t_snap t_snap_osc t_group t_nav t_pin t_wb_off × forms P, S × scales 1, 1.25
env HYPRVX_AGENT=claude bash tests/native/run_feature003.sh     # regression
env HYPRVX_AGENT=claude bash tests/native/run_feature002.sh     # regression
python3 -I tests/native/compare_forms.py --suite-p <suite dir> --suite-s <suite dir> --out report.json
```

Expected: every test PASS in both forms and scales; captures byte-identical between forms (exclusions only with a
recorded reason); Hyprland's window model unchanged and no `movewindow` event in every test.

## 5. Try it by hand (nested)

`tests/native/l6_session.sh` opens an interactive nested session (Drift on, three windows). Keys must come from the
harness keyboard (`hvx_combo`), because the host takes Super combinations over the nested window (Feature 003 T045).

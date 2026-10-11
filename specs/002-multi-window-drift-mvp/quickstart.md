# Quickstart: Multi-Window Drift MVP (validation guide)

Commands are for fish on the CachyOS host. Everything compositor-related runs in the nested session; nothing touches
the host Hyprland or Caelestia. Prerequisites and safety rules are those of
`specs/001-native-spatial-camera/quickstart.md`: agent cache `~/.cache/hyprvx/claude/`, heavy lock, MemAvailable
checks, and the host GPU profile Integrated (AMD only) for native runs.

## 1. Unit tests (no compositor)

```fish
cmake -S . -B build-v3 -G Ninja -DHYPRVX_WITH_SDL=OFF -DHYPRVX_WITH_WAYLAND_PLATFORM=OFF -DHYPRVX_WITH_HYPRLAND_PLUGIN=OFF
cmake --build build-v3 -j4; and ctest --test-dir build-v3 --output-on-failure
```

Expected: all tests pass, including the transform 1.2 invariants (`offsetCamera`, `worldDelta`) and `DriftModel`.

## 2. Build both delivery forms

```fish
set -x HYPRVX_AGENT claude
bash scripts/apply-patches.sh --variant patched --reset; and bash scripts/build-hyprland.sh --variant patched
bash scripts/build-plugin.sh --form P    # policy plugin against the patched prefix
bash scripts/build-plugin.sh --form S    # policy + mechanism hooks against the stock prefix
```

Expected: `build-info.json` in the patched prefix with the new patchset id, and two plugins, each with its own
`build-info.json` recording the plugin sha256 and its source hash.

## 3. Lint and tool tests

```fish
bash scripts/check-single-transform.sh --self-test; and bash scripts/check-single-transform.sh
python3 -I tools/tests/test_analyze_frame.py
bash tests/native/harness/test_identity_refusal.sh
```

## 4. Try Drift Mode by hand (nested only)

```fish
bash scripts/nested.sh start --variant patched --size 1280x800 --scale 1
bash scripts/nested.sh plugin load ~/.cache/hyprvx/claude/plugin/P/hyprvx-drift-P.so
bash -c 'source tests/native/lib.sh; HVX_EVID=/tmp hvx_client_start a 800x600'
bash -c 'source tests/native/lib.sh; hvx_hyprctl patched dispatch hyprvx:toggle'
bash -c 'source tests/native/lib.sh; hvx_hyprctl patched dispatch hyprvx:zoom 0.5'
bash scripts/nested.sh capture /tmp/drift-zoomed.png
bash -c 'source tests/native/lib.sh; hvx_hyprctl patched dispatch hyprvx:off'
bash scripts/nested.sh stop
```

Expected: `hyprvx:toggle` turns Drift on (`hyprctl hyprvx state` shows `DRIFT`). `hyprvx:zoom 0.5` zooms out at the
pointer, and after `hyprvx:off` the view is the stock view again.

## 5. Native suites

```fish
bash tests/native/run_feature002.sh          # §9 tests, forms P and S, scales 1 and 1.25
bash tests/native/run_sprint1.sh; and bash tests/native/run_phase2.sh   # Feature 001 regression, plugin loaded, Drift OFF
bash tests/native/t_scenario.sh              # 12-step scenario captures for human review (SC-008)
```

Expected: every test PASS with archived evidence (`~/.local/share/hyprvx/evidence/claude/<run-id>/`, `sha256sum -c`
OK). The scenario image set goes to `checklists/verification.md` for the user's review.

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
bash scripts/build-plugin.sh --form S --dev            # form S with the dev control, used by the nested tests
bash scripts/build-hyprland.sh --variant patched-nodev # patched build without the dev control (FR-016, t_fr016)
```

For the R1 clean-room rebuild (T057) run the same commands from a fresh `git worktree` with `env CCACHE_DISABLE=1`.

Expected: `build-info.json` in the patched prefix with the new patchset id, and two plugins, each with its own
`build-info.json` recording the plugin sha256 and its source hash.

## 3. Lint and tool tests

```fish
bash scripts/check-single-transform.sh --self-test; and bash scripts/check-single-transform.sh
python3 -I tools/tests/test_analyze_frame.py
for s in fetch-hyprland.sh build-hyprland.sh build-plugin.sh apply-patches.sh; python3 -I tools/tests/test_build_scripts.py $s; end
bash tests/native/static_evidence.sh   # archived V-03 (identity refusal, with the tool paths) and V-28 (lint, incl. policy)
```

`tests/native/harness/test_identity_refusal.sh` needs the vkeyboard and x11client paths as arguments; without them it
reports those cases NOT RUN and FAILS, so use `static_evidence.sh`, which builds the tools and passes them.

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
bash tests/native/run_feature002.sh          # §9 tests incl. t_scenario, forms P and S, scales 1 and 1.25
for f in P S; env HVX_REGRESSION_FORM=$f bash tests/native/run_sprint1.sh; env HVX_REGRESSION_FORM=$f bash tests/native/run_phase2.sh; end
                                             # Feature 001 regression with plugin $f loaded and Drift OFF (V-24)
bash tests/native/t_fr016.sh                 # dev control absent from form S and patched-nodev (FR-016)
bash tests/native/host_safety_audit.sh       # V-26, read-only, after the last nested session
python3 -I tests/native/compare_forms.py --suite-p <suite> --suite-s <suite> --out /tmp/parity.json   # V-23
```

Without `HVX_REGRESSION_FORM`, `run_sprint1.sh` and `run_phase2.sh` run the patched build with no plugin (the Feature 001
suites as they were), which is not the V-24 check.

Expected: every test PASS with archived evidence (`~/.local/share/hyprvx/evidence/claude/<run-id>/`, `sha256sum -c`
OK). The scenario image set goes to `checklists/verification.md` for the user's review.

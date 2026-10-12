# HyprVX Current State

## Phase

Phase 2E complete - user-driven runtime camera navigation over durable spatial projection

## Completed

- project concept and architecture defined
- production Hyprland/Caelestia configuration backed up
- SHA256 backup verification completed
- readonly Btrfs root snapshot created
- readonly Btrfs home snapshot created
- initial real-workload performance baseline captured
- project brain v1 created
- toolchain audit completed
- implementation stack selected: C++20 core + GTest
- Vec2 implemented
- Rect implemented
- Camera implemented
- free window model implemented
- Island implemented
- World implemented
- headless simulator implemented
- 12 deterministic unit tests passing

## Phase 1A validation

Build:
PASS

Unit tests:
12 / 12 PASS

Simulator:
PASS

Validated:
- negative world coordinates
- large world coordinates
- world/screen round trip
- cursor-anchor-preserving zoom math
- island local-to-world transform
- island movement preserving local member geometry
- island world bounds
- free window storage
- island storage

## Current host

- CachyOS
- Hyprland 0.56.2
- Wayland
- Fish shell
- Btrfs

## Production safety

No HyprVX compositor plugin is loaded.

No production Hyprland or Caelestia configuration has been modified.

## Phase 1B.1 validation

Build:
PASS

Unit tests:
20 / 20 PASS

Camera motion:
PASS

Validated:
- frame-rate-independent manual motion at 60 / 120 / 210 Hz
- frame-rate-independent inertia
- frame-rate-independent target flight
- no target overshoot
- normalized pan input
- velocity limiting
- zoom clamping
- cursor-anchor-preserving zoom
- invalid delta rejection

Observed simulator frame-rate delta 60 -> 210 Hz:
0.000000

## Phase 2A.1 validation

Build:
PASS

Full test suite:
104 / 104 PASS

Validated event probe behavior:
- explicit runtime socket discovery and `/tmp/hypr` fallback
- candidate ordering, duplicate removal and actionable errors
- fragmented and batched event lines
- CRLF, invalid UTF-8 and trailing partial event handling
- typed C++ parsing for the eight observed Hyprland 0.56.2 event formats
- unknown event preservation and malformed event rejection
- Hyprland IPC parser remains in a separate adapter library, outside hyprvx_core

Production Hyprland and Caelestia configuration remain unmodified.

## Phase 2A.3 validation

Build:
PASS

Full test suite:
112 / 112 PASS

Validated lifecycle events:
- openwindow fields split at the first three commas, preserving commas in titles
- closewindow address parsing
- timestamp-prefixed lifecycle events
- malformed openwindow payload remains an UnknownEvent with original payload
- urgent remains UnknownEvent because its schema has not been captured

Hyprland-specific parsing remains isolated in the IPC adapter library.

## Phase 2A.5 validation

Build:
PASS

Full test suite:
124 / 124 PASS

Validated captured events:
- movewindow and movewindowv2
- createworkspace and createworkspacev2
- destroyworkspace and destroyworkspacev2
- v2 workspace names preserve commas after the required leading fields
- malformed known workspace/move events remain UnknownEvent
- uncaptured monitoraddedv2 remains UnknownEvent

Timestamp-prefixed probe lines continue to parse.

## Phase 2B validation

Build:
PASS

Full test suite:
179 / 179 PASS

Validated:
- SemanticEventAdapter maps structured events and suppresses captured legacy/v2 duplicates
- RuntimeMirror safely tracks partial windows, workspace state and focus
- blocking socket2 reader frames split, combined, CRLF and trailing partial input
- event lines are capped at 64 KiB; oversize unterminated frames fail safely
- runtime socket path is preferred, with `/tmp/hypr` fallback when needed
- `hyprvx-ipc-watch --file` replay uses the same parser, adapter and mirror path
- read-only live socket smoke test connected and waited for events
- address-first `changefloatingmode` format and empty activewindowv2 focus-clear event
- malformed recognized payloads are preserved as UnknownEvent
- canonical hex window identity, merge-preserved out-of-order metadata, and bounded close tombstones
- tombstoned late metadata is suppressed from mirror state and semantic watch output
- mirror reset clears state for a new connection epoch
- runtime state contains metadata only; socket2 geometry is not inferred

Runtime integration stays outside `hyprvx_core`; it does not mutate `World`.
Runtime policy targets one active output, with external DP taking exclusive
priority while connected. Monitor hotplug typing remains deferred pending
hardware verification. Production Hyprland configuration and Caelestia remain
unmodified.

## Phase 2C validation

Configure/build: PASS

Full CTest suite: 207 / 207 PASS

Validated:
- separate `hyprvx_runtime_projection` library depends on the IPC adapter and compositor-agnostic core
- canonical address to collision-checked monotonic `WindowId` mapping with close cleanup and new-open lifetime
- stable numeric, named, and special workspace region assignment with configurable non-overlapping placement
- finite policy-derived workspace bounds that expand to contain allocated grid rows
- validation coverage for invalid placement policies and workspace names; multi-row placement geometry is deterministic and non-overlapping
- explicit 1000x700 default HyprVX-owned projected geometry, configurable workspace stride and grid placement
- metadata-only partial windows remain deferred until workspace metadata is known; pre-open move state projects safely
- idempotent reconciliation, metadata enrichment, workspace relocation, focus/focus-clear, and tombstoned close handling
- World removal safely handles free windows and island members while preserving single ownership
- camera framing helpers reuse `ViewportFraming`; no camera is automatically driven
- `hyprvx-project-replay` successfully projected a captured real Hyprland lifecycle log through the parser, adapter, mirror, and projector

No compositor mutation, plugin, polling, production config change, or multi-output expansion was introduced.

## Phase 2D validation

Configure/build: PASS

Full CTest suite: 231 / 231 PASS

Validated:
- schema-v1 deterministic JSON save/load, strict geometry and identity validation, missing/corrupt/future-version handling, and stale temporary file tolerance
- same-directory fsync/atomic rename, last-good `.bak`, corruption preservation, and explicit invalid-state replacement
- same-compositor address restoration, address reuse rejection after new-open observation, title changes, unique metadata fallback, and ambiguous-match refusal
- persisted numeric/named/special workspace positions and placement counters continue after reconstruction
- disconnected bridge epochs clear mirror/tombstones; projector epoch end clears World while retaining dormant layouts
- event-driven dirty revision and controllable debounce; metadata/focus churn does not schedule saves
- CLI save/destroy/recreate/load/replay reproduced the same World summary and geometry

## Phase 2D final audit hardening

- all persisted uint64 IDs and counters use strict canonical decimal strings, preserving values through UINT64_MAX
- corrupt diagnostic snapshots use unique `.corrupt.N` suffixes and preserve earlier files
- state debounce uses a configurable maximum dirty age in addition to the normal quiet period
- persistence bounds geometry and placement arithmetic to 1e12 world units; in-memory world coordinates remain double-based
- direct projector restoration rejects inconsistent placement slots
- full configure/build and CTest validation rerun after audit fixes

No compositor commands, polling, plugin, production config changes, or automatic reconnect were added.

## Phase 2E validation

Configure/build: PASS

Full CTest suite: 261 / 261 PASS

Validated runtime camera navigation:
- separate runtime navigation library reuses CameraController, CameraHistory, SpatialNavigator, and ViewportFraming without adding daemon dependencies to hyprvx_core
- semantic pan gestures create one history entry; zero-delta pan creates none; keyboard pan is delta-time driven and settles without overshoot
- projected window/address and numeric, named, special, and restored workspace navigation; missing targets fail without creating World state
- directional selection works in all four directions, follows projected geometry, and remains distinct from observed compositor focus
- observed focus seeds selection until explicit user navigation takes ownership; selected close/reconcile and epoch reset are safe
- history back/forward, branch truncation, bounded capacity, transition replacement, and direct-pan flight interruption
- Home prefers focused projected window, then supplied focused workspace, nearest known projected window, and origin fallback
- SDL visual simulator routes mouse pan, keyboard pan, spatial focus, history, zones, and Home through the runtime controller
- simulator `--capture` replays socket2 fixtures through RuntimeBridge and SpatialProjector, draws projected windows/workspace outlines, and accepts optional `--state` restore
- captured runtime event replay plus navigation produced deterministic final camera state

Safe visual smoke: SDL dummy-video simulator launch passed; it printed controls
and waited for input without connecting to Hyprland. Idle camera navigation
waits for events and only advances while input, velocity, or a transition is active.

No camera position, animation, velocity, or gesture state is persisted. No
Hyprland window/workspace control, production config, plugin, polling, or
multi-output camera behavior was added.

## Phase 2F implementation

Runtime input now has a device-independent `RuntimeNavigationAction` model,
`RuntimeNavigationActionDispatcher`, and keyboard, mouse, and gesture
adapters. SDL key/button decoding is isolated in the simulator adapter. Mouse
middle drag and keyboard movement use the shared semantic dispatcher; pan
deltas coalesce while SDL drains an event batch. Wheel zoom uses anchored
semantic zoom actions. Window clicks set HyprVX selection without framing or
changing compositor focus.

Keyboard simultaneous directions and modifier releases, mouse drag/click/
wheel/reset, three-finger pan, pinch, four-finger swipe/reset, invalid gesture
ordering, deterministic directional ties, Home selection priority, input
interruption/focus loss, stale pan-end, epoch reset, mouse/gesture equivalence,
and keyboard-driven camera movement are covered by 24 input tests. The audit
added 8 tests; full CTest is 285 / 285 PASS. Build: PASS. `git diff --check`:
PASS. SDL dummy-video capture smoke: PASS; the simulator loaded the fixture
and blocked on events while idle. `hyprvx-project-replay` run twice against
the same fixture produced byte-identical World output.

Selection ownership is explicit: observed focus seeds while compositor-owned;
selection made by user navigation owns Home/directional selection until reset.
Camera control remains local to HyprVX with one active output. There is no
plugin, compositor command, polling loop, or production config change.
Physical trackpad acquisition remains deferred; semantic gesture adapters are
implemented and testable.

## Phase 3 nested native prototype

The Phase 3 worktree adds an isolated nested Hyprland harness, a reusable
PID/socket peer safety guard, a Wayland layer-shell overlay with SHM rendering,
native XKB/pointer/optional gesture input, and the existing socket2 runtime
projection path. It also adds an exact-commit-guarded Hyprland plugin with
read-only event callbacks, a credential-checked JSON datagram bridge, actual
geometry observation separate from World, and nested-only semantic controls.
The transform API limitation is recorded in `docs/NATIVE_PHASE3.md`; unsafe
native content transform activation is refused and zoom is deferred.
Wayland callbacks contain C++ exceptions at the C ABI boundary; failures stop
the overlay, and native control-signal masks are restored during cleanup.

Nested smoke verified compositor startup, sample foot clients, layer
registration, socket2 plus authenticated plugin events, semantic control pan,
transform refusal, reset, repeated plugin unload/reload, and compositor
survival. A second isolated start/stop also passed. Native physical input is
wired but was not automatically injected in the smoke environment. A clean
Debug configure/build and the worktree build passed. Full CTest: 298 / 298 on
both builds. The native overlay used 0 CPU ticks in a five-second idle sample;
the nested compositor with the plugin loaded also used 0 ticks over five
seconds. `git diff --check`: PASS. No commit or host integration was made.

## Next

Phase 3 nested native prototype is implemented and validated. The remaining
native compositor transform, inverse hit testing, and true zoom depend on a
Hyprland API that can keep rendered content, popups, and input coordinates in
agreement. Physical input was wired through the existing adapters, but live
pointer/keyboard injection was not part of the safe nested smoke. Continue
development only in the simulator and nested compositor until those gaps are
resolved.


## V3 planning baseline (2026-10-10, worktree HyprVX-v3, branch v3/spec-foundation)

Architecture approved by the user. **No implementation has started.**

Done in the planning session (documentation only):
- Read-only audit of V1 (`dev@15f75b0`), V2 (`v2/drift-mvp@31086d7`, nested `14d1232`) and upstream Hyprland
  `efb5099`, with file:line evidence in `specs/001-native-spatial-camera/research.md`.
- V2 visual failure root causes identified from source: RC1 (surface scissor uses an untransformed box) and
  RC2 (pass simplify uses untransformed bounds). The "renderModif overwritten" hypothesis is refuted for the
  default path.
- V1 "298/298 PASS" came from stale binaries. `build/CMakeCache.txt` points at the deleted `/tmp/hyprland-src`.
  No V1 test measures compositor behavior.
- V1 Phase 4 patch and V2 patch copied to `research/` with matching SHA-256.
- Spec Kit initialised; constitution v1.0.0; feature 001 spec/plan/research/data-model/contracts/quickstart/checklist.
- Master plan: `docs/v3/HYPRVX_V3_MASTER_PLAN.md`.

Not run in this session: any build, any test, any nested or host compositor command (status NOT RUN).

Production safety: host Hyprland and Caelestia untouched; no plugin loaded; no package changes.

## GitHub handoff (2026-10-10)

- Publication target: private repository `https://github.com/Towfienes/HyprVX`, branch `v3/spec-foundation`.
- Local worktree remains `HyprVX-v3`; repository naming does not change its path.
- V3 planning documents, Spec Kit skills and read-only research references are included in the handoff.
- `CLAUDE.md` directs Claude Code to the shared agent instructions and current feature.
- Gitleaks found no secrets in the working tree or the 27 existing commits on this branch.
- Build/compositor verification: NOT RUN; this milestone changes documentation and Git publication only.

## V3 collaboration replan (2026-10-10)

- User approved the hybrid Claude Code + Codex strategy and changes P1–P10 (DV3-09..DV3-13).
- Applied on `v3/spec-foundation`: spec US2.2/US2.5/FR-012/SC-002, plan Phase 0/1/2 updates,
  `transform-contract.md` v1.1, new `contracts/test-tooling.md` v1.0, verification V-12b/V-23/V-24/V-25,
  master plan §H (U0), §I, §J, §K, §L and new §O.
- Least-privilege audit: Codex's `workspace-write` sandbox keeps `.git` read-only (verified with `codex sandbox`
  on scratch repos). Codex will use a separate `--no-hardlinks` clone with no access to the shared `.git`.
- Next gate: user approval of `specs/001-native-spatial-camera/tasks.md` on branch `v3/001-native-spatial-camera`.
- Build/compositor verification: NOT RUN (documentation and task generation only).

## Pre-approval consistency fixes (2026-10-11)

- Evidence handoff: raw evidence in `~/.cache/hyprvx/<agent>/evidence/<run-id>/`; Claude imports and verifies
  (`sha256sum -c`) into the retained archive `~/.local/share/hyprvx/evidence/<agent>/<run-id>/` before ticking.
- GATE-N: PASS only with S1/S2/P/N/H (`contracts/test-tooling.md` §6.1); analyzer defect, invalid setup and
  unreproduced V2 defect are classified separately and never PASS.
- `tools/vpointer` (T054) is fully inside the blocked Phase 2; no early start.
- Build/compositor verification: NOT RUN.

## Sprint 1 implementation (2026-10-11)

- User approved `tasks.md` at `966e6b6` and authorized T001–T053.
- T001/T002 done (contracts frozen; Codex clone `/home/towfienes/Projects/HyprVX-v3-codex`, branch `v3/001-codex`,
  push disabled). WO-01/WO-02/WO-03 issued to Codex.
- T007/T008 done: `core/include/hyprvx/core/CameraMath.hpp` (BSD-3-Clause, contract 1.1) and `hyprvx::Camera`
  delegation. Verified: ctest 298/298, simulator output byte-identical to `ab6647c`, 200 000-case bit-exact V1
  self-check (evidence `20261011-003807-core-e94ffd2`, archived). Independent verification is Codex's T009.
- Patches 0001–0005 drafted (render seam, culling, damage-union rule, HYPRVX_DEV dev control); `git am` on
  `efb5099` reproduces the draft tree; syntax-only checks pass. Hyprland build and native tests: NOT RUN (need
  Codex's scripts, GATE-A, GATE-N).
- `contracts/dev-control.md` bumped to 1.1 (additive: `hyprctl hyprvx-dev` transport, state JSON, `-DHYPRVX_DEV=ON`).

## Sprint 1 status at G1 (2026-10-11)

- Since DV3-15 Claude Code is the only implementation agent; Codex history is kept (merges, clone, evidence).
- T001–T053 done and ticked; the user approved G1 on 2026-10-11 and authorized Phase 2 (T054–T071) up to T071.
- Patch series 0001–0005 (patchset `47b6baa12904`): camera as a pure view in the render pass, culling, damage union,
  dev control behind `-DHYPRVX_DEV=ON`. Two defects found and fixed by native tests during T040 (border interior,
  fractional-edge round-out).
- Verification: V-01..V-07, V-09..V-15, V-22..V-25 PASS; V-04 two clean rebuilds, 346/346 captures byte-identical;
  V-08 PARTIAL (RC2 not reproduced on V2); V-20 PASS (user review, 2026-10-11); V-16..V-19, V-21 are Phase 2 (NOT RUN).
- T045 (informational): no stock/patched CPU difference visible at the 10 ms tick for redraw or idle; about 2 ms of
  compositor CPU per non-identity camera change; frames presented not measured.
- Production safety: host Hyprland 0.56.2 and Caelestia untouched (V-22 audit repeated at G1 over 201 nested sessions, `20261011-095853-audit-b15784b-v22`); every native run was nested with a
  verified identity; no package changes; `dev` still at `15f75b0`; nothing pushed.

## Phase 2 status at the Feature 001 gate (2026-10-11)

- T054–T069 done and ticked; T070 open only for the user's V-20 review of the Phase 2 image set; T071 (Feature 001
  gate) is next and needs user approval. U0 and Feature 002 stay blocked.
- Patch series 0001–0009 (patchset `efaf0ea2cf21`): 0006 hit-test, 0007 seat-local coordinates/constraints/warps,
  0008 decoration hit selection and blocked compositor drags, 0009 popups. Lint PASS (9 patches).
- US3 suite 8/8 at scales 1 and 1.25 (V-16..V-19 PASS) with negative controls failing on the 0001–0005 build;
  Sprint 1 regression 16/16; T069 clean-room rebuild (`a157e6a8`) reproduces every result with byte-identical
  captures. V-19 also passes without 0009 (closest-monitor fallback in efb5099).
- Delivery spike (T066–T068, `delivery-decision.md`): stock + plugin (24 hooks; 17 are generated copies of patched
  functions) passes the same suites with byte-identical captures and the stability checks; under DV3-01 this selects
  plugin delivery, with costs and open items listed for the T071 decision.
- Harness/test fixes found by running: nested log kept after the compositor exits, cursor sprite off in captures,
  pointer driver reads complete JSONL records only, t_drag_scope keeps a FAIL; GATE-A re-checked byte-identical to
  the Sprint 1 baseline after each test-config change.
- Environment: native runs need the host in GPU profile Integrated (AMD only); in Hybrid the nested compositor gets
  the NVIDIA render node, whose GBM rejects the headless swapchain (diagnosed 2026-10-11).
- Still open: V-08 PARTIAL (RC2 not reproduced), frames presented NOT MEASURED, `test_build_scripts.py` KeyError
  without `HYPRVX_AGENT`, three untracked Node files (`package.json`, `package-lock.json`, `node_modules/`) not
  created by Claude.
- Production safety: V-22 Phase 2 audit `20261011-141225-audit-c836666-v22p2` PASS; `dev` still at `15f75b0`;
  nothing pushed.

## Feature 001 closed at T071 (2026-10-11)

- The user approved the Feature 001 gate (T071) and the Phase 2 V-20 image set (T070); `tasks.md` T001–T071 are all
  ticked with archived evidence.
- Delivery decision DV3-16: plugin on stock Hyprland per DV3-01. The spike is not production-ready; risks recorded
  in `delivery-decision.md` and `.ai/DECISIONS.md`. Nothing installed into the host.
- Kept: V-08 PARTIAL, frames presented NOT MEASURED, NOT RUN items listed in the T071 integration-log row.
- Not done (needs user approval): merging `v3/001-native-spatial-camera` into `v3/spec-foundation`, the U0 spike,
  Feature 002 specification.

## Feature 001 merged; U0 spike done (2026-10-11)

- Feature 001 merged into `v3/spec-foundation` (`6931c04`, `--no-ff`, user-approved); post-merge checks PASS. Not pushed.
- U0 unified-world spike (DV3-12) finished: report `docs/v3/U0_FEASIBILITY_REPORT.md`. U3 feasible with five seams
  (render, per-window camera hint, hit-test, commit damage, suspended state); 7/8 questions PASS, Q7 (workspace
  animation while drifting) FAIL with sub-pixel shifts. Runs `20261011-152554-patched-c15c101-u0_spike` (BLOCKED,
  found the commit-damage seam) and `20261011-153555-patched-8ef32a4-u0_spike` (FAIL 7/8).
- Spike code stays on the throwaway branches `v3/u0-spike` (repo) and `u0-spike` (draft tree); never merged. Keep both
  branches: the evidence `git_sha` values point at them.

## Feature 002 at the T062 gate (2026-10-12)

- Branch `v3/002-multi-window-drift-mvp`, final code `420efff` (docs after it). `tasks.md` T001–T061 done with archived
  evidence; T062 (the Feature 002 gate) waits for the user. Not merged, not pushed; `dev` still at `15f75b0`.
- Delivered: CameraMath 1.2 (`offsetCamera`, `worldDelta`), `DriftModel` (core), patches 0010–0013 (patchset
  `d291f743b864`: world offsets in render/cull/damage, hit-test/input, floating resize through the camera, dev control
  1.2), the `hyprvx-drift` policy plugin in form P (patched build) and form S (stock + mechanism hooks, DV3-16),
  vkeyboard/x11client test tools, `scripts/build-plugin.sh`, build variant `patched-nodev`.
- Verification (`specs/002-multi-window-drift-mvp/checklists/verification.md`): V-01..V-24, V-26..V-28 PASS; V-25
  (T-SCENARIO image set, SC-008) NOT AUTOMATABLE, awaiting the user's review and signature. Final suite
  `20261012-023154-suite-420efff-feature002` (22 tests × forms P, S × scales 1, 1.25 = 88 PASS); P vs S 556/556 captures
  identical; Feature 001 regression with each plugin loaded PASS; V-27 two clean-room builds reproducible.
- Found and fixed during T057 (R1): policy hyprctl prefix match answered `hyprvx-dev` on non-dev builds (`2deb797`,
  FR-016); `t_fr016` static method (`6dce348`); timing-dependent resize drags in `t_resize`/`t_drag_scope` (`420efff`).
  Failed and blocked runs are kept in the archive and the integration log.
- Open items for the gate: a camera or offset change during a compositor floating resize makes the anchor jump (not in
  any scenario); damage union cost grows with the number of offset windows (performance not measured, VI deferred);
  x11client checks its X peer after the xcb handshake; form S has no camera hint for special-workspace windows drawn
  inline (carried from Feature 001; never a Drift scene); scenario uses stand-in clients, not VS Code/Firefox;
  Caelestia NOT RUN in nested; step 10 (pin) and snap are Phase 5.
- Production safety: V-26 audit `20261012-032253-audit-7f12c15-v26` PASS (1114 nested sessions); host Hyprland,
  Caelestia and GPU config untouched.

## Feature 002 merged (2026-10-12)

- The user signed V-25 and approved the Feature 002 gate (T062). Merged with `--no-ff` into `v3/spec-foundation`
  (`dd4f9b9`); merged tree == feature tree `9725b924`. Post-merge: ctest 326/326, analyzer, build-script tests, identity
  refusal and lint (`20261012-061638-static-dd4f9b9-f002_static`) PASS; 1101/1101 evidence indexes match their archives.
- Kept as is: Caelestia NOT RUN in nested; performance not measured (VI deferred); scenario stand-in clients; the open
  items listed in "Feature 002 at the T062 gate". No plugin installed into the host; host Hyprland, Caelestia and
  GPU config untouched; `dev` = `15f75b0`; nothing pushed.
- Next: Feature 003 (Phase 4, trackpad UX) through Spec Kit; `tasks.md` needs the user's approval.

## Antigravity experiment (2026-10-12)

- User-requested trial of task delegation to Antigravity (`agy` CLI). It does not change the Master Plan, DV3 decisions,
  architecture or gates; Claude Code stays the orchestrator, integrator and acceptance owner (DV3-15 otherwise holds).
- Setup follows the least-privilege model of §O3: independent clone `/home/towfienes/Projects/HyprVX-v3-agy`
  (`git clone --no-hardlinks`, no shared `.git`), branch `v3/agy-experiment`, push URL `DISABLED`.
- Antigravity never edits this worktree or the shared repo. It may only implement a small, independent group of
  Feature 003 tasks after `tasks.md` is approved; it hands off by commit + diff + test output + evidence. Claude
  fetches from outside the clone (never runs git inside it), reviews, rebuilds and re-runs tests before integrating.
- Known prerequisite: the tooling accepts only `HYPRVX_AGENT=claude|codex`; a separate lane for native runs by
  Antigravity needs a small tooling change, to be planned inside Feature 003 `tasks.md` if a delegated task needs it.
- Orientation (2026-10-12, Antigravity conversation `019ed250`, artifacts under `~/.gemini/antigravity-cli/brain/`):
  read the required documents and restated roles, hard limits, handoff trailers and §O5 correctly; reproduced lint
  (13 patches), analyzer 11/11, ctest 326/326 and the build-script tests; no change to tracked files, no push, no
  native run, nothing written to Claude's cache or evidence (checked from outside the clone). Deviations: it built
  `build-v3` (gitignored) although the prompt said read-only/no build; it did not answer the four requested questions
  (camera equation and invariants, task → evidence flow, BLOCKED cases, Phase 4 scope), so its grasp of the
  transform contract is unverified; it ran the build-script tests with `HYPRVX_AGENT=claude` (tests only, no cache
  writes). The clone's `.git/config` gained `push.autoSetupRemote = true` at 06:26:26, 10 s after an `agy` CLI start
  and not from any command in its transcript; push stays blocked (`pushurl = DISABLED`, `pre-push` hook unchanged).
  Verdict: ready for one small, unit-test-only task with an explicit work order; first handoff reviewed line by line.
- Ended (user, 2026-10-12): the trial was stopped before WO-01 started (clone branch still `4d4aaac`). Claude Code is
  the only agent for all remaining V3 work (DV3-15 in full); DV3-17 is marked ended in the master plan.

## Feature 003 at T045 (2026-10-12)

- Branch `v3/003-trackpad-native-ux`; plugin code final at `789c75c` (glide steps on `render.preChecks`; R6
  corrected). T001–T041 and T043 done with archived evidence; V-01..V-22 PASS. `dev` still `15f75b0`; nothing pushed.
- Product defect found and fixed during implementation: glide stalled on `render.pre` (emitted after Hyprland's
  damage check). Test defects fixed: a vacuous socket2 negative check (t_arbitrate; all archives re-checked), hidden
  clients captured, a setup point in a border grab area, the output round-trip comparison, a closed client's FIFO,
  and a pointer-driver read race (1/88 BLOCKED in the first Feature 002 regression run).
- Waiting for the user at T045 (L6 with the real trackpad). Pinch, 3/4-finger and U4 items are BLOCKED by the
  nested environment by design (user decision 2026-10-12); their automated evidence is V-10..V-12.
- External evaluation reported to the user (read only, nothing installed): phantomat (plugin, 0.56, mutates
  floating/workspace and restores: the §C5 fallback HyprVX rejected), hypr-canvas (alpha, single camera, no tests),
  infinity-land (Hyprland 0.51.1 fork, one day of commits). Recommendation: continue HyprVX, reference phantomat.
  Decided by the user (DV3-18): continue HyprVX; phantomat is a reference only.

## Feature 003 at T047 (2026-10-12)

- T045 L6 signed PARTIAL by the user (Towfienes): H9 PASS; H1–H8, H10 BLOCKED (nested scroll is source WHEEL:
  aquamarine 0.15.1 drops `axis_source`/`axis_stop`; host takes Super combinations); B1–B6 BLOCKED. The interactive
  session keeps the hardware cursor with `use_cpu_buffer = 1` (pointer invisible; a software cursor froze the window).
- T044 V-23 PASS: marker audit (4 sessions after the 12:46 reboot wiped /tmp) plus a new archive audit (531 sessions).
- T046 records and quickstart validated (ctest 367/367, 5 builds, lint, analyzer); constitution re-check in plan.md.


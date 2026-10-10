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

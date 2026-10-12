# HyprVX Architecture Decisions

## D001 - Hyprland remains the compositor

Status: accepted

HyprVX extends Hyprland rather than replacing it.

## D002 - Hybrid architecture

Status: accepted

Use separate layers:

- shared spatial/world logic
- daemon or service for state, persistence and control
- native Hyprland plugin for compositor hot-path capabilities when required
- Caelestia integration for optional UI

## D003 - Simulator before compositor integration

Status: accepted

World coordinates, camera transforms, island behavior and navigation must be testable without a real Hyprland session.

## D004 - Free windows are first-class

Status: accepted

Applications are not automatically forced into islands.

## D005 - Island and Zone are different concepts

Status: accepted

Island:
a group of windows.

Zone:
a named location or spatial region.

## D006 - One-level islands for V1

Status: accepted

No nested islands.

## D007 - Independent per-output cameras first

Status: accepted

All monitors observe the same world but may have different camera positions.

Linked camera behavior may be added later.

## D008 - Screen-space and world-space are separate

Status: accepted

Caelestia bar, launcher, notifications, lockscreen and other layer-shell UI remain screen-space.

Normal managed applications normally live in world-space.

## D009 - True zoom is not an MVP requirement

Status: accepted

Pan and world semantics must become stable before compositor-level true zoom.

True zoom is experimental until Hyprland render hooks and compatibility are proven stable.

## D010 - License is not frozen yet

Status: accepted

Project license will be selected after a third-party license audit.

## D011 - Read-only event-driven Hyprland runtime bridge

Status: accepted

Keep Hyprland wire parsing and socket access outside `hyprvx_core`. A semantic
adapter translates verified socket2 events into structured runtime events, and
an eventually consistent `RuntimeMirror` observes those events without mutating
`World`. The socket2 reader blocks for input; it does not poll Hyprland or issue
`hyprctl` requests.

The initial runtime policy targets one active output: the laptop output is
active alone, an external DP output takes over exclusively while connected,
and the laptop output resumes when it disconnects. Simultaneous multi-monitor
behavior is deferred. Typed monitor hotplug events remain deferred until their
payloads are verified on hardware.

## D012 - One-way runtime spatial projection

Status: accepted

`RuntimeMirror` is compositor observation state. The separate
`hyprvx_runtime_projection` integration layer reconciles it one-way into
`hyprvx_core::World`; there is no compositor control path in Phase 2C.
Projected geometry is owned by HyprVX and is never represented as geometry
reported by Hyprland. Workspace regions are spatial zones, not Islands, and
observed windows stay free windows by default. Window IDs are allocated
monotonically per projector epoch from canonical addresses and are removed on
close; workspace and placement slot assignments remain reserved for the epoch
to keep unrelated window lifecycle events from shifting existing geometry.
The primary runtime target remains one active output.

## D013 - Versioned durable spatial projection state

Status: accepted

Persisted state is HyprVX-owned spatial memory, not compositor truth. Schema v1
stores workspace regions, next workspace/placement counters, projected geometry,
the active `InitialPlacementPolicy`, WindowIds, and metadata hints. Restores
continue using that saved placement policy so later windows cannot overlap old
regions when process configuration changes. `RuntimeMirror` remains the source of truth for
which windows are currently alive. Recovery never creates World windows from
disk alone. Same-compositor restoration uses the instance signature plus
canonical address, excludes windows observed opening in the new connection,
and rejects conflicting class/workspace hints. Cross-address restoration uses
an exact, unique class/title/workspace fingerprint only; ambiguous candidates
receive fresh placements. Socket2 does not expose a stable app-instance ID, so
indistinguishable windows after missed close/open events remain a documented
limit.

State writes use a same-directory temporary file, fsync, atomic rename, and a
last-good `.bak`. Corrupt or future-version state is preserved; replacement
requires an explicit caller decision. Connection epochs are explicit in
`RuntimeBridge`, and the projector preserves dormant geometry across epoch
reset while requiring live mirror observations before restoring World entries.
No compositor control is introduced.

Unsigned 64-bit IDs and counters are serialized as canonical decimal strings
to preserve exact values in double-based JSON consumers. Persisted world
coordinates are limited to 1e12 units to bound geometry arithmetic; runtime
world coordinates remain double-based. Corruption snapshots use collision-safe
suffixes, and debounced writes have a configurable maximum dirty age.

## D014 - Semantic runtime camera navigation

Status: accepted

Physical and simulated inputs map to shared semantic camera actions in the
runtime navigation layer. `RuntimeCameraController` reuses core Camera,
CameraController motion, CameraHistory, SpatialNavigator, and ViewportFraming;
it never issues compositor focus, window, or workspace commands. HyprVX
selection is distinct from compositor focus: observed focus may seed selection
until explicit user navigation takes ownership. Direct pan interrupts a fly,
and a new explicit destination replaces the previous flight. Workspace jumps
change only the HyprVX camera. A continuous pan is one history transaction.
The single-active-output/external-exclusive policy remains the runtime target;
trackpad input can later adapt into the same semantic actions.

## D015 - Device-neutral runtime navigation input

Status: accepted

Keyboard, mouse, and gesture adapters emit `RuntimeNavigationAction` values;
only the SDL boundary decodes SDL key/button events. A shared dispatcher
coalesces direct pan deltas before applying them to the camera. An active
direct gesture owns its matching end event; interruption clears the gesture so
a delayed end cannot create an orphan history transaction. Click selection is
user-owned spatial selection and does not frame or focus the compositor
window. Home prioritizes that user-owned selection; compositor focus remains
the seed while ownership is compositor-seeded. Three-finger pan, pinch, and
four-finger directional swipe are represented by the same semantic actions,
with no hardware gesture library or production binding added.

## D016 - Nested-only native prototype and explicit transform refusal

Status: accepted for Phase 3 development

Native Wayland integration is confined to a PID-verified nested Hyprland
session with a generated config and private runtime directory. The platform
library is above runtime navigation; `hyprvx_core` remains independent. The
native overlay uses layer shell and bounded SHM with event-driven redraw. A
separate plugin for exact Hyprland 0.56.2 commit provides a guarded, read-only
event source and semantic camera control. Actual compositor geometry is kept
apart from projected World geometry. The installed window-transformer API
does not provide matching inverse input hit testing and excludes popups;
enabling native content translation is therefore refused. Compositor zoom is
deferred. No compositor model mutation or ownership echo is introduced.

## V3 decisions (2026-10-10) — see docs/v3/HYPRVX_V3_MASTER_PLAN.md §J

### DV3-01 - Hybrid: patch the mechanism, plugin the policy

Status: accepted (user)

A narrow, ordered Hyprland patch series on pinned v0.56.2 (`efb5099`) provides the view-transform seams:
render clip, pass simplification, culling, damage, hit-test, seat-local coordinates, drag and popups.
Policy (mode, gestures, offsets, snap, persistence) lives in a plugin controller that reuses `hyprvx_core`.
The production delivery mechanism (plugin hooks on stock Hyprland vs a side-by-side patched build) is decided
at the Phase 2 spike gate. Supersedes the D009 deferral for V3.

### DV3-02 - V3 lives in a separate linked worktree

Status: accepted (user)

`/home/towfienes/Projects/HyprVX-v3`, branch `v3/spec-foundation`, from `dev@15f75b0`. The V1 working tree
(including the untracked Phase 4 patch) is preserved untouched. V2 is a read-only reference. Copies with
SHA-256 are in `research/`.

### DV3-03 - Option A for MVP, unified world (Option B) is the final requirement

Status: accepted (user)

The MVP uses a per-workspace canvas. The data model (`DriftScene`, per-window world offset) is not coupled to a
single workspace. A unified cross-workspace world without ownership reassignment is an explicit milestone (U)
with acceptance tests U-1..U-6. An overview of separate canvases does not satisfy it.

### DV3-04 - Single transform contract

Status: accepted (plan approval)

`CameraMath.hpp` is the only implementation of forward/inverse/anchor math. See
`specs/001-native-spatial-camera/contracts/transform-contract.md`.

### DV3-05 - Drift is a pure view

Status: accepted (plan approval)

`W(w) = L(w) + O(w)`. Hyprland's logical geometry, workspace ownership and layout state are never written by
Drift. Turning Drift off restores Hyprland by construction.

### DV3-06 - Rewrite the patch series cleanly

Status: accepted (plan approval)

The V1 Phase 4 patch and the V2 series are references only. Root causes of the V2 visual failure (RC1 surface
clip, RC2 pass simplify) are documented in `specs/001-native-spatial-camera/research.md`.

### DV3-07 - No DriftWM code

Status: accepted (plan approval)

DriftWM is GPL-3.0-or-later and serves as a behavioral reference only.

### DV3-08 - Spec Kit workflow

Status: accepted (user)

Spec Kit 1.1.3 with the Claude integration. Constitution: `.specify/memory/constitution.md`. Features live in
`specs/NNN-*`. `tasks.md` and implementation start only after user approval.

### DV3-09 - Hybrid Claude Code + Codex collaboration

Status: accepted (user, 2026-10-10)

Claude Code is technical lead and integration owner. Codex works only on independently verifiable lanes
(Sprint 1: build/CMake, scripts, harness, test tools, native test scripts, baselines, independent review) with
exclusive file ownership. Codex runs in a separate `git clone --no-hardlinks` without write access to any shared
`.git`. See master plan §O and `docs/v3/HYPRVX_V3_COLLABORATION_REPLAN_PROPOSAL.md`.

### DV3-10 - CameraMath.hpp license

Status: accepted (user, 2026-10-10)

`core/include/hyprvx/core/CameraMath.hpp` carries `SPDX-License-Identifier: BSD-3-Clause` so it can be vendored
into the Hyprland patch series. The project-wide license (V1 D010) remains open.

### DV3-11 - Drag scope in Feature 001

Status: accepted (user, 2026-10-10)

Feature 001 keeps surface-local coordinates for client drags and inverse-transformed decoration hit selection
(FR-012). Compositor move/resize drags are blocked while the camera is non-identity. Moving windows through the
world offset `O(w)` belongs to Feature 002.

### DV3-12 - Early unified-world feasibility spike (U0)

Status: accepted (user, 2026-10-10)

A time-boxed, evidence-only spike answers U3 after the Feature 001 gate, before or at the start of Phase 3. It
does not implement Milestone U and its code is not merged.

### DV3-13 - Commit authorization for V3 feature branches

Status: accepted (user, 2026-10-10)

Regular project-local commits are authorized on `v3/001-*` branches and for approved planning changes on
`v3/spec-foundation`. No push, no destructive cleanup, no merge into `dev`, `main` or `v3/spec-foundation`
without approval.

### DV3-14 - Codex Auto-review for scoped escalations

Status: accepted (user, 2026-10-11); effective from WO-04. WO-01 to WO-03 keep manual approval.

Codex keeps `workspace-write` and `on-request`. A work order marked "Auto-review: allowed" may run with
`approvals_reviewer = "auto_review"`, so Codex's automatic reviewer approves escalations instead of the user, only
for: git commit/fetch/merge inside the Codex clone (no push, rebase or force), allowed source downloads,
`systemd-run --user --scope` and builds under the heavy lock, unit-test socket binds, and system commands the work
order lists. Starting or controlling a nested compositor, GPU/`/dev/dri`, or anything touching the host
Wayland/Hyprland socket stays under manual user approval in a session without the auto reviewer. Writes outside the
clone, its cache and `/tmp`, host configuration, Caelestia, `/usr` and sudo remain forbidden. Full Access and
Bypass Permissions are never enabled. A rejection makes the item BLOCKED. Every commit lists auto-approved
escalations in an `Escalations:` trailer. Acceptance gates, stop conditions and Claude's review and integration of
every handoff are unchanged. Details: master plan §O3.

### DV3-15 - Return to Claude-only execution

Status: accepted (user, 2026-10-11). Supersedes DV3-09 (hybrid collaboration) and DV3-14 (Codex Auto-review).

Claude Code is the only implementation agent. The Codex lane, work orders, cross-agent handoffs, ownership split and
inter-agent Git synchronization are retired; master plan §O is history except O5 (resource rules). All completed
work, integrated Codex commits, evidence and approved technical corrections (P1–P5, P10, infra-before-claims, K0,
GATE-A, GATE-N, U0) remain. Open tasks keep their IDs, dependencies and gates and are owned by Claude. Technical
difference: R1 (T044, T069) and T068 lose author/verifier independence; they become clean-room rebuilds with a
source-verification report written before reading new results. The Codex clone is left untouched as a reference.

### DV3-16 - Production delivery: plugin on stock Hyprland (DV3-01 outcome)

Status: accepted (user, 2026-10-11, at the Feature 001 gate T071).

Applying the DV3-01 rule: the plugin-hook spike (T066–T068) passed the same visual + input suites as the patch series
with byte-identical captures, so production delivery is a plugin on stock Hyprland. The spike is **not
production-ready**; these risks stay recorded and open (`specs/001-native-spatial-camera/delivery-decision.md`):
24 hooks, 17 of them whole copies of patched Hyprland functions (about 1700 lines, generated from the patch series,
which therefore stays maintained); private-member access through `#define private public`; ABI compatibility
(same compiler and exact headers of the running binary, x86_64 trampolines, signature mismatches found only at load
time); two seams a hook cannot reach (file-static `updateRelativeCursorCoords`, the special-workspace camera hint);
regeneration and re-verification on every Hyprland update. Not verified: loading into the distribution binary,
special workspaces, warp-on-focus, pointer constraints under a camera, physical hardware. The plugin is not installed
into the host and the host Hyprland binary is not replaced. Roadmap, architecture C (patch the mechanism, plugin the
policy) and all gates are unchanged.

### DV3-18 - External projects are references only

Status: accepted (user, 2026-10-12).

After a read-only review (`docs/v3/EXTERNAL_PROJECTS_REVIEW.md`) of phantomat, hypr-canvas and infinity-land, HyprVX
continues on the master plan. phantomat (BSD-3-Clause) is a reference for Phase 5 navigation/search UX, Phase 8 linked
screens and fullscreen/X11/popup test cases. Copying code needs the user's separate approval and keeps phantomat's
BSD-3 notice in THIRD_PARTY. Roadmap, architecture, Constitution V and U-1..U-6 are unchanged; phantomat's
mutate-and-restore model (the §C5 fallback) is not adopted.


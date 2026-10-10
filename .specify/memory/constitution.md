<!--
Sync Impact Report
- Version change: (template) → 1.0.0
- Added principles: I–X (all new; first ratification for HyprVX V3)
- Added sections: Platform & Safety Constraints, Development Workflow & Quality Gates, Governance
- Templates reviewed: .specify/templates/plan-template.md (Constitution Check gates map to I–X) ✅,
  spec-template.md ✅ (no change needed), tasks-template.md ✅ (verification-level tagging recommended)
- Follow-up TODOs: TODO(LICENSE): project license still open (V1 decision D010); principle IX constrains it.
-->

# HyprVX Constitution

HyprVX adds an optional, reversible, DriftWM-style infinite spatial desktop ("Drift Mode") to Hyprland while
preserving the user's Hyprland + Caelestia daily-driver desktop. Master plan: `docs/v3/HYPRVX_V3_MASTER_PLAN.md`.

## Core Principles

### I. Host Safety & Nested-First (NON-NEGOTIABLE)
- Experimental compositor code runs only in a nested Hyprland started by the project harness from a
  project-local prefix. It never runs from `/usr/bin` or `$PATH`.
- Before **every** mutating IPC command or signal, the harness re-verifies the target's identity: `SO_PEERCRED`
  PID, process start time, instance signature ≠ host signature, and a private runtime dir. Environment
  variables alone never establish isolation.
- Forbidden without separate, explicit user approval: replacing or modifying system Hyprland, running
  sudo/pacman/yay, editing production Hyprland or Caelestia config, loading plugins into the host compositor,
  restarting or killing the host compositor, touching kernel/bootloader/DM/GPU/brightness/HDR/monitor settings,
  destructive git cleanup, and deleting V1/V2 experiments.
- Host-wide input injection (`ydotool`, uinput) is forbidden. Test input must use protocols scoped to the
  verified nested display (e.g. `zwlr_virtual_pointer_v1`).

### II. Evidence-Based Verification (NON-NEGOTIABLE)
- Each result is one of PASS / FAIL / BLOCKED / NOT RUN / NOT AUTOMATABLE, with an artifact: log, image,
  JSON, commit, or binary sha256.
- A dispatcher returning `ok`, a clean compile, a simulator image, or a pre-existing binary is never evidence
  of compositor behavior.
- No test may pass unconditionally.
- Visual claims require captures of the actual nested or hardware compositor output. Trackpad claims require
  physical-hardware validation (Level 6).

### III. Normal-Mode Equivalence
- With Drift Mode OFF (identity camera, no world offsets), the compositor must be behaviorally stock Hyprland.
  This means pixel-identical output against a stock build of the same pinned commit for the same scene, no
  extra render pass elements, and no input interception.
- Mode switching never requires restarting the compositor.

### IV. Single Transform Contract
- Exactly one camera equation exists, defined in `CameraMath.hpp`. Rendering, damage, culling, hit-testing,
  drag, warps and constraints all use it.
- Re-deriving the equation inline in another subsystem is a defect.
- The contract, invariants and tolerances live in `specs/001-native-spatial-camera/contracts/transform-contract.md`.

### V. Reversibility — Drift Is a View
- Drift Mode must not mutate Hyprland's window model: workspace membership, logical geometry, tiled/floating
  state, layout tree.
- HyprVX owns per-window world offsets, cameras, clusters, pins and bookmarks.
- Turning Drift Mode OFF restores the exact pre-Drift Hyprland state without restart.
- Any design that reassigns ownership requires a documented PoC failure and explicit user approval.

### VI. Event-Driven Performance Budgets
- No polling where events exist. No `hyprctl` frame loops, no IPC round-trips per frame, no per-frame file
  I/O, and no hot-path heap allocations.
- Frames are produced only for damage or active camera motion.
- Budgets are set from measured stock baselines on the same hardware.
- A measured idle-CPU, frame-time or memory regression blocks feature work until it is understood.

### VII. Caelestia Is Screen-Space and Untouched
- Layer-shell, lock and cursor surfaces are never camera-transformed.
- HyprVX emits no synthetic Hyprland events, so Caelestia's view stays truthful.
- Production Caelestia files are never edited. Any optional integration lives in a separate, removable
  HyprVX-owned component.

### VIII. Reuse Verified Code, No Duplicates
- The V1 compositor-agnostic core (`hyprvx_core`: camera motion, world/island, navigation, history,
  framing, persistence, input adapters) is reused.
- New implementations of the same concepts require source-level evidence that the existing one is unsuitable.
- V1 and V2 artifacts are preserved as read-only references in `research/`.

### IX. License Hygiene
- No third-party source is copied without a recorded license audit (`docs/THIRD_PARTY.md`).
- DriftWM (GPL-3.0-or-later) is a behavioral reference only; its code must not enter HyprVX or the Hyprland
  patch series.
- Code vendored into Hyprland (BSD-3-Clause) must carry a compatible permissive license.

### X. Reproducible Pinned Builds
- Every compositor artifact is built from a clean checkout of a pinned upstream commit, plus an ordered patch
  series applied with `git am`, into a project-local prefix.
- Provenance is truthful: real `GIT_COMMIT_HASH`, plus a patchset identifier derived from the patch content
  hash. Never force `clean`.
- No absolute temporary paths and no reuse of stale CMake caches.
- Reproducible build verification is a release gate.

## Platform & Safety Constraints

- Target baseline: Hyprland v0.56.2 (`efb50993780079460b0cbed1363e2166a2de1d9f`) with the system libraries
  installed on the host. Libraries are never upgraded or downgraded to satisfy experimental builds without
  approval.
- Development order: unit/simulator → nested compositor → physical-hardware validation (nested, user-observed)
  → controlled production candidate (separate approval).
- Every risky integration has a written rollback path. Runtime escape: a `panic` action that returns to OFF
  within one frame; auto-reset on invalid camera state; OFF on monitor removal.
- Builds and nested sessions run under resource limits (`systemd-run --user --scope -p MemoryMax=…`, `-j4`)
  because host RAM headroom is small.
- Scripts are Bash with a shebang. Usage from the user's fish shell is documented.

## Development Workflow & Quality Gates

- Spec Kit flow per feature: `/speckit-specify` → `/speckit-clarify` (as needed) → `/speckit-plan` →
  `/speckit-tasks` → `/speckit-analyze` → `/speckit-implement`.
- Implementation of a feature begins only after the user approves its plan.
- Verification levels L1–L8 (master plan §F). A phase gate passes only when its listed levels have evidence.
- `.ai/CURRENT_STATE.md` and `.ai/NEXT_TASK.md` are updated after each meaningful milestone.
- Commits happen only when the user asks. No destructive git operations on shared branches or history.

## Governance

- This constitution supersedes conflicting guidance in older V1 documents for V3 work.
- Amendments require a written rationale, a version bump (semantic: MAJOR for removed or redefined
  principles, MINOR for added principles or sections, PATCH for clarifications) and user approval.
- Every plan's Constitution Check must list each principle as PASS or as a justified exception in
  Complexity Tracking.

**Version**: 1.0.0 | **Ratified**: 2026-10-10 | **Last Amended**: 2026-10-10

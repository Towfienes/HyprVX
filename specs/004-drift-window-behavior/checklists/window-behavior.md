# Requirements Quality Checklist: Window Behavior (Feature 004)

**Purpose**: Unit tests for the requirements of snap, groups, navigation keys, pin, ownership (Constitution V) and
Drift-off equivalence; standard depth; for the reviewer before `tasks.md` approval
**Created**: 2026-10-12
**Feature**: [spec.md](../spec.md)

**Ownership**: `[x]` means the reviewer judged the requirement-quality criterion satisfied. It does not mean that any
implementation is done. `/speckit-implement` reads these markers but never changes them.

## Requirement Completeness

- [ ] CHK001 Is the set of windows that can be snap targets specified (same workspace, non-pinned, visible or not, fullscreen workspace)? [Completeness, Spec §FR-002, Edge Cases]
- [ ] CHK002 Is the behavior of Shift + Super + drag on a window that belongs to no group specified? [Gap, Spec §FR-007]
- [ ] CHK003 Are the requirements for groups defined when snapping is switched off (do existing arrangements still form groups)? [Gap, Spec §FR-006, §FR-022]
- [ ] CHK004 Is the margin used when directional focus "reveals" a window specified? [Gap, Spec §FR-009]
- [ ] CHK005 Is the stacking order among several pinned windows specified? [Gap, Spec §FR-016]
- [ ] CHK006 Is what happens to a pin when its window moves to another workspace specified? [Gap, Spec §FR-016]
- [ ] CHK007 Are key auto-repeat requirements defined for the built-in keys? [Gap, Spec §FR-020]
- [ ] CHK008 Are the dispatcher results on misuse (not in Drift, no focused window, bad arguments) specified? [Completeness, Spec §FR-019]

## Requirement Clarity

- [ ] CHK009 Is the "small tolerance" for snapped edges in the group definition quantified? [Ambiguity, Spec §FR-006]
- [ ] CHK010 Are the screen-px and world-px units used consistently for threshold, gap and release? [Clarity, Spec §FR-002, §FR-004]
- [ ] CHK011 Is "fully visible" in directional focus defined (window box inside the screen, with or without margin)? [Clarity, Spec §FR-009]
- [ ] CHK012 Is it clear whether a camera jump that changes nothing (window already visible) records history? [Ambiguity, Spec §FR-014]
- [ ] CHK013 Is "home" defined in terms of the workspace camera rather than the monitor? [Clarity, Spec §FR-012]

## Requirement Consistency

- [ ] CHK014 Are FR-009 (key consumed with no neighbour) and FR-020 (consume only defined keys) consistent? [Consistency]
- [ ] CHK015 Is the Super + arrow override in Drift consistent with the clarification that built-in keys use only unbound combinations? [Consistency, Spec §Clarifications, §FR-009]
- [ ] CHK016 Are pin rules consistent with the Feature 002 fullscreen and panic rules? [Consistency, Spec §Edge Cases, §FR-016]
- [ ] CHK017 Is the group-move snap rule (US2.4) consistent with the single-window snap rule (FR-002–FR-004)? [Consistency]

## Acceptance Criteria Quality

- [ ] CHK018 Can "never alternates on consecutive frames" (FR-004, SC-002) be measured from recorded positions? [Measurability]
- [ ] CHK019 Are the tolerances of SC-001, SC-003, SC-004 and SC-005 stated in px and per output scale? [Measurability]
- [ ] CHK020 Is SC-006's list of Hyprland model fields complete for this feature (focus excluded on purpose)? [Measurability, Spec §SC-006]

## Scenario Coverage

- [ ] CHK021 Are exception flows for drags (cancel by Escape, Drift off, panic, monitor loss, workspace change) defined for group drags as well as single drags? [Coverage, Spec §FR-021]
- [ ] CHK022 Are recovery requirements defined for a window closed while it is a drag member or a snap target? [Coverage, Gap]
- [ ] CHK023 Are requirements for Drift-off equivalence defined for every new key and for pinned windows? [Coverage, Spec §FR-001, §SC-007]

## Edge Case Coverage

- [ ] CHK024 Are snap ties and zoom changes during a drag addressed? [Edge Case, Spec §Edge Cases]
- [ ] CHK025 Are framing limits (window too large or too small for the zoom range) addressed? [Edge Case, Spec §FR-011]
- [ ] CHK026 Is behavior specified when a pinned window's screen box lies off the screen (for example after an output change)? [Edge Case, Gap]

## Non-Functional, Dependencies, Ownership

- [ ] CHK027 Is the ownership rule (no write to Hyprland position, size, workspace, floating, layout, stacking order) stated for every action, including pin and focus? [Completeness, Spec §FR-005, §Context, Constitution V]
- [ ] CHK028 Is the dependency on a mechanism change for pin, and its impact on both delivery forms, documented? [Dependency, Plan R5]
- [ ] CHK029 Is the assumption that tests use the harness keyboard (not the host keyboard) documented? [Assumption, Spec §Assumptions]
- [ ] CHK030 Are performance expectations stated or explicitly deferred? [Non-Functional, Plan §Technical Context]

## Notes

- Generated for the reviewer. The author's own reading of these items is recorded in `integration-log.md`; the boxes
  stay for the reviewer.

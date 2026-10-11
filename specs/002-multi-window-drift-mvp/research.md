# Research: Multi-Window Drift MVP (Option A)

Source references are to Hyprland `efb5099` (stock worktree `~/.cache/hyprvx/claude/src/hyprland-efb5099/stock`) unless
a patch is named. Feature 001 research (`specs/001-native-spatial-camera/research.md`) stays valid; this file adds
what Feature 002 needs. Each entry: Decision, Rationale, Alternatives considered.

## R1. How the world offset `O(w)` enters drawing

**Decision**: Draw each window of a Drift scene through its own camera hint whose origin is shifted by the window's
offset: `cammath::offsetCamera(C, O(w)) = (P − O(w), z)`, so `S = M + (L + O − P)·z`. The hint is pushed before and
the pass camera restored after each window in `renderWorkspaceWindows`, and only while the camera is not identity or
`O(w) ≠ 0`. `offsetCamera` is added to `CameraMath.hpp` (transform contract 1.2), so the equation stays in one place.

**Rationale**: Everything `renderWindow` draws for a window (surface tree, decorations, popups in the popup pass)
is drawn under the current render modifier. One hint per window therefore moves all of it together. The U0 spike
used exactly this mechanism, and its Q1 boxes matched the contract to the pixel (`docs/v3/U0_FEASIBILITY_REPORT.md`).
Master plan §C3 asks for "one hint for the window pass + per-window offset `O(w)`". This keeps that: the pass hint is
unchanged, and the offset is composed into the window's camera instead of its position.

**Alternatives considered**: Add `O(w)` to `renderdata.pos`, the way `m_renderOffset` is added at `Renderer.cpp:562`.
That moves the surface, but decorations compute their boxes from the window's own position
(`CHyprBorderDecoration`, `CHyprDropShadowDecoration`), and popups and subsurfaces from theirs. Each would need its own
seam, plus damage, clip and decoration hit-testing for each, which means more seams for the same result. An offscreen
buffer per window is rejected by §C3 (VRAM, clipped to the monitor, loses popups).

## R2. Camera per workspace

**Decision**: The mechanism keeps one camera per monitor (Feature 001, patch 0001). The policy keeps a camera per
workspace and loads it into the monitor when `workspace.active` fires (`EventBus.hpp`, emitted from
`Monitor.cpp:1507` inside the workspace change). On leaving Drift the monitor camera goes back to identity.

**Rationale**: The camera the renderer needs is always "the camera of this monitor's active scene". Storing per
workspace is policy (master plan §C2 `scene = {…, C(m)}`, §C5 Option A). Swapping on the synchronous event means no
frame is drawn with the wrong camera.

**Alternatives considered**: A camera per workspace inside the mechanism. That adds workspace bookkeeping to the patch
series and to every copied function of the plugin form, without a rendering benefit.

## R3. Where the policy lives and the two delivery forms

**Decision**: The policy is a Hyprland plugin `platform/plugin/` (`hyprvx-drift`), built from one source set in two
forms:

- **Form P** (patched): loaded into the patched build (patches 0001–00NN). It is built against that prefix's installed
  headers and calls the mechanism API (`contracts/mechanism-api.md`) that the patched binary exports.
- **Form S** (stock, the DV3-16 delivery): built against the stock prefix. It contains the same policy, plus the
  mechanism as function hooks: the spike's hand-written wrappers, and copied functions regenerated from the patch
  series by the spike's `gen_copies.py`.

The policy's compositor-independent part (state machine, per-workspace cameras, offset store, drag and zoom math,
spawn placement) is a new pure component in `hyprvx_core` (`DriftModel`), unit-tested with GTest.

**Rationale**: Master plan §A/§C keeps "patch the mechanism (source of truth), plugin the policy". DV3-16 makes the
stock form the delivery, and FR-014 requires both forms to pass the same suite, as Feature 001 did. Keeping the policy
in a pure core makes the state machine and the math testable without a compositor.

**Alternatives considered**:
- Compile the policy into the patch series. This contradicts §C ("plugin the policy") and would leave the delivered
  plugin with a different policy.
- Form S only. Mechanism bugs would then show up only at runtime through hooks. Debugging at source level in the patch
  series is why the series exists (§A item 2, `delivery-decision.md`).

## R4. Intercepting input for Drift controls

**Decision**: The policy listens on the event bus: `input.mouse.button` (`InputManager.cpp:750`), `input.mouse.move`
(`:269`), `input.mouse.axis` (`:987`) and `input.keyboard.key` (`:1630`). All of them are `Cancellable` and are
emitted **before** keybinds, input capture and seat delivery. In Drift:

- Super + left button on a window starts a window-move drag.
- Super + left button on empty canvas starts a pan drag.
- Super + wheel zooms, anchored at the pointer.
- Super + Escape turns Drift off.

Each of these is cancelled so that neither the client nor Hyprland's own binds see it. Outside Drift nothing is
cancelled, and the listeners return at once (master plan §C2: "listeners exist but return immediately").

**Rationale**: Cancellation before keybinds means a user's `bindm = SUPER, mouse:272, movewindow` can never write
`L(w)` in Drift (Constitution V), and needs no user config change.

**Alternatives considered**: Require the user to bind dispatchers in their config. The interactive drags (pan,
move) still need pointer-motion handling, and Super+Escape would depend on the user's config. Keyboard dispatchers are
provided anyway (`contracts/drift-control.md`).

## R5. Window move and floating resize

**Decision**: The window move in Drift is a policy-owned drag that only changes `O(w)` by `Δscreen / z` (master plan
§C4). The compositor's own move drag stays blocked under a non-identity camera (patch 0008), and in Drift it never
starts, because R4 cancels Super + left button first. Floating resize by border drag is allowed in Drift through the
inverse transform: the mechanism patch converts the drag delta by `/ z`, and the drag anchor goes through the
offset camera. Tiled border drags are ignored and logged once per drag.

**Rationale**: §C4: "resize: floating → resize thật (configure); tiled → MVP: không hỗ trợ trong Drift". Resizing a
floating window is an intended exception to "Drift writes nothing" (§C5: "resize floating do user yêu cầu").

**Alternatives considered**: Implement move by writing `L(w)` (forbidden: Constitution V, DV3-11). Implement resize
in the policy: Hyprland's drag controller already handles corner selection, constraints and configure serials, so
only its delta and anchor need the camera.

## R6. New window placement

**Decision**: When a window maps on the active workspace of a Drift monitor, the policy sets
`O(w) = centre_world(viewport) − centre(L(w))` before the window's first frame is drawn (the earliest window event
that has `L(w)`; checked against `window.openEarly`/`open` during implementation, recorded in the log). Override-
redirect X11 windows are placed by R9 instead.

**Rationale**: Master plan §D "New window: spawn ở tâm viewport".

**Alternatives considered**: Place at the pointer (not in the master plan), or leave `O = 0`. With `O = 0` a window can
open off-screen under a panned or zoomed camera.

## R7. Fullscreen and workspace-switch animations

**Decision**:
- **Fullscreen**: a fullscreen workspace is drawn with the identity camera and no world offsets. This is Feature 001's
  `activeCamera` rule, now applied to offsets too.
- **Workspace switch**: while Drift is on, the switch is not animated. On `workspace.active` the policy finishes the
  workspace render-offset and alpha animations at once, so a switch never shows windows sliding through a camera.

**Rationale**: Master plan §C3 (fullscreen identity) and §C5 ("animation workspace bị suppress trong Drift"). The U0
spike showed that compensating the animation per window still leaves sub-pixel shifts (Q7 FAIL), so the animation is
suppressed instead.

**Alternatives considered**: Compensate the animation with the per-window hint. U0 Q7 rejected this.

## R8. Damage, culling and hit-testing with offsets

**Decision**: Every window-owned box is moved by its owner's `O(w)` before it goes through the camera:

- **Culling**: `visibleOnMonitor` (0003).
- **Damage**: the damage-union rule (0004) in `damageWindow` and `damageSurface`; the owner of a surface is its window,
  for popups and subsurfaces too.
- **Hit-testing**: `windowAt`, `windowSurfaceAt` and `surfaceLocalAt` (0006); the surface-position fallback in
  `mouseMoveUnified`; warps and pointer constraints (0007); decoration and corner hit selection (0008); popup
  reposition and unconstrain box (0009).

The patch keeps one helper, `HyprVX::windowOffset(window)`, which returns 0 for anything that is not window content.

**Rationale**: FR-005 and FR-006. These are the same seams Feature 001 already routes through the camera, so the list
is closed. A seam found outside it is a stop condition (record it with file:line before adding it).

**Alternatives considered**: Damage the whole output in Drift. U0 did this as a spike shortcut; it breaks the
partial-damage criterion (SC-007).

## R9. XWayland windows and override-redirect menus

**Decision**: Managed X11 windows are ordinary `CWindow`s, so they get `O(w)` like any window. An override-redirect
window inherits its offset at map time:

1. from the window of its `CXWaylandSurface::m_parent` (`XSurface.hpp:88`, WM_TRANSIENT_FOR) when set;
2. otherwise from the focused X11 window of the same client;
3. otherwise 0, and it is logged.

**Rationale**: Master plan U6 ("X11 override-redirect menu theo offset của parent"). Menus usually set
WM_TRANSIENT_FOR or belong to the focused window of their client.

**Alternatives considered**: Leave override-redirect windows at `O = 0`. Their menus would then be drawn away from the
window that opened them.

## R10. Entering validation

**Decision**: ENTERING checks that the running compositor is the one the plugin was built for. In form S the plugin
API hash is checked at load (the spike's check), then `GIT_COMMIT_HASH == efb5099…`
(`platform/include/hyprvx/platform/PluginCompatibility.hpp`, reused). In form P, the patchset id in `GIT_DIRTY`
(`hyprvx-<id>`) must equal the id the plugin was built against. A mismatch keeps Drift OFF and sends a notification
(§C7).

**Rationale**: Master plan §M item 1. V1 already has the commit check (Constitution VIII).

## R11. Test tooling additions

**Decision** (test-tooling contract 1.4):

- `hyprvx-vpointer` gains `scroll DX DY` (`wl_pointer` axis through `zwlr_virtual_pointer_v1`).
- A new `hyprvx-vkeyboard` (`zwp_virtual_keyboard_v1`, registered at `ProtocolManager.cpp:193`). It applies the same
  verified-nested-display refusal as vpointer. Virtual-keyboard modifiers count for binds and for bus listeners
  (`InputManager.cpp:1696-1712`; only an IME-grab client is ignored, `:1736-1745`), so Super can be held in tests.
- `hyprvx-testclient` logs `wl_keyboard` enter, key and modifiers, and `wl_pointer` axis events.
- A new `hyprvx-x11client` (xcb; headers present: xcb 1.17.0) draws the same markers, logs pointer and key events, and
  can map an override-redirect menu.
- The harness resolves the nested XWayland display only through the verified compositor PID (the X lock file's PID is
  the compositor or its child); otherwise it refuses.

**Rationale**: US3 needs keyboard, scroll and X11 evidence. `ydotool`/uinput stay forbidden (host safety). No X11 test
client is installed (`xterm`, `xeyes` absent; `Xwayland` present).

## R12. Reuse (Constitution VIII)

**Decision**: Reuse `CameraMath` (`anchorZoom`, `panByScreen`, `validate`, plus the new `offsetCamera`),
`PluginCompatibility`, the Feature 001 harness, analyzer, test client, vpointer, evidence workflow, and the spike's
hook registry and `gen_copies.py`. `DriftModel` is new.

**Rationale for new code**: V1 has no mode state machine (`core/`, `daemon/`, `platform/` searched). V1 `World` stores
absolute world geometry (`WorldWindow{id, geometry}`), but the V3 ownership model (§C5) is an offset over a logical box
that Hyprland owns. V1 `SpatialProjector` lays out workspace regions on a grid for persistence; that is Option B and
persistence, not this feature. V1 islands and `CameraController` (momentum, fly-to) belong to Phases 4–5.

## R13. Option A and the U0 seams

**Decision**: Option A shows only the active workspace. Stock visibility gates (`shouldRenderWindow`,
`commitWindow`, `updateSuspendedStates`) are therefore correct as they are, and none of the U0 seams is needed. U0
code is not used.

**Rationale**: `docs/v3/U0_FEASIBILITY_REPORT.md` (DV3-12, evidence only).

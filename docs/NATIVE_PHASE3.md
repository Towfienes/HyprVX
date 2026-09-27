# Phase 3 nested native prototype

## Safety and repeatable demo

The native executable and plugin are development tools for an isolated nested
Hyprland. The harness generates a temporary config and private runtime
directory. It never sources `~/.config/hypr`, changes Caelestia, installs a
service, or sends a command to the host compositor. Each harness operation
checks the nested Wayland and socket2 peer PIDs against the recorded compositor
PID. The C++ guard repeats that check. The plugin also requires that its own
process PID equal the nested compositor PID. Socket names alone are not trusted.

After building, run these commands from this repository, within the host
Wayland session:

```sh
python3 tools/hyprvx_nested.py start
python3 tools/hyprvx_nested.py overlay --diagnostics
python3 tools/hyprvx_nested.py clients
python3 tools/hyprvx_nested.py inspect-layers
python3 tools/hyprvx_nested.py inspect-clients
python3 tools/hyprvx_nested.py mode             # interactive input
python3 tools/hyprvx_nested.py mode             # passive input again
python3 tools/hyprvx_nested.py plugin-load
python3 tools/hyprvx_nested.py inspect-plugins
python3 tools/hyprvx_nested.py plugin-control pan 30 0
python3 tools/hyprvx_nested.py plugin-control enable
python3 tools/hyprvx_nested.py plugin-control reset
python3 tools/hyprvx_nested.py plugin-unload
python3 tools/hyprvx_nested.py camera-reset
python3 tools/hyprvx_nested.py stop
```

`plugin-control enable` deliberately returns an error: a native content
transform cannot preserve input hit testing with the exposed 0.56.2 window
transformer API. The `pan` command changes the HyprVX overlay camera through a
semantic action, demonstrating the guarded control bridge; it does not move
actual compositor window pixels. For a usable interactive overlay, switch mode
with `mode`, navigate with H/J/K/L, arrows, Shift+direction, Home, Alt+Left/
Right, middle-button drag, wheel, or supported touchpad gestures. Escape
returns the layer to passive mode. `camera-reset` sends a signal only to a
verified native child. `stop` terminates nested children and the nested
compositor and removes their temporary state after it confirms compositor exit.

## Architecture and ownership

`hyprvx_core` has no compositor or Wayland dependency. The existing socket2
parser, semantic adapter, RuntimeMirror, SpatialProjector, World, and
RuntimeCameraController remain the primary path. `hyprvx_wayland_platform`
owns the Wayland connection, registry objects, layer surface, seat devices,
XKB state, socket2 connection, native event datagram socket, and bounded SHM
buffers. All runtime and Wayland callbacks run on one thread. The poll loop
blocks indefinitely when there is no camera motion, wheel gesture, Wayland
frame callback, or socket event. Dirty states coalesce behind one frame
callback; there is no continuous frame loop or hyprctl camera loop.

The transparent `wlr-layer-shell-unstable-v1` layer uses zero exclusive zone.
Passive mode has an empty input region and no keyboard interactivity.
Interactive mode expands the input region and requests exclusive keyboard
interactivity. Protocols are Wayland core (`wl_compositor`, `wl_shm`, `wl_seat`,
`wl_output`), wlr layer shell, and optional `pointer-gestures-unstable-v1`.
The renderer uses ARGB8888 Wayland SHM with at most three buffers of at most
16 million pixels each. It draws projected window, workspace, and island
outlines and selection using the existing World and Camera transforms. Output
configure and scale update the viewport and buffer dimensions. One active
output remains the primary policy.

The plugin is a separate Hyprland-specific module, compiled for the locally
installed Hyprland 0.56.2 headers and exact commit
`efb50993780079460b0cbed1363e2166a2de1d9f`. Hyprland checks the plugin
API version; plugin init additionally checks the compiled and running commit
and nested process identity before registering callbacks or a dispatcher.
Unsupported versions leave the plugin inactive. All seven typed event
listeners are owned by `PluginState` and released before dispatcher removal on
unload. No render hook, input hook, compositor window mutation, or retained
transform is installed.

The plugin emits bounded JSON datagrams for window open/close/title/floating/
move, focus, workspace focus, and actual window geometry where `logicalBox()`
is available. The native receiver accepts only datagrams carrying the nested
compositor's PID/UID via `SO_PASSCRED`. The native adapter maps supported
events to the existing semantic mirror and keeps `ActualCompositorGeometry`
separate from projected World geometry. Socket2 stays active. Native movement
of a named workspace currently remains observational; the existing semantic
move type requires a numeric workspace ID. Geometry is never copied into
World. The plugin control interface contains `pan`, `reset`, `disable`,
`enable`, and `diagnostics`; it does not issue per-frame compositor commands.

## Native transform and zoom decision

The installed Hyprland 0.56.2 `IWindowTransformer` interface only changes a
window's main render pass, not popups. The installed header exposes a vector
of render transformers on `CWindow`, but provides no inverse mapping of
transformed pointer coordinates into the compositor's input hit test. A pan
through that API would leave the visible window and clickable window at
different positions, and popups would remain behind. Phase 3 therefore
**refuses content-transform activation** and keeps compositor rendering at
identity. Native camera pan is demonstrated in the overlay and control path.
True compositor zoom is **DEFERRED** for the same reason. This is an explicit
diagnostic result, not a claim of correct compositor content translation.

Emergency reset disables the plugin diagnostic mode, stops native transient
input motion, and resets the overlay camera to origin and zoom 1. Plugin
unload removes every listener and dispatcher. The compositor is unmodified
and continues rendering normally. No ownership/echo transaction is needed
because neither plugin nor overlay mutates the compositor model.

## Limits and failure handling

Native runtime starts with socket2's future event stream; socket2 provides no
initial window snapshot. Start the overlay before the demo clients. A socket2
EOF or Wayland disconnect ends the runtime cleanly; the harness can restart
it, but no reconnect polling loop runs. Seat capability loss cancels held
input; output removal ends the active surface path. Unsupported layer shell,
SHM format, excessive configure size, or corrupt restored state fails with a
diagnostic. Client-side touchpad swipe/pinch works only if the nested
compositor advertises `pointer-gestures-unstable-v1` and sends events to the
overlay; there is no `/dev/input` access. Physical pointer/keyboard gestures
were wired but cannot be automatically injected without host-affecting input
tools in this environment, so their live interaction was not asserted by the
smoke test.

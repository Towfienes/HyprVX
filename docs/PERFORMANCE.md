# Performance Engineering

## Philosophy

HyprVX should be invisible when idle.

## Hard rules

- no busy polling loops
- no repeated hyprctl frame loop
- no continuous redraw without visible change
- no permanent live thumbnails
- no hidden UI animation loop
- coalesce high-frequency input
- synchronize visual updates to compositor frames
- camera controller exposes whether a frame is needed; idle navigation waits for input events
- advance camera physics only while input, velocity, or a transition is active
- coalesce direct-pan deltas while draining an input event batch

## Initial baseline

Initial real-workload sample before HyprVX:
- CPU used average approximately 3.75 percent
- CPU idle average approximately 96.25 percent

The initial sample was taken under a real development workload and is not a clean laboratory benchmark.

## Initial targets

These are engineering targets, not guarantees.

- extra idle CPU should be near measurement noise
- daemon memory should remain in the tens of MiB rather than hundreds
- hidden Caelestia integration should have near-zero CPU activity
- camera input should normally affect the next rendered frame
- no continuous render loop while idle

Runtime direct-pan adapters accumulate deltas without allocating per event;
the simulator flushes them after the queued SDL events are drained. When no
input is held and the camera has no velocity or transition, the simulator
blocks on SDL events rather than scheduling frames. Timed waits are used only
while camera motion, a window-join dwell, or a wheel burst is active. Direct
middle-button pan updates only on pointer motion events and blocks between
them.

## Benchmark profiles

Eventually record:
- laptop AC
- laptop battery
- external display
- multi-monitor
- camera pan
- zoom
- many-window stress
- island stress

## Regression policy

A meaningful idle CPU, power, frame-time or memory regression blocks further feature work until understood.

## Phase 3 native prototype

The nested overlay blocks in `poll` while idle and schedules frames only for
dirty projection, camera, selection, or output state. One Wayland frame
callback gates outstanding redraws. ARGB8888 SHM is capped at three buffers
and 16 million pixels per buffer. The plugin has no render hot path: its
callbacks send bounded datagrams only on compositor events. Native transform
mode is refused, so its incremental idle CPU is zero. Measure actual idle and
active cost in the nested session before considering production integration.
In the Phase 3 smoke, `/proc` CPU accounting measured 0 ticks over five
seconds for the idle native overlay and 0 ticks over five seconds for the
nested compositor with the idle plugin loaded. Active navigation frame time
was not measured; this prototype is not yet a 120/210 Hz performance claim.

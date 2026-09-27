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

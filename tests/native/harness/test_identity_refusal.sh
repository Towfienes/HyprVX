#!/usr/bin/env bash
# V-07 (T017): the nested harness refuses to act on a session it cannot verify.
#
# A fake "compositor" (a Python process serving the session's Wayland and socket2 sockets) is recorded in a
# session marker under a test-only root. The untampered session must verify (control). Then each identity check is
# broken in turn: wrong PID, wrong start time, host signature, non-private runtime dir. For each, `run`, `dev`,
# `capture` and `stop` must exit 3, run nothing, and leave the fake compositor unsignalled. Finally a stale session
# (fake compositor gone) is cleaned by `recover` without signalling anything, and `stop` on it exits 3.
# Feature 002 (test-tooling 1.4, V-03): `xdisplay`, `plugin load` and `run --x11` refuse the tampered sessions too;
# on the intact fake session `xdisplay` accepts only a lock file holding the compositor's PID (or a direct child's)
# and `plugin load` refuses a session whose binary is outside the agent prefix (it would use a foreign hyprctl).
# hyprvx-vkeyboard and hyprvx-x11client (path in $1/$2, built by tests/native/lib.sh) refuse foreign displays.
# Feature 003 (test-tooling 1.5, V-02): an interactive (L6) session refuses `capture`, and `start --interactive` renders
# a config whose only change is the enabled WAYLAND-1 rule. `hvxtest` goes through `run` (refused above).
# No real compositor is started and the host session is never contacted (no X server is ever connected).
set -euo pipefail
readonly repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd)"
export HYPRVX_AGENT=claude
exec python3 -I - "$repo" "${1:-}" "${2:-}" <<'PY'
import json, os, shutil, signal, socket, subprocess, sys, tempfile, time
from pathlib import Path

repo = Path(sys.argv[1])
vkeyboard, x11client = sys.argv[2], sys.argv[3]
harness = [sys.executable, "-I", str(repo / "tools/hyprvx_nested.py")]
root = Path(tempfile.mkdtemp(prefix="hyprvx-nested-test-", dir="/tmp"))
runtime = Path(tempfile.mkdtemp(prefix="hvx-", dir="/tmp"))
os.chmod(runtime, 0o700)
signature = "hyprvx-test-signature"
host_signature = "host-signature-for-test"
(runtime / "hypr" / signature).mkdir(parents=True)
marker_file = root / "ran"
failures = []

server_code = r"""
import os, signal, socket, sys, time
paths = sys.argv[1:]
socks = []
for p in paths:
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM); s.bind(p); s.listen(8); s.setblocking(False); socks.append(s)
signal.signal(signal.SIGTERM, lambda *_: (open(os.environ["HVX_SIGNALLED"], "w").write("TERM"), sys.exit(0)))
while True:
    for s in socks:
        try:
            c, _ = s.accept(); c.close()
        except BlockingIOError:
            pass
    time.sleep(0.02)
"""
signalled = root / "signalled"
server = subprocess.Popen([sys.executable, "-I", "-c", server_code, str(runtime / "wayland-1"),
                           str(runtime / "hypr" / signature / ".socket2.sock")], env={**os.environ, "HVX_SIGNALLED": str(signalled)})
for _ in range(100):
    if (runtime / "wayland-1").exists() and (runtime / "hypr" / signature / ".socket2.sock").exists():
        break
    time.sleep(0.05)

def start_time(pid):
    return Path(f"/proc/{pid}/stat").read_text().rsplit(") ", 1)[1].split()[19]

session = root / "session-test"
session.mkdir(mode=0o700)
good = {"variant": "stock", "patchset_id": "none", "binary": os.path.realpath(f"/proc/{server.pid}/exe"),
        "binary_sha256": "test", "signature": signature, "host_signature": host_signature,
        "wayland_display": "wayland-1", "runtime_directory": str(runtime), "compositor_pid": server.pid,
        "process_start": start_time(server.pid), "launcher_pid": server.pid, "launcher_start": start_time(server.pid),
        "session_directory": str(session), "size": [1280, 800], "scale": "1", "children": []}

def write(data):
    path = root / "session.json"
    path.write_text(json.dumps(data))
    os.chmod(path, 0o600)

env = {**os.environ, "HYPRVX_NESTED_TEST_ROOT": str(root), "HYPRLAND_INSTANCE_SIGNATURE": host_signature}

def harness_run(*args, extra_env=None):
    return subprocess.run(harness + list(args), env={**env, **(extra_env or {})}, capture_output=True, text=True)

def check(name, condition, detail=""):
    print(("PASS " if condition else "FAIL ") + name + (f" ({detail})" if detail and not condition else ""))
    if not condition:
        failures.append(name)

try:
    # Control: an intact session verifies, so the refusals below are caused by the tampering alone.
    write(good)
    r = harness_run("status")
    check("control: intact fake session verifies", r.returncode == 0, r.stdout + r.stderr)

    cases = {
        "wrong PID": dict(good, compositor_pid=os.getpid(), process_start=start_time(os.getpid())),
        "wrong start time": dict(good, process_start=str(int(good["process_start"]) + 1)),
        "host signature": dict(good, host_signature=signature),
    }
    plugin_so = Path.home() / ".cache/hyprvx/claude/plugin/P/hyprvx-drift-P.so"
    for name, data in cases.items():
        write(data)
        for command in (["run", "--", "touch", str(marker_file)], ["dev", "state"], ["capture", str(root / "x.png")], ["stop"],
                        ["xdisplay"], ["plugin", "load", str(plugin_so)], ["run", "--x11", "--", "touch", str(marker_file)]):
            r = harness_run(*command)
            check(f"{name}: {command[0]} exits 3", r.returncode == 3, f"exit {r.returncode}: {r.stderr.strip()}")
    # Host signature taken from the live environment, not only from the marker.
    write(good)
    r = harness_run("run", "--", "touch", str(marker_file), extra_env={"HYPRLAND_INSTANCE_SIGNATURE": signature})
    check("env host signature equal to nested: run exits 3", r.returncode == 3, r.stderr.strip())
    # Non-private runtime directory.
    os.chmod(runtime, 0o755)
    for command in (["run", "--", "touch", str(marker_file)], ["stop"]):
        r = harness_run(*command)
        check(f"non-private runtime dir: {command[0]} exits 3", r.returncode == 3, r.stderr.strip())
    os.chmod(runtime, 0o700)

    # Intact fake session: the binary is not an agent-prefix build, so plugin load must not use its hyprctl.
    write(good)
    r = harness_run("plugin", "load", str(plugin_so))
    check("intact session, binary outside the agent prefix: plugin load exits 3", r.returncode == 3, f"exit {r.returncode}: {r.stderr.strip()}")
    # xdisplay on the intact fake session. Lock files are only read; the test never creates anything in /tmp/.X11-unix.
    # No lock file of ours holds the fake compositor's PID, so the display is refused (exit 3) - unless a stale lock with
    # that PID exists, which the check below would report.
    r = harness_run("xdisplay")
    check("intact session without its own XWayland lock: xdisplay exits 3", r.returncode == 3, f"exit {r.returncode}: {r.stdout} {r.stderr.strip()}")
    r = harness_run("run", "--x11", "--", "touch", str(marker_file))
    check("intact session without its own XWayland lock: run --x11 exits 3", r.returncode == 3, r.stderr.strip())

    # Interactive (L6) session: captures are refused on an otherwise intact session.
    write(dict(good, interactive=True))
    shot = root / "interactive.png"
    r = harness_run("capture", str(shot))
    check("interactive session: capture refused (exit 1, no file)", r.returncode == 1 and not shot.exists() and "interactive" in r.stderr,
          f"exit {r.returncode}: {r.stderr.strip()}")
    # start --interactive renders the template with only the WAYLAND-1 rule changed: that one line becomes the visible
    # output rule plus the cursor block of the L6 session (CPU cursor buffer, T045), and nothing else differs.
    import importlib.util
    spec = importlib.util.spec_from_file_location("hn", repo / "tools/hyprvx_nested.py")
    hn = importlib.util.module_from_spec(spec); spec.loader.exec_module(hn)
    normal, inter = root / "normal.conf", root / "inter.conf"
    hn.render_config(normal, (1280, 800), "1"); hn.render_config(inter, (1280, 800), "1", True)
    a, b = normal.read_text(), inter.read_text()
    expected_on = "monitor = WAYLAND-1, preferred, auto, 1\ncursor {\n    use_cpu_buffer = 1\n}"
    check("render_config --interactive changes only the WAYLAND-1 rule",
          a.count(hn.INTERACTIVE_RULE_OFF) == 1 and hn.INTERACTIVE_RULE_ON == expected_on
          and b == a.replace(hn.INTERACTIVE_RULE_OFF, expected_on), repr(hn.INTERACTIVE_RULE_ON))
    write(good)

    check("nothing ran for any refused command", not marker_file.exists())
    check("fake compositor never signalled", server.poll() is None and not signalled.exists())

    # Stale session: the recorded process is gone.
    server.kill(); server.wait()
    write(good)
    r = harness_run("stop")
    check("stale session: stop exits 3", r.returncode == 3, r.stderr.strip())
    r = harness_run("recover")
    check("stale session: recover cleans up (exit 0)", r.returncode == 0 and not (root / "session.json").exists(), r.stdout + r.stderr)
    check("stale session: logs kept", (session / "session.json").exists())
    check("stale session: runtime dir removed", not runtime.exists())

    # Test tools on foreign displays (no compositor of ours behind them).
    def tool_env(extra):
        e = {k: v for k, v in os.environ.items() if k not in ("WAYLAND_DISPLAY", "XDG_RUNTIME_DIR", "WAYLAND_SOCKET", "DISPLAY")}
        e.update(extra)
        return e
    fake_rt = Path(tempfile.mkdtemp(prefix="hvx-", dir="/tmp"))
    os.chmod(fake_rt, 0o700)
    fake_srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    fake_srv.bind(str(fake_rt / "wayland-9")); fake_srv.listen(4)
    fifo = root / "tool.fifo"
    os.mkfifo(fifo, 0o600)
    try:
        if vkeyboard:
            for name, extra in {"host-like runtime dir": {"XDG_RUNTIME_DIR": os.environ.get("XDG_RUNTIME_DIR", "/run/user/1000"), "WAYLAND_DISPLAY": "wayland-1"},
                                "display path": {"XDG_RUNTIME_DIR": str(fake_rt), "WAYLAND_DISPLAY": "/tmp/x/wayland-1"},
                                "private dir, foreign compositor": {"XDG_RUNTIME_DIR": str(fake_rt), "WAYLAND_DISPLAY": "wayland-9"}}.items():
                r = subprocess.run([vkeyboard, "--log", str(root / "vk.jsonl"), "--control", str(fifo)], env=tool_env(extra),
                                   capture_output=True, text=True, timeout=10)
                check(f"vkeyboard, {name}: exits 3", r.returncode == 3, f"exit {r.returncode}: {r.stderr.strip()}")
        else:
            print("NOT RUN vkeyboard refusal (no binary given)")
            failures.append("vkeyboard refusal not run")
        if x11client:
            for name, extra in {"host-like runtime dir": {"XDG_RUNTIME_DIR": os.environ.get("XDG_RUNTIME_DIR", "/run/user/1000"), "DISPLAY": ":99"},
                                "no DISPLAY": {"XDG_RUNTIME_DIR": str(fake_rt)},
                                "DISPLAY not a local display": {"XDG_RUNTIME_DIR": str(fake_rt), "DISPLAY": "tcp/host:0"}}.items():
                r = subprocess.run([x11client, "--size", "100x100", "--title", "t", "--log", str(root / "x.jsonl"), "--control", str(fifo)],
                                   env=tool_env(extra), capture_output=True, text=True, timeout=10)
                check(f"x11client, {name}: exits 3", r.returncode == 3, f"exit {r.returncode}: {r.stderr.strip()}")
        else:
            print("NOT RUN x11client refusal (no binary given)")
            failures.append("x11client refusal not run")
    finally:
        fake_srv.close()
        shutil.rmtree(fake_rt, ignore_errors=True)
finally:
    if server.poll() is None:
        server.kill(); server.wait()
    shutil.rmtree(root, ignore_errors=True)
    shutil.rmtree(runtime, ignore_errors=True)

print("RESULT:", "PASS" if not failures else "FAIL " + ", ".join(failures))
sys.exit(0 if not failures else 1)
PY

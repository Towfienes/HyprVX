#!/usr/bin/env bash
# V-07 (T017): the nested harness refuses to act on a session it cannot verify.
#
# A fake "compositor" (a Python process serving the session's Wayland and socket2 sockets) is recorded in a
# session marker under a test-only root. The untampered session must verify (control). Then each identity check is
# broken in turn: wrong PID, wrong start time, host signature, non-private runtime dir. For each, `run`, `dev`,
# `capture` and `stop` must exit 3, run nothing, and leave the fake compositor unsignalled. Finally a stale session
# (fake compositor gone) is cleaned by `recover` without signalling anything, and `stop` on it exits 3.
# No real compositor is started and the host session is never contacted.
set -euo pipefail
readonly repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd)"
export HYPRVX_AGENT=claude
exec python3 -I - "$repo" <<'PY'
import json, os, shutil, signal, socket, subprocess, sys, tempfile, time
from pathlib import Path

repo = Path(sys.argv[1])
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
    for name, data in cases.items():
        write(data)
        for command in (["run", "--", "touch", str(marker_file)], ["dev", "state"], ["capture", str(root / "x.png")], ["stop"]):
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
finally:
    if server.poll() is None:
        server.kill(); server.wait()
    shutil.rmtree(root, ignore_errors=True)
    shutil.rmtree(runtime, ignore_errors=True)

print("RESULT:", "PASS" if not failures else "FAIL " + ", ".join(failures))
sys.exit(0 if not failures else 1)
PY

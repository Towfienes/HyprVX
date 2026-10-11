#!/usr/bin/env bash
# T054: hyprvx-vpointer refuses every display that is not the verified nested compositor (test-tooling 4a).
#
# Cases: host-like environment (XDG_RUNTIME_DIR outside /tmp/hvx-*), a /tmp/hvx-* dir with the wrong mode, a
# WAYLAND_DISPLAY with a path, and a private /tmp/hvx-* dir whose Wayland socket is served by a fake "compositor"
# (a Python process, so its /proc/<pid>/exe is not under ~/.cache/hyprvx/<agent>/prefix/). Each must exit 3 and the
# fake server must receive no bytes (wl_display_connect sends nothing; the registry is only requested after the peer
# check). No real compositor is started and the host session is never contacted.
# Usage: test_vpointer_refusal.sh /path/to/hyprvx-vpointer
set -euo pipefail
readonly vpointer="${1:?usage: test_vpointer_refusal.sh /path/to/hyprvx-vpointer}"
exec python3 -I - "$vpointer" <<'PY'
import os, shutil, socket, subprocess, sys, tempfile, threading
from pathlib import Path

vpointer = sys.argv[1]
failures = []
work = Path(tempfile.mkdtemp(prefix="hyprvx-vpointer-test-"))
script = work / "script"
script.write_text("move 1 1\n")

def check(name, ok, detail=""):
    print(("PASS " if ok else "FAIL ") + name + (f" ({detail})" if detail and not ok else ""))
    if not ok:
        failures.append(name)

def run(env_extra):
    env = {k: v for k, v in os.environ.items() if k not in ("WAYLAND_DISPLAY", "XDG_RUNTIME_DIR", "WAYLAND_SOCKET")}
    env.update(env_extra)
    return subprocess.run([vpointer, "--layout", "100x100", "--log", str(work / "log.jsonl"), "--script", str(script)],
                          env=env, capture_output=True, text=True, timeout=10)

runtime = Path(tempfile.mkdtemp(prefix="hvx-", dir="/tmp"))
try:
    r = run({"XDG_RUNTIME_DIR": str(work), "WAYLAND_DISPLAY": "wayland-1"})
    check("runtime dir outside /tmp/hvx-*: exit 3", r.returncode == 3, r.stderr.strip())

    os.chmod(runtime, 0o755)
    r = run({"XDG_RUNTIME_DIR": str(runtime), "WAYLAND_DISPLAY": "wayland-1"})
    check("runtime dir mode 0755: exit 3", r.returncode == 3, r.stderr.strip())
    os.chmod(runtime, 0o700)

    r = run({"XDG_RUNTIME_DIR": str(runtime), "WAYLAND_DISPLAY": "/tmp/elsewhere/wayland-1"})
    check("WAYLAND_DISPLAY with a path: exit 3", r.returncode == 3, r.stderr.strip())

    sock_path = runtime / "wayland-1"
    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.bind(str(sock_path))
    server.listen(1)
    received = []
    def serve():
        conn, _ = server.accept()
        conn.settimeout(5)
        try:
            while True:
                data = conn.recv(4096)
                if not data:
                    break
                received.append(data)
        except OSError:
            pass
        conn.close()
    t = threading.Thread(target=serve, daemon=True)
    t.start()
    r = run({"XDG_RUNTIME_DIR": str(runtime), "WAYLAND_DISPLAY": "wayland-1"})
    t.join(6)
    check("fake compositor (exe not under the agent prefix): exit 3", r.returncode == 3, r.stderr.strip())
    check("fake compositor received no protocol bytes", not received, repr(received[:1]))
    check("refusal names the peer check", "agent-prefix" in r.stderr, r.stderr.strip())
    server.close()
    check("no log line written on refusal", not (work / "log.jsonl").exists() or "done" not in (work / "log.jsonl").read_text())
finally:
    shutil.rmtree(runtime, ignore_errors=True)
    shutil.rmtree(work, ignore_errors=True)

print("RESULT:", "PASS" if not failures else "FAIL " + ", ".join(failures))
sys.exit(0 if not failures else 1)
PY

#!/usr/bin/env python3
"""Socket2 event logger for HyprVX Feature 002 native tests (test-tooling 1.4, T014).

  python3 -I tests/native/events_log.py OUT

Started only through `scripts/nested.sh run --background`, which re-verifies the nested session and sets
XDG_RUNTIME_DIR and HYPRLAND_INSTANCE_SIGNATURE to it. Connects to that session's .socket2.sock, checks that its
SO_PEERCRED peer is an agent-prefix Hyprland binary, and appends "<t_ns> <event line>" to OUT until terminated (each line
flushed). Refuses (exit 3) anything that is not a private /tmp/hvx-* runtime directory; never the host socket.
"""
import os
from pathlib import Path
import socket
import stat
import struct
import sys
import time


def refuse(message):
    print(f"events_log: refusing: {message}", file=sys.stderr)
    sys.exit(3)


def main(out):
    runtime = Path(os.environ.get("XDG_RUNTIME_DIR", ""))
    signature = os.environ.get("HYPRLAND_INSTANCE_SIGNATURE", "")
    try:
        info = os.lstat(runtime)
    except OSError:
        refuse("XDG_RUNTIME_DIR is missing")
    if (runtime.parent != Path("/tmp") or not runtime.name.startswith("hvx-") or not stat.S_ISDIR(info.st_mode)
            or info.st_uid != os.getuid() or stat.S_IMODE(info.st_mode) != 0o700):
        refuse("XDG_RUNTIME_DIR is not a private /tmp/hvx-* directory (use scripts/nested.sh run)")
    if not signature or "/" in signature:
        refuse("no nested instance signature")
    path = runtime / "hypr" / signature / ".socket2.sock"
    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    client.connect(str(path))
    pid, uid, _ = struct.unpack("3i", client.getsockopt(socket.SOL_SOCKET, socket.SO_PEERCRED, 12))
    exe = os.path.realpath(f"/proc/{pid}/exe")
    cache_root = str(Path.home() / ".cache/hyprvx") + "/"
    if uid != os.getuid() or not exe.startswith(cache_root) or "/prefix/" not in exe:
        refuse(f"socket2 peer is not an agent-prefix Hyprland ({exe})")
    with open(out, "a") as log:
        log.write(f"{time.monotonic_ns()} hyprvx-events-log>>ready\n")
        log.flush()
        pending = b""
        while True:
            chunk = client.recv(65536)
            if not chunk:
                break
            pending += chunk
            *lines, pending = pending.split(b"\n")
            for line in lines:
                log.write(f"{time.monotonic_ns()} {line.decode('utf-8', 'replace')}\n")
            log.flush()


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        sys.exit(2)
    main(sys.argv[1])

#!/usr/bin/env python3
"""U0 spike (throwaway): records the nested compositor's socket2 events with a monotonic timestamp.
      u0_events.py OUT
Run through `scripts/nested.sh run --background --`, which re-verifies the session and sets XDG_RUNTIME_DIR and
HYPRLAND_INSTANCE_SIGNATURE to the nested instance (never the host). One line per event: "<ns> <event>>><data>".
"""
import os
import socket
import sys
import time

path = os.path.join(os.environ["XDG_RUNTIME_DIR"], "hypr", os.environ["HYPRLAND_INSTANCE_SIGNATURE"], ".socket2.sock")
with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock, open(sys.argv[1], "a", buffering=1) as out:
    sock.connect(path)
    out.write(f"{time.monotonic_ns()} hvx>>connected\n")
    buffer = b""
    while chunk := sock.recv(65536):
        buffer += chunk
        *lines, buffer = buffer.split(b"\n")
        for line in lines:
            out.write(f"{time.monotonic_ns()} {line.decode(errors='replace')}\n")

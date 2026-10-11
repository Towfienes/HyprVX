#!/usr/bin/env python3
"""Drives hyprvx-vpointer and attributes testclient pointer events to each command (test-tooling 4a).

  python3 -I tests/native/pointer_driver.py --vpointer FIFO:LOG --client NAME:FIFO:LOG [--client ...] \
      --commands FILE --out JSON [--timeout S]

For every command line in FILE ("move X Y", "down", "up", "click"): send it to the vpointer FIFO, wait for its
"done" line (the compositor has processed it and sent any pointer events), then send "sync" to every client and wait
for that commit's frame_done. Pointer lines a client logged before that frame_done and after the previous barrier
belong to the command. Output JSON: {"steps": [{"cmd": ..., "events": {NAME: [event, ...]}}, ...]}.
Exit codes: 0 done, 2 BLOCKED (a ready signal timed out or vpointer rejected a command; the partial result is
still written).
"""

import argparse
import json
import sys
import time
from pathlib import Path

POINTER_EVENTS = {"enter", "leave", "motion", "button", "warp"}


class Blocked(Exception):
    pass


def read_lines(path):
    with open(path, encoding="utf-8") as f:
        return [json.loads(line) for line in f if line.strip()]


def wait_for(predicate, timeout, what):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        found = predicate()
        if found is not None:
            return found
        time.sleep(0.005)
    raise Blocked(f"timeout waiting for {what}")


def send(fifo, line):
    with open(fifo, "w", encoding="utf-8") as f:
        f.write(line + "\n")


def run_vpointer(fifo, log, cmd, timeout):
    """Sends one command and waits for its done line; a new error line means vpointer rejected it."""
    before = read_lines(log)
    done0 = sum(1 for e in before if e.get("ev") == "done")
    err0 = sum(1 for e in before if e.get("ev") == "error")
    send(fifo, cmd)

    def finished():
        lines = read_lines(log)
        if sum(1 for e in lines if e.get("ev") == "error") > err0:
            raise Blocked(f"vpointer rejected {cmd!r}")
        return True if sum(1 for e in lines if e.get("ev") == "done") > done0 else None
    wait_for(finished, timeout, f"vpointer done for {cmd!r}")


def barrier(name, fifo, log, timeout):
    """sync the client and wait for frame_done of that commit; returns the index of the frame_done line."""
    last = max((e.get("seq", 0) for e in read_lines(log) if e.get("ev") == "commit"), default=0)
    send(fifo, "sync")
    seq = wait_for(lambda: next((e["seq"] for e in read_lines(log) if e.get("ev") == "commit" and e["seq"] > last), None),
                   timeout, f"{name} sync commit")

    def done():
        for i, e in enumerate(read_lines(log)):
            if e.get("ev") == "frame_done" and e.get("seq") == seq:
                return i
        return None
    return wait_for(done, timeout, f"{name} frame_done seq {seq}")


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("--vpointer", required=True, help="FIFO:LOG of a running hyprvx-vpointer --control")
    parser.add_argument("--client", action="append", required=True, help="NAME:FIFO:LOG of a running testclient")
    parser.add_argument("--commands", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--timeout", type=float, default=5.0)
    args = parser.parse_args(argv)

    vp_fifo, vp_log = args.vpointer.split(":", 1)
    clients = [tuple(c.split(":", 2)) for c in args.client]
    commands = [line.strip() for line in Path(args.commands).read_text().splitlines() if line.strip()]
    cursor = {name: len(read_lines(log)) for name, _, log in clients}
    steps, status = [], 0
    try:
        for cmd in commands:
            run_vpointer(vp_fifo, vp_log, cmd, args.timeout)
            step = {"cmd": cmd, "events": {}}
            for name, fifo, log in clients:
                end = barrier(name, fifo, log, args.timeout)
                lines = read_lines(log)
                step["events"][name] = [e for e in lines[cursor[name]:end] if e.get("ev") in POINTER_EVENTS]
                cursor[name] = end + 1
            steps.append(step)
    except Blocked as error:
        status = 2
        steps.append({"blocked": str(error)})
    Path(args.out).write_text(json.dumps({"steps": steps}, indent=1) + "\n")
    if status:
        print(f"BLOCKED: {steps[-1]['blocked']}", file=sys.stderr)
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

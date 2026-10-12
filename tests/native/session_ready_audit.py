#!/usr/bin/env python3
"""Archive-based nested-session audit (Feature 003 T044, supplement to host_safety_audit.sh).

host_safety_audit.sh reads the live session markers in /tmp, which a reboot wipes (tmpfs). This audit reads the
archived evidence runs instead: every "nested <variant> ready ...: <signature> on <display>; pid=..; session=<dir>"
line of a run's session log must name a private harness session directory of the agent and a nested instance
signature (<40-hex commit>_<time>_<n>), and no signature may appear in more than one run (the host instance keeps one
signature for its whole lifetime, so a repeated signature would point at a session that was not the harness's own).

usage: session_ready_audit.py --agent claude --since "2026-10-12 07:00" --out REPORT.json
Exit 0 when every line passes and at least one line was found, 1 otherwise.
"""
import argparse
import collections
import datetime
import json
import os
import re
import sys
from pathlib import Path

READY = re.compile(r"nested (\w[\w-]*) ready[^:]*: (\S+) on (\S+); pid=\d+; session=(\S+)")
SIGNATURE = re.compile(r"^[0-9a-f]{40}_\d+_\d+$")


def main(argv):
    p = argparse.ArgumentParser()
    p.add_argument("--agent", required=True)
    p.add_argument("--since", required=True)
    p.add_argument("--out", required=True)
    args = p.parse_args(argv)
    since = datetime.datetime.strptime(args.since, "%Y-%m-%d %H:%M").timestamp()
    evidence = Path.home() / ".local/share/hyprvx/evidence" / args.agent
    session_prefix = f"/tmp/hyprvx-nested-{os.getuid()}-{args.agent}/session-"
    seen = collections.defaultdict(set)
    bad, lines, runs = [], 0, 0
    for run in sorted(evidence.iterdir()):
        if not run.is_dir() or run.stat().st_mtime < since:
            continue
        found = False
        for log in sorted(run.glob("*session*.log")):
            for m in READY.finditer(log.read_text(errors="replace")):
                found, lines = True, lines + 1
                _variant, signature, _display, session = m.groups()
                seen[signature].add(run.name)
                if not (session.startswith(session_prefix) and SIGNATURE.match(signature)):
                    bad.append({"run": run.name, "log": log.name, "signature": signature, "session": session})
        runs += found
    repeated = {s: sorted(r) for s, r in seen.items() if len(r) > 1}
    commits = collections.Counter(s[:7] for s in seen)
    verdict = "PASS" if lines and not bad and not repeated else "FAIL"
    report = {"since": args.since, "runs": runs, "ready_lines": lines, "distinct_signatures": len(seen),
              "compositor_commits": dict(commits), "bad": bad, "repeated": repeated, "verdict": verdict}
    Path(args.out).write_text(json.dumps(report, indent=1) + "\n")
    print(f"session ready audit: {runs} runs, {lines} ready lines, {len(seen)} distinct signatures, "
          f"{len(bad)} bad, {len(repeated)} repeated -> {verdict}")
    return 0 if verdict == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

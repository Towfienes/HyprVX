#!/usr/bin/env bash
set -euo pipefail

case ${HYPRVX_AGENT:-} in
    claude|codex) ;;
    *) echo 'Set HYPRVX_AGENT to claude or codex.' >&2; exit 2 ;;
esac
variant=stock
dry_run=false
while [[ $# -gt 0 ]]; do
    case $1 in
        --variant) [[ $# -ge 2 ]] || exit 2; variant=$2; shift 2 ;;
        --dry-run) dry_run=true; shift ;;
        *) echo 'Usage: build-hyprland.sh [--variant stock|v2|patched] [--dry-run]' >&2; exit 2 ;;
    esac
done
case $variant in stock|v2|patched) ;; *) echo 'Unknown variant.' >&2; exit 2 ;; esac
readonly repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if ! "$dry_run"; then
    available=$(awk '$1 == "MemAvailable:" {print $2}' /proc/meminfo)
    [[ $available =~ ^[0-9]+$ ]] && (( available >= 6815744 )) || {
        echo "BLOCKED: MemAvailable=${available:-unknown} KiB; need 6815744 KiB (6.5 GiB)." >&2; exit 1;
    }
    exec 9< "$HOME/.cache/hyprvx/locks/heavy.lock"
    flock -w 3600 9 || { echo 'BLOCKED: heavy lock timeout.' >&2; exit 1; }
fi

python3 - "$repo" "$variant" "$dry_run" <<'PY'
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import select
import shlex
import signal
import subprocess
import sys
import tempfile
import uuid

repo, variant, dry_run = Path(sys.argv[1]), sys.argv[2], sys.argv[3] == "true"
pin = "efb50993780079460b0cbed1363e2166a2de1d9f"
agent = os.environ["HYPRVX_AGENT"]
cache = Path.home() / ".cache/hyprvx" / agent
src = cache / "src/hyprland-efb5099" / ("v3-dev" if variant == "patched" else variant)
patch_dir = repo / ("research/v2-drift-mvp" if variant == "v2" else "patches/hyprland")
patches = [] if variant == "stock" else sorted(patch_dir.glob("*.patch"))


def blocked(reason):
    sys.exit("BLOCKED: " + reason)


def output(command, env=None):
    return subprocess.check_output(command, env=env, text=True).strip()


def git(*args):
    return output(["git", "-C", str(src), *args])


def full_avg10():
    for line in Path("/proc/pressure/memory").read_text().splitlines():
        if line.startswith("full "):
            return float(dict(field.split("=") for field in line.split()[1:])["avg10"])
    blocked("memory PSI full avg10 is unavailable")


if variant != "stock" and not patches:
    blocked("no patches for this variant")
patchset_id = hashlib.sha256(b"".join(p.read_bytes() for p in patches)).hexdigest()[:12] if patches else "none"
identifier = "stock" if variant == "stock" else ("v2-" + patchset_id if variant == "v2" else patchset_id)
prefix = cache / "prefix" / identifier
git_dirty = "clean-upstream" if variant == "stock" else "hyprvx-" + patchset_id
env = os.environ.copy()
env["CCACHE_DIR"] = str(cache / "ccache")
env["PATH"] = str(cache / "tools/bin") + os.pathsep + env["PATH"]
env["PKG_CONFIG_PATH"] = str(cache / "tools/lib/pkgconfig") + os.pathsep + env.get("PKG_CONFIG_PATH", "")
build = cache / "build" / (identifier + "-fresh")

if not dry_run:
    available = int(next(line.split()[1] for line in Path("/proc/meminfo").read_text().splitlines()
                         if line.startswith("MemAvailable:")))
    if available < 6815744 or full_avg10() > 10:
        blocked("memory headroom or PSI changed while waiting for the heavy lock")
    for directory in (src, prefix, cache / "build", cache / "ccache", cache / "tools"):
        if directory.is_symlink():
            blocked("build directory is a symlink: " + str(directory))
    if not (src / ".git").exists():
        blocked("fetch the agent worktrees first")
    if git("rev-parse", "--path-format=absolute", "--git-common-dir") != str(src.parent / ".git"):
        blocked("source is not a worktree of the agent clone")
    if git("status", "--porcelain"):
        blocked("source checkout has changes")
    head = git("rev-parse", "HEAD")
    if variant == "stock":
        if head != pin:
            blocked("stock HEAD differs from the pinned upstream")
    else:
        receipt_path = cache / "src" / ("hyprland-" + variant + "-patchset.json")
        try:
            receipt = json.loads(receipt_path.read_text())
        except (OSError, ValueError) as error:
            blocked("apply-patches receipt is unavailable: " + str(error))
        if receipt != {"patchset_id": patchset_id, "head": head}:
            blocked("patch hashes or applied HEAD differ from the receipt")
        subprocess.run(["git", "-C", str(src), "merge-base", "--is-ancestor", pin, head], check=True)
    if not os.access(cache / "tools/bin/hyprwayland-scanner", os.X_OK):
        blocked("run scripts/build-scanner.sh first")
    scanner_version = output(["pkg-config", "--modversion", "hyprwayland-scanner"], env)
    if scanner_version != "0.4.6":
        blocked("local scanner is not v0.4.6")
    env.update(GIT_COMMIT_HASH=head, GIT_DIRTY=git_dirty,
               GIT_BRANCH=git("branch", "--show-current"),
               GIT_COMMIT_MESSAGE=git("show", "-s", "--format=%s", "--no-show-signature"),
               GIT_COMMIT_DATE=git("show", "-s", "--format=%cd", "--date=local", "--no-show-signature"),
               GIT_TAG=git("describe", "--tags"), GIT_COMMITS=git("rev-list", "--count", "HEAD"))
    (cache / "build").mkdir(parents=True, exist_ok=True)
    (cache / "ccache").mkdir(parents=True, exist_ok=True)
    build = Path(tempfile.mkdtemp(prefix=identifier + "-", dir=cache / "build"))

configure = ["cmake", "-S", str(src), "-B", str(build), "-G", "Ninja",
             "-DCMAKE_BUILD_TYPE=RelWithDebInfo", "-DNO_HYPRPM=TRUE",
             "-DCMAKE_INSTALL_PREFIX=" + str(prefix), "-DCMAKE_PREFIX_PATH=" + str(cache / "tools"),
             "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache", "-DCMAKE_C_COMPILER_LAUNCHER=ccache",
             "-DCMAKE_DISABLE_FIND_PACKAGE_Git=TRUE"]
if variant == "patched":
    configure.append("-DCMAKE_CXX_FLAGS=-DHYPRVX_DEV=1")
commands = [configure, ["cmake", "--build", str(build), "-j4"], ["cmake", "--install", str(build)]]
unit = "hyprvx-build-" + agent + "-" + uuid.uuid4().hex + ".scope"
scope = ["systemd-run", "--user", "--scope", "--unit=" + unit, "-p", "MemoryMax=6G", "--",
         "bash", "-c", "set -e; " + "; ".join(shlex.join(command) for command in commands)]
if dry_run:
    print("DRY RUN: require MemAvailable >= 6.5 GiB and full avg10 <= 10")
    print("DRY RUN: flock -w 3600 " + str(Path.home() / ".cache/hyprvx/locks/heavy.lock"))
    print("DRY RUN: verify pinned source, receipt, scanner v0.4.6 and real GIT_COMMIT_HASH")
    print("DRY RUN: GIT_DIRTY=" + git_dirty + " CCACHE_DIR=" + env["CCACHE_DIR"])
    print("DRY RUN: " + shlex.join(scope))
    print("DRY RUN: write " + str(prefix / "build-info.json"))
    sys.exit(0)

# Kernel PSI notifications avoid a polling loop; absent trigger access blocks the job.
try:
    pressure = os.open("/proc/pressure/memory", os.O_RDWR | os.O_NONBLOCK)
    os.write(pressure, b"full 100000 2000000")
except OSError as error:
    blocked("cannot register the memory PSI trigger: " + str(error))


def interrupted(signum, frame):
    raise InterruptedError("build interrupted")


signal.signal(signal.SIGTERM, interrupted)
process = subprocess.Popen(scope, env=env)
pidfd = None
try:
    pidfd = os.pidfd_open(process.pid)
    poller = select.poll()
    poller.register(pidfd, select.POLLIN)
    poller.register(pressure, select.POLLPRI)
    while process.poll() is None:
        for fd, event in poller.poll():
            if fd == pressure and event & (select.POLLERR | select.POLLHUP | select.POLLNVAL):
                blocked("memory PSI notification descriptor failed; stopping " + unit)
            if fd == pressure and event & select.POLLPRI and full_avg10() > 10:
                blocked("memory full avg10 exceeded 10; stopping " + unit)
    if process.wait() != 0:
        blocked("scoped configure/build/install failed; inspect the build log")
finally:
    if process.poll() is None:
        subprocess.run(["systemctl", "--user", "stop", unit], check=True)
        process.wait()
    os.close(pressure)
    if pidfd is not None:
        os.close(pidfd)

version_header = (src / "src/version.h").read_text()
if ('#define GIT_COMMIT_HASH "' + head + '"' not in version_header or
        '#define GIT_DIRTY "' + git_dirty + '"' not in version_header):
    blocked("generated version.h does not match truthful build provenance")
compiler = re.search(r"^CMAKE_CXX_COMPILER:FILEPATH=(.+)$", (build / "CMakeCache.txt").read_text(), re.MULTILINE)
if not compiler:
    blocked("configured C++ compiler is unavailable")
info = {"variant": variant, "upstream_commit": pin, "source_commit": head, "patchset_id": patchset_id,
        "patches": [{"file": str(p.relative_to(repo)), "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
                    for p in patches], "git_dirty": git_dirty,
        "binary_sha256": hashlib.sha256((prefix / "bin/Hyprland").read_bytes()).hexdigest(),
        "compiler": output([compiler.group(1), "--version"]).splitlines()[0],
        "cmake": output(["cmake", "--version"]).splitlines()[0],
        "libs": {lib: output(["pkg-config", "--modversion", lib], env)
                 for lib in ("aquamarine", "hyprutils", "hyprlang", "hyprgraphics", "hyprcursor")},
        "scanner": "hyprwayland-scanner v" + scanner_version, "built_at": datetime.now().astimezone().isoformat()}
fd, temporary = tempfile.mkstemp(prefix="build-info.", dir=prefix)
try:
    with os.fdopen(fd, "w") as handle:
        json.dump(info, handle, indent=2)
        handle.write("\n")
        handle.flush()
        os.fsync(handle.fileno())
    os.replace(temporary, prefix / "build-info.json")
finally:
    if os.path.exists(temporary):
        os.unlink(temporary)
print("Build info: " + str(prefix / "build-info.json"))
PY

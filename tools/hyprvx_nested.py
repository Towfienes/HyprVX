#!/usr/bin/env python3
"""HyprVX V3 nested Hyprland harness (contracts/test-tooling.md section 7). Never targets the host instance.

  hyprvx_nested.py start --variant stock|v2|patched [--size WxH] [--scale S] [--binary PATH] [--lock-timeout SEC]
  hyprvx_nested.py run [--background] [--x11] -- CMD...  # --x11: DISPLAY from xdisplay (test-tooling 1.4 §7)
  hyprvx_nested.py plugin load|unload PATH        # a plugin from the agent cache's plugin/ dir; prints its sha256
  hyprvx_nested.py xdisplay                       # the nested XWayland display (:N), only via the verified PID
  hyprvx_nested.py dev COMMAND [ARGS...]          # hyprctl hyprvx-dev COMMAND (dev-control.md transport A)
  hyprvx_nested.py capture PATH                   # grim on the nested output (.png or .ppm)
  hyprvx_nested.py dispatch hyprvx:NAME [ARGS...] # V2-as-is adapter (variant v2 only): hyprctl dispatch hyprvx:*
  hyprvx_nested.py status | stop | recover

Every command except start and status re-verifies the session before acting: the SO_PEERCRED peer of the nested
Wayland and socket2 sockets is the recorded compositor PID, the PID has the recorded start time and executable, the
instance signature differs from the host's, and the runtime directory is a private /tmp/hvx-* directory. A failed
verification exits 3 and sends nothing. Exit codes: 0 ok, 1 command failed, 2 usage, 3 identity verification failed.
The nested compositor runs under `flock heavy.lock` (held until it exits), `dbus-run-session` and
HYPRLAND_NO_SD_VARS=1, from the agent prefix only (never $PATH). Logs stay in the session directory after stop.
(The V1 Phase 3 commands of this file live on the frozen V1 branch `dev`.)
"""

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import stat
import struct
import subprocess
import sys
import tempfile
import time

EXIT_OK, EXIT_FAILED, EXIT_USAGE, EXIT_IDENTITY = 0, 1, 2, 3
REPO = Path(__file__).resolve().parent.parent
CONFIG_TEMPLATE = REPO / "tests/native/config/hyprland.conf.in"
LOCK = Path.home() / ".cache/hyprvx/locks/heavy.lock"
NESTED_MIN_MEMORY_KIB = 2 * 1024 * 1024  # test-tooling section 2
OUTPUT = "HVX-1"  # headless test output created after start (tests/native/config/hyprland.conf.in)


class Failed(Exception):
    pass


class IdentityError(Exception):
    pass


def agent():
    value = os.environ.get("HYPRVX_AGENT", "")
    if value not in ("claude", "codex"):
        raise Failed("set HYPRVX_AGENT to claude or codex")
    return value


def cache():
    return Path.home() / ".cache/hyprvx" / agent()


def session_root():
    # Harness self-tests use a separate root so they never see a real session; only /tmp/hyprvx-nested-test-* is accepted.
    override = os.environ.get("HYPRVX_NESTED_TEST_ROOT", "")
    if override:
        path = Path(override)
        if path.parent != Path("/tmp") or not path.name.startswith("hyprvx-nested-test-"):
            raise Failed("HYPRVX_NESTED_TEST_ROOT must be /tmp/hyprvx-nested-test-*")
        return path
    return Path(f"/tmp/hyprvx-nested-{os.getuid()}-{agent()}")


def session_file():
    return session_root() / "session.json"


def load_build_module():
    spec = importlib.util.spec_from_file_location("hyprvx_build", REPO / "tools/hyprvx_build.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


# ---- process and socket facts ------------------------------------------------------------------------------------

def process_start(pid):
    return Path(f"/proc/{pid}/stat").read_text().rsplit(") ", 1)[1].split()[19]


def process_state(pid):
    return Path(f"/proc/{pid}/stat").read_text().rsplit(") ", 1)[1].split()[0]


def parent_pid(pid):
    return int(Path(f"/proc/{pid}/stat").read_text().rsplit(") ", 1)[1].split()[1])


def process_exe(pid):
    return os.path.realpath(f"/proc/{pid}/exe")


def alive(pid, start):
    try:
        return process_start(pid) == start and process_state(pid) != "Z"
    except (FileNotFoundError, ProcessLookupError, IndexError):
        return False


def peer_pid(path):
    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        client.settimeout(1)
        client.connect(str(path))
        pid, uid, _ = struct.unpack("3i", client.getsockopt(socket.SOL_SOCKET, socket.SO_PEERCRED, 12))
    finally:
        client.close()
    if uid != os.getuid():
        raise IdentityError(f"socket peer has another uid: {path}")
    return pid


def descends_from(pid, ancestor, depth=8):
    for _ in range(depth):
        if pid == ancestor:
            return True
        if pid <= 1:
            return False
        pid = parent_pid(pid)
    return False


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def private_runtime_ok(runtime):
    try:
        info = os.lstat(runtime)
    except FileNotFoundError:
        return False
    return (runtime.parent == Path("/tmp") and runtime.name.startswith("hvx-") and stat.S_ISDIR(info.st_mode)
            and info.st_uid == os.getuid() and stat.S_IMODE(info.st_mode) == 0o700)


def write_private(path, data):
    temporary = path.with_name(path.name + ".tmp")
    fd = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w") as handle:
        json.dump(data, handle, indent=2, sort_keys=True)
        handle.write("\n")
    os.replace(temporary, path)


def memory_ok():
    available = next(int(line.split()[1]) for line in Path("/proc/meminfo").read_text().splitlines()
                     if line.startswith("MemAvailable:"))
    full = next(line for line in Path("/proc/pressure/memory").read_text().splitlines() if line.startswith("full "))
    avg10 = float(dict(field.split("=") for field in full.split()[1:])["avg10"])
    if available < NESTED_MIN_MEMORY_KIB or avg10 > 10:
        raise Failed(f"BLOCKED: MemAvailable={available} KiB (need {NESTED_MIN_MEMORY_KIB}), PSI full avg10={avg10}")


# ---- session -----------------------------------------------------------------------------------------------------

def read_session():
    path = session_file()
    if path.is_symlink() or not path.is_file():
        raise IdentityError("no nested session marker")
    info = os.lstat(path)
    if info.st_uid != os.getuid() or stat.S_IMODE(info.st_mode) & 0o077:
        raise IdentityError("session marker is shared or not owned by this user")
    return json.loads(path.read_text())


def verify(data):
    """Raises IdentityError unless the recorded session is still the nested compositor we started."""
    pid = data["compositor_pid"]
    if not alive(pid, data["process_start"]):
        raise IdentityError("nested compositor PID is gone or reused")
    if process_exe(pid) != data["binary"]:
        raise IdentityError("nested compositor executable differs from the recorded agent-prefix binary")
    host_signature = os.environ.get("HYPRLAND_INSTANCE_SIGNATURE", "")
    if not data["signature"] or data["signature"] in (data["host_signature"], host_signature):
        raise IdentityError("nested signature equals the host signature")
    runtime = Path(data["runtime_directory"])
    if not private_runtime_ok(runtime):
        raise IdentityError("runtime directory is not a private /tmp/hvx-* directory")
    if peer_pid(runtime / data["wayland_display"]) != pid:
        raise IdentityError("nested Wayland socket peer is not the compositor")
    if peer_pid(runtime / "hypr" / data["signature"] / ".socket2.sock") != pid:
        raise IdentityError("nested socket2 peer is not the compositor")
    return data


def nested_env(data):
    env = os.environ.copy()
    for name in ("HYPRLAND_INSTANCE_SIGNATURE", "WAYLAND_DISPLAY", "DISPLAY"):
        env.pop(name, None)
    env.update({"HYPRLAND_INSTANCE_SIGNATURE": data["signature"], "WAYLAND_DISPLAY": data["wayland_display"],
                "XDG_RUNTIME_DIR": data["runtime_directory"]})
    return env


def resolve_binary(variant, explicit):
    prefix_root = (cache() / "prefix").resolve()
    if explicit:
        binary = Path(explicit).resolve()
        if prefix_root not in binary.parents:
            raise Failed(f"--binary must be inside {prefix_root}")
        info_path = binary.parent.parent / "build-info.json"
    else:
        identifier, _, _ = load_build_module().variant_identity(REPO, variant)
        binary = (prefix_root / identifier / "bin/Hyprland").resolve()
        info_path = prefix_root / identifier / "build-info.json"
    if not binary.is_file() or not os.access(binary, os.X_OK):
        raise Failed(f"BLOCKED: no {variant} build at {binary}; run scripts/build-hyprland.sh --variant {variant}")
    try:
        info = json.loads(info_path.read_text())
    except (OSError, ValueError) as error:
        raise Failed(f"BLOCKED: build-info.json unavailable for {binary}: {error}") from error
    if info.get("binary_sha256") != sha256_file(binary) or info.get("variant") != variant:
        raise Failed("BLOCKED: binary does not match its build-info.json (variant or sha256)")
    return binary, info


INTERACTIVE_RULE_OFF = "monitor = WAYLAND-1, disable"
INTERACTIVE_RULE_ON = "monitor = WAYLAND-1, preferred, auto, 1"


def render_config(path, size, scale, interactive=False):
    width, height = size
    text = CONFIG_TEMPLATE.read_text()
    text = text.replace("@WIDTH@", str(width)).replace("@HEIGHT@", str(height)).replace("@SCALE@", scale)
    if interactive:
        # L6 (Feature 003 T011): the nested window on the host is the only output, so a human can use the real trackpad on
        # it. No headless output is created and captures are refused in this mode.
        if text.count(INTERACTIVE_RULE_OFF) != 1:
            raise Failed("config template has no single WAYLAND-1 disable rule")
        text = text.replace(INTERACTIVE_RULE_OFF, INTERACTIVE_RULE_ON)
    path.write_text(text)


def start(args):
    if session_file().exists() or session_file().is_symlink():
        raise Failed("a session marker exists; run 'stop' or 'recover' first")
    memory_ok()
    binary, info = resolve_binary(args.variant, args.binary)
    host_signature = os.environ.get("HYPRLAND_INSTANCE_SIGNATURE", "")
    host_runtime = Path(os.environ.get("XDG_RUNTIME_DIR", ""))
    host_display = os.environ.get("WAYLAND_DISPLAY", "")
    host_socket = host_runtime / host_display
    if not host_signature or not host_display or not host_socket.is_socket():
        raise Failed("BLOCKED: a host Wayland session (WAYLAND_DISPLAY and its signature) is required for nested mode")

    root = session_root()
    root.mkdir(mode=0o700, exist_ok=True)
    if root.is_symlink() or os.lstat(root).st_uid != os.getuid():
        raise Failed("unsafe session root")
    session = Path(tempfile.mkdtemp(prefix="session-", dir=root))
    runtime = Path(tempfile.mkdtemp(prefix="hvx-", dir="/tmp"))  # short path: AF_UNIX sun_path limit
    os.chmod(runtime, 0o700)
    (runtime / host_display).symlink_to(host_socket)  # only the parent display socket is shared
    for name in ("config", "state", "cache"):
        (session / name).mkdir(mode=0o700)
    config = session / "hyprland.conf"
    render_config(config, args.size, args.scale, args.interactive)

    env = os.environ.copy()
    env.pop("HYPRLAND_INSTANCE_SIGNATURE", None)
    env.update({"WAYLAND_DISPLAY": host_display, "XDG_RUNTIME_DIR": str(runtime),
                "XDG_CONFIG_HOME": str(session / "config"), "XDG_STATE_HOME": str(session / "state"),
                "XDG_CACHE_HOME": str(session / "cache"), "HYPRLAND_NO_SD_VARS": "1", "HYPRLAND_NO_SD_NOTIFY": "1"})
    command = ["flock", "-w", str(args.lock_timeout), str(LOCK), "dbus-run-session", "--", str(binary), "--config", str(config)]
    log = (session / "launcher.log").open("w")
    launcher = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    log.close()
    launcher_start = process_start(launcher.pid)
    try:
        deadline = time.monotonic() + args.lock_timeout + 30
        found = None
        while time.monotonic() < deadline and found is None:
            if launcher.poll() is not None:
                raise Failed(f"nested launcher exited ({launcher.returncode}); see {session / 'launcher.log'}")
            sockets = list((runtime / "hypr").glob("*/.socket2.sock")) if (runtime / "hypr").is_dir() else []
            displays = [p for p in runtime.iterdir()
                        if p.name.startswith("wayland-") and not p.name.endswith(".lock")
                        and not p.is_symlink() and stat.S_ISSOCK(os.lstat(p).st_mode)]
            if len(sockets) == 1 and len(displays) == 1:
                try:
                    pid = peer_pid(displays[0])
                    if (peer_pid(sockets[0]) == pid and sockets[0].parent.name != host_signature
                            and process_exe(pid) == str(binary) and descends_from(pid, launcher.pid)):
                        found = (pid, sockets[0].parent.name, displays[0].name)
                except (OSError, IdentityError):
                    pass
            if found is None:
                time.sleep(0.1)  # startup readiness only; never used for frame readiness
        if found is None:
            raise Failed(f"nested sockets were not safely identified; see {session}")
        pid, signature, display = found
        data = {
            "variant": args.variant, "patchset_id": info.get("patchset_id"), "binary": str(binary),
            "binary_sha256": info["binary_sha256"], "signature": signature, "host_signature": host_signature,
            "wayland_display": display, "runtime_directory": str(runtime), "compositor_pid": pid,
            "process_start": process_start(pid), "launcher_pid": launcher.pid, "launcher_start": launcher_start,
            "session_directory": str(session), "size": list(args.size), "scale": args.scale, "children": [],
            "interactive": bool(args.interactive),
        }
        write_private(session_file(), data)
        verify(data)
        hyprctl = Path(data["binary"]).with_name("hyprctl")
        # Efb5099 always posts a timed ".conf format" notice for legacy configs; it would make captures time-dependent.
        # The config's exec-once marker appears in the callback that posts it; a hyprctl request sent after the marker
        # is handled after that callback. Dismissing damages the notice region on the next frame (ready rule).
        marker = runtime / "hyprvx-exec-once"
        marker_deadline = time.monotonic() + 30
        while not marker.exists() and time.monotonic() < marker_deadline:
            time.sleep(0.05)
        if not marker.exists():
            raise Failed("nested compositor never ran exec-once (startup marker missing)")
        reply = subprocess.run([str(hyprctl), "--instance", signature, "dismissnotify"], env=nested_env(data),
                               capture_output=True, text=True).stdout.strip()
        if reply != "ok":
            raise Failed(f"could not dismiss startup notifications: {reply!r}")
        if args.interactive:
            print(f"nested {args.variant} ready (interactive, no headless output, captures refused): {signature} on {display}; "
                  f"pid={pid}; session={session}")
            return EXIT_OK
        # Fixed-size headless output (config rule HVX-1); the host-sized Wayland output is disabled by the config.
        # It is created after exec-once, as in Sprint 1: created earlier, the cursor starts centred on HVX-1 and is
        # drawn into every capture (GATE-A frames then differ from the Sprint 1 baseline by the cursor sprite).
        reply = subprocess.run([str(hyprctl), "--instance", signature, "output", "create", "headless", OUTPUT],
                               env=nested_env(data), capture_output=True, text=True).stdout.strip()
        if reply != "ok":
            raise Failed(f"could not create the {OUTPUT} headless output: {reply!r}")
        output_deadline = time.monotonic() + 10
        while time.monotonic() < output_deadline:
            monitors = subprocess.run([str(hyprctl), "--instance", signature, "-j", "monitors"], env=nested_env(data),
                                      capture_output=True, text=True).stdout
            try:
                found_output = [m for m in json.loads(monitors) if m.get("name") == OUTPUT]
            except ValueError:
                found_output = []
            if found_output:
                (session / "monitors.json").write_text(monitors)
                break
            time.sleep(0.05)
        else:
            raise Failed(f"{OUTPUT} did not appear in hyprctl monitors")
        print(f"nested {args.variant} ready: {signature} on {display}; pid={pid}; session={session}")
    except BaseException:
        if alive(launcher.pid, launcher_start):
            os.killpg(launcher.pid, signal.SIGTERM)
            try:
                launcher.wait(timeout=10)
            except subprocess.TimeoutExpired:
                os.killpg(launcher.pid, signal.SIGKILL)
                launcher.wait()
        session_file().unlink(missing_ok=True)
        for inner in (runtime / "hypr").glob("*/hyprland.log*") if (runtime / "hypr").is_dir() else []:
            shutil.copy2(inner, session / inner.name)  # keep the nested compositor's own log for diagnosis
        shutil.rmtree(runtime, ignore_errors=True)
        print(f"nested start failed; logs kept in {session}", file=sys.stderr)
        raise


def agent_hyprctl(data):
    """The hyprctl next to the verified compositor binary, only inside the agent prefix root (never the host's)."""
    hyprctl = Path(data["binary"]).with_name("hyprctl")
    if (cache() / "prefix").resolve() not in hyprctl.resolve().parents:
        raise IdentityError("the session's binary is not an agent-prefix build; refusing to use its hyprctl")
    return hyprctl


def nested_xdisplay(data, wait=5.0):
    """The nested XWayland display, resolved only through the verified compositor PID (test-tooling 1.4 §7).

    Hyprland's XWayland server writes its own PID into /tmp/.X<N>-lock (xwayland/Server.cpp tryOpenSockets). A display
    is accepted only when that PID is the verified compositor or a direct child of it, and /tmp/.X11-unix/X<N> is a
    socket. Anything else is refused (exit 3); the host display is never a fallback."""
    pid = data["compositor_pid"]
    deadline = time.monotonic() + wait
    while True:
        for lock in sorted(Path("/tmp").glob(".X*-lock")):
            number = lock.name[2:-5]
            if not number.isdigit():
                continue
            try:
                owner = int(lock.read_text().strip())
                info = os.lstat(lock)
            except (OSError, ValueError):
                continue
            if info.st_uid != os.getuid() or stat.S_ISLNK(info.st_mode):
                continue
            try:
                child_of_compositor = owner != pid and parent_pid(owner) == pid
            except (OSError, IndexError):
                child_of_compositor = False
            sock = Path(f"/tmp/.X11-unix/X{number}")
            if (owner == pid or child_of_compositor) and sock.is_socket():
                return f":{number}"
        if time.monotonic() >= deadline:
            raise IdentityError("no XWayland display whose lock file belongs to the verified compositor")
        time.sleep(0.1)  # startup readiness only (XWayland sockets are opened after the compositor starts)


def run(args):
    data = verify(read_session())
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command:
        raise Failed("run needs a command after --")
    env = nested_env(data)
    if args.x11:
        env["DISPLAY"] = nested_xdisplay(data)
        verify(data)
    if args.background:
        log = (Path(data["session_directory"]) / "children.log").open("a")
        child = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        log.close()
        data["children"].append({"pid": child.pid, "start": process_start(child.pid), "argv": command})
        write_private(session_file(), data)
        print(f"nested child pid={child.pid}")
        return EXIT_OK
    return subprocess.run(command, env=env).returncode


def xdisplay(_):
    data = verify(read_session())
    print(nested_xdisplay(data))
    return EXIT_OK


def plugin(args):
    """Loads or unloads a plugin built into the agent cache (scripts/build-plugin.sh) in the verified nested session."""
    data = verify(read_session())
    hyprctl = agent_hyprctl(data)
    path = Path(args.path).resolve()
    plugin_root = (cache() / "plugin").resolve()
    if plugin_root not in path.parents or not path.is_file() or path.suffix != ".so":
        raise Failed(f"plugin must be a .so file under {plugin_root}")
    print(f"sha256 {sha256_file(path)} {path}")
    verify(data)
    result = subprocess.run([str(hyprctl), "--instance", data["signature"], "plugin", args.action, str(path)],
                            env=nested_env(data), capture_output=True, text=True)
    reply = result.stdout.strip()
    print(reply)
    return EXIT_OK if result.returncode == 0 and reply == "ok" else EXIT_FAILED


def dev(args):
    data = verify(read_session())
    hyprctl = agent_hyprctl(data)
    result = subprocess.run([str(hyprctl), "--instance", data["signature"], "hyprvx-dev", *args.words],
                            env=nested_env(data), capture_output=True, text=True)
    reply = result.stdout.strip()
    print(reply)
    if result.returncode != 0 or not (reply == "ok" or reply.startswith("[")):
        if result.stderr:
            print(result.stderr.strip(), file=sys.stderr)
        return EXIT_FAILED
    return EXIT_OK


def dispatch(args):
    """V2-as-is adapter (T033): V2 has no hyprvx-dev control, only its own hyprvx:* dispatchers."""
    data = verify(read_session())
    if data["variant"] != "v2" or not args.words[0].startswith("hyprvx:"):
        raise Failed("dispatch is limited to hyprvx:* dispatchers on a v2 session")
    hyprctl = Path(data["binary"]).with_name("hyprctl")
    result = subprocess.run([str(hyprctl), "--instance", data["signature"], "dispatch", " ".join(args.words)],
                            env=nested_env(data), capture_output=True, text=True)
    reply = result.stdout.strip()
    print(reply)
    return EXIT_OK if result.returncode == 0 and reply == "ok" else EXIT_FAILED


def capture(args):
    data = verify(read_session())
    if data.get("interactive"):
        raise Failed("captures are refused in an interactive (L6) session")
    path = Path(args.path)
    kind = "ppm" if path.suffix == ".ppm" else "png"
    return subprocess.run(["grim", "-t", kind, "-o", OUTPUT, str(path)], env=nested_env(data)).returncode


def status(_):
    try:
        data = read_session()
    except IdentityError as error:
        print(f"no verified session: {error}")
        return EXIT_IDENTITY
    try:
        verify(data)
    except (IdentityError, OSError) as error:
        print(f"session {data.get('signature')} FAILED verification: {error}")
        return EXIT_IDENTITY
    print(f"nested {data['variant']} verified: {data['signature']} on {data['wayland_display']}; "
          f"pid={data['compositor_pid']}; session={data['session_directory']}")
    return EXIT_OK


def keep_logs(data):
    session = Path(data["session_directory"])
    inner = Path(data["runtime_directory"]) / "hypr" / data["signature"] / "hyprland.log"
    if inner.is_file() and not inner.is_symlink():
        shutil.copy2(inner, session / "hyprland.log")


def remove_runtime(data):
    runtime = Path(data["runtime_directory"])
    if runtime.parent == Path("/tmp") and runtime.name.startswith("hvx-") and not runtime.is_symlink():
        shutil.rmtree(runtime, ignore_errors=True)


def retire_marker(data):
    session = Path(data["session_directory"])
    if session.parent == session_root() and not session.is_symlink():
        shutil.copy2(session_file(), session / "session.json")
    session_file().unlink()


def stop(_):
    data = verify(read_session())
    for child in data.get("children", []):
        if alive(child["pid"], child["start"]):
            os.kill(child["pid"], signal.SIGTERM)
    os.kill(data["compositor_pid"], signal.SIGTERM)
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline and alive(data["compositor_pid"], data["process_start"]):
        time.sleep(0.1)
    # The compositor writes its log file in blocks; copied while it runs, the last lines may still be buffered. It
    # flushes on exit, so the log is kept after it has exited (or, if it did not exit, as far as it was written).
    keep_logs(data)
    if alive(data["compositor_pid"], data["process_start"]):
        raise Failed("nested compositor did not exit; session kept for 'stop' or 'recover'")
    retire_marker(data)
    remove_runtime(data)
    print(f"nested session stopped; logs kept in {data['session_directory']}")
    return EXIT_OK


def recover(_):
    path = session_file()
    if not path.exists() and not path.is_symlink():
        print("no session marker; nothing to recover")
        return EXIT_OK
    data = read_session()
    if alive(data["compositor_pid"], data["process_start"]):
        try:
            verify(data)
        except (IdentityError, OSError) as error:
            print(f"a live process holds the recorded PID but fails verification ({error}); nothing signalled")
            return EXIT_IDENTITY
        print("the nested compositor is alive and verified; use 'stop'")
        return EXIT_FAILED
    retire_marker(data)
    remove_runtime(data)
    print(f"stale session cleaned without signalling any process; logs kept in {data['session_directory']}")
    return EXIT_OK


def parse_size(text):
    try:
        width, height = (int(v) for v in text.lower().split("x"))
    except ValueError as error:
        raise argparse.ArgumentTypeError("size must be WxH") from error
    return width, height


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command_name", required=True)
    p = commands.add_parser("start")
    p.add_argument("--variant", choices=["stock", "v2", "patched"], required=True)
    p.add_argument("--size", type=parse_size, default=(1280, 800))
    p.add_argument("--scale", default="1")
    p.add_argument("--binary")
    p.add_argument("--lock-timeout", type=int, default=3600)
    p.add_argument("--interactive", action="store_true", help="L6 only: keep the visible nested window, no headless output")
    p.set_defaults(handler=start)
    p = commands.add_parser("run")
    p.add_argument("--background", action="store_true")
    p.add_argument("--x11", action="store_true")
    p.add_argument("command", nargs=argparse.REMAINDER)
    p.set_defaults(handler=run)
    p = commands.add_parser("dev")
    p.add_argument("words", nargs="+")
    p.set_defaults(handler=dev)
    p = commands.add_parser("dispatch")
    p.add_argument("words", nargs="+")
    p.set_defaults(handler=dispatch)
    p = commands.add_parser("plugin")
    p.add_argument("action", choices=["load", "unload"])
    p.add_argument("path")
    p.set_defaults(handler=plugin)
    commands.add_parser("xdisplay").set_defaults(handler=xdisplay)
    p = commands.add_parser("capture")
    p.add_argument("path")
    p.set_defaults(handler=capture)
    for name, handler in (("status", status), ("stop", stop), ("recover", recover)):
        commands.add_parser(name).set_defaults(handler=handler)
    try:
        args = parser.parse_args(argv)
    except SystemExit as error:
        return EXIT_USAGE if error.code else EXIT_OK
    try:
        result = args.handler(args)
        return EXIT_OK if result is None else result
    except IdentityError as error:
        print(f"hyprvx-nested: identity verification failed: {error}; nothing sent", file=sys.stderr)
        return EXIT_IDENTITY
    except (Failed, OSError, subprocess.SubprocessError) as error:
        print(f"hyprvx-nested: {error}", file=sys.stderr)
        return EXIT_FAILED


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

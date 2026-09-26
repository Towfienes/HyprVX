#!/usr/bin/env python3

import argparse
import os
import pathlib
import select
import socket
import sys
import time


_ENV = object()


def socket_candidates(signature=_ENV, runtime=_ENV):
    if signature is _ENV:
        signature = os.environ.get("HYPRLAND_INSTANCE_SIGNATURE")
    if runtime is _ENV:
        runtime = os.environ.get("XDG_RUNTIME_DIR")

    if not signature:
        raise RuntimeError(
            "HYPRLAND_INSTANCE_SIGNATURE is not set"
        )

    candidates = []

    if runtime:
        candidates.append(
            pathlib.Path(runtime)
            / "hypr"
            / signature
            / ".socket2.sock"
        )

    fallback = pathlib.Path("/tmp/hypr") / signature / ".socket2.sock"
    if fallback not in candidates:
        candidates.append(fallback)

    return candidates


def find_socket(candidates=None):
    candidates = socket_candidates() if candidates is None else candidates

    for candidate in candidates:
        if candidate.exists():
            return candidate

    message = [
        "Hyprland event socket not found.",
        "Checked:",
    ]

    message.extend(
        f"  {candidate}"
        for candidate in candidates
    )

    raise RuntimeError(
        "\n".join(message)
    )


class EventLineBuffer:
    """Turn arbitrary socket chunks into complete UTF-8 event lines."""

    def __init__(self):
        self._buffer = b""

    def feed(self, chunk):
        self._buffer += chunk
        lines = []

        while b"\n" in self._buffer:
            raw, self._buffer = self._buffer.split(b"\n", 1)
            if raw.endswith(b"\r"):
                raw = raw[:-1]
            lines.append(raw.decode("utf-8", errors="replace"))

        return lines

    def finish(self):
        if not self._buffer:
            return []

        raw = self._buffer
        self._buffer = b""
        if raw.endswith(b"\r"):
            raw = raw[:-1]
        return [raw.decode("utf-8", errors="replace")]


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--seconds",
        type=float,
        default=30.0,
    )

    parser.add_argument(
        "--output",
        type=pathlib.Path,
    )

    args = parser.parse_args()

    socket_path = find_socket()

    print(
        f"HyprVX probe socket: {socket_path}"
    )

    print(
        f"Capture duration: {args.seconds:.1f}s"
    )

    print(
        "Perform Hyprland actions now..."
    )

    print()

    client = socket.socket(
        socket.AF_UNIX,
        socket.SOCK_STREAM,
    )

    client.connect(
        str(socket_path)
    )

    client.setblocking(False)

    deadline = (
        time.monotonic()
        + args.seconds
    )

    line_buffer = EventLineBuffer()

    output_file = None

    if args.output:
        args.output.parent.mkdir(
            parents=True,
            exist_ok=True,
        )

        output_file = args.output.open(
            "w",
            encoding="utf-8",
        )

    try:
        while time.monotonic() < deadline:
            remaining = max(
                0.0,
                deadline
                - time.monotonic(),
            )

            readable, _, _ = select.select(
                [client],
                [],
                [],
                min(0.5, remaining),
            )

            if not readable:
                continue

            chunk = client.recv(65536)

            if not chunk:
                break

            for line in line_buffer.feed(chunk):
                stamp = time.strftime(
                    "%H:%M:%S"
                )

                rendered = (
                    f"[{stamp}] {line}"
                )

                print(
                    rendered,
                    flush=True,
                )

                if output_file:
                    output_file.write(
                        rendered + "\n"
                    )

                    output_file.flush()

        for line in line_buffer.finish():
            stamp = time.strftime("%H:%M:%S")
            rendered = f"[{stamp}] {line}"
            print(rendered, flush=True)
            if output_file:
                output_file.write(rendered + "\n")
                output_file.flush()

    finally:
        client.close()

        if output_file:
            output_file.close()

    print()
    print("Capture complete.")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print(
            "\nCapture interrupted."
        )
    except Exception as exc:
        print(
            f"ERROR: {exc}",
            file=sys.stderr,
        )

        sys.exit(1)

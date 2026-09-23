#!/usr/bin/env python3

import argparse
import os
import pathlib
import select
import socket
import sys
import time


def socket_candidates():
    signature = os.environ.get(
        "HYPRLAND_INSTANCE_SIGNATURE"
    )

    runtime = os.environ.get(
        "XDG_RUNTIME_DIR"
    )

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

    candidates.append(
        pathlib.Path("/tmp/hypr")
        / signature
        / ".socket2.sock"
    )

    return candidates


def find_socket():
    candidates = socket_candidates()

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

    buffer = b""

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

            buffer += chunk

            while b"\n" in buffer:
                raw, buffer = buffer.split(
                    b"\n",
                    1,
                )

                line = raw.decode(
                    "utf-8",
                    errors="replace",
                )

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

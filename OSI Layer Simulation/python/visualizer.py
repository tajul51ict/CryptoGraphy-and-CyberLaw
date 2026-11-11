#!/usr/bin/env python3
"""Run the C socket demo and visualize its OSI layer progress."""

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = PROJECT_ROOT / "OSI-communication"
LAYER_NAMES = {
    1: "Physical",
    2: "Data Link",
    3: "Network",
    4: "Transport",
    5: "Session",
    6: "Presentation",
    7: "Application",
}
EVENT_PATTERN = re.compile(r"^OSI_EVENT\|(TX|RX)\|([1-7])\|([^|]+)\|(\d+)$")


def build_programs():
    make = shutil.which("make") or shutil.which("mingw32-make")
    if make is None:
        raise RuntimeError(
            "GNU Make was not found. Install make (or mingw32-make on Windows)."
        )
    subprocess.run([make, "-C", str(SOURCE_DIR)], check=True)


def executable(name):
    suffix = ".exe" if os.name == "nt" else ""
    return SOURCE_DIR / (name + suffix)


def read_events(output, direction):
    events = []
    for line in output.splitlines():
        match = EVENT_PATTERN.match(line.strip())
        if match and match.group(1) == direction:
            events.append((int(match.group(2)), match.group(3)))
    return events


def display_progress(label, events, expected_layers):
    observed_layers = [layer for layer, _ in events]
    if observed_layers != expected_layers:
        raise RuntimeError(
            f"{label} produced an incomplete or out-of-order OSI trace: "
            f"{observed_layers}"
        )

    print(f"\n{label}")
    for start in range(0, len(events), 2):
        group = events[start : start + 2]
        end = min(start + len(group), len(events))
        route = " -> ".join(
            f"Layer {layer} {name}" for layer, name in group
        )
        print(f"  Progress {end}/7: {route}")


def run_simulation(args):
    build_programs()
    server = subprocess.Popen(
        [
            str(executable("osi_server")),
            args.host,
            str(args.port),
        ],
        cwd=SOURCE_DIR,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        bufsize=1,
    )

    try:
        ready_line = server.stdout.readline().strip()
        ready_match = re.fullmatch(r"READY\|([^|]+)\|(\d+)", ready_line)
        if ready_match is None:
            server_output, _ = server.communicate(timeout=5)
            raise RuntimeError(
                f"The C server did not start successfully:\n"
                f"{ready_line}\n{server_output}".strip()
            )
        server_host, server_port = ready_match.groups()
        print(f"TCP server ready at {server_host}:{server_port}")

        client = subprocess.run(
            [
                str(executable("osi_client")),
                args.host,
                server_port,
                args.message,
            ],
            cwd=SOURCE_DIR,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=10,
        )
        server_output, _ = server.communicate(timeout=10)

        if client.returncode != 0:
            raise RuntimeError(
                f"The C client failed:\n{client.stdout}{client.stderr}"
            )
        if server.returncode != 0:
            raise RuntimeError(f"The C server failed:\n{server_output}")

        client_tx = read_events(client.stdout, "TX")
        client_rx = read_events(client.stdout, "RX")
        server_tx = read_events(server_output, "TX")
        server_rx = read_events(server_output, "RX")
        descending = list(range(7, 0, -1))
        ascending = list(range(1, 8))

        print("\nZIG-ZAG DATA FLOW (progress after every two OSI layers)")
        display_progress(
            "Client -> server: encapsulation",
            client_tx,
            descending,
        )
        display_progress(
            "Client -> server: decapsulation",
            server_rx,
            ascending,
        )
        display_progress(
            "Server -> client: encapsulation",
            server_tx,
            descending,
        )
        display_progress(
            "Server -> client: decapsulation",
            client_rx,
            ascending,
        )
        if "ACK|Message delivered" not in client.stdout.splitlines():
            raise RuntimeError("The client did not confirm message delivery.")
        print("\nMessage delivered and acknowledged over the TCP socket.")
    finally:
        if server.poll() is None:
            server.terminate()
            try:
                server.wait(timeout=3)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait()


def main():
    parser = argparse.ArgumentParser(
        description="Visualize a seven-layer OSI message over a TCP socket."
    )
    parser.add_argument(
        "--host",
        default="127.0.0.1",
        help="server bind/connect address (default: 127.0.0.1)",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=8080,
        help="TCP port, or 0 to select an available port (default: 8080)",
    )
    parser.add_argument(
        "message",
        nargs="?",
        default="Hello from the OSI model",
        help="text to send",
    )
    args = parser.parse_args()
    if not 0 <= args.port <= 65535:
        parser.error("--port must be between 0 and 65535")
    if len(args.message.encode("utf-8")) > 4096:
        parser.error("message must be no more than 4096 UTF-8 bytes")

    try:
        run_simulation(args)
    except (OSError, subprocess.SubprocessError, RuntimeError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

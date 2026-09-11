#!/usr/bin/env python3
"""Translate ATVVoice D-Bus mic state changes into a held F9 key."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import signal
import subprocess
import sys
import threading
import time
from typing import Iterable


MIC_STATE_RE = re.compile(r"\.MicStateChanged\s*\(\s*'([^']+)'")
BLUETOOTH_ADDRESS_RE = re.compile(r"(?i)\b(?:[0-9a-f]{2}:){5}[0-9a-f]{2}\b")
LOG_MARKERS = (
    "START_SEARCH",
    "AUDIO_START",
    "AUDIO_STOP",
    "Frame timeout",
    "PipeWire source",
    "ERROR",
    "failed",
)
DEFAULT_START_DELAY_MS = 0


class VoiceInputState:
    """Pure state machine for the remote voice input lifecycle.

    ATVVoice reports ``opening`` as soon as the remote's voice button is
    pressed, before the first audio packet arrives. Pressing F9 at that point
    avoids losing the beginning of a short utterance.
    """

    def __init__(self) -> None:
        self.pressed = False

    def transition(self, state: str) -> list[str]:
        if state in {"opening", "streaming"} and not self.pressed:
            self.pressed = True
            return ["start"]
        if state not in {"opening", "streaming"} and self.pressed:
            self.pressed = False
            return ["stop"]
        return []


def parse_mic_state(line: str) -> str | None:
    match = MIC_STATE_RE.search(line)
    return match.group(1) if match else None


def safe_atvvoice_log(line: str) -> str | None:
    """Keep useful lifecycle diagnostics without forwarding identity data."""
    if not any(marker in line for marker in LOG_MARKERS):
        return None
    return BLUETOOTH_ADDRESS_RE.sub("<redacted-address>", line.rstrip())


def input_command(action: str, mode: str, key: str) -> list[str]:
    if mode == "direct":
        return ["vinput", "recording", action]
    keyboard_action = "keydown" if action == "start" else "keyup"
    return ["xdotool", keyboard_action, key]


def send_input(action: str, mode: str, key: str, dry_run: bool) -> None:
    command = input_command(action, mode, key)
    print(f"input_event action={action} mode={mode}", flush=True)
    if dry_run:
        return
    try:
        result = subprocess.run(command, check=False, capture_output=True, text=True)
    except OSError as exc:
        raise RuntimeError(f"unable to execute input command: {exc}") from exc
    if result.returncode != 0:
        detail = result.stderr.strip() or f"exit={result.returncode}"
        raise RuntimeError(f"input command failed: {detail}")


def start_delay_seconds(delay_ms: int) -> float:
    return max(0, delay_ms) / 1000.0


def wait_for_dbus(name: str, child: subprocess.Popen[str], timeout: float = 15.0) -> None:
    deadline = time.monotonic() + timeout
    command = [
        "gdbus",
        "call",
        "--session",
        "--dest",
        f"org.atvvoice.{name}",
        "--object-path",
        "/org/atvvoice/Daemon",
        "--method",
        "org.freedesktop.DBus.Properties.Get",
        "org.atvvoice.Daemon",
        "State",
    ]
    while time.monotonic() < deadline:
        if child.poll() is not None:
            raise RuntimeError(f"atvvoice exited before D-Bus was ready (exit={child.returncode})")
        result = subprocess.run(command, check=False, capture_output=True, text=True)
        if result.returncode == 0:
            return
        time.sleep(0.2)
    raise RuntimeError(f"timed out waiting for org.atvvoice.{name}")


def terminate(process: subprocess.Popen[str] | None) -> None:
    if process is None or process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=3)


def forward_logs(stream: Iterable[str]) -> None:
    for line in stream:
        safe_line = safe_atvvoice_log(line)
        if safe_line is not None:
            print(f"atvvoice {safe_line}", flush=True)


def run(args: argparse.Namespace) -> int:
    binary = args.atvvoice_bin or os.environ.get("ATVVOICE_BIN") or "atvvoice"
    if "/" not in binary and shutil.which(binary) is None:
        raise RuntimeError(
            "atvvoice is not installed; set --atvvoice-bin or ATVVOICE_BIN to its executable"
        )
    if "/" in binary and not os.access(binary, os.X_OK):
        raise RuntimeError(f"atvvoice executable is not available: {binary}")
    if args.input_mode == "direct" and shutil.which("vinput") is None:
        raise RuntimeError("vinput is not installed; use --input-mode keyboard or install vinput")

    command = [binary, "-v", "--gain", str(args.gain), "--name", args.name]
    if args.device:
        command.extend(["-d", args.device])
    print(f"starting_atvvoice name={args.name}", flush=True)
    child = subprocess.Popen(
        command,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )
    monitor: subprocess.Popen[str] | None = None
    state = VoiceInputState()
    input_started = False
    start_timer: threading.Timer | None = None

    def cancel_start() -> None:
        nonlocal start_timer
        if start_timer is not None:
            start_timer.cancel()
            start_timer = None

    def start_input() -> None:
        nonlocal input_started, start_timer
        start_timer = None
        if input_started:
            return
        send_input("start", args.input_mode, args.key, args.dry_run)
        input_started = True

    def stop_input() -> None:
        nonlocal input_started
        cancel_start()
        if not input_started:
            return
        send_input("stop", args.input_mode, args.key, args.dry_run)
        input_started = False

    def schedule_start() -> None:
        nonlocal start_timer
        cancel_start()
        delay = start_delay_seconds(args.start_delay_ms)
        print(f"input_schedule action=start delay_ms={args.start_delay_ms}", flush=True)
        start_timer = threading.Timer(delay, start_input)
        start_timer.daemon = True
        start_timer.start()

    try:
        if child.stdout is not None:
            threading.Thread(target=forward_logs, args=(child.stdout,), daemon=True).start()
        if child.stderr is not None:
            threading.Thread(target=forward_logs, args=(child.stderr,), daemon=True).start()
        wait_for_dbus(args.name, child)
        monitor = subprocess.Popen(
            [
                "gdbus",
                "monitor",
                "--session",
                "--dest",
                f"org.atvvoice.{args.name}",
                "--object-path",
                "/org/atvvoice/Daemon",
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        print("bridge_ready waiting_for=MicStateChanged", flush=True)
        assert monitor.stdout is not None
        for line in monitor.stdout:
            mic_state = parse_mic_state(line)
            if mic_state is None:
                continue
            print(f"mic_state={mic_state}", flush=True)
            for action in state.transition(mic_state):
                if action == "start":
                    schedule_start()
                else:
                    stop_input()
    finally:
        stop_input()
        terminate(monitor)
        terminate(child)
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--atvvoice-bin", help="path to the ATVVoice executable")
    parser.add_argument("-d", "--device", help="ATVV device address; omit to auto-discover")
    parser.add_argument("--name", default="xiaomi-remote", help="ATVVoice instance name")
    parser.add_argument(
        "--gain",
        type=float,
        default=0.0,
        help="ATVVoice microphone gain in dB (default: 0 for Xiaomi remotes)",
    )
    parser.add_argument(
        "--start-delay-ms",
        type=int,
        default=DEFAULT_START_DELAY_MS,
        help="delay F9/vinput start after mic opening (default: 0)",
    )
    parser.add_argument(
        "--input-mode",
        choices=("direct", "keyboard"),
        default="keyboard",
        help="inject F9 through X11 (default) or directly control vinput",
    )
    parser.add_argument("--key", default="F9", help="key to hold while the mic streams")
    parser.add_argument("--dry-run", action="store_true", help="log key events without sending them")
    args = parser.parse_args()
    try:
        return run(args)
    except KeyboardInterrupt:
        return 130
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    signal.signal(signal.SIGTERM, lambda *_: (_ for _ in ()).throw(KeyboardInterrupt))
    raise SystemExit(main())

#!/usr/bin/env python3
"""Keep the Xiaomi remote F9 recording minimum consistent across VoCoType."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
import time
from datetime import datetime
from pathlib import Path

DEFAULT_SOURCE = Path.home() / ".config" / "xiaomi-remote" / "voice.json"
DEFAULT_SHARED = Path.home() / ".config" / "vocotype" / "config.json"
DEFAULT_FCITX = Path.home() / ".config" / "fcitx5" / "conf" / "vocotype.conf"
FCITX_CONFIG_URI = "fcitx://config/addon/vocotype"


def read_min_recording_ms(path: Path) -> int:
    value = json.loads(path.read_text(encoding="utf-8"))
    minimum = value.get("f9", {}).get("min_recording_ms")
    if not isinstance(minimum, int) or isinstance(minimum, bool) or not 0 <= minimum <= 5000:
        raise ValueError("f9.min_recording_ms must be an integer from 0 to 5000")
    return minimum


def backup_file(path: Path, reason: str) -> str | None:
    if not path.exists():
        return None
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    backup = path.with_name(f"{path.name}.before-{reason}-{stamp}")
    shutil.copy2(path, backup)
    return str(backup)


def atomic_write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.tmp")
    temporary.write_text(text, encoding="utf-8")
    temporary.replace(path)


def update_shared_config(path: Path, minimum: int) -> tuple[bool, str | None]:
    value = json.loads(path.read_text(encoding="utf-8"))
    audio = value.setdefault("audio", {})
    if audio.get("min_recording_ms") == minimum:
        return False, None
    backup = backup_file(path, "xiaomi-voice-sync")
    audio["min_recording_ms"] = minimum
    atomic_write(path, json.dumps(value, ensure_ascii=False, indent=2) + "\n")
    return True, backup


def updated_fcitx_text(text: str, minimum: int) -> str:
    replacement = f"MinRecordingMs={minimum}"
    if re.search(r"(?m)^MinRecordingMs=.*$", text):
        return re.sub(r"(?m)^MinRecordingMs=.*$", replacement, text)
    suffix = "" if not text or text.endswith("\n") else "\n"
    return f"{text}{suffix}{replacement}\n"


def update_fcitx_file(path: Path, minimum: int) -> tuple[bool, str | None]:
    text = path.read_text(encoding="utf-8") if path.exists() else ""
    updated = updated_fcitx_text(text, minimum)
    if updated == text:
        return False, None
    backup = backup_file(path, "xiaomi-voice-sync")
    atomic_write(path, updated)
    return True, backup


class FcitxController:
    def __init__(self):
        from gi.repository import Gio

        self.Gio = Gio
        self.proxy = Gio.DBusProxy.new_for_bus_sync(
            Gio.BusType.SESSION,
            Gio.DBusProxyFlags.NONE,
            None,
            "org.fcitx.Fcitx5",
            "/controller",
            "org.fcitx.Fcitx.Controller1",
            None,
        )

    def get_config(self) -> dict[str, str]:
        from gi.repository import GLib

        result = self.proxy.call_sync(
            "GetConfig",
            GLib.Variant("(s)", (FCITX_CONFIG_URI,)),
            self.Gio.DBusCallFlags.NONE,
            3000,
            None,
        )
        return {key: str(value) for key, value in result.unpack()[0].items()}

    def set_config(self, config: dict[str, str]) -> None:
        from gi.repository import GLib

        raw = {key: GLib.Variant("s", value) for key, value in config.items()}
        self.proxy.call_sync(
            "SetConfig",
            GLib.Variant("(sv)", (FCITX_CONFIG_URI, GLib.Variant("a{sv}", raw))),
            self.Gio.DBusCallFlags.NONE,
            3000,
            None,
        )


def persisted_fcitx_minimum(path: Path) -> int | None:
    if not path.exists():
        return None
    match = re.search(r"(?m)^MinRecordingMs=(\d+)\s*$", path.read_text(encoding="utf-8"))
    return int(match.group(1)) if match else None


def synchronize(source: Path, shared: Path, fcitx: Path) -> str:
    minimum = read_min_recording_ms(source)
    update_shared_config(shared, minimum)

    try:
        controller = FcitxController()
        current = controller.get_config()
    except Exception:
        update_fcitx_file(fcitx, minimum)
        return "persisted_pending_live_apply"

    if current.get("MinRecordingMs") != str(minimum):
        backup_file(fcitx, "xiaomi-voice-sync")
        current["MinRecordingMs"] = str(minimum)
        controller.set_config(current)

    live = controller.get_config().get("MinRecordingMs")
    for _ in range(10):
        if persisted_fcitx_minimum(fcitx) == minimum:
            break
        time.sleep(0.1)
    if live != str(minimum) or persisted_fcitx_minimum(fcitx) != minimum:
        raise RuntimeError("Fcitx live or persisted readback did not match")
    return "live_and_persisted"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--shared-config", type=Path, default=DEFAULT_SHARED)
    parser.add_argument("--fcitx-config", type=Path, default=DEFAULT_FCITX)
    args = parser.parse_args()
    try:
        result = synchronize(args.config, args.shared_config, args.fcitx_config)
        minimum = read_min_recording_ms(args.config)
        print(f"voice_config_sync phase=completed result={result} f9_min_recording_ms={minimum}")
        return 0
    except Exception as error:
        print(
            f"voice_config_sync phase=failed reason={type(error).__name__}",
            file=sys.stderr,
        )
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

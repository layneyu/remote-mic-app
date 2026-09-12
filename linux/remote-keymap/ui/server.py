#!/usr/bin/env python3
"""Local-only configuration panel for the Xiaomi remote bridge."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
from datetime import datetime
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parent
CONFIG_PATH = Path.home() / ".config" / "xiaomi-remote" / "keymap-ui.json"
RUNTIME_CONFIG_PATH = CONFIG_PATH.with_name("keymapd.conf")
ALLOWED_ACTIONS = {
    "none", "native", "super", "chatgpt", "slash", "workspace-prev",
    "workspace-next", "voice", "disable",
    "right-ctrl", "workspace-layer",
}
CUSTOM_KEY_ACTION = re.compile(r"^key:[A-Za-z0-9+_ -]{1,120}$")
BUTTON_IDS = {
    "power", "up", "left", "back", "home", "menu", "voice", "right",
    "ok", "down", "volume_up", "volume_down", "tv",
}

DEFAULT_CONFIG = {
    "enabled": True,
    "device_name": "小米蓝牙语音遥控器",
    "buttons": [
        {"id": "power", "label": "电源键", "icon": "⏻", "note": "单击 /，双击右 Ctrl", "slots": {"single": "slash", "double": "right-ctrl", "long": "none"}},
        {"id": "up", "label": "上键", "icon": "⌃", "slots": {"single": "native", "double": "none", "long": "none"}},
        {"id": "left", "label": "左键", "icon": "‹", "slots": {"single": "native", "double": "none", "long": "none"}},
        {"id": "back", "label": "返回键", "icon": "↶", "slots": {"single": "native", "double": "none", "long": "none"}},
        {"id": "home", "label": "主页键", "icon": "⌂", "note": "启动或切换 ChatGPT", "slots": {"single": "chatgpt", "double": "none", "long": "none"}},
        {"id": "menu", "label": "菜单键", "icon": "≡", "note": "松开后 700ms 内可组合", "slots": {"single": "super", "double": "none", "long": "none"}},
        {"id": "voice", "label": "语音键", "icon": "♩", "note": "按住说话，松开结束", "slots": {"single": "voice", "double": "none", "long": "none"}},
        {"id": "right", "label": "右键", "icon": "›", "slots": {"single": "native", "double": "none", "long": "none"}},
        {"id": "ok", "label": "确定键", "icon": "◎", "slots": {"single": "native", "double": "none", "long": "none"}},
        {"id": "down", "label": "下键", "icon": "⌄", "slots": {"single": "native", "double": "none", "long": "none"}},
        {"id": "volume_up", "label": "音量 +", "icon": "⊕", "note": "暂保持原生", "slots": {"single": "native", "double": "none", "long": "none"}},
        {"id": "volume_down", "label": "音量 −", "icon": "⊖", "note": "暂保持原生", "slots": {"single": "native", "double": "none", "long": "none"}},
        {"id": "tv", "label": "TV 键", "icon": "TV", "note": "TV 后 700ms 内按左右切 workspace", "slots": {"single": "workspace-layer", "double": "none", "long": "none"}},
    ],
}


def validate_config(value: object) -> dict:
    if not isinstance(value, dict):
        raise ValueError("配置必须是对象")
    result = dict(DEFAULT_CONFIG)
    result["enabled"] = bool(value.get("enabled", True))
    result["device_name"] = str(value.get("device_name", DEFAULT_CONFIG["device_name"]))[:100]
    buttons = value.get("buttons")
    if not isinstance(buttons, list) or {item.get("id") for item in buttons if isinstance(item, dict)} != BUTTON_IDS:
        raise ValueError("按键列表不完整")
    normalized = []
    for item in buttons:
        if not isinstance(item, dict) or item.get("id") not in BUTTON_IDS:
            raise ValueError("存在未知按键")
        slots = item.get("slots")
        if not isinstance(slots, dict):
            raise ValueError("按键动作格式错误")
        clean_slots = {}
        for trigger in ("single", "double", "long"):
            action = slots.get(trigger, "none")
            if action not in ALLOWED_ACTIONS and not (isinstance(action, str) and CUSTOM_KEY_ACTION.fullmatch(action)):
                raise ValueError(f"不支持的动作: {action}")
            clean_slots[trigger] = action
        clean = {"id": item["id"], "label": str(item.get("label", item["id"]))[:40], "icon": str(item.get("icon", "•"))[:4], "slots": clean_slots}
        if item.get("note"):
            clean["note"] = str(item["note"])[:100]
        normalized.append(clean)
    result["buttons"] = normalized
    return result


def read_config(path: Path = CONFIG_PATH) -> dict:
    if not path.exists():
        return json.loads(json.dumps(DEFAULT_CONFIG, ensure_ascii=False))
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
        for button in value.get("buttons", []):
            if button.get("id") == "tv" and button.get("slots", {}).get("single") == "none":
                button["slots"]["single"] = "workspace-layer"
        return validate_config(value)
    except (OSError, ValueError, json.JSONDecodeError):
        return json.loads(json.dumps(DEFAULT_CONFIG, ensure_ascii=False))


def write_config(value: object, path: Path = CONFIG_PATH) -> str | None:
    config = validate_config(value)
    path.parent.mkdir(parents=True, exist_ok=True)
    backup = None
    if path.exists():
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        backup_path = path.with_name(f"{path.name}.before-ui-{stamp}")
        shutil.copy2(path, backup_path)
        backup = str(backup_path)
    temporary = path.with_name(f".{path.name}.tmp")
    temporary.write_text(json.dumps(config, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)
    if path == CONFIG_PATH:
        write_runtime_config(config)
    return backup


def write_runtime_config(value: object, path: Path = RUNTIME_CONFIG_PATH) -> str | None:
    config = validate_config(value)
    path.parent.mkdir(parents=True, exist_ok=True)
    backup = None
    if path.exists():
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        backup_path = path.with_name(f"{path.name}.before-ui-{stamp}")
        shutil.copy2(path, backup_path)
        backup = str(backup_path)
    lines = [
        "# Generated by xiaomi-remote-keymap UI. Do not edit while the UI is open.",
        f"enabled={1 if config['enabled'] else 0}",
        f"device_name={config['device_name']}",
    ]
    for button in config["buttons"]:
        for trigger in ("single", "double", "long"):
            lines.append(f"{button['id']}.{trigger}={button['slots'][trigger]}")
    temporary = path.with_name(f".{path.name}.tmp")
    temporary.write_text("\n".join(lines) + "\n", encoding="utf-8")
    temporary.replace(path)
    return backup


def restart_runtime() -> None:
    result = subprocess.run(
        ["systemctl", "--user", "restart", "xiaomi-remote-keymap.service"],
        capture_output=True,
        text=True,
        timeout=5,
    )
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip() or f"exit={result.returncode}"
        raise OSError(f"桥接服务重载失败: {detail}")


def runtime_status() -> dict:
    try:
        service = subprocess.run(["systemctl", "--user", "is-active", "xiaomi-remote-keymap.service"], capture_output=True, text=True, timeout=2).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        service = "unknown"
    return {"service": service, "device_present": service == "active"}


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def do_GET(self):  # noqa: N802
        if urlparse(self.path).path == "/api/config":
            self.send_json({"config": read_config(), "runtime": runtime_status()})
            return
        super().do_GET()

    def do_POST(self):  # noqa: N802
        if urlparse(self.path).path != "/api/config":
            self.send_error(404)
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            value = json.loads(self.rfile.read(length).decode("utf-8"))
            backup = write_config(value)
            restart_runtime()
            self.send_json({"ok": True, "backup": backup, "runtime": runtime_status()})
        except (ValueError, OSError, json.JSONDecodeError) as error:
            self.send_json({"ok": False, "error": str(error)}, status=400)

    def send_json(self, value: object, status: int = 200):
        payload = json.dumps(value, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, format, *args):
        return


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"remote_keymap_ui_ready address=http://{args.host}:{args.port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()

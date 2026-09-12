import json
import tempfile
import unittest
from pathlib import Path

import server


class ConfigTests(unittest.TestCase):
    def test_default_config_is_complete(self):
        value = server.validate_config(server.DEFAULT_CONFIG)
        self.assertEqual({button["id"] for button in value["buttons"]}, server.BUTTON_IDS)
        power = next(button for button in value["buttons"] if button["id"] == "power")
        self.assertEqual(power["slots"]["double"], "right-ctrl")
        tv = next(button for button in value["buttons"] if button["id"] == "tv")
        self.assertEqual(tv["slots"]["single"], "workspace-layer")

    def test_read_config_migrates_legacy_tv_placeholder(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "keymap-ui.json"
            legacy = json.loads(json.dumps(server.DEFAULT_CONFIG, ensure_ascii=False))
            tv = next(button for button in legacy["buttons"] if button["id"] == "tv")
            tv["slots"]["single"] = "none"
            path.write_text(json.dumps(legacy, ensure_ascii=False), encoding="utf-8")

            loaded = server.read_config(path)

            migrated_tv = next(button for button in loaded["buttons"] if button["id"] == "tv")
            self.assertEqual(migrated_tv["slots"]["single"], "workspace-layer")

    def test_write_config_makes_same_directory_backup(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "keymap-ui.json"
            path.write_text(json.dumps(server.DEFAULT_CONFIG, ensure_ascii=False), encoding="utf-8")
            changed = json.loads(json.dumps(server.DEFAULT_CONFIG, ensure_ascii=False))
            changed["enabled"] = False
            backup = server.write_config(changed, path)
            self.assertIsNotNone(backup)
            self.assertTrue(Path(backup).parent == path.parent)
            self.assertFalse(json.loads(path.read_text(encoding="utf-8"))["enabled"])
            self.assertTrue(Path(backup).exists())

    def test_write_runtime_config_exports_power_actions(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "keymapd.conf"
            value = json.loads(json.dumps(server.DEFAULT_CONFIG, ensure_ascii=False))
            value["buttons"][0]["slots"] = {"single": "key:Ctrl", "double": "chatgpt", "long": "none"}
            backup = server.write_runtime_config(value, path)
            self.assertIsNone(backup)
            content = path.read_text(encoding="utf-8")
            self.assertIn("power.single=key:Ctrl", content)
            self.assertIn("power.double=chatgpt", content)
            self.assertIn("tv.single=workspace-layer", content)

    def test_rejects_unknown_action(self):
        value = json.loads(json.dumps(server.DEFAULT_CONFIG, ensure_ascii=False))
        value["buttons"][0]["slots"]["single"] = "shutdown"
        with self.assertRaises(ValueError):
            server.validate_config(value)

    def test_accepts_recorded_keyboard_shortcut(self):
        value = json.loads(json.dumps(server.DEFAULT_CONFIG, ensure_ascii=False))
        value["buttons"][0]["slots"]["double"] = "key:Ctrl+Shift+P"
        self.assertEqual(server.validate_config(value)["buttons"][0]["slots"]["double"], "key:Ctrl+Shift+P")

    def test_accepts_escape_keyboard_shortcut(self):
        value = json.loads(json.dumps(server.DEFAULT_CONFIG, ensure_ascii=False))
        value["buttons"][0]["slots"]["single"] = "key:Esc"
        self.assertEqual(server.validate_config(value)["buttons"][0]["slots"]["single"], "key:Esc")


if __name__ == "__main__":
    unittest.main()

import json
import tempfile
import unittest
from pathlib import Path

import sync_config


class VoiceConfigSyncTests(unittest.TestCase):
    def test_reads_authoritative_f9_minimum(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "voice.json"
            path.write_text('{"f9": {"min_recording_ms": 300}}\n', encoding="utf-8")

            self.assertEqual(sync_config.read_min_recording_ms(path), 300)

    def test_updates_shared_json_and_creates_same_directory_backup(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "config.json"
            path.write_text(
                json.dumps({"audio": {"min_recording_ms": 1000}, "asr": {"use_vad": True}}),
                encoding="utf-8",
            )

            changed, backup = sync_config.update_shared_config(path, 300)

            self.assertTrue(changed)
            self.assertIsNotNone(backup)
            self.assertEqual(Path(backup).parent, path.parent)
            value = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(value["audio"]["min_recording_ms"], 300)
            self.assertTrue(value["asr"]["use_vad"])

    def test_updates_fcitx_text_without_changing_other_settings(self):
        source = "PTTKey=F9\nPTTHoldThresholdMs=0\nMinRecordingMs=1000\nPanelStyle=minimal\n"

        updated = sync_config.updated_fcitx_text(source, 300)

        self.assertIn("PTTKey=F9", updated)
        self.assertIn("PTTHoldThresholdMs=0", updated)
        self.assertIn("MinRecordingMs=300", updated)
        self.assertIn("PanelStyle=minimal", updated)

    def test_rejects_out_of_range_minimum(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "voice.json"
            path.write_text('{"f9": {"min_recording_ms": 9000}}\n', encoding="utf-8")

            with self.assertRaises(ValueError):
                sync_config.read_min_recording_ms(path)


if __name__ == "__main__":
    unittest.main()

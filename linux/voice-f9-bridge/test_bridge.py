import unittest

from bridge import (
    DEFAULT_START_DELAY_MS,
    VoiceInputState,
    input_command,
    safe_atvvoice_log,
    start_delay_seconds,
)


class F9BridgeStateTests(unittest.TestCase):
    def test_opening_presses_f9_before_streaming_and_once_until_stream_ends(self):
        state = VoiceInputState()

        self.assertEqual(state.transition("opening"), ["start"])
        self.assertEqual(state.transition("opening"), [])
        self.assertEqual(state.transition("streaming"), [])
        self.assertEqual(state.transition("streaming"), [])
        self.assertEqual(state.transition("connected"), ["stop"])
        self.assertEqual(state.transition("connected"), [])

    def test_disconnect_releases_a_stuck_key(self):
        state = VoiceInputState()

        self.assertEqual(state.transition("streaming"), ["start"])
        self.assertEqual(state.transition("disconnected"), ["stop"])

    def test_unknown_states_are_safe(self):
        state = VoiceInputState()

        self.assertEqual(state.transition("not-a-state"), [])
        self.assertEqual(state.transition("opening"), ["start"])
        self.assertEqual(state.transition("not-a-state"), ["stop"])
        self.assertFalse(state.pressed)

    def test_atvvoice_logs_are_filtered_and_addresses_redacted(self):
        self.assertIsNone(safe_atvvoice_log("Found ATVV device (C0:5D:39:C3:A0:C0)"))
        self.assertEqual(
            safe_atvvoice_log("AUDIO_START device=C0:5D:39:C3:A0:C0"),
            "AUDIO_START device=<redacted-address>",
        )

    def test_input_command_has_direct_and_keyboard_modes(self):
        self.assertEqual(input_command("start", "direct", "F9"), ["vinput", "recording", "start"])
        self.assertEqual(input_command("stop", "keyboard", "F9"), ["xdotool", "keyup", "F9"])

    def test_start_delay_is_milliseconds(self):
        self.assertEqual(DEFAULT_START_DELAY_MS, 0)
        self.assertEqual(start_delay_seconds(300), 0.3)
        self.assertEqual(start_delay_seconds(0), 0.0)
        self.assertEqual(start_delay_seconds(-1), 0.0)


if __name__ == "__main__":
    unittest.main()

import unittest

from bridge import F9BridgeState, safe_atvvoice_log


class F9BridgeStateTests(unittest.TestCase):
    def test_opening_presses_f9_before_streaming_and_once_until_stream_ends(self):
        state = F9BridgeState()

        self.assertEqual(state.transition("opening"), ["keydown"])
        self.assertEqual(state.transition("streaming"), [])
        self.assertEqual(state.transition("streaming"), [])
        self.assertEqual(state.transition("connected"), ["keyup"])
        self.assertEqual(state.transition("connected"), [])

    def test_disconnect_releases_a_stuck_key(self):
        state = F9BridgeState()

        self.assertEqual(state.transition("streaming"), ["keydown"])
        self.assertEqual(state.transition("disconnected"), ["keyup"])

    def test_unknown_states_are_safe(self):
        state = F9BridgeState()

        self.assertEqual(state.transition("not-a-state"), [])
        self.assertEqual(state.transition("opening"), ["keydown"])
        self.assertEqual(state.transition("not-a-state"), ["keyup"])
        self.assertFalse(state.pressed)

    def test_atvvoice_logs_are_filtered_and_addresses_redacted(self):
        self.assertIsNone(safe_atvvoice_log("Found ATVV device (C0:5D:39:C3:A0:C0)"))
        self.assertEqual(
            safe_atvvoice_log("AUDIO_START device=C0:5D:39:C3:A0:C0"),
            "AUDIO_START device=<redacted-address>",
        )


if __name__ == "__main__":
    unittest.main()

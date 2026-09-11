# ATVV → F9 bridge

This small Linux bridge starts an `atvvoice` daemon, watches its session-bus
`MicStateChanged` signal, and holds F9 for the entire remote voice session.
F9 is pressed at `opening`—before the first audio packet—to avoid losing the
beginning of short utterances. It is released on `connected` or
`disconnected`, including cleanup after an interrupted bridge process.

The bridge intentionally leaves ordinary HID keys alone. The ATVVoice daemon
must be built separately from [ATVVoice](https://github.com/b0o/ATVVoice) and
must expose its default D-Bus interface.

Example:

```sh
python linux/voice-f9-bridge/bridge.py \
  --atvvoice-bin /path/to/atvvoice \
  --device '<ATVV device address>' \
  --name xiaomi-remote
```

For a safe lifecycle check, add `--dry-run`; it logs F9 events without sending
keyboard events. The current VoCoType setup must use F9 as its PTT key and
`vinput` must select the matching `atvvoice-xiaomi-remote` PipeWire source.

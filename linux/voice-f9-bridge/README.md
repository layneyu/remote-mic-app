# ATVV → F9 bridge

This small Linux bridge starts an `atvvoice` daemon, watches its session-bus
`MicStateChanged` signal, and controls the existing vinput voice session.
Recording starts at `opening`—before the first audio packet—to avoid losing
the beginning of short utterances. It stops on `connected` or `disconnected`,
including cleanup after an interrupted bridge process.

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

For a safe lifecycle check, add `--dry-run`; it logs input events without
starting/stopping recording. The default `--input-mode direct` calls
`vinput recording start/stop` and is preferred on this Manjaro setup because
the daemon is the actual voice-input entry point. `--input-mode keyboard`
retains the original X11 F9 keydown/keyup behavior for compatibility.
`vinput` must select the matching `atvvoice-xiaomi-remote` PipeWire source.

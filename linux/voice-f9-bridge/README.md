# ATVV → F9 bridge

This small Linux bridge starts an `atvvoice` daemon, watches its session-bus
`MicStateChanged` signal, and controls the existing F9 voice session.
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

The bridge passes `--gain 0` to ATVVoice by default. This avoids amplifying
the Xiaomi remote's idle noise into false local-ASR text; adjust `--gain` only
after checking a real recording.
F9/vinput activation has no added startup delay by default. Use
`--start-delay-ms` to add one when testing button click/release transients.

For a safe lifecycle check, add `--dry-run`; it logs input events without
starting/stopping recording. The default `--input-mode keyboard` injects
X11 F9 keydown/keyup events, so the remote uses VoCoType's local F9 model.
`--input-mode direct` is an explicit fallback that calls `vinput recording
start/stop`; that path uses the separate vinput provider configuration and
is not the F9 local-model path. When using `direct`, `vinput` must select the
matching `atvvoice-xiaomi-remote` PipeWire source.

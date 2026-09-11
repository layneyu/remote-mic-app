# ATVVoice Xiaomi toggle fix

The Xiaomi remote reports `HoldToTalk`, but its control stream does not provide
a reliable physical hold/release lifecycle. Use toggle behavior: the first
`START_SEARCH` starts a session, `HttButtonRelease` is ignored, and the next
`START_SEARCH` stops it. Also reset the no-frame timer at `AUDIO_START` and do
not run that timer while the session is still `Opening`.

Apply the equivalent changes to ATVVoice's `src/atvv.rs` before building:

```diff
 use crate::protocol::types::{AudioFrame, AudioStopReason, CtlEvent, StreamId};
+use crate::protocol::types::AudioStartReason;

+fn should_ignore_repeated_start_search(state: State) -> bool {
+    state == State::Opening
 }
+}

     let mut current_stream_id: Option<StreamId> = None;
+    let mut hold_to_talk = false;

     CtlEvent::AudioStart { reason, .. } => {
+        hold_to_talk = matches!(reason, AudioStartReason::HoldToTalk);
+        frame_timer.as_mut().reset(Instant::now() + timeouts.frame_timeout);
     }

     CtlEvent::StartSearch => {
-        if state == State::Streaming || state == State::Opening {
+        if should_ignore_repeated_start_search(state) {
+            tracing::debug!("Ignoring repeated START_SEARCH while opening");
+        } else if state == State::Streaming || state == State::Opening {
             // existing toggle/close branch
```

The temporary ATVVoice build used for the current machine includes this
change and passes all 97 unit tests. In toggle mode,
`AUDIO_STOP(HttButtonRelease)` is ignored and the next `START_SEARCH` closes
the session.

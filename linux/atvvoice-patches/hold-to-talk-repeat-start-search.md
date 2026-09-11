# ATVVoice HoldToTalk fix

The Xiaomi remote reports `HoldToTalk`, but may repeat `START_SEARCH` while
the voice button remains pressed. Apply the following change to ATVVoice's
`src/atvv.rs` before building:

```diff
 use crate::protocol::types::{AudioFrame, AudioStopReason, CtlEvent, StreamId};
+use crate::protocol::types::AudioStartReason;

+fn should_ignore_repeated_start_search(state: State, hold_to_talk: bool) -> bool {
+    state == State::Opening || (state == State::Streaming && hold_to_talk)
+}

     let mut current_stream_id: Option<StreamId> = None;
+    let mut hold_to_talk = false;

     CtlEvent::AudioStart { reason, .. } => {
+        hold_to_talk = matches!(reason, AudioStartReason::HoldToTalk);
     }

     CtlEvent::StartSearch => {
-        if state == State::Streaming || state == State::Opening {
+        if should_ignore_repeated_start_search(state, hold_to_talk) {
+            tracing::debug!("Ignoring repeated START_SEARCH during active voice session");
+        } else if state == State::Streaming || state == State::Opening {
             // existing toggle/close branch
```

The temporary ATVVoice build used for the current machine includes this
change and passes all 97 unit tests. The normal stop path remains
`AUDIO_STOP(HttButtonRelease)`.

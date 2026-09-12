#include "policy.h"

#include <linux/input-event-codes.h>
#include <X11/keysym.h>

/* XF86keysym.h undefines _EVDEVK at EOF, so keep these public X11 values local. */
enum {
    REMOTE_XKEYSYM_VOICE_COMMAND = 0x10081246,
    REMOTE_XKEYSYM_ASSISTANT = 0x10081247,
};

void button_tracker_init(ButtonTracker *tracker) {
    *tracker = (ButtonTracker){0};
}

void button_tracker_press(ButtonTracker *tracker, uint64_t now_ms) {
    if (!tracker->pressed) {
        tracker->pressed = true;
        tracker->pressed_at_ms = now_ms;
    }
}

ButtonTrigger button_tracker_release(
    ButtonTracker *tracker,
    uint64_t now_ms,
    uint64_t long_press_ms,
    uint64_t double_click_ms
) {
    if (!tracker->pressed) {
        return BUTTON_TRIGGER_NONE;
    }
    tracker->pressed = false;
    uint64_t held_ms = now_ms - tracker->pressed_at_ms;
    if (held_ms >= long_press_ms) {
        tracker->single_pending = false;
        tracker->last_release_ms = 0;
        return BUTTON_TRIGGER_LONG;
    }
    if (tracker->single_pending && now_ms - tracker->last_release_ms <= double_click_ms) {
        tracker->single_pending = false;
        tracker->last_release_ms = 0;
        return BUTTON_TRIGGER_DOUBLE;
    }
    tracker->single_pending = true;
    tracker->last_release_ms = now_ms;
    return BUTTON_TRIGGER_NONE;
}

ButtonTrigger button_tracker_flush(
    ButtonTracker *tracker,
    uint64_t now_ms,
    uint64_t double_click_ms
) {
    if (!tracker->single_pending || now_ms - tracker->last_release_ms <= double_click_ms) {
        return BUTTON_TRIGGER_NONE;
    }
    tracker->single_pending = false;
    tracker->last_release_ms = 0;
    return BUTTON_TRIGGER_SINGLE;
}

bool chord_window_active(uint64_t now_ms, uint64_t deadline_ms) {
    return now_ms < deadline_ms;
}

bool remote_keysym_is_voice(unsigned long keysym) {
    return (keysym >= XK_F1 && keysym <= XK_F12) ||
        keysym == REMOTE_XKEYSYM_VOICE_COMMAND ||
        keysym == REMOTE_XKEYSYM_ASSISTANT;
}

bool remote_evdev_key_is_voice(unsigned int keycode) {
    return (keycode >= KEY_F1 && keycode <= KEY_F10) ||
        keycode == KEY_F11 ||
        keycode == KEY_F12 ||
        (keycode >= KEY_F13 && keycode <= KEY_F24) ||
        keycode == KEY_VOICECOMMAND ||
        keycode == KEY_ASSISTANT;
}

PowerAction power_action_for_trigger(ButtonTrigger trigger) {
    switch (trigger) {
        case BUTTON_TRIGGER_SINGLE:
            return POWER_ACTION_SLASH;
        case BUTTON_TRIGGER_DOUBLE:
            return POWER_ACTION_RIGHT_CTRL;
        default:
            return POWER_ACTION_NONE;
    }
}

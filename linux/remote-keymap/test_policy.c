#include "policy.h"

#include <assert.h>
#include <linux/input-event-codes.h>
#include <X11/keysym.h>

int main(void) {
    ButtonTracker tracker;
    button_tracker_init(&tracker);

    button_tracker_press(&tracker, 1000);
    assert(button_tracker_release(&tracker, 1100, 500, 280) == BUTTON_TRIGGER_NONE);
    assert(button_tracker_flush(&tracker, 1279, 280) == BUTTON_TRIGGER_NONE);
    assert(button_tracker_flush(&tracker, 1381, 280) == BUTTON_TRIGGER_SINGLE);

    button_tracker_press(&tracker, 2000);
    assert(button_tracker_release(&tracker, 2050, 500, 280) == BUTTON_TRIGGER_NONE);
    button_tracker_press(&tracker, 2150);
    assert(button_tracker_release(&tracker, 2200, 500, 280) == BUTTON_TRIGGER_DOUBLE);

    button_tracker_press(&tracker, 3000);
    assert(button_tracker_release(&tracker, 3600, 500, 280) == BUTTON_TRIGGER_LONG);
    assert(button_tracker_flush(&tracker, 4000, 280) == BUTTON_TRIGGER_NONE);

    assert(chord_window_active(5000, 5400));
    assert(!chord_window_active(5400, 5400));
    assert(power_action_for_trigger(BUTTON_TRIGGER_SINGLE) == POWER_ACTION_SLASH);
    assert(power_action_for_trigger(BUTTON_TRIGGER_DOUBLE) == POWER_ACTION_RIGHT_CTRL);
    assert(power_action_for_trigger(BUTTON_TRIGGER_LONG) == POWER_ACTION_NONE);
    assert(remote_keysym_is_voice(XK_F3));
    assert(remote_keysym_is_voice(XK_F5));
    assert(remote_keysym_is_voice(0x10081246UL));
    assert(remote_keysym_is_voice(0x10081247UL));
    assert(!remote_keysym_is_voice(XK_Control_L));
    assert(remote_evdev_key_is_voice(KEY_F3));
    assert(remote_evdev_key_is_voice(KEY_F5));
    assert(remote_evdev_key_is_voice(KEY_VOICECOMMAND));
    assert(remote_evdev_key_is_voice(KEY_ASSISTANT));
    assert(!remote_evdev_key_is_voice(KEY_POWER));
    return 0;
}

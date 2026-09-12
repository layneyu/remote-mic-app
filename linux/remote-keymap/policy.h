#ifndef XIAOMI_REMOTE_KEYMAP_POLICY_H
#define XIAOMI_REMOTE_KEYMAP_POLICY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BUTTON_TRIGGER_NONE = 0,
    BUTTON_TRIGGER_SINGLE,
    BUTTON_TRIGGER_DOUBLE,
    BUTTON_TRIGGER_LONG,
} ButtonTrigger;

typedef enum {
    POWER_ACTION_NONE = 0,
    POWER_ACTION_SLASH,
    POWER_ACTION_RIGHT_CTRL,
} PowerAction;

typedef struct {
    bool pressed;
    uint64_t pressed_at_ms;
    uint64_t last_release_ms;
    bool single_pending;
} ButtonTracker;

void button_tracker_init(ButtonTracker *tracker);
void button_tracker_press(ButtonTracker *tracker, uint64_t now_ms);
ButtonTrigger button_tracker_release(
    ButtonTracker *tracker,
    uint64_t now_ms,
    uint64_t long_press_ms,
    uint64_t double_click_ms
);
ButtonTrigger button_tracker_flush(
    ButtonTracker *tracker,
    uint64_t now_ms,
    uint64_t double_click_ms
);

bool chord_window_active(uint64_t now_ms, uint64_t deadline_ms);

bool remote_keysym_is_voice(unsigned long keysym);

bool remote_evdev_key_is_voice(unsigned int keycode);

PowerAction power_action_for_trigger(ButtonTrigger trigger);

#endif

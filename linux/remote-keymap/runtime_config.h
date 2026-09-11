#ifndef XIAOMI_REMOTE_RUNTIME_CONFIG_H
#define XIAOMI_REMOTE_RUNTIME_CONFIG_H

#include <stdbool.h>

#include "policy.h"

enum {
    KEYMAP_BUTTON_COUNT = 13,
    KEYMAP_ACTION_MAX = 128,
    KEYMAP_DEVICE_NAME_MAX = 256,
};

typedef enum {
    KEYMAP_BUTTON_POWER = 0,
    KEYMAP_BUTTON_UP,
    KEYMAP_BUTTON_LEFT,
    KEYMAP_BUTTON_BACK,
    KEYMAP_BUTTON_HOME,
    KEYMAP_BUTTON_MENU,
    KEYMAP_BUTTON_VOICE,
    KEYMAP_BUTTON_RIGHT,
    KEYMAP_BUTTON_OK,
    KEYMAP_BUTTON_DOWN,
    KEYMAP_BUTTON_VOLUME_UP,
    KEYMAP_BUTTON_VOLUME_DOWN,
    KEYMAP_BUTTON_TV,
} KeymapButton;

typedef struct {
    char single[KEYMAP_ACTION_MAX];
    char double_click[KEYMAP_ACTION_MAX];
    char long_press[KEYMAP_ACTION_MAX];
} KeymapButtonActions;

typedef struct {
    bool enabled;
    char device_name[KEYMAP_DEVICE_NAME_MAX];
    KeymapButtonActions buttons[KEYMAP_BUTTON_COUNT];
} KeymapConfig;

void keymap_config_defaults(KeymapConfig *config);
int keymap_config_load(const char *path, KeymapConfig *config);
const char *keymap_config_action(
    const KeymapConfig *config,
    KeymapButton button,
    ButtonTrigger trigger
);

#endif

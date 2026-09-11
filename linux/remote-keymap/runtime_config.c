#define _POSIX_C_SOURCE 200809L

#include "runtime_config.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const char *BUTTON_NAMES[KEYMAP_BUTTON_COUNT] = {
    "power", "up", "left", "back", "home", "menu", "voice",
    "right", "ok", "down", "volume_up", "volume_down", "tv",
};

static void copy_action(char destination[KEYMAP_ACTION_MAX], const char *source) {
    snprintf(destination, KEYMAP_ACTION_MAX, "%s", source);
}

static char *trim(char *value) {
    while (isspace((unsigned char)*value)) {
        value++;
    }
    char *end = value + strlen(value);
    while (end > value && isspace((unsigned char)end[-1])) {
        *--end = '\0';
    }
    return value;
}

void keymap_config_defaults(KeymapConfig *config) {
    memset(config, 0, sizeof(*config));
    config->enabled = true;
    copy_action(config->buttons[KEYMAP_BUTTON_POWER].single, "slash");
    copy_action(config->buttons[KEYMAP_BUTTON_POWER].double_click, "right-ctrl");
    copy_action(config->buttons[KEYMAP_BUTTON_POWER].long_press, "none");

    for (int button = KEYMAP_BUTTON_UP; button < KEYMAP_BUTTON_TV; button++) {
        copy_action(config->buttons[button].single, "native");
        copy_action(config->buttons[button].double_click, "none");
        copy_action(config->buttons[button].long_press, "none");
    }
    copy_action(config->buttons[KEYMAP_BUTTON_HOME].single, "chatgpt");
    copy_action(config->buttons[KEYMAP_BUTTON_MENU].single, "super");
    copy_action(config->buttons[KEYMAP_BUTTON_VOICE].single, "voice");
    copy_action(config->buttons[KEYMAP_BUTTON_TV].single, "none");
    snprintf(config->device_name, KEYMAP_DEVICE_NAME_MAX, "%s", "小米蓝牙语音遥控器");
}

static int find_button(const char *name) {
    for (int index = 0; index < KEYMAP_BUTTON_COUNT; index++) {
        if (strcmp(name, BUTTON_NAMES[index]) == 0) {
            return index;
        }
    }
    return -1;
}

static char *action_slot(KeymapButtonActions *actions, const char *trigger) {
    if (strcmp(trigger, "single") == 0) {
        return actions->single;
    }
    if (strcmp(trigger, "double") == 0) {
        return actions->double_click;
    }
    if (strcmp(trigger, "long") == 0) {
        return actions->long_press;
    }
    return NULL;
}

int keymap_config_load(const char *path, KeymapConfig *config) {
    keymap_config_defaults(config);
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    char line[KEYMAP_ACTION_MAX + KEYMAP_DEVICE_NAME_MAX + 32];
    while (fgets(line, sizeof(line), file) != NULL) {
        char *entry = trim(line);
        if (*entry == '\0' || *entry == '#') {
            continue;
        }
        char *separator = strchr(entry, '=');
        if (separator == NULL) {
            continue;
        }
        *separator = '\0';
        char *key = trim(entry);
        char *value = trim(separator + 1);
        if (strcmp(key, "enabled") == 0) {
            config->enabled = strcmp(value, "0") != 0 && strcmp(value, "false") != 0;
            continue;
        }
        if (strcmp(key, "device_name") == 0) {
            snprintf(config->device_name, KEYMAP_DEVICE_NAME_MAX, "%s", value);
            continue;
        }

        char *dot = strchr(key, '.');
        if (dot == NULL) {
            continue;
        }
        *dot = '\0';
        int button = find_button(key);
        if (button < 0) {
            continue;
        }
        char *slot = action_slot(&config->buttons[button], dot + 1);
        if (slot != NULL && value[0] != '\0') {
            copy_action(slot, value);
        }
    }
    fclose(file);
    return 0;
}

const char *keymap_config_action(
    const KeymapConfig *config,
    KeymapButton button,
    ButtonTrigger trigger
) {
    if ((int)button < 0 || (int)button >= KEYMAP_BUTTON_COUNT) {
        return "none";
    }
    switch (trigger) {
        case BUTTON_TRIGGER_SINGLE:
            return config->buttons[button].single;
        case BUTTON_TRIGGER_DOUBLE:
            return config->buttons[button].double_click;
        case BUTTON_TRIGGER_LONG:
            return config->buttons[button].long_press;
        default:
            return "none";
    }
}

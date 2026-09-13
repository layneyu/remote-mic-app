#define _POSIX_C_SOURCE 200809L

#include "runtime_config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    char path[] = "/tmp/xiaomi-keymap-config-XXXXXX";
    int file_descriptor = mkstemp(path);
    assert(file_descriptor >= 0);
    FILE *file = fdopen(file_descriptor, "w");
    assert(file != NULL);
    fputs("power.single=key:Ctrl\n", file);
    fputs("power.double=chatgpt\n", file);
    fputs("back.single=key:Tab\n", file);
    fputs("back.double=key:Shift+Tab\n", file);
    fputs("home.single=none\n", file);
    fputs("volume_up.single=command-sequence\n", file);
    fputs("volume_down.single=key:Backspace\n", file);
    fclose(file);

    KeymapConfig config;
    assert(keymap_config_load(path, &config) == 0);
    assert(strcmp(keymap_config_action(&config, KEYMAP_BUTTON_POWER, BUTTON_TRIGGER_SINGLE), "key:Ctrl") == 0);
    assert(strcmp(keymap_config_action(&config, KEYMAP_BUTTON_POWER, BUTTON_TRIGGER_DOUBLE), "chatgpt") == 0);
    assert(strcmp(keymap_config_action(&config, KEYMAP_BUTTON_BACK, BUTTON_TRIGGER_SINGLE), "key:Tab") == 0);
    assert(strcmp(keymap_config_action(&config, KEYMAP_BUTTON_BACK, BUTTON_TRIGGER_DOUBLE), "key:Shift+Tab") == 0);
    assert(strcmp(keymap_config_action(&config, KEYMAP_BUTTON_HOME, BUTTON_TRIGGER_SINGLE), "none") == 0);
    assert(strcmp(keymap_config_action(&config, KEYMAP_BUTTON_VOLUME_UP, BUTTON_TRIGGER_SINGLE), "command-sequence") == 0);
    assert(strcmp(keymap_config_action(&config, KEYMAP_BUTTON_VOLUME_DOWN, BUTTON_TRIGGER_SINGLE), "key:Backspace") == 0);
    assert(strcmp(keymap_config_action(&config, KEYMAP_BUTTON_MENU, BUTTON_TRIGGER_SINGLE), "super") == 0);
    assert(strcmp(keymap_config_action(&config, KEYMAP_BUTTON_TV, BUTTON_TRIGGER_SINGLE), "workspace-layer") == 0);

    unlink(path);
    return 0;
}

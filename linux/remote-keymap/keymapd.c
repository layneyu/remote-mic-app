#define _POSIX_C_SOURCE 200809L

#include "policy.h"
#include "runtime_config.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/record.h>
#include <X11/extensions/XTest.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

enum {
    KEYCODE_TV = 49,
    KEYCODE_OK = 36,
    KEYCODE_UP = 111,
    KEYCODE_DOWN = 116,
    KEYCODE_LEFT = 113,
    KEYCODE_RIGHT = 114,
    KEYCODE_HOME = 110,
    KEYCODE_MENU = 135,
    KEYCODE_BACK = 166,
    KEYCODE_VOLUME_DOWN = 122,
    KEYCODE_VOLUME_UP = 123,
    KEYCODE_POWER = 124,
    KEYCODE_VOICE = 69,
    TV_CHORD_WINDOW_MS = 400,
    MENU_CHORD_WINDOW_MS = 700,
};

static volatile sig_atomic_t stop_requested = 0;

static void request_stop(int signal_number) {
    (void)signal_number;
    stop_requested = 1;
}

static uint64_t monotonic_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }
    return (uint64_t)now.tv_sec * 1000U + (uint64_t)now.tv_nsec / 1000000U;
}

static void fake_key(Display *display, KeyCode keycode, bool is_press, bool dry_run) {
    if (keycode == 0) {
        fprintf(stderr, "keymap error: requested key has no X keycode\n");
        return;
    }
    printf("inject keycode=%u action=%s\n", keycode, is_press ? "down" : "up");
    fflush(stdout);
    if (!dry_run) {
        XTestFakeKeyEvent(display, keycode, is_press ? True : False, CurrentTime);
        XFlush(display);
    }
}

static void fake_tap(Display *display, KeyCode keycode, bool dry_run) {
    fake_key(display, keycode, true, dry_run);
    fake_key(display, keycode, false, dry_run);
}

typedef struct {
    KeyCode super;
    KeyCode slash;
    KeyCode right_ctrl;
} OutputKeycodes;

static void run_workspace_command(const char *direction, bool dry_run);
static void run_chatgpt(bool dry_run);

static KeyCode keycode_for_name(Display *display, const char *name) {
    if (strcmp(name, "Ctrl") == 0 || strcmp(name, "Control") == 0 || strcmp(name, "Control_R") == 0) {
        return XKeysymToKeycode(display, XK_Control_R);
    }
    if (strcmp(name, "Alt") == 0 || strcmp(name, "Alt_L") == 0) {
        return XKeysymToKeycode(display, XK_Alt_L);
    }
    if (strcmp(name, "Shift") == 0 || strcmp(name, "Shift_L") == 0) {
        return XKeysymToKeycode(display, XK_Shift_L);
    }
    if (strcmp(name, "Super") == 0 || strcmp(name, "Super_L") == 0) {
        return XKeysymToKeycode(display, XStringToKeysym("Super_L"));
    }
    if (strcmp(name, "/") == 0 || strcmp(name, "slash") == 0) {
        return XKeysymToKeycode(display, XK_slash);
    }
    if (strcmp(name, "Enter") == 0 || strcmp(name, "Return") == 0) {
        return XKeysymToKeycode(display, XK_Return);
    }
    if (strcmp(name, "Esc") == 0 || strcmp(name, "Escape") == 0) {
        return XKeysymToKeycode(display, XK_Escape);
    }
    if (strcmp(name, "Left") == 0) {
        return XKeysymToKeycode(display, XK_Left);
    }
    if (strcmp(name, "Right") == 0) {
        return XKeysymToKeycode(display, XK_Right);
    }
    if (strcmp(name, "Up") == 0) {
        return XKeysymToKeycode(display, XK_Up);
    }
    if (strcmp(name, "Down") == 0) {
        return XKeysymToKeycode(display, XK_Down);
    }
    KeySym keysym = XStringToKeysym(name);
    if (keysym == NoSymbol && strlen(name) == 1) {
        keysym = XStringToKeysym(name);
    }
    return keysym == NoSymbol ? 0 : XKeysymToKeycode(display, keysym);
}

static bool emit_custom_key_action(Display *display, const char *action, bool dry_run) {
    if (strncmp(action, "key:", 4) != 0) {
        return false;
    }
    char specification[KEYMAP_ACTION_MAX];
    snprintf(specification, sizeof(specification), "%s", action + 4);
    KeyCode keycodes[16];
    size_t count = 0;
    char *save_pointer = NULL;
    for (char *token = strtok_r(specification, "+", &save_pointer);
         token != NULL && count < 16;
         token = strtok_r(NULL, "+", &save_pointer)) {
        keycodes[count] = keycode_for_name(display, token);
        if (keycodes[count] == 0) {
            fprintf(stderr, "keymap warning: unsupported custom key: %s\n", token);
            return true;
        }
        count++;
    }
    if (count == 0) {
        return true;
    }
    for (size_t index = 0; index < count; index++) {
        fake_key(display, keycodes[index], true, dry_run);
    }
    for (size_t index = count; index > 0; index--) {
        fake_key(display, keycodes[index - 1], false, dry_run);
    }
    return true;
}

static void emit_action(Display *display, const char *action, const OutputKeycodes *keys, bool dry_run) {
    if (action == NULL || strcmp(action, "none") == 0 || strcmp(action, "native") == 0 ||
        strcmp(action, "voice") == 0 || strcmp(action, "disable") == 0) {
        return;
    }
    if (strcmp(action, "chatgpt") == 0) {
        run_chatgpt(dry_run);
        return;
    }
    if (strcmp(action, "workspace-prev") == 0) {
        run_workspace_command("prev", dry_run);
        return;
    }
    if (strcmp(action, "workspace-next") == 0) {
        run_workspace_command("next", dry_run);
        return;
    }
    if (strcmp(action, "slash") == 0) {
        fake_tap(display, keys->slash, dry_run);
        return;
    }
    if (strcmp(action, "right-ctrl") == 0) {
        fake_tap(display, keys->right_ctrl, dry_run);
        return;
    }
    if (strcmp(action, "super") == 0) {
        fake_tap(display, keys->super, dry_run);
        return;
    }
    (void)emit_custom_key_action(display, action, dry_run);
}

static const char *trigger_name(ButtonTrigger trigger) {
    switch (trigger) {
        case BUTTON_TRIGGER_SINGLE:
            return "single";
        case BUTTON_TRIGGER_DOUBLE:
            return "double";
        case BUTTON_TRIGGER_LONG:
            return "long";
        default:
            return "none";
    }
}

static void emit_button_action(
    Display *display,
    const KeymapConfig *config,
    KeymapButton button,
    const char *button_name,
    ButtonTrigger trigger,
    const OutputKeycodes *keys,
    bool dry_run,
    KeyCode native_keycode
) {
    const char *action = keymap_config_action(config, button, trigger);
    if (action == NULL || strcmp(action, "none") == 0) {
        return;
    }
    printf("action button=%s trigger=%s action=%s\n",
           button_name, trigger_name(trigger), action);
    fflush(stdout);
    if (strcmp(action, "native") == 0 && native_keycode != 0) {
        fake_tap(display, native_keycode, dry_run);
    } else {
        emit_action(display, action, keys, dry_run);
    }
}

static void run_workspace_command(const char *direction, bool dry_run) {
    printf("workspace action=%s\n", direction);
    fflush(stdout);
    if (dry_run) {
        return;
    }
    pid_t child = fork();
    if (child == 0) {
        int null_fd = open("/dev/null", O_WRONLY);
        if (null_fd >= 0) {
            dup2(null_fd, STDOUT_FILENO);
            dup2(null_fd, STDERR_FILENO);
            close(null_fd);
        }
        execlp("i3-msg", "i3-msg", "workspace", direction, (char *)NULL);
        _exit(127);
    }
    if (child > 0) {
        int status;
        while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
        }
    }
}

static void run_chatgpt(bool dry_run) {
    printf("action button=home target=chatgpt\n");
    fflush(stdout);
    if (dry_run) {
        return;
    }
    pid_t child = fork();
    if (child == 0) {
        execl("/home/layne/.local/bin/xiaomi-focus-chatgpt",
              "xiaomi-focus-chatgpt", (char *)NULL);
        _exit(127);
    }
}

static const char *device_name_or_default(const char *value) {
    return value == NULL ? "小米蓝牙语音遥控器" : value;
}

static int find_device(Display *display, const char *wanted_name) {
    int count = 0;
    XIDeviceInfo *devices = XIQueryDevice(display, XIAllDevices, &count);
    int device_id = -1;
    for (int index = 0; devices != NULL && index < count; index++) {
        if (devices[index].name != NULL && strcmp(devices[index].name, wanted_name) == 0) {
            device_id = devices[index].deviceid;
            break;
        }
    }
    if (devices != NULL) {
        XIFreeDeviceInfo(devices);
    }
    return device_id;
}

static int tracker_timeout_ms(const ButtonTracker *tracker, uint64_t now_ms) {
    if (!tracker->single_pending) {
        return 100;
    }
    uint64_t deadline = tracker->last_release_ms + 280U;
    if (deadline <= now_ms) {
        return 0;
    }
    uint64_t remaining = deadline - now_ms;
    return remaining > 100U ? 100 : (int)remaining;
}

static int poll_timeout_ms(
    const ButtonTracker *power_tracker,
    const ButtonTracker *back_tracker,
    uint64_t now_ms
) {
    int timeout_ms = tracker_timeout_ms(power_tracker, now_ms);
    int back_timeout_ms = tracker_timeout_ms(back_tracker, now_ms);
    return back_timeout_ms < timeout_ms ? back_timeout_ms : timeout_ms;
}

static int menu_poll_timeout_ms(bool menu_chord_pending, uint64_t deadline_ms, uint64_t now_ms) {
    if (!menu_chord_pending) {
        return 100;
    }
    if (deadline_ms <= now_ms) {
        return 0;
    }
    uint64_t remaining = deadline_ms - now_ms;
    return remaining > 100U ? 100 : (int)remaining;
}

static void flush_power(
    Display *display,
    const KeymapConfig *config,
    ButtonTracker *power_tracker,
    const OutputKeycodes *keys,
    bool dry_run
) {
    ButtonTrigger trigger = button_tracker_flush(power_tracker, monotonic_ms(), 280);
    if (trigger != BUTTON_TRIGGER_NONE) {
        emit_button_action(
            display, config, KEYMAP_BUTTON_POWER, "power", trigger,
            keys, dry_run, 0
        );
    }
}

static void handle_key_event(
    Display *display,
    const XIDeviceEvent *event,
    bool *pressed,
    bool *menu_held,
    bool *menu_chord_pending,
    uint64_t *menu_chord_deadline_ms,
    bool *tv_held,
    ButtonTracker *power_tracker,
    ButtonTracker *back_tracker,
    const KeymapConfig *config,
    const OutputKeycodes *keys,
    bool *tv_chord_pending,
    uint64_t *tv_chord_deadline_ms,
    bool *consume_direction_release,
    bool dry_run
) {
    unsigned int code = event->detail;
    if (code >= 256) {
        return;
    }
    bool is_press = event->evtype == XI_KeyPress;
    if (is_press && pressed[code]) {
        return;
    }
    if (is_press) {
        pressed[code] = true;
    } else if (!pressed[code]) {
        return;
    } else {
        pressed[code] = false;
    }

    if (code == KEYCODE_VOICE) {
        printf("remote key=voice action=consume\n");
        return;
    }
    if (code == KEYCODE_MENU) {
        if (is_press) {
            *menu_held = true;
            *menu_chord_pending = false;
            fake_key(display, keys->super, true, dry_run);
        } else {
            *menu_held = false;
            *menu_chord_pending = true;
            *menu_chord_deadline_ms = monotonic_ms() + MENU_CHORD_WINDOW_MS;
            printf("remote key=menu action=modifier-up chord-window-ms=%u\n", MENU_CHORD_WINDOW_MS);
            fflush(stdout);
        }
        return;
    }
    if (code == KEYCODE_TV) {
        if (is_press) {
            *tv_held = true;
            *tv_chord_pending = false;
            printf("remote key=tv action=modifier-down\n");
        } else {
            *tv_held = false;
            *tv_chord_pending = true;
            *tv_chord_deadline_ms = monotonic_ms() + TV_CHORD_WINDOW_MS;
            printf("remote key=tv action=modifier-up chord-window-ms=%u\n", TV_CHORD_WINDOW_MS);
        }
        fflush(stdout);
        return;
    }
    if (code == KEYCODE_POWER) {
        if (is_press) {
            button_tracker_press(power_tracker, monotonic_ms());
        } else {
            ButtonTrigger trigger = button_tracker_release(power_tracker, monotonic_ms(), 500, 280);
            printf("remote key=power trigger=%d\n", trigger);
            fflush(stdout);
            if (trigger != BUTTON_TRIGGER_NONE) {
                emit_button_action(
                    display, config, KEYMAP_BUTTON_POWER, "power", trigger,
                    keys, dry_run, 0
                );
            }
        }
        return;
    }
    if (code == KEYCODE_HOME) {
        if (is_press) {
            emit_action(
                display,
                keymap_config_action(config, KEYMAP_BUTTON_HOME, BUTTON_TRIGGER_SINGLE),
                keys,
                dry_run
            );
        }
        return;
    }
    if (code == KEYCODE_LEFT || code == KEYCODE_RIGHT) {
        bool chord_active = *tv_held ||
            (*tv_chord_pending && chord_window_active(monotonic_ms(), *tv_chord_deadline_ms));
        if (is_press && chord_active) {
            *consume_direction_release = true;
            *tv_chord_pending = false;
            run_workspace_command(code == KEYCODE_LEFT ? "prev" : "next", dry_run);
        } else if (!is_press && *consume_direction_release) {
            *consume_direction_release = false;
        } else {
            fake_key(display, (KeyCode)code, is_press, dry_run);
        }
        if (!is_press && *menu_chord_pending) {
            fake_key(display, keys->super, false, dry_run);
            *menu_chord_pending = false;
        }
        return;
    }
    if (code == KEYCODE_BACK) {
        if (is_press) {
            button_tracker_press(back_tracker, monotonic_ms());
        } else {
            ButtonTrigger trigger = button_tracker_release(back_tracker, monotonic_ms(), 500, 280);
            printf("remote key=back trigger=%s\n", trigger_name(trigger));
            fflush(stdout);
            if (trigger != BUTTON_TRIGGER_NONE) {
                emit_button_action(
                    display, config, KEYMAP_BUTTON_BACK, "back", trigger,
                    keys, dry_run, (KeyCode)KEYCODE_BACK
                );
            }
        }
        return;
    }
    if (code == KEYCODE_OK || code == KEYCODE_UP || code == KEYCODE_DOWN ||
        code == KEYCODE_VOLUME_UP ||
        code == KEYCODE_VOLUME_DOWN) {
        fake_key(display, (KeyCode)code, is_press, dry_run);
        return;
    }
    printf("remote key=unknown keycode=%u action=passthrough\n", code);
    fake_key(display, (KeyCode)code, is_press, dry_run);
}

static void usage(const char *program) {
    fprintf(stderr, "Usage: %s [--device-name NAME] [--config PATH] [--dry-run]\n", program);
}

int main(int argc, char **argv) {
    const char *wanted_name = NULL;
    const char *config_path = NULL;
    bool dry_run = false;
    for (int index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--dry-run") == 0) {
            dry_run = true;
        } else if (strcmp(argv[index], "--config") == 0 && index + 1 < argc) {
            config_path = argv[++index];
        } else if (strcmp(argv[index], "--device-name") == 0 && index + 1 < argc) {
            wanted_name = argv[++index];
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    KeymapConfig config;
    keymap_config_load(config_path, &config);
    if (!config.enabled) {
        printf("keymap_disabled\n");
        return 0;
    }
    wanted_name = device_name_or_default(wanted_name == NULL ? config.device_name : wanted_name);

    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "keymap error: cannot open X display\n");
        return 1;
    }
    int xi_opcode = 0;
    int xi_event = 0;
    int xi_error = 0;
    if (!XQueryExtension(display, "XInputExtension", &xi_opcode, &xi_event, &xi_error)) {
        fprintf(stderr, "keymap error: XInput extension is unavailable\n");
        XCloseDisplay(display);
        return 1;
    }
    int major = 2;
    int minor = 0;
    if (XIQueryVersion(display, &major, &minor) != Success) {
        fprintf(stderr, "keymap error: XInput2 is unavailable\n");
        XCloseDisplay(display);
        return 1;
    }
    int device_id = find_device(display, wanted_name);
    if (device_id < 0) {
        fprintf(stderr, "keymap error: device not found: %s\n", wanted_name);
        XCloseDisplay(display);
        return 1;
    }

    unsigned char mask[(XI_LASTEVENT + 7) / 8];
    memset(mask, 0, sizeof(mask));
    XISetMask(mask, XI_KeyPress);
    XISetMask(mask, XI_KeyRelease);
    XIEventMask event_mask = {
        .deviceid = device_id,
        .mask_len = sizeof(mask),
        .mask = mask,
    };
    if (XIGrabDevice(
            display,
            device_id,
            DefaultRootWindow(display),
            CurrentTime,
            None,
            GrabModeAsync,
            GrabModeAsync,
            False,
            &event_mask
        ) != GrabSuccess) {
        fprintf(stderr, "keymap error: cannot grab device: %s\n", wanted_name);
        XCloseDisplay(display);
        return 1;
    }

    KeyCode super_keycode = XKeysymToKeycode(display, XStringToKeysym("Super_L"));
    KeyCode slash_keycode = XKeysymToKeycode(display, XK_slash);
    KeyCode right_ctrl_keycode = XKeysymToKeycode(display, XK_Control_R);
    if (super_keycode == 0 || slash_keycode == 0 || right_ctrl_keycode == 0) {
        fprintf(stderr, "keymap error: required X key symbols are unavailable\n");
        XIUngrabDevice(display, device_id, CurrentTime);
        XCloseDisplay(display);
        return 1;
    }
    OutputKeycodes output_keys = {
        .super = super_keycode,
        .slash = slash_keycode,
        .right_ctrl = right_ctrl_keycode,
    };

    signal(SIGINT, request_stop);
    signal(SIGTERM, request_stop);
    printf("keymap_ready device=%s device_id=%d dry_run=%s\n", wanted_name, device_id, dry_run ? "true" : "false");
    fflush(stdout);

    bool pressed[256] = {false};
    bool menu_held = false;
    bool menu_chord_pending = false;
    uint64_t menu_chord_deadline_ms = 0;
    bool tv_held = false;
    bool tv_chord_pending = false;
    uint64_t tv_chord_deadline_ms = 0;
    bool consume_direction_release = false;
    ButtonTracker power_tracker;
    ButtonTracker back_tracker;
    button_tracker_init(&power_tracker);
    button_tracker_init(&back_tracker);
    int connection_fd = ConnectionNumber(display);
    while (!stop_requested) {
        flush_power(display, &config, &power_tracker, &output_keys, dry_run);
        ButtonTrigger back_trigger = button_tracker_flush(&back_tracker, monotonic_ms(), 280);
        if (back_trigger != BUTTON_TRIGGER_NONE) {
            emit_button_action(
                display, &config, KEYMAP_BUTTON_BACK, "back", back_trigger,
                &output_keys, dry_run, (KeyCode)KEYCODE_BACK
            );
        }
        if (menu_chord_pending && !chord_window_active(monotonic_ms(), menu_chord_deadline_ms)) {
            fake_key(display, output_keys.super, false, dry_run);
            menu_chord_pending = false;
        }
        while (!stop_requested && XPending(display) > 0) {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type != GenericEvent || event.xcookie.extension != xi_opcode) {
                continue;
            }
            if (!XGetEventData(display, &event.xcookie)) {
                continue;
            }
            if ((event.xcookie.evtype == XI_KeyPress || event.xcookie.evtype == XI_KeyRelease) &&
                event.xcookie.data != NULL) {
                XIDeviceEvent *device_event = event.xcookie.data;
                if (device_event->deviceid == device_id) {
                    handle_key_event(
                        display,
                        device_event,
                        pressed,
                        &menu_held,
                        &menu_chord_pending,
                        &menu_chord_deadline_ms,
                        &tv_held,
                        &power_tracker,
                        &back_tracker,
                        &config,
                        &output_keys,
                        &tv_chord_pending,
                        &tv_chord_deadline_ms,
                        &consume_direction_release,
                        dry_run
                    );
                }
            }
            XFreeEventData(display, &event.xcookie);
        }
        if (stop_requested) {
            break;
        }
        struct pollfd descriptor = {.fd = connection_fd, .events = POLLIN};
        uint64_t now_ms = monotonic_ms();
        int timeout_ms = poll_timeout_ms(&power_tracker, &back_tracker, now_ms);
        int menu_timeout_ms = menu_poll_timeout_ms(menu_chord_pending, menu_chord_deadline_ms, now_ms);
        if (menu_timeout_ms < timeout_ms) {
            timeout_ms = menu_timeout_ms;
        }
        int result = poll(&descriptor, 1, timeout_ms);
        if (result < 0 && errno != EINTR) {
            perror("keymap poll");
            break;
        }
    }

    if (menu_held || menu_chord_pending) {
        fake_key(display, output_keys.super, false, dry_run);
    }
    XIUngrabDevice(display, device_id, CurrentTime);
    XFlush(display);
    XCloseDisplay(display);
    return 0;
}

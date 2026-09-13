#define _POSIX_C_SOURCE 200809L

#include "policy.h"
#include "runtime_config.h"
#include "key_names.h"
#include "device_wait.h"
#include "command_sequence.h"
#include "terminal_paste.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/extensions/XTest.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

enum {
    KEYCODE_TV = KEY_GRAVE,
    KEYCODE_OK = KEY_ENTER,
    KEYCODE_UP = KEY_UP,
    KEYCODE_DOWN = KEY_DOWN,
    KEYCODE_LEFT = KEY_LEFT,
    KEYCODE_RIGHT = KEY_RIGHT,
    KEYCODE_HOME = KEY_HOME,
    KEYCODE_MENU = KEY_COMPOSE,
    KEYCODE_BACK = KEY_BACK,
    KEYCODE_VOLUME_DOWN = KEY_VOLUMEDOWN,
    KEYCODE_VOLUME_UP = KEY_VOLUMEUP,
    KEYCODE_POWER = KEY_POWER,
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
    KeyCode f9;
} OutputKeycodes;

static void run_workspace_command(const char *direction, bool dry_run);
static void run_chatgpt(bool dry_run);
static void run_command_sequence(Display *display, CommandSequenceState *sequence, bool dry_run);

static KeyCode keycode_for_name(Display *display, const char *name) {
    KeySym keysym = key_sym_for_name(name);
    return keysym == NoSymbol ? 0 : XKeysymToKeycode(display, keysym);
}

static int open_remote_evdev(const char *wanted_name) {
    DIR *directory = opendir("/sys/class/input");
    if (directory == NULL) {
        fprintf(stderr, "keymap error: cannot scan Linux input devices\n");
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) != 0 || entry->d_name[5] == '\0') {
            continue;
        }
        bool numeric = true;
        for (const char *cursor = entry->d_name + 5; *cursor != '\0'; cursor++) {
            if (*cursor < '0' || *cursor > '9') {
                numeric = false;
                break;
            }
        }
        if (!numeric) {
            continue;
        }

        char name_path[PATH_MAX];
        snprintf(name_path, sizeof(name_path), "/sys/class/input/%s/device/name", entry->d_name);
        FILE *name_file = fopen(name_path, "r");
        if (name_file == NULL) {
            continue;
        }
        char device_name[256];
        bool matches = fgets(device_name, sizeof(device_name), name_file) != NULL;
        fclose(name_file);
        if (!matches) {
            continue;
        }
        device_name[strcspn(device_name, "\r\n")] = '\0';
        if (strcmp(device_name, wanted_name) != 0) {
            continue;
        }

        char device_path[PATH_MAX];
        snprintf(device_path, sizeof(device_path), "/dev/input/%s", entry->d_name);
        int descriptor = open(device_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (descriptor < 0) {
            fprintf(stderr, "keymap error: cannot open matching remote input device\n");
            continue;
        }
        if (ioctl(descriptor, EVIOCGRAB, 1) < 0) {
            fprintf(stderr, "keymap error: cannot exclusively grab remote input device\n");
            close(descriptor);
            continue;
        }
        closedir(directory);
        return descriptor;
    }
    closedir(directory);
    return -1;
}

static int open_remote_for_wait(void *context) {
    return open_remote_evdev((const char *)context);
}

static bool remote_wait_stopped(void *context) {
    (void)context;
    return stop_requested != 0;
}

static void wait_for_remote_retry(unsigned int delay_ms, void *context) {
    (void)context;
    poll(NULL, 0, (int)delay_ms);
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

static void emit_action(
    Display *display,
    const char *action,
    const OutputKeycodes *keys,
    CommandSequenceState *sequence,
    bool dry_run
) {
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
    if (strcmp(action, "command-sequence") == 0) {
        run_command_sequence(display, sequence, dry_run);
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
    CommandSequenceState *sequence,
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
        emit_action(display, action, keys, sequence, dry_run);
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

static void run_command_sequence(Display *display, CommandSequenceState *sequence, bool dry_run) {
    char command[COMMAND_SEQUENCE_COMMAND_MAX];
    size_t index = 0;
    size_t count = 0;
    if (!command_sequence_next(sequence->path, sequence, command, &index, &count)) {
        printf("command_sequence phase=ignored result=empty\n");
        fflush(stdout);
        return;
    }
    printf("command_sequence phase=submitted index=%zu count=%zu\n", index + 1, count);
    fflush(stdout);
    (void)terminal_paste_command(display, command, dry_run);
}

static const char *device_name_or_default(const char *value) {
    return value == NULL ? "小米蓝牙语音遥控器" : value;
}

static KeyCode native_keycode_for_remote_key(Display *display, unsigned int code) {
    KeySym keysym = NoSymbol;
    switch (code) {
        case KEY_GRAVE:
            keysym = XK_grave;
            break;
        case KEY_ENTER:
        case KEY_OK:
            keysym = XK_Return;
            break;
        case KEY_UP:
            keysym = XK_Up;
            break;
        case KEY_DOWN:
            keysym = XK_Down;
            break;
        case KEY_LEFT:
            keysym = XK_Left;
            break;
        case KEY_RIGHT:
            keysym = XK_Right;
            break;
        case KEY_HOME:
            keysym = XK_Home;
            break;
        case KEY_BACK:
            keysym = XF86XK_Back;
            break;
        case KEY_VOLUMEUP:
            keysym = XF86XK_AudioRaiseVolume;
            break;
        case KEY_VOLUMEDOWN:
            keysym = XF86XK_AudioLowerVolume;
            break;
        default:
            break;
    }
    return keysym == NoSymbol ? 0 : XKeysymToKeycode(display, keysym);
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
    CommandSequenceState *sequence,
    bool dry_run
) {
    ButtonTrigger trigger = button_tracker_flush(power_tracker, monotonic_ms(), 280);
    if (trigger != BUTTON_TRIGGER_NONE) {
        emit_button_action(
            display, config, KEYMAP_BUTTON_POWER, "power", trigger,
            keys, sequence, dry_run, 0
        );
    }
}

static void handle_evdev_key_event(
    Display *display,
    const struct input_event *event,
    bool pressed[KEY_MAX + 1],
    bool *menu_held,
    bool *menu_chord_pending,
    uint64_t *menu_chord_deadline_ms,
    bool *tv_held,
    ButtonTracker *power_tracker,
    ButtonTracker *back_tracker,
    const KeymapConfig *config,
    const OutputKeycodes *keys,
    CommandSequenceState *sequence,
    bool *tv_chord_pending,
    uint64_t *tv_chord_deadline_ms,
    bool *consume_direction_release,
    bool dry_run
) {
    if (event->type != EV_KEY || event->code > KEY_MAX) {
        return;
    }
    unsigned int code = event->code;
    if (event->value == 2) {
        return;
    }
    if (event->value != 0 && event->value != 1) {
        return;
    }
    bool is_press = event->value == 1;
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

    if (remote_evdev_key_is_voice(code)) {
        printf("remote key=voice evdev_semantic=true edge=%s\n",
               is_press ? "down" : "up");
        fflush(stdout);
        fake_key(display, keys->f9, is_press, dry_run);
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
                    keys, sequence, dry_run, 0
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
                sequence,
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
            fake_key(display, native_keycode_for_remote_key(display, code), is_press, dry_run);
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
                    keys, sequence, dry_run, native_keycode_for_remote_key(display, KEYCODE_BACK)
                );
            }
        }
        return;
    }
    if (code == KEYCODE_VOLUME_UP || code == KEYCODE_VOLUME_DOWN) {
        KeymapButton button = code == KEYCODE_VOLUME_UP ?
            KEYMAP_BUTTON_VOLUME_UP : KEYMAP_BUTTON_VOLUME_DOWN;
        const char *button_name = code == KEYCODE_VOLUME_UP ? "volume_up" : "volume_down";
        KeyCode native_keycode = native_keycode_for_remote_key(display, code);
        const char *action = keymap_config_action(config, button, BUTTON_TRIGGER_SINGLE);
        if (strcmp(action, "native") == 0) {
            fake_key(display, native_keycode, is_press, dry_run);
        } else if (is_press) {
            emit_button_action(
                display, config, button, button_name, BUTTON_TRIGGER_SINGLE,
                keys, sequence, dry_run, 0
            );
        }
        return;
    }
    if (code == KEYCODE_OK || code == KEYCODE_UP || code == KEYCODE_DOWN) {
        fake_key(display, native_keycode_for_remote_key(display, code), is_press, dry_run);
        return;
    }
    printf("remote key=unknown evdev_code=%u action=passthrough\n", code);
    fake_key(display, native_keycode_for_remote_key(display, code), is_press, dry_run);
}

static void usage(const char *program) {
    fprintf(stderr, "Usage: %s [--device-name NAME] [--config PATH] [--command-sequence PATH] [--dry-run]\n", program);
}

int main(int argc, char **argv) {
    const char *wanted_name = NULL;
    const char *config_path = NULL;
    const char *command_sequence_path = NULL;
    bool dry_run = false;
    for (int index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--dry-run") == 0) {
            dry_run = true;
        } else if (strcmp(argv[index], "--config") == 0 && index + 1 < argc) {
            config_path = argv[++index];
        } else if (strcmp(argv[index], "--device-name") == 0 && index + 1 < argc) {
            wanted_name = argv[++index];
        } else if (strcmp(argv[index], "--command-sequence") == 0 && index + 1 < argc) {
            command_sequence_path = argv[++index];
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
    char default_command_sequence_path[PATH_MAX];
    if (command_sequence_path == NULL) {
        const char *home = getenv("HOME");
        if (home != NULL) {
            snprintf(default_command_sequence_path, sizeof(default_command_sequence_path),
                     "%s/.config/xiaomi-remote/command-sequence.conf", home);
            command_sequence_path = default_command_sequence_path;
        }
    }
    CommandSequenceState command_sequence;
    command_sequence_state_init(&command_sequence, command_sequence_path);

    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "keymap error: cannot open X display\n");
        return 1;
    }
    KeyCode super_keycode = XKeysymToKeycode(display, XStringToKeysym("Super_L"));
    KeyCode slash_keycode = XKeysymToKeycode(display, XK_slash);
    KeyCode right_ctrl_keycode = XKeysymToKeycode(display, XK_Control_R);
    KeyCode f9_keycode = XKeysymToKeycode(display, XK_F9);
    if (super_keycode == 0 || slash_keycode == 0 || right_ctrl_keycode == 0 || f9_keycode == 0) {
        fprintf(stderr, "keymap error: required X key symbols are unavailable\n");
        XCloseDisplay(display);
        return 1;
    }
    OutputKeycodes output_keys = {
        .super = super_keycode,
        .slash = slash_keycode,
        .right_ctrl = right_ctrl_keycode,
        .f9 = f9_keycode,
    };

    signal(SIGINT, request_stop);
    signal(SIGTERM, request_stop);
    while (!stop_requested) {
        printf("keymap_waiting device=%s\n", wanted_name);
        fflush(stdout);
        int input_descriptor = wait_for_device(
            open_remote_for_wait, remote_wait_stopped, wait_for_remote_retry, (void *)wanted_name
        );
        if (input_descriptor < 0) {
            break;
        }
        printf("keymap_ready device=%s source=evdev exclusive=true dry_run=%s\n",
               wanted_name, dry_run ? "true" : "false");
        fflush(stdout);

        bool pressed[KEY_MAX + 1] = {false};
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

        while (!stop_requested) {
            flush_power(display, &config, &power_tracker, &output_keys, &command_sequence, dry_run);
            ButtonTrigger back_trigger = button_tracker_flush(&back_tracker, monotonic_ms(), 280);
            if (back_trigger != BUTTON_TRIGGER_NONE) {
                emit_button_action(
                    display, &config, KEYMAP_BUTTON_BACK, "back", back_trigger,
                    &output_keys, &command_sequence, dry_run, (KeyCode)KEYCODE_BACK
                );
            }
            if (menu_chord_pending && !chord_window_active(monotonic_ms(), menu_chord_deadline_ms)) {
                fake_key(display, output_keys.super, false, dry_run);
                menu_chord_pending = false;
            }
            struct pollfd descriptor = {.fd = input_descriptor, .events = POLLIN};
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
            if (result == 0 || stop_requested) {
                continue;
            }
            if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                break;
            }
            if ((descriptor.revents & POLLIN) != 0) {
                struct input_event event;
                ssize_t bytes = read(input_descriptor, &event, sizeof(event));
                if (bytes == (ssize_t)sizeof(event)) {
                    handle_evdev_key_event(
                        display,
                        &event,
                        pressed,
                        &menu_held,
                        &menu_chord_pending,
                        &menu_chord_deadline_ms,
                        &tv_held,
                        &power_tracker,
                        &back_tracker,
                        &config,
                        &output_keys,
                        &command_sequence,
                        &tv_chord_pending,
                        &tv_chord_deadline_ms,
                        &consume_direction_release,
                        dry_run
                    );
                } else if (bytes < 0 && errno != EAGAIN && errno != EINTR) {
                    perror("keymap read");
                    break;
                } else if (bytes != (ssize_t)sizeof(event) && bytes >= 0) {
                    fprintf(stderr, "keymap error: incomplete remote input event\n");
                    break;
                }
            }
        }

        if (menu_held || menu_chord_pending) {
            fake_key(display, output_keys.super, false, dry_run);
        }
        ioctl(input_descriptor, EVIOCGRAB, 0);
        close(input_descriptor);
        if (!stop_requested) {
            fprintf(stderr, "keymap notice: remote input device disconnected; waiting for reconnect\n");
        }
    }

    XFlush(display);
    XCloseDisplay(display);
    return 0;
}

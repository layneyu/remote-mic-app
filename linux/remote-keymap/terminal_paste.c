#define _POSIX_C_SOURCE 200809L

#include "terminal_paste.h"

#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

enum {
    TERMINAL_PASTE_SUBMIT_DELAY_MS = 300,
};

unsigned int terminal_paste_submit_delay_ms(void) {
    return TERMINAL_PASTE_SUBMIT_DELAY_MS;
}

static bool contains_case_insensitive(const char *value, const char *needle) {
    if (value == NULL || needle == NULL || *needle == '\0') {
        return false;
    }
    size_t needle_length = strlen(needle);
    for (const char *cursor = value; *cursor != '\0'; cursor++) {
        if (strncasecmp(cursor, needle, needle_length) == 0) {
            return true;
        }
    }
    return false;
}

bool terminal_window_class_allowed(const char *instance, const char *class_name) {
    const char *values[] = {instance, class_name};
    for (size_t index = 0; index < sizeof(values) / sizeof(values[0]); index++) {
        const char *value = values[index];
        if (value == NULL) {
            continue;
        }
        if (contains_case_insensitive(value, "terminal") ||
            contains_case_insensitive(value, "xterm") ||
            contains_case_insensitive(value, "alacritty") ||
            contains_case_insensitive(value, "kitty") ||
            contains_case_insensitive(value, "konsole") ||
            contains_case_insensitive(value, "urxvt") ||
            contains_case_insensitive(value, "foot") ||
            contains_case_insensitive(value, "wezterm") ||
            strcasecmp(value, "st") == 0 ||
            contains_case_insensitive(value, "st-")) {
            return true;
        }
    }
    return false;
}

static bool focused_window_is_terminal(Display *display) {
    Window current;
    int revert_to;
    XGetInputFocus(display, &current, &revert_to);
    for (unsigned int depth = 0; current != None && current != PointerRoot && depth < 32; depth++) {
        XClassHint hint = {0};
        if (XGetClassHint(display, current, &hint)) {
            bool allowed = terminal_window_class_allowed(hint.res_name, hint.res_class);
            if (hint.res_name != NULL) {
                XFree(hint.res_name);
            }
            if (hint.res_class != NULL) {
                XFree(hint.res_class);
            }
            if (allowed) {
                return true;
            }
        }

        Window root = None;
        Window parent = None;
        Window *children = NULL;
        unsigned int child_count = 0;
        if (!XQueryTree(display, current, &root, &parent, &children, &child_count)) {
            break;
        }
        if (children != NULL) {
            XFree(children);
        }
        if (parent == None || parent == current) {
            break;
        }
        current = parent;
    }
    return false;
}

static bool write_all(int descriptor, const char *value, size_t length) {
    size_t offset = 0;
    while (offset < length) {
        ssize_t written = write(descriptor, value + offset, length - offset);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            return false;
        }
        offset += (size_t)written;
    }
    return true;
}

static pid_t start_clipboard_owner(const char *command) {
    int pipe_descriptors[2];
    if (pipe(pipe_descriptors) != 0) {
        return -1;
    }
    pid_t child = fork();
    if (child == 0) {
        close(pipe_descriptors[1]);
        if (dup2(pipe_descriptors[0], STDIN_FILENO) < 0) {
            _exit(126);
        }
        close(pipe_descriptors[0]);
        execlp("xclip", "xclip", "-selection", "clipboard", "-in", (char *)NULL);
        _exit(127);
    }
    close(pipe_descriptors[0]);
    if (child < 0 || !write_all(pipe_descriptors[1], command, strlen(command))) {
        close(pipe_descriptors[1]);
        if (child > 0) {
            kill(child, SIGTERM);
            waitpid(child, NULL, 0);
        }
        return -1;
    }
    close(pipe_descriptors[1]);
    return child;
}

static bool wait_for_clipboard_owner(pid_t child) {
    int status;
    while (waitpid(child, &status, 0) < 0) {
        if (errno == EINTR) {
            continue;
        }
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static void send_key(Display *display, KeyCode keycode, bool is_press) {
    XTestFakeKeyEvent(display, keycode, is_press ? True : False, CurrentTime);
}

static bool paste_and_submit(Display *display) {
    KeyCode control = XKeysymToKeycode(display, XK_Control_L);
    KeyCode shift = XKeysymToKeycode(display, XK_Shift_L);
    KeyCode v = XKeysymToKeycode(display, XK_v);
    KeyCode enter = XKeysymToKeycode(display, XK_Return);
    if (control == 0 || shift == 0 || v == 0 || enter == 0) {
        return false;
    }
    send_key(display, control, true);
    send_key(display, shift, true);
    send_key(display, v, true);
    send_key(display, v, false);
    send_key(display, shift, false);
    send_key(display, control, false);
    XSync(display, False);
    struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = (long)TERMINAL_PASTE_SUBMIT_DELAY_MS * 1000000L,
    };
    nanosleep(&delay, NULL);
    send_key(display, enter, true);
    send_key(display, enter, false);
    XFlush(display);
    return true;
}

bool terminal_paste_command(Display *display, const char *command, bool dry_run) {
    if (display == NULL || command == NULL || *command == '\0') {
        return false;
    }
    if (!focused_window_is_terminal(display)) {
        printf("command_sequence phase=ignored result=focused_window_not_terminal\n");
        fflush(stdout);
        return false;
    }
    if (dry_run) {
        printf("command_sequence phase=terminal_paste result=dry_run\n");
        fflush(stdout);
        return true;
    }

    pid_t clipboard_owner = start_clipboard_owner(command);
    if (clipboard_owner < 0) {
        printf("command_sequence phase=terminal_paste result=clipboard_unavailable\n");
        fflush(stdout);
        return false;
    }
    struct timespec delay = {.tv_sec = 0, .tv_nsec = 20000000L};
    nanosleep(&delay, NULL);
    bool submitted = paste_and_submit(display);
    bool clipboard_finished = wait_for_clipboard_owner(clipboard_owner);
    if (!submitted || !clipboard_finished) {
        printf("command_sequence phase=terminal_paste result=failed\n");
        fflush(stdout);
        return false;
    }
    printf("command_sequence phase=terminal_paste result=pasted_and_submitted\n");
    fflush(stdout);
    return true;
}

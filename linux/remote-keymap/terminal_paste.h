#ifndef XIAOMI_REMOTE_TERMINAL_PASTE_H
#define XIAOMI_REMOTE_TERMINAL_PASTE_H

#include <stdbool.h>

#include <X11/Xlib.h>

bool terminal_window_class_allowed(const char *instance, const char *class_name);
unsigned int terminal_paste_submit_delay_ms(void);
bool terminal_paste_command(Display *display, const char *command, bool dry_run);

#endif

#include "key_names.h"

#include <X11/Xlib.h>
#include <string.h>

KeySym key_sym_for_name(const char *name) {
    if (strcmp(name, "Ctrl") == 0 || strcmp(name, "Control") == 0 ||
        strcmp(name, "Control_L") == 0 || strcmp(name, "Ctrl_L") == 0) {
        return XK_Control_L;
    }
    if (strcmp(name, "Right Ctrl") == 0 || strcmp(name, "Control_R") == 0 ||
        strcmp(name, "Ctrl_R") == 0) {
        return XK_Control_R;
    }
    if (strcmp(name, "Alt") == 0 || strcmp(name, "Alt_L") == 0) {
        return XK_Alt_L;
    }
    if (strcmp(name, "Shift") == 0 || strcmp(name, "Shift_L") == 0) {
        return XK_Shift_L;
    }
    if (strcmp(name, "Super") == 0 || strcmp(name, "Super_L") == 0) {
        return XK_Super_L;
    }
    if (strcmp(name, "/") == 0 || strcmp(name, "slash") == 0) {
        return XK_slash;
    }
    if (strcmp(name, "Enter") == 0 || strcmp(name, "Return") == 0) {
        return XK_Return;
    }
    if (strcmp(name, "Esc") == 0 || strcmp(name, "Escape") == 0) {
        return XK_Escape;
    }
    if (strcmp(name, "Backspace") == 0 || strcmp(name, "BackSpace") == 0) {
        return XK_BackSpace;
    }
    if (strcmp(name, "Left") == 0) {
        return XK_Left;
    }
    if (strcmp(name, "Right") == 0) {
        return XK_Right;
    }
    if (strcmp(name, "Up") == 0) {
        return XK_Up;
    }
    if (strcmp(name, "Down") == 0) {
        return XK_Down;
    }
    return XStringToKeysym(name);
}

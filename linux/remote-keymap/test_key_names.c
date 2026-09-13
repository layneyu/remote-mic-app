#include "key_names.h"

#include <assert.h>
#include <X11/keysym.h>

int main(void) {
    assert(key_sym_for_name("Ctrl") == XK_Control_L);
    assert(key_sym_for_name("Control") == XK_Control_L);
    assert(key_sym_for_name("Control_L") == XK_Control_L);
    assert(key_sym_for_name("Right Ctrl") == XK_Control_R);
    assert(key_sym_for_name("Control_R") == XK_Control_R);
    assert(key_sym_for_name("Ctrl_R") == XK_Control_R);
    assert(key_sym_for_name("Backspace") == XK_BackSpace);
    assert(key_sym_for_name("BackSpace") == XK_BackSpace);
    return 0;
}

#include <assert.h>
#include <stdbool.h>

bool terminal_window_class_allowed(const char *instance, const char *class_name);
unsigned int terminal_paste_submit_delay_ms(void);

int main(void) {
    assert(terminal_window_class_allowed("org.gnome.Terminal", "Gnome-terminal"));
    assert(terminal_window_class_allowed("Alacritty", "Alacritty"));
    assert(terminal_window_class_allowed("kitty", "kitty"));
    assert(!terminal_window_class_allowed("google-chrome", "Google-chrome"));
    assert(!terminal_window_class_allowed("org.gnome.Nautilus", "Nautilus"));
    assert(terminal_paste_submit_delay_ms() >= 250);
    return 0;
}

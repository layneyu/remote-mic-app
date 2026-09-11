#!/bin/sh
set -eu

# Focus the existing top-level ChatGPT window when present. Otherwise launch
# the installed Linux ChatGPT application. The class name is taken from the
# current X11 window, not inferred from the executable name.
if /usr/bin/wmctrl -l -x 2>/dev/null | /usr/bin/awk '$0 ~ /\.Chatgpt([[:space:]]|$)/ { found = 1 } END { exit !found }'; then
    exec /usr/bin/i3-msg '[class="Chatgpt"] focus' >/dev/null
fi

exec /usr/bin/chatgpt

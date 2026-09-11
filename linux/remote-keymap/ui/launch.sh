#!/bin/sh
set -eu

/usr/bin/systemctl --user start xiaomi-remote-keymap-ui.service
exec /usr/bin/xdg-open http://127.0.0.1:8765/

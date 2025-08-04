#!/bin/bash
set -e

systemctl --user stop ptt-listen.service || true
systemctl --user disable ptt-listen.service || true

rm -f "$HOME/.config/systemd/user/ptt-listen.service"
rm -f "$HOME/.local/share/ptt-listen/ptt-listen.py"

echo "🧹 Removed ptt-listen user service"


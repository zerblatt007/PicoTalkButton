#!/bin/bash
set -e

# Constants
TARGET="$HOME/.local/share/ptt-listen"
VENV_DIR="$TARGET/venv"
SYSTEMD_USER_DIR="$HOME/.config/systemd/user"
SERVICE_FILE="$SYSTEMD_USER_DIR/ptt-listen.service"
PYTHON="$VENV_DIR/bin/python"
REQUIRED_PACKAGES=("pyserial")

# Check required commands and offer install hints
for cmd in python3 systemctl pactl; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "❌ Error: '$cmd' is required but not installed."
    case "$cmd" in
      pactl)
        echo "   → Try installing it with: sudo apt install pulseaudio-utils"
        ;;
      *)
        echo "   → Please install '$cmd' using your system's package manager."
        ;;
    esac
    exit 1
  fi
done


# Create directories
mkdir -p "$TARGET" "$SYSTEMD_USER_DIR"

# Set up virtual environment
if [ ! -d "$VENV_DIR" ]; then
  echo "Creating virtual environment at $VENV_DIR"
  python3 -m venv "$VENV_DIR"
fi

# Upgrade pip and install required packages
"$PYTHON" -m pip install --upgrade pip
for pkg in "${REQUIRED_PACKAGES[@]}"; do
  "$PYTHON" -m pip install "$pkg"
done

# Install script and service
cp "$(dirname "$0")/ptt-listen.py" "$TARGET/"
cp "$(dirname "$0")/ptt-listen.service" "$SERVICE_FILE"
chmod +x "$TARGET/ptt-listen.py"

# Patch ExecStart in systemd unit
sed -i "s|^ExecStart=.*|ExecStart=$PYTHON $TARGET/ptt-listen.py|" "$SERVICE_FILE"

# Enable and start service
systemctl --user daemon-reload
systemctl --user enable --now ptt-listen.service

echo "✅ Installed and running ptt-listen.service using venv: $PYTHON"


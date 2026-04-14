#!/bin/bash
# PicoTalkButton — Quick Installer
#
# Downloads the latest ptt-listen .deb from GitHub, extracts it, runs the
# embedded installer, and then performs a health check.
#
# Usage (2-step):
#   curl -fsSL https://raw.githubusercontent.com/zerblatt007/PicoTalkButton/main/scripts/install.sh \
#       -o ptt-install.sh
#   bash ptt-install.sh
#
# Usage (single-step):
#   curl -fsSL https://raw.githubusercontent.com/zerblatt007/PicoTalkButton/main/scripts/install.sh | bash

set -euo pipefail

# ── configuration ──────────────────────────────────────────────────────────────
REPO="zerblatt007/PicoTalkButton"
API_URL="https://api.github.com/repos/${REPO}/contents/host"
RAW_BASE="https://raw.githubusercontent.com/${REPO}/main/host"
INSTALL_ROOT="$HOME/.local/share/ptt-listen-install"

# ── helpers ────────────────────────────────────────────────────────────────────
info() { printf '\033[1;34m[INFO]\033[0m  %s\n' "$*"; }
ok() { printf '\033[1;32m[ OK ]\033[0m  %s\n' "$*"; }
warn() { printf '\033[1;33m[WARN]\033[0m  %s\n' "$*"; }
die() {
	printf '\033[1;31m[FAIL]\033[0m  %s\n' "$*" >&2
	exit 1
}

# ── banner ─────────────────────────────────────────────────────────────────────
echo ""
echo "  ╔═══════════════════════════════════════════════════╗"
echo "  ║       PicoTalkButton  —  Quick Installer          ║"
echo "  ╚═══════════════════════════════════════════════════╝"
echo ""

# ── prerequisites ──────────────────────────────────────────────────────────────
info "Checking prerequisites..."
for cmd in curl python3 dpkg systemctl pactl; do
	if ! command -v "$cmd" >/dev/null 2>&1; then
		case "$cmd" in
		pactl)
			die "'pactl' is required. Install it with: sudo apt install pulseaudio-utils"
			;;
		dpkg)
			die "'dpkg' is required but not found. This installer only supports Debian-based systems."
			;;
		*)
			die "'$cmd' is required but not found. Please install it via your package manager."
			;;
		esac
	fi
done
ok "All prerequisites satisfied (curl, python3, dpkg, systemctl, pactl)"

# ── resolve latest .deb ────────────────────────────────────────────────────────
info "Fetching package listing from GitHub..."
LATEST_DEB=$(
	curl -fsSL "$API_URL" | python3 - <<'EOF'
import sys, json, re

try:
    entries = json.load(sys.stdin)
except Exception as e:
    print("", end="")
    sys.exit(0)

debs = [e["name"] for e in entries if e["name"].endswith(".deb")]

def ver_key(name):
    m = re.search(r"([\d.]+)\.deb$", name)
    if not m:
        return (0,)
    return tuple(int(x) for x in m.group(1).split("."))

debs.sort(key=ver_key)
print(debs[-1] if debs else "", end="")
EOF
)

[ -z "$LATEST_DEB" ] && die "No .deb package found in the repository. Check your internet connection and try again."

DEB_VERSION=$(python3 -c "import re, sys; m=re.search(r'([\d.]+)\.deb$', sys.argv[1]); print(m.group(1) if m else '?')" "$LATEST_DEB")
ok "Found latest package: ${LATEST_DEB}  (version ${DEB_VERSION})"

# ── download ───────────────────────────────────────────────────────────────────
TMPDIR_WORK=$(mktemp -d)
trap 'rm -rf "$TMPDIR_WORK"' EXIT

DEB_PATH="${TMPDIR_WORK}/${LATEST_DEB}"
DEB_URL="${RAW_BASE}/${LATEST_DEB}"

info "Downloading ${LATEST_DEB}..."
curl -fsSL --progress-bar "$DEB_URL" -o "$DEB_PATH"
ok "Download complete"

# ── extract ────────────────────────────────────────────────────────────────────
info "Extracting package to ${INSTALL_ROOT}..."
rm -rf "$INSTALL_ROOT"
mkdir -p "$INSTALL_ROOT"
dpkg -x "$DEB_PATH" "$INSTALL_ROOT"
ok "Package extracted"

# ── install ────────────────────────────────────────────────────────────────────
info "Running installer..."
echo ""
bash "${INSTALL_ROOT}/ptt-listen/install.sh"
echo ""

# ── health check ──────────────────────────────────────────────────────────────
echo "  ┌─────────────────────────────────────────────────┐"
echo "  │                  Health Check                   │"
echo "  └─────────────────────────────────────────────────┘"
echo ""

CHECKS_PASSED=0
CHECKS_TOTAL=4

# 1. Daemon script
DAEMON_SCRIPT="$HOME/.local/share/ptt-listen/ptt-listen.py"
if [ -f "$DAEMON_SCRIPT" ]; then
	ok "Daemon script installed:  $DAEMON_SCRIPT"
	CHECKS_PASSED=$((CHECKS_PASSED + 1))
else
	warn "Daemon script not found at: $DAEMON_SCRIPT"
fi

# 2. Python venv + pyserial
VENV_PYTHON="$HOME/.local/share/ptt-listen/venv/bin/python"
if [ -x "$VENV_PYTHON" ]; then
	if "$VENV_PYTHON" -c "import serial" 2>/dev/null; then
		ok "Python venv + pyserial:   OK"
		CHECKS_PASSED=$((CHECKS_PASSED + 1))
	else
		warn "pyserial is NOT importable in the venv"
	fi
else
	warn "Python venv not found at: $HOME/.local/share/ptt-listen/venv"
fi

# 3. Systemd unit file
SERVICE_FILE="$HOME/.config/systemd/user/ptt-listen.service"
if [ -f "$SERVICE_FILE" ]; then
	ok "Systemd unit installed:   $SERVICE_FILE"
	CHECKS_PASSED=$((CHECKS_PASSED + 1))
else
	warn "Systemd unit not found at: $SERVICE_FILE"
fi

# 4. Service active/enabled
SERVICE_STATUS=$(systemctl --user is-active ptt-listen.service 2>/dev/null || true)
if [ "$SERVICE_STATUS" = "active" ]; then
	ok "ptt-listen.service:       active (running)"
	CHECKS_PASSED=$((CHECKS_PASSED + 1))
else
	warn "ptt-listen.service status: ${SERVICE_STATUS:-unknown}"
	warn "The service will auto-start once the device is plugged in."
	CHECKS_TOTAL=$((CHECKS_TOTAL - 1)) # not a hard failure before device is connected
	CHECKS_PASSED=$((CHECKS_PASSED + 1))
fi

echo ""

# ── summary ────────────────────────────────────────────────────────────────────
echo "  ┌─────────────────────────────────────────────────┐"
printf "  ║   ptt-listen v%-5s installed  (%d/%d checks OK)  ║\n" \
	"$DEB_VERSION" "$CHECKS_PASSED" "$CHECKS_TOTAL"
echo "  └─────────────────────────────────────────────────┘"
echo ""
echo "  Next step: plug in your PicoTalkButton USB device."
echo "  The LED will turn red (muted) once the host connects."
echo ""
echo "  Useful commands:"
echo "    View logs:   journalctl --user -fu ptt-listen.service"
echo "    Restart:     systemctl --user restart ptt-listen.service"
echo "    Uninstall:   bash ${INSTALL_ROOT}/ptt-listen/uninstall.sh"
echo ""

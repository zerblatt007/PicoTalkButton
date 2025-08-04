#!/usr/bin/env bash
set -euo pipefail

LOGTAG="[PTT]"
VENDOR_NAME="Storvik IT"
PRODUCT_NAME="PicoTalkButton"

log() {
    echo "$(date '+%F %T') $LOGTAG $*" >&2
}

find_device() {
    for dev in /dev/ttyACM*; do
        [ -e "$dev" ] || continue

        # use udevadm to get properties
        info=$(udevadm info -a -n "$dev")

        manufacturer=$(echo "$info" | grep -m1 '{manufacturer}' | awk -F'==' '{print $2}' | tr -d '\"')
        product=$(echo "$info" | grep -m1 '{product}' | awk -F'==' '{print $2}' | tr -d '\"')

        if [[ "$manufacturer" == "${VENDOR_NAME}" && "$product" == "${PRODUCT_NAME}" ]]; then
            echo "$dev"
            return 0
        fi
    done
    return 1
}

while true; do
    pactl set-source-mute @DEFAULT_SOURCE@ 0
    log "Device missing -> unmute for safety"
    log "Waiting for ${VENDOR_NAME} ${PRODUCT_NAME} to appear..."
    while ! DEVICE=$(find_device); do
        sleep 1
    done
    log "Device found at $DEVICE. Muting mic until pressed."
    pactl set-source-mute @DEFAULT_SOURCE@ 1

    # Open device
    exec 3< "$DEVICE"

    while true; do
        if ! read -r line <&3; then
            # read failed (EOF / device unplugged)
            log "Device disconnected -> unmute for safety"
            pactl set-source-mute @DEFAULT_SOURCE@ 0
            break
        fi

        case "$line" in
            *"PTT pressed"*)
                log "PTT pressed -> unmute"
                pactl set-source-mute @DEFAULT_SOURCE@ 0
                ;;
            *"PTT released"*)
                log "PTT released -> mute"
                pactl set-source-mute @DEFAULT_SOURCE@ 1
                ;;
            *"Device disabled"*)
                log "Device disabled -> unmute for safety"
                pactl set-source-mute @DEFAULT_SOURCE@ 0
                ;;
            *"Device enabled"*)
                log "Device enabled -> mute until pressed"
                pactl set-source-mute @DEFAULT_SOURCE@ 1
                ;;
            "")
                # Empty line, ignore
                ;;
            *)
                log "Unknown message: $line"
                ;;
        esac
    done

    exec 3<&-  # Close fd
done

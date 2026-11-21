#!/bin/sh
# Run ON the S5, inside the running Sxmo/sway session (or with SWAYSOCK set):
#   doas apk add jq && sh detect-klte-profile.sh
# Prints the values to paste into sxmo_deviceprofile_samsung,klte.sh.

echo "== Device name (profile must be sxmo_deviceprofile_<this>.sh) =="
tr '\0' '\n' < /proc/device-tree/compatible | head -n1

echo
echo "== Existing device profiles / hooks (check before installing) =="
ls /usr/bin/sxmo_deviceprofile_* 2>/dev/null | grep -i klte || echo "no klte profile installed"
ls -l /usr/share/sxmo/default_hooks 2>/dev/null | grep -i klte || echo "no klte default_hooks link"

if command -v swaymsg >/dev/null 2>&1 && command -v jq >/dev/null 2>&1; then
	echo
	echo "== Outputs (SXMO_MONITOR) =="
	swaymsg -t get_outputs --raw | jq -r '.[] | "\(.name)  \(.current_mode.width)x\(.current_mode.height)"'

	echo
	echo "== Keyboard-type inputs (pick SXMO_POWER_BUTTON / SXMO_VOLUME_BUTTON) =="
	swaymsg -t get_inputs --raw | jq -r '.[] | select(.type=="keyboard") | "\(.identifier)    (\(.name))"'

	echo
	echo "== Touch inputs =="
	swaymsg -t get_inputs --raw | jq -r '.[] | select(.type=="touch") | "\(.identifier)    (\(.name))"'
else
	echo "swaymsg or jq missing: run inside the sway session after 'doas apk add jq'"
fi

echo
echo "== Touchscreen symlink =="
ls -l /dev/input/touchscreen 2>/dev/null || echo "MISSING: lisgd needs SXMO_LISGD_INPUT_DEVICE pointed at the right /dev/input/eventN"

echo
echo "== LEDs (SXMO_LED_*_TYPE is the part after the colon) =="
ls /sys/class/leds 2>/dev/null || echo "none"

echo
echo "== Vibrator (SXMO_VIBRATE_DEV) =="
ls /dev/input/by-path/ 2>/dev/null | grep -i vib || echo "none found"

echo
echo "== Wi-Fi module (SXMO_WIFI_MODULE) =="
basename "$(readlink -f /sys/class/net/wlan0/device/driver/module 2>/dev/null)" 2>/dev/null || echo "wlan0 not found"

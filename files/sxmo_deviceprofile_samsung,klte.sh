#!/bin/sh
# Sxmo device profile: Samsung Galaxy S5 (samsung,klte), mainline msm8974 kernel
# Sxmo sources this file based on the first entry in /proc/device-tree/compatible.
# Values detected on the device 2026-11-16 (G900V running the klte port).

# --- Display ---
export SXMO_MONITOR="DSI-1"            # 1080x1920
export SXMO_SWAY_SCALE="2.5"           # ~432 PPI; try 3 if the UI is too small
export SXMO_MIN_BRIGHTNESS="10"        # AMOLED panel gets very dim at the default of 5

# --- Input ---
export SXMO_POWER_BUTTON="0:0:pm8941_pwrkey"
export SXMO_VOLUME_BUTTON="1:1:gpio-keys"  # vol up/down (and home) share one gpio-keys device
export SXMO_LISGD_INPUT_DEVICE="/dev/input/touchscreen"  # created by 90-klte-touchscreen.rules
export SXMO_LISGD_THRESHOLD="125"      # raise if swipes trigger too easily
export SXMO_LISGD_THRESHOLD_PRESSED="60"

# --- Wi-Fi ---
export SXMO_WIFI_MODULE="brcmfmac"     # BCM4354

# --- LEDs ---
# red:status / green:status / blue:status exist, matching Sxmo's defaults.
export SXMO_DISABLE_LEDS="0"
export SXMO_LED_RED_TYPE="status"
export SXMO_LED_GREEN_TYPE="status"
export SXMO_LED_BLUE_TYPE="status"

# --- Vibration ---
# No vibrator input device on this kernel, so SXMO_VIBRATE_DEV is left unset.
# Haptic feedback will silently do nothing.

# --- Modem ---
# Cellular is broken on this port; nothing to set.

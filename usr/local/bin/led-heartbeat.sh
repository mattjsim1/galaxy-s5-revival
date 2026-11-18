#!/bin/sh
# Blink the Galaxy S5's green status LED once every 3 seconds as a "the system is alive" heartbeat.
# The postmarketOS kernel for klte has no `timer` or `heartbeat` LED trigger, so this loop does it by hand.
# Recovered from my chat history; started at boot from /etc/local.d/led.start (OpenRC "local" service).
L=/sys/class/leds/green:status
echo none > "$L/trigger"
max=$(cat "$L/max_brightness")
while :; do
	echo "$max" > "$L/brightness"; sleep 0.2
	echo 0 > "$L/brightness"; sleep 2.8
done

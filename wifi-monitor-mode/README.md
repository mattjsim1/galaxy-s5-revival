# Wi-Fi monitor mode on the Galaxy S5

The S5's Wi-Fi chip is a Broadcom BCM4354. I wanted it to do **monitor mode** —
listening to raw 802.11 frames in the air instead of just the traffic on my own
connection — the way you would with Wireshark or Kismet on a laptop. Getting there
turned into the deepest part of this whole project, so it gets its own writeup.

Everything here was done on my own phone, against my own network. Captures of other
people's traffic aren't in this repo and shouldn't be anywhere public.

## Round one: it half-works out of the box

Stock LineageOS says monitor mode isn't supported — `iw list` shows only managed, AP
and P2P. But that's the *driver's* opinion. The chip firmware still understands the
older Broadcom `WLC_SET_MONITOR` ioctl, and if you poke it directly, raw frames start
coming up the interface.

I wrote two small tools for this (both in this folder, built with
`arm-linux-gnueabihf-gcc`):

- **`wlmon.c`** — flips monitor mode on and off through the ioctl.
- **`wlcap.c`** — turns monitor mode on, reads frames off an `AF_PACKET` socket, and
  writes a standard `.pcap` Wireshark can open. It turns monitor mode back off when it
  exits, even on Ctrl-C.

That was enough to capture beacons, probes, ACKs and data frames. One honest limitation
I never beat: in monitor mode the chip won't actually retune to a channel you ask for —
it stays on whatever channel the phone is associated with — so you only see one channel
at a time.

## Why there were no radiotap headers

The captures worked, but every frame came in "naked" — no signal strength, no channel,
no data rate. That metadata lives in a small **radiotap** header that the radio is
supposed to prepend to each frame, and this chip wasn't adding it.

The reason is that the BCM4354 is a **FullMAC** design: the 802.11 stack runs on the
chip's own firmware, and in its monitor path the firmware hands the frame up but throws
the radio metadata away. The driver can't add back what it never received. The known fix
is a research project called **Nexmon** that patches the firmware itself — but Nexmon
has never supported the BCM4354.

## Round two: porting Nexmon to the BCM4354

So I ported it. The closest chip Nexmon does support is the BCM4356 (Nexus 6), which is
a sibling of the 4354 — same firmware family, but a genuinely different memory layout, so
none of its addresses carried over. Every address had to be recovered from my phone's own
firmware. The full address-by-address log is in
[`nexmon-bcm4354-port.md`](nexmon-bcm4354-port.md); the short version:

- **Dumped the chip's ROM** off the running device through the driver's `membytes`
  debug path — no firmware change needed to read it.
- **Disassembled** the firmware and found the functions the patch has to hook
  (`wl_monitor` / `wlc_monitor`), both of which live in ROM on this build.
- Because they're in ROM, the hook can't just overwrite them. I used the firmware's own
  **flashpatch** mechanism (the table it uses to patch its ROM at boot), which meant
  relocating that table to make room for a new entry and rewriting the pointers to it.
- Built the patched firmware with Nexmon's toolchain plus a small `fp_relocate.py`
  helper that rewrites the flashpatch table, then loaded it.

The patch adds a radiotap monitor mode: `WLC_SET_MONITOR` value `2` now returns frames
*with* a radiotap header. (`wlcap -m 2` selects it.)

## Result

With the patched firmware installed, a capture on channel 6 produced frames with a full
radiotap header — per-frame data rate, frequency, and signal/noise in dBm:

```
link-type IEEE802_11_RADIO (802.11 plus radiotap header)
   6.0 Mb/s  2437 MHz  -79dBm signal  -92dBm noise
   1.0 Mb/s  2437 MHz  -41dBm signal  -92dBm noise
```

Normal Wi-Fi still works — the phone associates and gets online as usual; monitor mode is
just a mode you switch into. The firmware is swapped in a fully reversible way, with the
stock blob kept as a backup.

## Honest limits

- Channel selection still doesn't retune in monitor mode (same as stock) — one channel at
  a time.
- Frames that fail the checksum are passed through without the radiotap "bad FCS" flag
  set, so a strict reader can't tell which ones are corrupt.
- This is a patch for one exact firmware build (7.26.28.8); it isn't a general 4354 patch.

## Files

- [`wlmon.c`](wlmon.c) — toggle monitor mode via ioctl.
- [`wlcap.c`](wlcap.c) — capture raw 802.11 (or radiotap, with `-m 2`) to a pcap.
- [`nexmon-bcm4354-port.md`](nexmon-bcm4354-port.md) — the firmware reverse-engineering
  notes: every address, how it was found, and how the ROM hook works.

The firmware patch sources themselves live on the `bcm4354-port` branch of my Nexmon
checkout (source only; the firmware binaries aren't redistributed here).

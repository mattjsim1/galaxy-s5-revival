# BCM4354A1 (Galaxy S5 klte) port — firmware 7.26.28.8 sta

Firmware string: `4354a1-roml/sdio-ag-p2p-pno-aoe-sr-mchan-...` Version 7.26.28.8,
FWID 01-f600cc71, ucode 922.132. The bus is SDIO; the closest sibling (4356, Nexus 6) uses PCIe.
This build is ROM-heavy, so many functions that sit in RAM on the 4356 (including
`wlc_monitor` and `wl_monitor`) are in ROM here.

Status (2026-11-19): monitor mode with radiotap works. Tested with WLC_SET_MONITOR=2 on ch 6:
1561 frames, all with radiotap (TSFT, flags=FCS, rate, 2437 MHz/2GHz, dBm signal and noise).
1523 of them pass the FCS check.

## How each address was found

| What | Value | How |
|---|---|---|
| ROM | 0x0–0xA0000 | Dumped from the device with the bcmdhd `membytes` bus iovar (`~/s5-files/rfmon/dhdmem.c`), no firmware change needed. Reads past 0xA0000 fail with EINVAL. |
| reclaim table | fn 0x182784 | Built for `hndrte_reclaim` (strings "reclaim section 0/1"). |
| `HNDRTE_RECLAIM_0_END(_PTR)` | 0x209C34 @ 0x1827F0 | Last entry of that table. |
| heap start literal | 0x209C34 @ 0x1DB6CC | `hndrte_init`: `ldr r0,=_end; bl arena_init(0x1DB3A4)`. |
| `UCODESTART/SIZE` | 0x1FBAF4 / 0xD390 | The size word at 0x1FBAF0 comes right before the ucode, which starts with the same `0x0300104E 0x0001BC60` words as the 4356 ucode. |
| `TEMPLATERAMSTART(_PTR)/SIZE` | 0x208E84 @ 0x1FB9E8 / 0xDB0 | Getter function at 0x1FB9B4 (`mov.w r3,#0xDB0`). It ends exactly at the reclaim end. |
| `WLC_UCODE_WRITE_BL_HOOK_ADDR` | 0x1E5008 | `wlc_ucode_download` at 0x1E4FF8 calls `bl wlc_ucode_write` (ROM 0x53994) there. Not used by this port. |
| FP config table | 0x181800–0x18207C | 181 × {rom_addr, size, data}. A 0 terminator follows, then the exception vectors at 0x182080. |
| FP data slots | 0x181000–0x181800 | 8 B each, 0x1815A8 used. The data-end variable is at 0x1D9640. |
| FP table literals | 0x1DBA30/34, 0x1DBCB4/B8, 0x1DBA84/88, 0x1DBB9C/A0 | verify fn (same shape as 4356 PTR_1), program fn 0x1DBBB0 (= 4356 PTR_2), stats printf fn, reclaim fn 0x1DBB2C. |
| `wlc_monitor` | ROM 0x26F9C | Matched against the 4356 code that builds `wl_rxsts`. |
| `bl wl_monitor` site | ROM 0x273EA | Calls `wl_monitor` at ROM 0x20214. |
| `wl_monitor` | ROM 0x20214 | Same shape as 4356 0x199112. |
| sendup used by wl_monitor | ROM tramp 0x91D60 → `*(0x180CE0)` = 0x18956E | Called as `(wl, 0, p, 1)`. |
| pktget | ROM tramp 0x91080 → `*(0x1809A8)` = 0x185A78 | |
| memcpy / memset / printf | 0x11E20 / 0x11EC8 / 0x11F14 | |
| `wlc->monitor` | wlc+0x208 | Found by diffing live RAM after setting monitor to 0/2/1/0. Only 0x1FF698 tracked the value; 247 pointers lead to wlc=0x1FF490. The value is stored raw. |
| wl/wlc/pub offsets | wl->wlc +8, wlc->osh +4, pub->osh +0x14 | Checked in live RAM. |

Boot order (c_main tail at 0x184C06): read trailer → program flashpatches (0x1DBBB0) → hndrte_init/heap (0x1DC848) → free the FP table (0x1DBB2C) → wl init → reclaim.

## Port design (differs from the 4356 port)

* **No ucode compression.** The patch code goes after the image trailer at PATCHSTART=0x209E00
  (0x1000 bytes). The relocated FP config table goes at 0x20AE00 (0xC00 bytes). The heap-start
  literal at 0x1DB6CC moves from 0x209C34 to 0x20BA00, which costs about 7.6 KB of heap.
* **The hook is a ROM flashpatch.** `fp_relocate.py` copies the 181 original FP entries to
  0x20AE00. It appends `{0x273EA, 4, slot}`, writes the BL into the free data slot 0x1815A8,
  and rewrites all 4 base/end literal pairs plus the data-end variable. The firmware's own
  loader merges the BL with the surrounding ROM bytes into an 8-byte block. On the device
  that block was verified to read `mov r2,r6 | bl hook | add sp,#120`.
* Monitor value: 1 = stock raw 802.11, 2 = radiotap, 3 = log only, 4 = drop.

## Testing without flashing
Bind-mount the build over `/system/vendor/etc/wifi/bcmdhd_sta.bin` (set the
`u:object_r:vendor_configs_file:s0` label first), then toggle Wi-Fi. To capture, run
`wlcap -m 2 -t 10 -w x.pcap`. To restore stock, run `umount` and toggle Wi-Fi again.
Only use adb over USB for this.

## Known limits / follow-ups
* Frames with a bad FCS are passed through without the radiotap BADFCS flag.
  `sts->pkterror` could set it.
* Changing channel in monitor mode still doesn't retune (same as stock).
* The `rom_extraction/` and `include/bcmpcie.h` files copied over from the 4356 are unused.

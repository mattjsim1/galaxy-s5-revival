/* wlmon: get/set Broadcom WLC_*_MONITOR on a bcmdhd interface via SIOCDEVPRIVATE.
 * usage: wlmon wlan0 [0|1]   (no value = read current setting)
 * Build: arm-linux-gnueabihf-gcc -static -o wlmon wlmon.c */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>

#define WLC_GET_MONITOR 107
#define WLC_SET_MONITOR 108

typedef struct { unsigned cmd; void *buf; unsigned len; unsigned char set; unsigned used, needed; } wl_ioctl_t;

static int wl(int s, const char *ifn, unsigned cmd, int *val, int set) {
	wl_ioctl_t ioc = { cmd, val, sizeof(int), set, 0, 0 };
	struct ifreq ifr; memset(&ifr, 0, sizeof ifr);
	strncpy(ifr.ifr_name, ifn, IFNAMSIZ - 1);
	ifr.ifr_data = (void *)&ioc;
	return ioctl(s, SIOCDEVPRIVATE, &ifr);
}

int main(int argc, char **argv) {
	if (argc < 2) { fprintf(stderr, "usage: %s ifname [0|1]\n", argv[0]); return 2; }
	int s = socket(AF_INET, SOCK_DGRAM, 0), v = 0;
	if (argc > 2) {
		v = atoi(argv[2]);
		if (wl(s, argv[1], WLC_SET_MONITOR, &v, 1) < 0) { printf("set monitor %d: error %d (%s)\n", v, errno, strerror(errno)); return 1; }
		printf("set monitor %d: ok\n", v);
	}
	if (wl(s, argv[1], WLC_GET_MONITOR, &v, 0) < 0) { printf("get monitor: error %d (%s)\n", errno, strerror(errno)); return 1; }
	printf("monitor = %d\n", v);
	return 0;
}

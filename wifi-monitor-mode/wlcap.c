/* wlcap: capture raw 802.11 frames from a bcmdhd interface into a pcap file.
 * Turns on Broadcom monitor mode (WLC_SET_MONITOR via SIOCDEVPRIVATE), reads frames
 * from an AF_PACKET socket, writes pcap link type 105 (IEEE802_11, no radiotap),
 * and turns monitor mode back off on exit (also on Ctrl-C, SIGTERM and timeout).
 * usage: wlcap [-i wlan0] [-c channel] [-d] [-m mode] [-t seconds] [-n packets] -w out.pcap
 * -m 2 selects the Nexmon radiotap monitor mode (patched firmware) and writes link type 127.
 * -d disassociates from the current access point first (needed to change channel).
 * Needs root. Build: arm-linux-gnueabihf-gcc -static -O2 -o wlcap wlcap.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <poll.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>

#define WLC_SET_CHANNEL 30
#define WLC_DISASSOC    52
#define WLC_GET_MONITOR 107
#define WLC_SET_MONITOR 108
#define DLT_IEEE802_11  105
#define DLT_IEEE802_11_RADIO 127

typedef struct { unsigned cmd; void *buf; unsigned len; unsigned char set; unsigned used, needed; } wl_ioctl_t;

static const char *ifn = "wlan0";
static volatile sig_atomic_t stop;

static int wl(unsigned cmd, int *val, int set) {
	int s = socket(AF_INET, SOCK_DGRAM, 0), r;
	wl_ioctl_t ioc = { cmd, val, sizeof(int), set, 0, 0 };
	struct ifreq ifr; memset(&ifr, 0, sizeof ifr);
	strncpy(ifr.ifr_name, ifn, IFNAMSIZ - 1);
	ifr.ifr_data = (void *)&ioc;
	r = ioctl(s, SIOCDEVPRIVATE, &ifr);
	close(s);
	return r;
}

static void on_signal(int sig) { (void)sig; stop = 1; }
static void monitor_off(void) { int v = 0; wl(WLC_SET_MONITOR, &v, 1); }

struct pcap_hdr { uint32_t magic; uint16_t vmaj, vmin; int32_t tz; uint32_t sig, snaplen, linktype; };
struct rec_hdr { uint32_t sec, usec, incl, orig; };

int main(int argc, char **argv) {
	const char *out = NULL; int chan = 0, secs = 0, disassoc = 0, mode = 1; long maxpk = 0, n = 0, bytes = 0, opt;
	while ((opt = getopt(argc, argv, "i:c:dm:t:n:w:")) != -1) switch (opt) {
		case 'i': ifn = optarg; break;
		case 'c': chan = atoi(optarg); break;
		case 'd': disassoc = 1; break;
		case 'm': mode = atoi(optarg); break;
		case 't': secs = atoi(optarg); break;
		case 'n': maxpk = atol(optarg); break;
		case 'w': out = optarg; break;
		default: goto usage;
	}
	if (!out) { usage: fprintf(stderr, "usage: %s [-i ifname] [-c channel] [-d] [-m mode] [-t seconds] [-n packets] -w out.pcap\n", argv[0]); return 2; }

	FILE *f = fopen(out, "wb");
	if (!f) { perror(out); return 1; }
	struct pcap_hdr gh = { 0xa1b2c3d4, 2, 4, 0, 0, 65535, mode == 2 ? DLT_IEEE802_11_RADIO : DLT_IEEE802_11 };
	fwrite(&gh, sizeof gh, 1, f);

	if (disassoc) { int zero[3] = {0,0,0}; if (wl(WLC_DISASSOC, zero, 1) < 0) fprintf(stderr, "warning: disassociate failed: %s\n", strerror(errno)); }
	if (wl(WLC_SET_MONITOR, &mode, 1) < 0) { fprintf(stderr, "cannot enable monitor mode: %s\n", strerror(errno)); fclose(f); return 1; }
	atexit(monitor_off);
	signal(SIGINT, on_signal); signal(SIGTERM, on_signal); signal(SIGHUP, on_signal);
	if (chan && wl(WLC_SET_CHANNEL, &chan, 1) < 0)
		fprintf(stderr, "warning: set channel %d failed: %s (capturing on the current channel)\n", chan, strerror(errno));

	int s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (s < 0) { perror("socket"); return 1; }
	struct ifreq ifr; memset(&ifr, 0, sizeof ifr); strncpy(ifr.ifr_name, ifn, IFNAMSIZ - 1);
	if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) { perror("ifindex"); return 1; }
	struct sockaddr_ll sll = { .sll_family = AF_PACKET, .sll_protocol = htons(ETH_P_ALL), .sll_ifindex = ifr.ifr_ifindex };
	if (bind(s, (struct sockaddr *)&sll, sizeof sll) < 0) { perror("bind"); return 1; }

	time_t end = secs ? time(NULL) + secs : 0;
	static unsigned char buf[65536];
	while (!stop && (!maxpk || n < maxpk) && (!end || time(NULL) < end)) {
		struct pollfd p = { s, POLLIN, 0 };
		int r = poll(&p, 1, 500);
		if (r <= 0) continue;
		ssize_t len = recv(s, buf, sizeof buf, 0);
		if (len <= 0) continue;
		struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
		struct rec_hdr rh = { (uint32_t)ts.tv_sec, (uint32_t)(ts.tv_nsec / 1000), (uint32_t)len, (uint32_t)len };
		fwrite(&rh, sizeof rh, 1, f); fwrite(buf, 1, len, f);
		n++; bytes += len;
	}
	fclose(f);
	fprintf(stderr, "wrote %ld packets (%ld bytes) to %s; monitor mode off\n", n, bytes, out);
	return 0;
}

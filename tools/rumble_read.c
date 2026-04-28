/*
 * rumble_read.c — Simple packet reader for the rumble driver
 *
 * Team: PathFinders
 *
 * Build:
 *   cd tools && make rumble_read
 *
 * Run:
 *   sudo ./rumble_read
 *
 * Usage:
 *   - Opens /dev/rumble0 and continuously reads struct rumble_input packets
 *   - Pretty-prints each packet to stdout
 *   - Press Enter to fire a test rumble (50% both motors, 500ms)
 *   - Press Ctrl+C to exit
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <time.h>

#include "rumble.h"   /* struct rumble_input, struct rumble_motors, RUMBLE_SET_MOTORS */

/* -------------------------------------------------------------------------
 * Signal handling — Ctrl+C sets this flag so we exit the main loop cleanly.
 * ------------------------------------------------------------------------- */
static volatile sig_atomic_t g_stop = 0;

static void sig_handler(int sig)
{
	(void)sig;
	g_stop = 1;
}

/* -------------------------------------------------------------------------
 * Helper: fire rumble at a given intensity for a given duration, then off.
 * ------------------------------------------------------------------------- */
static void do_rumble(int fd, uint8_t left_pct, uint8_t right_pct,
		      unsigned int ms)
{
	struct rumble_motors m;
	struct timespec ts;

	m.left  = left_pct;
	m.right = right_pct;
	if (ioctl(fd, RUMBLE_SET_MOTORS, &m) < 0) {
		perror("ioctl RUMBLE_SET_MOTORS (on)");
		return;
	}

	/* Sleep for requested duration */
	ts.tv_sec  = ms / 1000U;
	ts.tv_nsec = (long)(ms % 1000U) * 1000000L;
	nanosleep(&ts, NULL);

	m.left  = 0;
	m.right = 0;
	if (ioctl(fd, RUMBLE_SET_MOTORS, &m) < 0)
		perror("ioctl RUMBLE_SET_MOTORS (off)");
}

/* -------------------------------------------------------------------------
 * Helper: build a human-readable button string from the bitmask.
 * ------------------------------------------------------------------------- */
static void format_buttons(uint16_t buttons, char *buf, size_t len)
{
	static const struct { uint16_t bit; const char *name; } map[] = {
		{ BTN_A,          "A"      },
		{ BTN_B,          "B"      },
		{ BTN_X,          "X"      },
		{ BTN_Y,          "Y"      },
		{ BTN_LB,         "LB"     },
		{ BTN_RB,         "RB"     },
		{ BTN_MENU,       "MENU"   },
		{ BTN_VIEW,       "VIEW"   },
		{ BTN_LS,         "LS"     },
		{ BTN_RS,         "RS"     },
		{ BTN_DPAD_UP,    "D-UP"   },
		{ BTN_DPAD_DOWN,  "D-DN"   },
		{ BTN_DPAD_LEFT,  "D-LT"   },
		{ BTN_DPAD_RIGHT, "D-RT"   },
	};

	size_t out = 0;
	buf[0] = '\0';

	for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
		if (buttons & map[i].bit) {
			if (out > 0 && out < len - 2) {
				buf[out++] = '+';
				buf[out]   = '\0';
			}
			size_t nlen = strlen(map[i].name);
			if (out + nlen < len - 1) {
				memcpy(buf + out, map[i].name, nlen);
				out += nlen;
				buf[out] = '\0';
			}
		}
	}

	if (out == 0)
		snprintf(buf, len, "(none)");
}

/* -------------------------------------------------------------------------
 * Main
 * ------------------------------------------------------------------------- */
int main(void)
{
	int fd;
	struct sigaction sa;

	/* Install Ctrl+C handler */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_handler;
	sigaction(SIGINT,  &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	/* Open the character device */
	fd = open("/dev/rumble0", O_RDWR);
	if (fd < 0) {
		perror("open /dev/rumble0");
		fprintf(stderr,
			"Hint: is the rumble module loaded?  "
			"Try: sudo insmod driver/rumble.ko\n");
		return EXIT_FAILURE;
	}
	printf("rumble: opened /dev/rumble0\n");
	printf("Press Enter to fire a test rumble.  Ctrl+C to quit.\n\n");

	/*
	 * We need to watch two fds simultaneously:
	 *   fd      — the character device (readable when new input arrives)
	 *   STDIN   — keyboard Enter press triggers rumble
	 *
	 * Use poll() with a short timeout so we can also check g_stop.
	 */
	while (!g_stop) {
		struct pollfd pfds[2];
		int nready;

		pfds[0].fd      = fd;
		pfds[0].events  = POLLIN;
		pfds[0].revents = 0;

		pfds[1].fd      = STDIN_FILENO;
		pfds[1].events  = POLLIN;
		pfds[1].revents = 0;

		nready = poll(pfds, 2, 100 /* ms timeout */);

		if (nready < 0) {
			if (errno == EINTR)
				continue;   /* signal interrupted — loop again */
			perror("poll");
			break;
		}

		/* --- Controller data available --- */
		if (pfds[0].revents & POLLIN) {
			struct rumble_input inp;
			ssize_t n = read(fd, &inp, sizeof(inp));

			if (n < 0) {
				if (errno == ENODEV) {
					printf("\nrumble: controller disconnected\n");
					break;
				}
				if (errno == EAGAIN)
					continue;
				perror("read");
				break;
			}
			if (n != (ssize_t)sizeof(inp)) {
				fprintf(stderr,
					"Unexpected read size %zd (expected %zu)\n",
					n, sizeof(inp));
				continue;
			}

			/* Pretty-print the packet */
			char btn_str[128];
			format_buttons(inp.buttons, btn_str, sizeof(btn_str));

			printf("[%8.3f ms] BTN=%-30s "
			       "LT=%3u RT=%3u  "
			       "LX=%+6d LY=%+6d  "
			       "RX=%+6d RY=%+6d\n",
			       (double)inp.timestamp_us / 1000.0,
			       btn_str,
			       inp.lt, inp.rt,
			       (int)inp.lx, (int)inp.ly,
			       (int)inp.rx, (int)inp.ry);
			fflush(stdout);
		}

		/* --- Enter pressed — fire test rumble --- */
		if (pfds[1].revents & POLLIN) {
			char c;
			if (read(STDIN_FILENO, &c, 1) > 0 && c == '\n') {
				printf(">>> Firing rumble: 50%% / 50%% for 500 ms ...\n");
				do_rumble(fd, 50, 50, 500);
				printf(">>> Rumble done\n");
			}
		}

		/* --- Hang-up / error on device fd --- */
		if (pfds[0].revents & (POLLHUP | POLLERR)) {
			printf("\nrumble: device error or disconnected\n");
			break;
		}
	}

	close(fd);
	printf("rumble: exiting\n");
	return EXIT_SUCCESS;
}

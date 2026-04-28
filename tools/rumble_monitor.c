/*
 * rumble_monitor.c — ncurses TUI for live Xbox controller monitoring
 *
 * Team: PathFinders
 *
 * Build:
 *   cd tools && make rumble_monitor
 *
 * Run:
 *   sudo ./rumble_monitor
 *
 * Controls:
 *   q       — quit
 *   r       — test rumble (50% both motors, 500ms)
 *   arrows  — adjust rumble intensity (for testing)
 *   space   — fire current rumble setting
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <time.h>
#include <ncurses.h>

#include "rumble.h"

#define DEVICE_PATH "/dev/rumble0"
#define POLL_TIMEOUT_MS 50

static int g_fd = -1;
static uint8_t g_rumble_left = 50;
static uint8_t g_rumble_right = 50;

static void cleanup(void)
{
	if (g_fd >= 0) {
		close(g_fd);
		g_fd = -1;
	}
	endwin();
}

static void fire_rumble(uint8_t left, uint8_t right, unsigned int ms)
{
	struct rumble_motors m;
	struct timespec ts;

	if (g_fd < 0)
		return;

	m.left = left;
	m.right = right;
	if (ioctl(g_fd, RUMBLE_SET_MOTORS, &m) < 0)
		return;

	ts.tv_sec = ms / 1000U;
	ts.tv_nsec = (long)(ms % 1000U) * 1000000L;
	nanosleep(&ts, NULL);

	m.left = 0;
	m.right = 0;
	ioctl(g_fd, RUMBLE_SET_MOTORS, &m);
}

static void draw_bar(int y, int x, const char *label, int value, int max_val)
{
	int bar_width = 20;
	int filled = (value * bar_width) / max_val;

	mvprintw(y, x, "%s", label);
	mvaddch(y, x + 10, '[');
	for (int i = 0; i < bar_width; i++) {
		mvaddch(y, x + 11 + i, i < filled ? '=' : ' ');
	}
	mvaddch(y, x + 11 + bar_width, ']');
	mvprintw(y, x + 33, "%3d", value);
}

static void draw_stick(int y, int x, const char *label, int16_t sx, int16_t sy)
{
	int grid_size = 9;
	int center = grid_size / 2;
	int px = center + (sx * center) / 32768;
	int py = center - (sy * center) / 32768;

	if (px < 0) px = 0;
	if (px >= grid_size) px = grid_size - 1;
	if (py < 0) py = 0;
	if (py >= grid_size) py = grid_size - 1;

	mvprintw(y, x, "%s", label);
	for (int row = 0; row < grid_size; row++) {
		mvaddch(y + 1 + row, x, '|');
		for (int col = 0; col < grid_size; col++) {
			char ch = '.';
			if (row == center && col == center)
				ch = '+';
			if (row == py && col == px)
				ch = 'O';
			mvaddch(y + 1 + row, x + 1 + col, ch);
		}
		mvaddch(y + 1 + row, x + 1 + grid_size, '|');
	}
	mvprintw(y + 1 + grid_size, x, "%+6d,%+6d", sx, sy);
}

static void draw_buttons(int y, int x, uint16_t buttons)
{
	mvprintw(y, x, "Buttons:");
	int row = y + 1;
	int col = x;

	if (buttons & RUMBLE_BTN_A) mvprintw(row, col, "[A]"); else mvprintw(row, col, " A ");
	col += 4;
	if (buttons & RUMBLE_BTN_B) mvprintw(row, col, "[B]"); else mvprintw(row, col, " B ");
	col += 4;
	if (buttons & RUMBLE_BTN_X) mvprintw(row, col, "[X]"); else mvprintw(row, col, " X ");
	col += 4;
	if (buttons & RUMBLE_BTN_Y) mvprintw(row, col, "[Y]"); else mvprintw(row, col, " Y ");

	row++;
	col = x;
	if (buttons & RUMBLE_BTN_LB) mvprintw(row, col, "[LB]"); else mvprintw(row, col, " LB ");
	col += 5;
	if (buttons & RUMBLE_BTN_RB) mvprintw(row, col, "[RB]"); else mvprintw(row, col, " RB ");
	col += 5;
	if (buttons & RUMBLE_BTN_LS) mvprintw(row, col, "[LS]"); else mvprintw(row, col, " LS ");
	col += 5;
	if (buttons & RUMBLE_BTN_RS) mvprintw(row, col, "[RS]"); else mvprintw(row, col, " RS ");

	row++;
	col = x;
	if (buttons & RUMBLE_BTN_MENU) mvprintw(row, col, "[MENU]"); else mvprintw(row, col, " MENU ");
	col += 7;
	if (buttons & RUMBLE_BTN_VIEW) mvprintw(row, col, "[VIEW]"); else mvprintw(row, col, " VIEW ");

	row++;
	col = x;
	if (buttons & RUMBLE_BTN_DPAD_UP) mvprintw(row, col, "[UP]"); else mvprintw(row, col, " UP ");
	col += 5;
	if (buttons & RUMBLE_BTN_DPAD_DOWN) mvprintw(row, col, "[DN]"); else mvprintw(row, col, " DN ");
	col += 5;
	if (buttons & RUMBLE_BTN_DPAD_LEFT) mvprintw(row, col, "[LT]"); else mvprintw(row, col, " LT ");
	col += 5;
	if (buttons & RUMBLE_BTN_DPAD_RIGHT) mvprintw(row, col, "[RT]"); else mvprintw(row, col, " RT ");
}

int main(void)
{
	struct pollfd pfd;
	struct rumble_input inp;
	ssize_t n;
	int ch;
	uint64_t pkt_count = 0;
	uint64_t last_ts = 0;
	double hz = 0.0;

	g_fd = open(DEVICE_PATH, O_RDWR | O_NONBLOCK);
	if (g_fd < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", DEVICE_PATH, strerror(errno));
		fprintf(stderr, "Hint: is the rumble module loaded?\n");
		return EXIT_FAILURE;
	}

	initscr();
	cbreak();
	noecho();
	nodelay(stdscr, TRUE);
	keypad(stdscr, TRUE);
	curs_set(0);

	atexit(cleanup);

	memset(&inp, 0, sizeof(inp));

	while (1) {
		pfd.fd = g_fd;
		pfd.events = POLLIN;
		pfd.revents = 0;

		int ret = poll(&pfd, 1, POLL_TIMEOUT_MS);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			break;
		}

		if (pfd.revents & POLLIN) {
			n = read(g_fd, &inp, sizeof(inp));
			if (n == sizeof(inp)) {
				pkt_count++;
				if (last_ts > 0) {
					uint64_t delta = inp.timestamp_us - last_ts;
					if (delta > 0)
						hz = 1000000.0 / (double)delta;
				}
				last_ts = inp.timestamp_us;
			} else if (n < 0 && errno != EAGAIN) {
				break;
			}
		}

		if (pfd.revents & (POLLHUP | POLLERR))
			break;

		ch = getch();
		if (ch == 'q' || ch == 'Q')
			break;
		else if (ch == 'r' || ch == 'R')
			fire_rumble(50, 50, 500);
		else if (ch == ' ')
			fire_rumble(g_rumble_left, g_rumble_right, 300);
		else if (ch == KEY_UP && g_rumble_left < 100)
			g_rumble_left += 10;
		else if (ch == KEY_DOWN && g_rumble_left > 0)
			g_rumble_left -= 10;
		else if (ch == KEY_RIGHT && g_rumble_right < 100)
			g_rumble_right += 10;
		else if (ch == KEY_LEFT && g_rumble_right > 0)
			g_rumble_right -= 10;

		clear();
		mvprintw(0, 0, "=== Rumble Monitor === [q]uit [r]umble [space]test [arrows]adjust");
		mvprintw(1, 0, "Packets: %lu  Rate: %.1f Hz", pkt_count, hz);

		draw_stick(3, 2, "Left Stick", inp.lx, inp.ly);
		draw_stick(3, 22, "Right Stick", inp.rx, inp.ry);

		draw_bar(14, 2, "LT", inp.lt, 255);
		draw_bar(15, 2, "RT", inp.rt, 255);

		draw_buttons(17, 2, inp.buttons);

		mvprintw(22, 2, "Rumble Test: L=%d%% R=%d%%", g_rumble_left, g_rumble_right);

		refresh();
	}

	return EXIT_SUCCESS;
}

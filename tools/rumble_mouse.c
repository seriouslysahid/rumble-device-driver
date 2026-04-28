/*
 * rumble_mouse.c — Controller-to-mouse mapper daemon
 *
 * Reads controller input from /dev/rumble0 and emits virtual mouse events
 * via uinput. Uses proper motion modeling, deadzones, filtering, and
 * acceleration for smooth, responsive cursor control.
 *
 * Architecture:
 *   - epoll-based event loop
 *   - timerfd for fixed 125 Hz updates
 *   - radial deadzones with smooth scaling
 *   - exponential smoothing filter
 *   - velocity-based motion with acceleration
 *   - fractional pixel accumulation
 *
 * Build:
 *   cd tools && make rumble_mouse
 *
 * Run:
 *   sudo ./rumble_mouse
 *
 * Controls:
 *   Left stick  → cursor movement
 *   Right stick → scrolling
 *   LB → left click
 *   RB → right click
 *   LS click → middle click
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <linux/input.h>
#include <linux/uinput.h>

#include "rumble.h"

/* Configuration */
#define DEVICE_PATH "/dev/rumble0"
#define UPDATE_HZ 125
#define UPDATE_INTERVAL_NS (1000000000 / UPDATE_HZ)

#define STICK_MAX 32768.0f
#define DEADZONE_RADIUS 4000.0f
#define DEADZONE_RADIUS_SQ (DEADZONE_RADIUS * DEADZONE_RADIUS)

#define CURSOR_BASE_SPEED 800.0f
#define CURSOR_ACCEL 1.5f
#define PRECISION_THRESHOLD 0.3f
#define PRECISION_SCALE 0.4f

#define SCROLL_DEADZONE 8000.0f
#define SCROLL_SCALE 0.3f

#define FILTER_ALPHA 0.7f

/* State */
struct mapper_state {
	/* Raw input */
	int16_t lx_raw, ly_raw;
	int16_t rx_raw, ry_raw;
	uint16_t buttons;
	uint16_t buttons_prev;

	/* Filtered input */
	float lx_filt, ly_filt;
	float rx_filt, ry_filt;

	/* Accumulated fractional pixels */
	float accum_x, accum_y;
	float accum_sx, accum_sy;

	/* File descriptors */
	int rumble_fd;
	int uinput_fd;
	int timer_fd;
	int epoll_fd;
};

static volatile sig_atomic_t g_running = 1;

static void sig_handler(int sig)
{
	(void)sig;
	g_running = 0;
}

/* Apply radial deadzone with smooth scaling */
static void apply_deadzone(float *x, float *y, float deadzone)
{
	float mag_sq = (*x) * (*x) + (*y) * (*y);
	
	if (mag_sq < deadzone * deadzone) {
		*x = 0.0f;
		*y = 0.0f;
		return;
	}

	float mag = sqrtf(mag_sq);
	float scale = (mag - deadzone) / (STICK_MAX - deadzone);
	
	*x = (*x / mag) * scale * STICK_MAX;
	*y = (*y / mag) * scale * STICK_MAX;
}

/* Exponential moving average filter */
static float ema_filter(float raw, float prev, float alpha)
{
	return alpha * raw + (1.0f - alpha) * prev;
}

/* Compute cursor velocity with acceleration curve */
static float compute_velocity(float stick_norm)
{
	float abs_norm = fabsf(stick_norm);
	float sign = (stick_norm >= 0.0f) ? 1.0f : -1.0f;

	if (abs_norm < PRECISION_THRESHOLD) {
		/* Precision zone: linear, low sensitivity */
		return sign * abs_norm * CURSOR_BASE_SPEED * PRECISION_SCALE;
	} else {
		/* Speed zone: accelerated */
		float accel = 1.0f + CURSOR_ACCEL * abs_norm;
		return sign * abs_norm * CURSOR_BASE_SPEED * accel;
	}
}

/* Emit uinput event */
static void emit(int fd, int type, int code, int val)
{
	struct input_event ev = {0};
	ev.type = type;
	ev.code = code;
	ev.value = val;
	
	if (write(fd, &ev, sizeof(ev)) < 0) {
		perror("write uinput event");
	}
}

/* Setup uinput virtual mouse */
static int setup_uinput(void)
{
	int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if (fd < 0) {
		perror("open /dev/uinput");
		return -1;
	}

	/* Enable event types */
	ioctl(fd, UI_SET_EVBIT, EV_KEY);
	ioctl(fd, UI_SET_EVBIT, EV_REL);
	ioctl(fd, UI_SET_EVBIT, EV_SYN);

	/* Enable relative axes */
	ioctl(fd, UI_SET_RELBIT, REL_X);
	ioctl(fd, UI_SET_RELBIT, REL_Y);
	ioctl(fd, UI_SET_RELBIT, REL_WHEEL);
	ioctl(fd, UI_SET_RELBIT, REL_HWHEEL);

	/* Enable mouse buttons */
	ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
	ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);
	ioctl(fd, UI_SET_KEYBIT, BTN_MIDDLE);

	/* Create device */
	struct uinput_setup setup = {0};
	snprintf(setup.name, UINPUT_MAX_NAME_SIZE, "Xbox Controller Mouse");
	setup.id.bustype = BUS_USB;
	setup.id.vendor = 0x045e;
	setup.id.product = 0x02dd;
	setup.id.version = 1;

	if (ioctl(fd, UI_DEV_SETUP, &setup) < 0) {
		perror("UI_DEV_SETUP");
		close(fd);
		return -1;
	}

	if (ioctl(fd, UI_DEV_CREATE) < 0) {
		perror("UI_DEV_CREATE");
		close(fd);
		return -1;
	}

	return fd;
}

/* Process controller input packet */
static void process_packet(struct mapper_state *st, const struct rumble_input *inp)
{
	/* Update raw values */
	st->lx_raw = inp->lx;
	st->ly_raw = inp->ly;
	st->rx_raw = inp->rx;
	st->ry_raw = inp->ry;
	st->buttons = inp->buttons;
}

/* Update cursor motion (called at fixed 125 Hz) */
static void update_motion(struct mapper_state *st)
{
	float dt = 1.0f / UPDATE_HZ;

	/* Convert to float and apply deadzone */
	float lx = (float)st->lx_raw;
	float ly = (float)st->ly_raw;
	apply_deadzone(&lx, &ly, DEADZONE_RADIUS);

	/* Filter */
	st->lx_filt = ema_filter(lx, st->lx_filt, FILTER_ALPHA);
	st->ly_filt = ema_filter(ly, st->ly_filt, FILTER_ALPHA);

	/* Normalize to [-1, 1] */
	float lx_norm = st->lx_filt / STICK_MAX;
	float ly_norm = st->ly_filt / STICK_MAX;

	/* Compute velocity */
	float vx = compute_velocity(lx_norm);
	float vy = compute_velocity(-ly_norm);  /* Invert Y */

	/* Integrate to get delta */
	float dx = vx * dt;
	float dy = vy * dt;

	/* Accumulate fractional pixels */
	st->accum_x += dx;
	st->accum_y += dy;

	/* Extract integer part */
	int move_x = (int)st->accum_x;
	int move_y = (int)st->accum_y;

	/* Keep fractional part */
	st->accum_x -= (float)move_x;
	st->accum_y -= (float)move_y;

	/* Emit cursor movement */
	if (move_x != 0 || move_y != 0) {
		if (move_x) emit(st->uinput_fd, EV_REL, REL_X, move_x);
		if (move_y) emit(st->uinput_fd, EV_REL, REL_Y, move_y);
	}

	/* Process scrolling (right stick) */
	float rx = (float)st->rx_raw;
	float ry = (float)st->ry_raw;
	apply_deadzone(&rx, &ry, SCROLL_DEADZONE);

	st->rx_filt = ema_filter(rx, st->rx_filt, FILTER_ALPHA);
	st->ry_filt = ema_filter(ry, st->ry_filt, FILTER_ALPHA);

	float scroll_x = st->rx_filt * SCROLL_SCALE * dt;
	float scroll_y = -st->ry_filt * SCROLL_SCALE * dt;

	st->accum_sx += scroll_x;
	st->accum_sy += scroll_y;

	int wheel_x = (int)st->accum_sx;
	int wheel_y = (int)st->accum_sy;

	st->accum_sx -= (float)wheel_x;
	st->accum_sy -= (float)wheel_y;

	if (wheel_x != 0 || wheel_y != 0) {
		if (wheel_x) emit(st->uinput_fd, EV_REL, REL_HWHEEL, wheel_x);
		if (wheel_y) emit(st->uinput_fd, EV_REL, REL_WHEEL, wheel_y);
	}

	/* Process buttons */
	uint16_t changed = st->buttons ^ st->buttons_prev;

	if (changed & RUMBLE_BTN_LB) {
		int pressed = !!(st->buttons & RUMBLE_BTN_LB);
		emit(st->uinput_fd, EV_KEY, BTN_LEFT, pressed);
	}

	if (changed & RUMBLE_BTN_RB) {
		int pressed = !!(st->buttons & RUMBLE_BTN_RB);
		emit(st->uinput_fd, EV_KEY, BTN_RIGHT, pressed);
	}

	if (changed & RUMBLE_BTN_LS) {
		int pressed = !!(st->buttons & RUMBLE_BTN_LS);
		emit(st->uinput_fd, EV_KEY, BTN_MIDDLE, pressed);
	}

	st->buttons_prev = st->buttons;

	/* Sync */
	emit(st->uinput_fd, EV_SYN, SYN_REPORT, 0);
}

int main(void)
{
	struct mapper_state st = {0};
	struct sigaction sa = {0};

	/* Setup signal handler */
	sa.sa_handler = sig_handler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	/* Open controller device */
	st.rumble_fd = open(DEVICE_PATH, O_RDONLY | O_NONBLOCK);
	if (st.rumble_fd < 0) {
		perror("open " DEVICE_PATH);
		fprintf(stderr, "Hint: is the rumble module loaded?\n");
		return EXIT_FAILURE;
	}

	/* Setup uinput */
	st.uinput_fd = setup_uinput();
	if (st.uinput_fd < 0) {
		close(st.rumble_fd);
		return EXIT_FAILURE;
	}

	/* Create timerfd for fixed update rate */
	st.timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	if (st.timer_fd < 0) {
		perror("timerfd_create");
		close(st.uinput_fd);
		close(st.rumble_fd);
		return EXIT_FAILURE;
	}

	struct itimerspec its = {0};
	its.it_value.tv_nsec = UPDATE_INTERVAL_NS;
	its.it_interval.tv_nsec = UPDATE_INTERVAL_NS;
	
	if (timerfd_settime(st.timer_fd, 0, &its, NULL) < 0) {
		perror("timerfd_settime");
		close(st.timer_fd);
		close(st.uinput_fd);
		close(st.rumble_fd);
		return EXIT_FAILURE;
	}

	/* Create epoll instance */
	st.epoll_fd = epoll_create1(0);
	if (st.epoll_fd < 0) {
		perror("epoll_create1");
		close(st.timer_fd);
		close(st.uinput_fd);
		close(st.rumble_fd);
		return EXIT_FAILURE;
	}

	/* Add fds to epoll */
	struct epoll_event ev = {0};
	
	ev.events = EPOLLIN;
	ev.data.fd = st.rumble_fd;
	epoll_ctl(st.epoll_fd, EPOLL_CTL_ADD, st.rumble_fd, &ev);

	ev.events = EPOLLIN;
	ev.data.fd = st.timer_fd;
	epoll_ctl(st.epoll_fd, EPOLL_CTL_ADD, st.timer_fd, &ev);

	printf("rumble_mouse: started\n");
	printf("Left stick → cursor, Right stick → scroll\n");
	printf("LB → left click, RB → right click, LS → middle click\n");

	/* Main event loop */
	while (g_running) {
		struct epoll_event events[2];
		int nfds = epoll_wait(st.epoll_fd, events, 2, -1);

		if (nfds < 0) {
			if (errno == EINTR)
				continue;
			perror("epoll_wait");
			break;
		}

		for (int i = 0; i < nfds; i++) {
			if (events[i].data.fd == st.rumble_fd) {
				/* Read controller packet */
				struct rumble_input inp;
				ssize_t n = read(st.rumble_fd, &inp, sizeof(inp));
				
				if (n == sizeof(inp)) {
					process_packet(&st, &inp);
				} else if (n < 0 && errno != EAGAIN) {
					if (errno == ENODEV) {
						printf("rumble_mouse: controller disconnected\n");
						g_running = 0;
					} else {
						perror("read");
					}
				}
			} else if (events[i].data.fd == st.timer_fd) {
				/* Timer tick: update motion */
				uint64_t exp;
				read(st.timer_fd, &exp, sizeof(exp));
				update_motion(&st);
			}
		}
	}

	/* Cleanup */
	printf("rumble_mouse: shutting down\n");
	
	ioctl(st.uinput_fd, UI_DEV_DESTROY);
	close(st.epoll_fd);
	close(st.timer_fd);
	close(st.uinput_fd);
	close(st.rumble_fd);

	return EXIT_SUCCESS;
}

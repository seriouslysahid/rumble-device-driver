/* SPDX-License-Identifier: GPL-2.0 */
/*
 * rumble.h — Shared header for the rumble Xbox controller driver
 *
 * Team: PathFinders
 * Target: Xbox Wireless Controller Model 1708 (USB, VID 0x045E PID 0x02FD)
 * Kernel: Linux 6.1 LTS+
 */

#ifndef RUMBLE_H
#define RUMBLE_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

/* -------------------------------------------------------------------------
 * USB identity
 * ------------------------------------------------------------------------- */
#define XBOX_VENDOR_ID   0x045EU
#define XBOX_PRODUCT_ID  0x02DDU   /* Firmware 2015 wired USB */

/* -------------------------------------------------------------------------
 * Input data packet — one per 8 ms interrupt IN report (125 Hz)
 * Explicit padding ensures stable ABI between kernel and user space.
 * Packed to prevent trailing padding (22 bytes total).
 * ------------------------------------------------------------------------- */
struct rumble_input {
	uint16_t buttons;       /* bitmask of digital buttons (see below)   */
	uint8_t  lt;            /* left trigger  0–255                      */
	uint8_t  rt;            /* right trigger 0–255                      */
	int16_t  lx;            /* left  stick X, signed 16-bit             */
	int16_t  ly;            /* left  stick Y, signed 16-bit             */
	int16_t  rx;            /* right stick X, signed 16-bit             */
	int16_t  ry;            /* right stick Y, signed 16-bit             */
	uint16_t _pad;          /* explicit padding for 8-byte alignment    */
	uint64_t timestamp_us;  /* ktime_get() expressed in microseconds    */
} __attribute__((packed));

/* -------------------------------------------------------------------------
 * Button bitmask — matches bytes 2–3 of the Xbox 1708 interrupt IN report
 * ------------------------------------------------------------------------- */
#define BTN_MENU        (1U <<  0)   /* Menu / Start         */
#define BTN_VIEW        (1U <<  1)   /* View / Back          */
#define BTN_LS          (1U <<  2)   /* Left stick click     */
#define BTN_RS          (1U <<  3)   /* Right stick click    */
#define BTN_A           (1U <<  4)   /* A                    */
#define BTN_B           (1U <<  5)   /* B                    */
#define BTN_X           (1U <<  6)   /* X                    */
#define BTN_Y           (1U <<  7)   /* Y                    */
#define BTN_DPAD_UP     (1U <<  8)   /* D-pad Up             */
#define BTN_DPAD_DOWN   (1U <<  9)   /* D-pad Down           */
#define BTN_DPAD_LEFT   (1U << 10)   /* D-pad Left           */
#define BTN_DPAD_RIGHT  (1U << 11)   /* D-pad Right          */
#define BTN_LB          (1U << 12)   /* Left  bumper         */
#define BTN_RB          (1U << 13)   /* Right bumper         */

/* -------------------------------------------------------------------------
 * Rumble motor command — passed via ioctl
 * ------------------------------------------------------------------------- */
struct rumble_motors {
	uint8_t left;    /* left  motor intensity 0–100 (percent) */
	uint8_t right;   /* right motor intensity 0–100 (percent) */
};

/* -------------------------------------------------------------------------
 * ioctl definitions
 * ------------------------------------------------------------------------- */
#define RUMBLE_IOC_MAGIC   'R'
#define RUMBLE_SET_MOTORS  _IOW(RUMBLE_IOC_MAGIC, 1, struct rumble_motors)

#ifdef __KERNEL__
#define RUMBLE_COMPAT_IOCTL  RUMBLE_SET_MOTORS  /* no translation needed */
#endif

/* -------------------------------------------------------------------------
 * Ring-buffer capacity (number of struct rumble_input slots)
 * Must be a power of two so the wrap mask trick works.
 * ------------------------------------------------------------------------- */
#define RING_BUF_SIZE  64U

#endif /* RUMBLE_H */

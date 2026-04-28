/* SPDX-License-Identifier: GPL-2.0 */

#ifndef RUMBLE_H
#define RUMBLE_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#define XBOX_VENDOR_ID   0x045EU
#define XBOX_PRODUCT_ID  0x02DDU

struct rumble_input {
	uint16_t buttons;
	uint8_t  lt;
	uint8_t  rt;
	int16_t  lx;
	int16_t  ly;
	int16_t  rx;
	int16_t  ry;
	uint16_t _pad;
	uint64_t timestamp_us;
} __attribute__((packed));

#define BTN_MENU        (1U <<  0)
#define BTN_VIEW        (1U <<  1)
#define BTN_LS          (1U <<  2)
#define BTN_RS          (1U <<  3)
#define BTN_A           (1U <<  4)
#define BTN_B           (1U <<  5)
#define BTN_X           (1U <<  6)
#define BTN_Y           (1U <<  7)
#define BTN_DPAD_UP     (1U <<  8)
#define BTN_DPAD_DOWN   (1U <<  9)
#define BTN_DPAD_LEFT   (1U << 10)
#define BTN_DPAD_RIGHT  (1U << 11)
#define BTN_LB          (1U << 12)
#define BTN_RB          (1U << 13)

struct rumble_motors {
	uint8_t left;
	uint8_t right;
};

#define RUMBLE_IOC_MAGIC   'R'
#define RUMBLE_SET_MOTORS  _IOW(RUMBLE_IOC_MAGIC, 1, struct rumble_motors)

#ifdef __KERNEL__
#define RUMBLE_COMPAT_IOCTL  RUMBLE_SET_MOTORS
#endif

#define RING_BUF_SIZE  64U

#endif /* RUMBLE_H */
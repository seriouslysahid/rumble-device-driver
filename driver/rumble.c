// SPDX-License-Identifier: GPL-2.0
/*
 * rumble.c — Character device driver for Xbox Wireless Controller (Model 1708)
 *
 * Team:    PathFinders
 * Target:  Xbox Wireless Controller Model 1708, USB (VID 0x045E PID 0x02FD)
 * Kernel:  Linux 6.4+
 *
 * Overview
 * --------
 * This module registers a USB driver that binds to the Xbox 1708 controller
 * and exposes raw GIP input to user space through a character device at
 * /dev/rumble0.  It deliberately bypasses the Linux Input Subsystem so that
 * applications can receive GIP input reports and control rumble motors
 * without requiring evdev, joydev or any other in-tree HID glue.
 *
 * Character device interface
 *   open()   – check connected flag; take kref reference
 *   read()   – dequeue one struct rumble_input from the ring buffer;
 *              blocks until data is available (or O_NONBLOCK → -EAGAIN)
 *   ioctl()  – RUMBLE_SET_MOTORS: send a 13-byte GIP rumble packet via USB
 *   release()– drop kref reference
 *
 * Ring buffer
 *   64-slot power-of-two ring protected by a spinlock.  The URB completion
 *   handler (interrupt context) writes into the ring; read() drains it from
 *   process context.
 *
 * URB pipeline
 *   A single interrupt-IN URB is kept permanently in flight after probe().
 *   The completion handler re-submits it so new data arrives continuously at
 *   ~125 Hz.  On disconnect the URB is killed via usb_kill_urb().
 */

#define pr_fmt(fmt) "[rumble] " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/ktime.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/kref.h>
#include <linux/poll.h>
#include <linux/input.h>

#include "rumble.h"

/* =========================================================================
 * Module metadata
 * ========================================================================= */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PathFinders");
MODULE_DESCRIPTION("Character device driver for Xbox Wireless Controller (Model 1708, USB)");
MODULE_VERSION("1.0.0");

static struct usb_device_id rumble_id_table[] = {
	{ USB_DEVICE(XBOX_VENDOR_ID, XBOX_PRODUCT_ID) },          /* Model 1708 wired */
	{ }
};
MODULE_DEVICE_TABLE(usb, rumble_id_table);

/* =========================================================================
 * Internal constants
 * ========================================================================= */

/* Number of minor numbers we claim (only 0 is used right now) */
#define RUMBLE_MINOR_COUNT  1U
#define RUMBLE_MINOR_BASE   0U

/* Endpoint numbers */
#define XBOX_EP_IN          0x81U   /* EP1 IN  */
#define XBOX_EP_OUT         0x01U   /* EP1 OUT (rumble) */

/* Buffer large enough for GIP reports */
#define XBOX_PKT_SIZE       32U
#define XBOX_RUMBLE_SIZE    13U   /* GIP rumble packet size */

/* GIP report types */
#define GIP_CMD_INPUT       0x20U   /* Input report */
#define GIP_CMD_VIRTUAL_KEY 0x07U   /* Guide button */

/* Ring-buffer wrap mask (RING_BUF_SIZE must be power of 2) */
#define RING_MASK           (RING_BUF_SIZE - 1U)

/* =========================================================================
 * Per-device state
 * ========================================================================= */

/**
 * struct rumble_dev - all state associated with one connected controller
 *
 * Lifetime: allocated in rumble_probe(), freed when the last reference drops
 * (via kref). References are held by open file descriptors and the USB interface.
 */
struct rumble_dev {
	struct usb_device   *udev;        /* the underlying USB device          */
	struct usb_interface *intf;       /* the bound USB interface            */
	struct kref          kref;        /* reference count for safe disconnect */

	/* Character device bookkeeping */
	struct cdev          cdev;        /* embedded cdev (no separate alloc)  */
	struct device       *dev_node;    /* /dev/rumble0 created by udev       */
	dev_t                devno;       /* major:minor assigned at probe       */

	/* Ring buffer */
	struct rumble_input  ring[RING_BUF_SIZE]; /* circular buffer            */
	unsigned int         r_head;      /* producer index (written by URB CB) */
	unsigned int         r_tail;      /* consumer index (read by read())    */
	spinlock_t           ring_lock;   /* guards ring, r_head, r_tail        */

	/* Blocking read support */
	wait_queue_head_t    read_wq;     /* woken by URB CB or disconnect      */

	/* URB for continuous interrupt IN polling */
	struct urb          *in_urb;      /* allocated in probe, freed on disc  */
	uint8_t             *in_buf;      /* DMA-coherent transfer buffer       */
	dma_addr_t           in_dma;     /* DMA address for in_buf             */
	uint8_t              ep_out_addr; /* interrupt OUT endpoint address      */

	/* Hotplug / lifecycle */
	atomic_t             disconnected; /* set to 1 in rumble_disconnect     */

	/* GIP rumble sequence counter */
	uint8_t              rumble_seq;  /* incremented for each rumble packet */

	/* Serialises ioctl rumble transmissions */
	struct mutex         tx_mutex;

	/* Mouse emulation state */
	struct input_dev    *idev;
	int                  residue_x;
	int                  residue_y;
	int                  residue_sx;
	int                  residue_sy;
	bool                 lt_pressed;
	bool                 rt_pressed;
};

/* =========================================================================
 * Module-level globals
 * =========================================================================
 *
 * We support a single controller (minor 0).  The pointer is protected by
 * dev_mutex; it is set in probe() and cleared in disconnect().
 */

static struct class   *rumble_class;
static dev_t           rumble_base_devno;   /* first major:minor            */
static DEFINE_MUTEX(dev_mutex);             /* serialises probe/disconnect  */
static struct rumble_dev *g_dev;            /* the one connected controller */

/* =========================================================================
 * Ring buffer helpers  (must be called with ring_lock held)
 * ========================================================================= */

static inline bool ring_full(const struct rumble_dev *rd)
{
	return ((rd->r_head + 1U) & RING_MASK) == (rd->r_tail & RING_MASK);
}

static inline bool ring_empty(const struct rumble_dev *rd)
{
	return rd->r_head == rd->r_tail;
}

/**
 * ring_put() - write one input packet into the ring (IRQ context safe).
 *
 * If the ring is full the oldest entry is silently dropped (producer wins)
 * so that the URB pipeline is never stalled.
 */
static void ring_put(struct rumble_dev *rd, const struct rumble_input *inp)
{
	if (ring_full(rd)) {
		/* Drop oldest entry to make room */
		rd->r_tail = (rd->r_tail + 1U) & RING_MASK;
	}
	rd->ring[rd->r_head & RING_MASK] = *inp;
	rd->r_head = (rd->r_head + 1U) & RING_MASK;
}

/**
 * ring_get() - dequeue one packet.  Returns false if the ring was empty.
 */
static bool ring_get(struct rumble_dev *rd, struct rumble_input *out)
{
	if (ring_empty(rd))
		return false;
	*out = rd->ring[rd->r_tail & RING_MASK];
	rd->r_tail = (rd->r_tail + 1U) & RING_MASK;
	return true;
}

/* =========================================================================
 * URB completion handler  (interrupt context — must NOT sleep)
 * ========================================================================= */

/**
 * rumble_urb_complete() - URB completion handler (interrupt context).
 *
 * The wired Xbox 1708 (0x02FD) uses GIP protocol:
 *   - Type 0x20: 18-byte input report with buttons/axes
 *   - Type 0x07: virtual-key report (Guide button) — requires ACK
 *   - Other types: silently ignored
 *
 * GIP input report (type 0x20):
 *   [0] type(0x20)  [1] seq  [2] flags  [3] len(0x0e)
 *   [4] buttons_lo  [5] buttons_hi
 *   [6-7] LT(u16)  [8-9] RT(u16)  [10-11] LX  [12-13] LY  [14-15] RX  [16-17] RY
 */
static void rumble_urb_complete(struct urb *urb)
{
	struct rumble_dev *rd = urb->context;
	uint8_t *buf = urb->transfer_buffer;
	struct rumble_input inp;
	unsigned long flags;
	uint8_t b1, b2;
	uint16_t lt_raw, rt_raw;
	int ret;

	switch (urb->status) {
	case 0:
		break;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		return;
	case -EOVERFLOW:
		pr_warn("URB overflow\n");
		goto resubmit;
	default:
		goto resubmit;
	}

	if (urb->actual_length < 2U)
		goto resubmit;

	if (buf[0] != GIP_CMD_INPUT && buf[0] != GIP_CMD_VIRTUAL_KEY) {
		pr_info_ratelimited("rumble: received non-input packet type 0x%02x\n", buf[0]);
	}

	/* GIP frame: byte 0 is the command type */
	if (buf[0] == GIP_CMD_INPUT) {
		/* Input report */
		if (urb->actual_length < 18U)
			goto resubmit;

		/*
		 * GIP button bytes:
		 *   buf[4] bit2=MENU  bit3=VIEW  bit4=A  bit5=B  bit6=X  bit7=Y
		 *   buf[5] bit0=D-UP  bit1=D-DN  bit2=D-LT  bit3=D-RT
		 *          bit4=LB    bit5=RB    bit6=LS     bit7=RS
		 */
		b1 = buf[4];
		b2 = buf[5];
		inp.buttons = 0;
		if (b1 & BIT(2)) inp.buttons |= RUMBLE_BTN_MENU;
		if (b1 & BIT(3)) inp.buttons |= RUMBLE_BTN_VIEW;
		if (b1 & BIT(4)) inp.buttons |= RUMBLE_BTN_A;
		if (b1 & BIT(5)) inp.buttons |= RUMBLE_BTN_B;
		if (b1 & BIT(6)) inp.buttons |= RUMBLE_BTN_X;
		if (b1 & BIT(7)) inp.buttons |= RUMBLE_BTN_Y;
		if (b2 & BIT(0)) inp.buttons |= RUMBLE_BTN_DPAD_UP;
		if (b2 & BIT(1)) inp.buttons |= RUMBLE_BTN_DPAD_DOWN;
		if (b2 & BIT(2)) inp.buttons |= RUMBLE_BTN_DPAD_LEFT;
		if (b2 & BIT(3)) inp.buttons |= RUMBLE_BTN_DPAD_RIGHT;
		if (b2 & BIT(4)) inp.buttons |= RUMBLE_BTN_LB;
		if (b2 & BIT(5)) inp.buttons |= RUMBLE_BTN_RB;
		if (b2 & BIT(6)) inp.buttons |= RUMBLE_BTN_LS;
		if (b2 & BIT(7)) inp.buttons |= RUMBLE_BTN_RS;

		/* Triggers: 10-bit (0-1023) → scale to 0-255 */
		lt_raw  = (uint16_t)buf[6] | ((uint16_t)buf[7] << 8);
		rt_raw  = (uint16_t)buf[8] | ((uint16_t)buf[9] << 8);
		inp.lt  = (uint8_t)(lt_raw >> 2);
		inp.rt  = (uint8_t)(rt_raw >> 2);

		inp.lx = (int16_t)((uint16_t)buf[10] | ((uint16_t)buf[11] << 8));
		inp.ly = (int16_t)((uint16_t)buf[12] | ((uint16_t)buf[13] << 8));
		inp.rx = (int16_t)((uint16_t)buf[14] | ((uint16_t)buf[15] << 8));
		inp.ry = (int16_t)((uint16_t)buf[16] | ((uint16_t)buf[17] << 8));

		inp._pad = 0;
		inp.timestamp_us = (uint64_t)ktime_to_us(ktime_get());

		if (rd->idev) {
			/* --- Cursor movement: left stick ---
			 * Deadzone: ignore small stick noise.
			 * Speed multiplier 10, divided by 28767 (axis max after deadzone removal)
			 * so full deflection gives ~10 px/report (~1250 px/s at 125 Hz).
			 */
			const long DZ  = 4000L;
			const long MAX = 28767L;   /* 32767 - 4000 */
			const long SPD = 10L;      /* cursor pixels per max-deflection report */
			const long SSPD = 3L;     /* scroll units per max-deflection report */

			long lx = inp.lx, ly = inp.ly, rx = inp.rx, ry = inp.ry;

			if (lx >  DZ) lx -= DZ; else if (lx < -DZ) lx += DZ; else lx = 0;
			if (ly >  DZ) ly -= DZ; else if (ly < -DZ) ly += DZ; else ly = 0;
			if (rx >  DZ) rx -= DZ; else if (rx < -DZ) rx += DZ; else rx = 0;
			if (ry >  DZ) ry -= DZ; else if (ry < -DZ) ry += DZ; else ry = 0;

			/* Accumulate sub-pixel residue for smooth movement */
			long acc_x  = lx  * SPD  + rd->residue_x;
			long acc_y  = -ly * SPD  + rd->residue_y;
			long acc_sx = rx  * SSPD + rd->residue_sx;
			long acc_sy = -ry * SSPD + rd->residue_sy;

			int move_x   = (int)(acc_x  / MAX);
			int move_y   = (int)(acc_y  / MAX);
			int scroll_x = (int)(acc_sx / MAX);
			int scroll_y = (int)(acc_sy / MAX);

			rd->residue_x  = (int)(acc_x  % MAX);
			rd->residue_y  = (int)(acc_y  % MAX);
			rd->residue_sx = (int)(acc_sx % MAX);
			rd->residue_sy = (int)(acc_sy % MAX);

			if (move_x)   input_report_rel(rd->idev, REL_X,      move_x);
			if (move_y)   input_report_rel(rd->idev, REL_Y,      move_y);
			if (scroll_x) input_report_rel(rd->idev, REL_HWHEEL, scroll_x);
			if (scroll_y) input_report_rel(rd->idev, REL_WHEEL,  scroll_y);

			/* --- LB = left single click, RB = right single click --- */
			bool lb_now = !!(inp.buttons & RUMBLE_BTN_LB);
			bool rb_now = !!(inp.buttons & RUMBLE_BTN_RB);
			input_report_key(rd->idev, BTN_LEFT,  lb_now);
			input_report_key(rd->idev, BTN_RIGHT, rb_now);
			input_sync(rd->idev);

			/* --- LT = left double-click (on press edge only) --- */
			bool lt_now = (inp.lt > 64);
			if (lt_now && !rd->lt_pressed) {
				input_report_key(rd->idev, BTN_LEFT, 1); input_sync(rd->idev);
				input_report_key(rd->idev, BTN_LEFT, 0); input_sync(rd->idev);
				input_report_key(rd->idev, BTN_LEFT, 1); input_sync(rd->idev);
				input_report_key(rd->idev, BTN_LEFT, 0); input_sync(rd->idev);
			}
			rd->lt_pressed = lt_now;

			/* --- RT = right double-click (on press edge only) --- */
			bool rt_now = (inp.rt > 64);
			if (rt_now && !rd->rt_pressed) {
				input_report_key(rd->idev, BTN_RIGHT, 1); input_sync(rd->idev);
				input_report_key(rd->idev, BTN_RIGHT, 0); input_sync(rd->idev);
				input_report_key(rd->idev, BTN_RIGHT, 1); input_sync(rd->idev);
				input_report_key(rd->idev, BTN_RIGHT, 0); input_sync(rd->idev);
			}
			rd->rt_pressed = rt_now;
		}

		spin_lock_irqsave(&rd->ring_lock, flags);
		ring_put(rd, &inp);
		spin_unlock_irqrestore(&rd->ring_lock, flags);
		wake_up_interruptible(&rd->read_wq);

	} else if (buf[0] == GIP_CMD_VIRTUAL_KEY) {
		/*
		 * Virtual-key report (Guide button).
		 * Format: [0]=0x07 [1]=seq [2]=flags [3]=len [4]=key_code
		 * If flags bit 0 is set, controller expects an ACK.
		 * We silently ACK to keep the controller happy but don't
		 * expose the Guide button to user space (not in button mask).
		 */
		if (urb->actual_length >= 5 && (buf[2] & 0x01)) {
			/* Send ACK: same packet with flags cleared */
			uint8_t ack[5];
			memcpy(ack, buf, 5);
			ack[2] = 0;  /* clear ACK-request flag */
			usb_interrupt_msg(rd->udev,
					  usb_sndintpipe(rd->udev, rd->ep_out_addr),
					  ack, 5, &ret, msecs_to_jiffies(100));
		}
	}
	/* All other GIP types (0x02 announce, etc.) are silently ignored */

resubmit:
	ret = usb_submit_urb(urb, GFP_ATOMIC);
	if (ret && ret != -ENODEV)
		pr_warn("URB re-submit failed: %d\n", ret);
}

/* =========================================================================
 * Lifetime management
 * ========================================================================= */

/**
 * rumble_delete() - kref release callback, frees the rumble_dev structure.
 */
static void rumble_delete(struct kref *kref)
{
	struct rumble_dev *rd = container_of(kref, struct rumble_dev, kref);
	usb_put_dev(rd->udev);
	kfree(rd);
}

/* =========================================================================
 * file_operations
 * ========================================================================= */

/**
 * rumble_open() - called when user space opens /dev/rumble0.
 *
 * Stores a pointer to the per-device structure in filp->private_data and
 * takes a kref reference to prevent use-after-free during disconnect.
 */
static int rumble_open(struct inode *inode, struct file *filp)
{
	struct rumble_dev *rd;

	mutex_lock(&dev_mutex);
	rd = g_dev;
	if (!rd) {
		mutex_unlock(&dev_mutex);
		return -ENODEV;
	}
	if (atomic_read(&rd->disconnected)) {
		mutex_unlock(&dev_mutex);
		return -ENODEV;
	}
	kref_get(&rd->kref);
	filp->private_data = rd;
	mutex_unlock(&dev_mutex);

	pr_info("device opened\n");
	return 0;
}

/**
 * rumble_release() - called when the last file descriptor referencing this
 *   open is closed.  Drops the kref reference.
 */
static int rumble_release(struct inode *inode, struct file *filp)
{
	struct rumble_dev *rd = filp->private_data;

	if (!rd)
		return 0;

	kref_put(&rd->kref, rumble_delete);
	pr_info("device closed\n");
	return 0;
}

/**
 * rumble_read() - dequeue one struct rumble_input and copy it to user space.
 *
 * Behaviour:
 *   - If the ring buffer is empty and O_NONBLOCK is set, return -EAGAIN.
 *   - Otherwise block until data is available or the device disconnects.
 *   - On disconnect return -ENODEV.
 *   - Returns the size of struct rumble_input on success.
 *   - Returns -EINVAL if the user buffer is too small.
 */
static ssize_t rumble_read(struct file *filp, char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	struct rumble_dev *rd = filp->private_data;
	struct rumble_input inp;
	unsigned long flags;
	int ret;

	if (count < sizeof(struct rumble_input))
		return -EINVAL;

	while (1) {
		/* Fast-path: try to pull a packet from the ring */
		spin_lock_irqsave(&rd->ring_lock, flags);
		if (ring_get(rd, &inp)) {
			spin_unlock_irqrestore(&rd->ring_lock, flags);
			break;
		}
		spin_unlock_irqrestore(&rd->ring_lock, flags);

		/* Device went away while we were waiting */
		if (atomic_read(&rd->disconnected))
			return -ENODEV;

		/* Non-blocking: caller asked us not to sleep */
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		/* Block until the URB callback writes new data or disconnect */
		ret = wait_event_interruptible(rd->read_wq,
			!ring_empty(rd) || atomic_read(&rd->disconnected));
		if (ret)
			return ret;   /* -ERESTARTSYS */

		if (atomic_read(&rd->disconnected))
			return -ENODEV;
	}

	if (copy_to_user(ubuf, &inp, sizeof(inp)))
		return -EFAULT;

	return (ssize_t)sizeof(inp);
}

/**
 * rumble_poll() - poll/select support.
 *
 * Returns POLLIN | POLLRDNORM when data is available or device disconnected.
 */
static __poll_t rumble_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct rumble_dev *rd = filp->private_data;
	__poll_t mask = 0;

	poll_wait(filp, &rd->read_wq, wait);

	if (!ring_empty(rd) || atomic_read(&rd->disconnected))
		mask |= POLLIN | POLLRDNORM;

	if (atomic_read(&rd->disconnected))
		mask |= POLLHUP;

	return mask;
}

/**
 * rumble_ioctl() - handle RUMBLE_SET_MOTORS.
 *
 * Sends a 13-byte GIP rumble packet to the controller's interrupt OUT
 * endpoint using usb_interrupt_msg() (synchronous, sleeps — OK in ioctl
 * context which is always process context).
 *
 * GIP rumble packet format (wired Xbox One / 1708):
 *   [0] 0x09          command type
 *   [1] seq           sequence counter (incremented per packet)
 *   [2] 0x00          subcommand
 *   [3] 0x09          motor mask (0x09 = left+right main motors)
 *   [4] left_trigger  trigger motor intensity (0 for main motors only)
 *   [5] right_trigger trigger motor intensity (0 for main motors only)
 *   [6] left_main     left main motor intensity (0-255)
 *   [7] right_main    right main motor intensity (0-255)
 *   [8] duration      pulse duration (0xFF = continuous)
 *   [9] delay         delay before start (0x00 = immediate)
 *   [10] repeat       repeat count (0x00 = once)
 *   [11-12] padding   (0x00)
 *
 * Intensity values are scaled from the 0–100 percent range supplied by the
 * caller to the 0–255 range expected by the controller.
 */
static long rumble_ioctl(struct file *filp, unsigned int cmd,
			 unsigned long arg)
{
	struct rumble_dev   *rd = filp->private_data;
	struct rumble_motors motors;
	uint8_t              pkt[XBOX_RUMBLE_SIZE];
	int                  transferred;
	int                  ret;

	if (atomic_read(&rd->disconnected))
		return -ENODEV;

	switch (cmd) {
	case RUMBLE_SET_MOTORS:
		if (copy_from_user(&motors, (void __user *)arg, sizeof(motors)))
			return -EFAULT;

		/* Clamp to 0–100 */
		if (motors.left  > 100U) motors.left  = 100U;
		if (motors.right > 100U) motors.right = 100U;

		/* Build GIP rumble packet */
		pkt[0]  = 0x09;
		pkt[1]  = rd->rumble_seq++;
		pkt[2]  = 0x00;
		pkt[3]  = 0x09;   /* motor mask: main motors only */
		pkt[4]  = 0x00;   /* left trigger motor */
		pkt[5]  = 0x00;   /* right trigger motor */
		pkt[6]  = (uint8_t)((motors.left  * 255U) / 100U);
		pkt[7]  = (uint8_t)((motors.right * 255U) / 100U);
		pkt[8]  = 0xFF;   /* duration: continuous */
		pkt[9]  = 0x00;   /* delay: immediate */
		pkt[10] = 0x00;   /* repeat: once */
		pkt[11] = 0x00;
		pkt[12] = 0x00;

		mutex_lock(&rd->tx_mutex);
		if (atomic_read(&rd->disconnected)) {
			mutex_unlock(&rd->tx_mutex);
			return -ENODEV;
		}
		ret = usb_interrupt_msg(rd->udev,
					usb_sndintpipe(rd->udev, rd->ep_out_addr),
					pkt, sizeof(pkt),
					&transferred,
					msecs_to_jiffies(200));
		mutex_unlock(&rd->tx_mutex);

		if (ret < 0) {
			pr_warn("rumble TX failed: %d\n", ret);
			return ret;
		}
		pr_info("rumble: left=%u%% right=%u%%\n",
			motors.left, motors.right);
		return 0;

	default:
		return -ENOTTY;
	}
}

static const struct file_operations rumble_fops = {
	.owner          = THIS_MODULE,
	.open           = rumble_open,
	.read           = rumble_read,
	.poll           = rumble_poll,
	.release        = rumble_release,
	.unlocked_ioctl = rumble_ioctl,
	.compat_ioctl   = rumble_ioctl,
};

/* =========================================================================
 * USB probe
 * ========================================================================= */

/**
 * rumble_probe() - called by the USB core when an Xbox 1708 controller is
 *   plugged in (or when the module is loaded with the device already present).
 *
 * Sets up the per-device structure, finds endpoints, registers the cdev,
 * allocates a DMA buffer + URB, and starts the interrupt-IN data stream.
 */
static int rumble_probe(struct usb_interface *intf,
			const struct usb_device_id *id)
{
	struct usb_device              *udev = interface_to_usbdev(intf);
	struct usb_host_interface      *iface_desc;
	struct usb_endpoint_descriptor *ep_in = NULL;
	struct usb_endpoint_descriptor *ep_out = NULL;
	struct rumble_dev              *rd;
	int                             ret, i;

	/* We only want interface 0 (the GIP interface) */
	iface_desc = intf->cur_altsetting;
	if (iface_desc->desc.bInterfaceNumber != 0)
		return -ENODEV;

	/* Verify interface class is vendor-specific (Xbox controllers vary in subclass/protocol) */
	if (iface_desc->desc.bInterfaceClass != USB_CLASS_VENDOR_SPEC) {
		pr_warn("Not a vendor-specific interface (class=0x%02x)\n",
			iface_desc->desc.bInterfaceClass);
		return -ENODEV;
	}
	pr_info("Interface class/subclass/protocol: %02x/%02x/%02x\n",
		iface_desc->desc.bInterfaceClass,
		iface_desc->desc.bInterfaceSubClass,
		iface_desc->desc.bInterfaceProtocol);

	/* Find both the interrupt IN and OUT endpoints */
	for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
		struct usb_endpoint_descriptor *ep =
			&iface_desc->endpoint[i].desc;

		if (usb_endpoint_is_int_in(ep) && !ep_in)
			ep_in = ep;
		else if (usb_endpoint_is_int_out(ep) && !ep_out)
			ep_out = ep;
	}
	if (!ep_in) {
		pr_err("no interrupt IN endpoint found\n");
		return -ENODEV;
	}
	if (!ep_out) {
		pr_err("no interrupt OUT endpoint found\n");
		return -ENODEV;
	}
	pr_info("endpoints: IN=0x%02x OUT=0x%02x\n",
		ep_in->bEndpointAddress, ep_out->bEndpointAddress);

	/* Allocate and zero-initialise the device structure */
	rd = kzalloc(sizeof(*rd), GFP_KERNEL);
	if (!rd)
		return -ENOMEM;

	rd->udev       = usb_get_dev(udev);
	rd->intf       = intf;
	rd->ep_out_addr = ep_out->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
	rd->rumble_seq = 0;

	kref_init(&rd->kref);
	spin_lock_init(&rd->ring_lock);
	init_waitqueue_head(&rd->read_wq);
	atomic_set(&rd->disconnected, 0);
	mutex_init(&rd->tx_mutex);

	/* Claim the major:minor slot 0 */
	mutex_lock(&dev_mutex);
	if (g_dev) {
		/* Already have one controller — reject the second */
		mutex_unlock(&dev_mutex);
		pr_warn("second controller ignored (only one supported)\n");
		ret = -EBUSY;
		goto err_put_dev;
	}

	rd->devno = MKDEV(MAJOR(rumble_base_devno), RUMBLE_MINOR_BASE);

	/* Register the cdev */
	cdev_init(&rd->cdev, &rumble_fops);
	rd->cdev.owner = THIS_MODULE;
	ret = cdev_add(&rd->cdev, rd->devno, 1);
	if (ret) {
		pr_err("cdev_add failed: %d\n", ret);
		mutex_unlock(&dev_mutex);
		goto err_put_dev;
	}

	/* Ask udev to create /dev/rumble0 */
	rd->dev_node = device_create(rumble_class, &intf->dev,
				     rd->devno, rd, "rumble0");
	if (IS_ERR(rd->dev_node)) {
		ret = PTR_ERR(rd->dev_node);
		pr_err("device_create failed: %d\n", ret);
		cdev_del(&rd->cdev);
		mutex_unlock(&dev_mutex);
		goto err_put_dev;
	}

	g_dev = rd;
	mutex_unlock(&dev_mutex);

	/* Allocate DMA-coherent buffer for the interrupt IN URB */
	rd->in_buf = usb_alloc_coherent(udev, XBOX_PKT_SIZE,
					GFP_KERNEL, &rd->in_dma);
	if (!rd->in_buf) {
		pr_err("failed to allocate USB transfer buffer\n");
		ret = -ENOMEM;
		goto err_destroy_dev;
	}

	/* Allocate the URB (0 = non-isochronous) */
	rd->in_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!rd->in_urb) {
		pr_err("usb_alloc_urb failed\n");
		ret = -ENOMEM;
		goto err_free_buf;
	}

	/* Initialise the interrupt IN URB */
	usb_fill_int_urb(rd->in_urb,
			 udev,
			 usb_rcvintpipe(udev, usb_endpoint_num(ep_in)),
			 rd->in_buf,
			 XBOX_PKT_SIZE,
			 rumble_urb_complete,
			 rd,
			 ep_in->bInterval);

	rd->in_urb->transfer_dma   = rd->in_dma;
	rd->in_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	/* Send Xbox One initialization packet to start input streaming */
	{
		uint8_t *init_pkt = kmalloc(5, GFP_KERNEL);
		if (init_pkt) {
			int transferred;
			/* Original Xbox One */
			init_pkt[0] = 0x05; init_pkt[1] = 0x20; init_pkt[2] = 0x00; init_pkt[3] = 0x01; init_pkt[4] = 0x00;
			usb_interrupt_msg(udev, usb_sndintpipe(udev, rd->ep_out_addr),
					  init_pkt, 5, &transferred, 1000);
			/* Firmware 2015 (PID 0x02DD) */
			init_pkt[0] = 0x05; init_pkt[1] = 0x20; init_pkt[2] = 0x00; init_pkt[3] = 0x01; init_pkt[4] = 0x04;
			usb_interrupt_msg(udev, usb_sndintpipe(udev, rd->ep_out_addr),
					  init_pkt, 5, &transferred, 1000);
			/* Xbox One S / Bluetooth */
			init_pkt[0] = 0x05; init_pkt[1] = 0x20; init_pkt[2] = 0x00; init_pkt[3] = 0x0f; init_pkt[4] = 0x06;
			usb_interrupt_msg(udev, usb_sndintpipe(udev, rd->ep_out_addr),
					  init_pkt, 5, &transferred, 1000);
			kfree(init_pkt);
		}
	}

	/* Submit the URB — this starts the data stream */
	ret = usb_submit_urb(rd->in_urb, GFP_KERNEL);
	if (ret) {
		pr_err("usb_submit_urb failed: %d\n", ret);
		goto err_free_urb;
	}

	rd->idev = input_allocate_device();
	if (rd->idev) {
		rd->idev->name = "Xbox 1708 Mouse";
		rd->idev->id.bustype = BUS_USB;
		rd->idev->id.vendor = le16_to_cpu(udev->descriptor.idVendor);
		rd->idev->id.product = le16_to_cpu(udev->descriptor.idProduct);
		rd->idev->dev.parent = &intf->dev;

		__set_bit(EV_REL, rd->idev->evbit);
		__set_bit(REL_X, rd->idev->relbit);
		__set_bit(REL_Y, rd->idev->relbit);
		__set_bit(REL_WHEEL, rd->idev->relbit);
		__set_bit(REL_HWHEEL, rd->idev->relbit);

		__set_bit(EV_KEY, rd->idev->evbit);
		__set_bit(BTN_LEFT, rd->idev->keybit);
		__set_bit(BTN_RIGHT, rd->idev->keybit);

		if (input_register_device(rd->idev)) {
			input_free_device(rd->idev);
			rd->idev = NULL;
		}
	}

	usb_set_intfdata(intf, rd);
	pr_info("Xbox 1708 controller connected (bus %d dev %d)\n",
		udev->bus->busnum, udev->devnum);
	return 0;

err_free_urb:
	usb_free_urb(rd->in_urb);
err_free_buf:
	usb_free_coherent(udev, XBOX_PKT_SIZE, rd->in_buf, rd->in_dma);
err_destroy_dev:
	mutex_lock(&dev_mutex);
	g_dev = NULL;
	device_destroy(rumble_class, rd->devno);
	cdev_del(&rd->cdev);
	mutex_unlock(&dev_mutex);
err_put_dev:
	usb_put_dev(udev);
	kfree(rd);
	return ret;
}

/* =========================================================================
 * USB disconnect
 * ========================================================================= */

/**
 * rumble_disconnect() - called when the controller is unplugged or the
 *   module is being removed while the device is connected.
 *
 * Marks the device as disconnected, wakes blocked readers, kills the
 * in-flight URB, tears down the /dev node and cdev, and drops the kref.
 * Actual memory free happens when all open file descriptors are closed.
 */
static void rumble_disconnect(struct usb_interface *intf)
{
	struct rumble_dev *rd = usb_get_intfdata(intf);

	if (!rd)
		return;

	usb_set_intfdata(intf, NULL);

	/* Tell read() / ioctl() that the hardware is gone */
	atomic_set(&rd->disconnected, 1);
	wake_up_interruptible(&rd->read_wq);

	/* Stop the URB stream (blocks until the completion handler returns) */
	usb_kill_urb(rd->in_urb);

	/* Remove the /dev entry and the cdev */
	mutex_lock(&dev_mutex);
	g_dev = NULL;
	device_destroy(rumble_class, rd->devno);
	cdev_del(&rd->cdev);
	mutex_unlock(&dev_mutex);

	/* Release resources */
	if (rd->idev)
		input_unregister_device(rd->idev);
	usb_free_urb(rd->in_urb);
	usb_free_coherent(rd->udev, XBOX_PKT_SIZE, rd->in_buf, rd->in_dma);

	pr_info("Xbox 1708 controller disconnected\n");

	/* Drop the initial kref; rd will be freed when all fds are closed */
	kref_put(&rd->kref, rumble_delete);
}

/* =========================================================================
 * USB driver registration
 * ========================================================================= */

static struct usb_driver rumble_usb_driver = {
	.name       = "rumble",
	.probe      = rumble_probe,
	.disconnect = rumble_disconnect,
	.id_table   = rumble_id_table,
};

/* =========================================================================
 * Module init / exit
 * ========================================================================= */

/**
 * rumble_init() - reserve a char-device major number and register the
 *   USB driver.
 *
 * The class and the dynamically allocated major number are module-global so
 * they persist across multiple probe/disconnect cycles (hot-plug).
 */
static int __init rumble_init(void)
{
	int ret;

	/* Allocate a dynamic major number for RUMBLE_MINOR_COUNT minors */
	ret = alloc_chrdev_region(&rumble_base_devno, RUMBLE_MINOR_BASE,
				  RUMBLE_MINOR_COUNT, "rumble");
	if (ret < 0) {
		pr_err("alloc_chrdev_region failed: %d\n", ret);
		return ret;
	}

	/* Create the /sys/class/rumble class so udev can make /dev/rumble0 */
	rumble_class = class_create("rumble");
	if (IS_ERR(rumble_class)) {
		ret = PTR_ERR(rumble_class);
		pr_err("class_create failed: %d\n", ret);
		goto err_unreg_chrdev;
	}

	/* Register the USB driver — this triggers probe() for any already-
	 * connected compatible device. */
	ret = usb_register(&rumble_usb_driver);
	if (ret) {
		pr_err("usb_register failed: %d\n", ret);
		goto err_destroy_class;
	}

	pr_info("module loaded (major=%d)\n", MAJOR(rumble_base_devno));
	return 0;

err_destroy_class:
	class_destroy(rumble_class);
err_unreg_chrdev:
	unregister_chrdev_region(rumble_base_devno, RUMBLE_MINOR_COUNT);
	return ret;
}

/**
 * rumble_exit() - deregister the USB driver, destroy the class, and
 *   release the major number.
 *
 * After usb_deregister() returns, rumble_disconnect() has been called for
 * every bound interface, so g_dev == NULL and all per-device memory has been
 * freed.
 */
static void __exit rumble_exit(void)
{
	usb_deregister(&rumble_usb_driver);
	class_destroy(rumble_class);
	unregister_chrdev_region(rumble_base_devno, RUMBLE_MINOR_COUNT);
	pr_info("module unloaded\n");
}

module_init(rumble_init);
module_exit(rumble_exit);

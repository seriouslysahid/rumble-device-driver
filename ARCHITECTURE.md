# Rumble Driver Architecture

Technical architecture documentation for the rumble Xbox controller character device driver.

---

## Overview

The rumble driver implements a Linux character device interface for the Xbox Wireless Controller Model 1708, providing direct access to raw GIP (Gaming Input Protocol) packets without going through the Linux Input Subsystem.

**Key Design Decision:** Bypass the Input Subsystem to demonstrate custom character device implementation and direct USB handling.

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        User Space                           │
├─────────────────────────────────────────────────────────────┤
│  rumble_read          rumble_monitor                        │
│  (CLI reader)         (ncurses TUI)                         │
│       │                     │                               │
│       └─────────┬───────────┘                               │
│                 │                                           │
│            open/read/poll/ioctl/close                       │
│                 │                                           │
└─────────────────┼───────────────────────────────────────────┘
                  │
         /dev/rumble0 (character device)
                  │
┌─────────────────┼───────────────────────────────────────────┐
│                 │          Kernel Space                     │
│                 ▼                                           │
│  ┌──────────────────────────────────────────────┐          │
│  │         file_operations                      │          │
│  │  .open    = rumble_open                      │          │
│  │  .read    = rumble_read                      │          │
│  │  .poll    = rumble_poll                      │          │
│  │  .ioctl   = rumble_ioctl                     │          │
│  │  .release = rumble_release                   │          │
│  └──────────────────────────────────────────────┘          │
│                 │                                           │
│                 ▼                                           │
│  ┌──────────────────────────────────────────────┐          │
│  │         struct rumble_dev                    │          │
│  │  - ring buffer (64 slots)                    │          │
│  │  - spinlock                                  │          │
│  │  - wait queue                                │          │
│  │  - kref (lifetime management)                │          │
│  │  - URB + DMA buffer                          │          │
│  └──────────────────────────────────────────────┘          │
│                 │                                           │
│                 ▼                                           │
│  ┌──────────────────────────────────────────────┐          │
│  │         USB Driver                           │          │
│  │  .probe      = rumble_probe                  │          │
│  │  .disconnect = rumble_disconnect             │          │
│  └──────────────────────────────────────────────┘          │
│                 │                                           │
└─────────────────┼───────────────────────────────────────────┘
                  │
         USB Interrupt Transfers
                  │
┌─────────────────┼───────────────────────────────────────────┐
│                 ▼                                           │
│  Xbox Wireless Controller Model 1708                       │
│  - Interrupt IN endpoint (EP1 IN)                          │
│  - Interrupt OUT endpoint (EP1 OUT)                        │
│  - GIP protocol                                            │
└─────────────────────────────────────────────────────────────┘
```

---

## Component Details

### 1. Character Device Interface

**Device Node:** `/dev/rumble0`
- **Major:** Dynamically allocated
- **Minor:** 0
- **Class:** `rumble`
- **Permissions:** 0660, group `input`

**File Operations:**

| Operation | Function | Context | Description |
|-----------|----------|---------|-------------|
| `open()` | `rumble_open()` | Process | Take kref, check connected |
| `read()` | `rumble_read()` | Process | Dequeue from ring buffer |
| `poll()` | `rumble_poll()` | Process | Check data availability |
| `ioctl()` | `rumble_ioctl()` | Process | Send rumble commands |
| `release()` | `rumble_release()` | Process | Drop kref |

### 2. Ring Buffer

**Design:**
- **Size:** 64 slots (power-of-2 for efficient masking)
- **Type:** Circular buffer
- **Protection:** spinlock (`ring_lock`)
- **Overflow:** Drop oldest packet (producer wins)

**Indices:**
- `r_head`: Producer index (written by URB callback)
- `r_tail`: Consumer index (read by `read()`)

**Masking:**
```c
#define RING_MASK (RING_BUF_SIZE - 1U)
index = (index + 1U) & RING_MASK;
```

**Full Condition:**
```c
((head + 1) & RING_MASK) == (tail & RING_MASK)
```

**Empty Condition:**
```c
head == tail
```

### 3. URB Pipeline

**Interrupt IN URB:**
- **Endpoint:** EP1 IN (0x81)
- **Buffer Size:** 32 bytes (DMA-coherent)
- **Interval:** Controller-specified (typically 8ms → 125 Hz)
- **Lifetime:** Allocated in `probe()`, freed in `disconnect()`
- **Resubmission:** Automatic in completion handler

**Flow:**
```
Controller sends packet
       ↓
USB core delivers to URB
       ↓
rumble_urb_complete() [INTERRUPT CONTEXT]
       ↓
Parse GIP packet
       ↓
Fill struct rumble_input
       ↓
ring_put() [spinlock protected]
       ↓
wake_up_interruptible()
       ↓
Resubmit URB
```

**Interrupt OUT (Rumble):**
- **Endpoint:** EP1 OUT (0x01)
- **Method:** `usb_interrupt_msg()` (synchronous)
- **Context:** Process context (ioctl)
- **Packet Size:** 13 bytes (GIP rumble command)

### 4. GIP Protocol

**Input Report (Type 0x20):**
```
Offset  Size  Field
------  ----  -----
0       1     Type (0x20)
1       1     Sequence
2       1     Flags
3       1     Length (0x0E)
4       1     Buttons low byte
5       1     Buttons high byte
6-7     2     Left trigger (10-bit)
8-9     2     Right trigger (10-bit)
10-11   2     Left stick X
12-13   2     Left stick Y
14-15   2     Right stick X
16-17   2     Right stick Y
```

**Virtual-Key Report (Type 0x07):**
```
Offset  Size  Field
------  ----  -----
0       1     Type (0x07)
1       1     Sequence
2       1     Flags (bit 0 = ACK request)
3       1     Length
4       1     Key code
```

**Rumble Command (Type 0x09):**
```
Offset  Size  Field
------  ----  -----
0       1     Type (0x09)
1       1     Sequence
2       1     Subcommand (0x00)
3       1     Motor mask (0x09 = main motors)
4       1     Left trigger motor
5       1     Right trigger motor
6       1     Left main motor (0-255)
7       1     Right main motor (0-255)
8       1     Duration (0xFF = continuous)
9       1     Delay (0x00 = immediate)
10      1     Repeat (0x00 = once)
11-12   2     Padding
```

### 5. Lifetime Management

**Reference Counting (kref):**
- **Initial ref:** Taken in `probe()`
- **Additional refs:** Taken in `open()`
- **Release:** Dropped in `release()` and `disconnect()`
- **Free:** When last ref drops → `rumble_delete()`

**Disconnect Handling:**
```
rumble_disconnect() called
       ↓
Set disconnected flag
       ↓
Wake up blocked readers
       ↓
usb_kill_urb() [blocks until URB callback returns]
       ↓
Remove /dev/rumble0
       ↓
kref_put() [may free if no open fds]
```

**Open File Descriptors:**
- Each `open()` takes a kref
- `disconnect()` can happen while fds open
- Memory freed only when last fd closed

### 6. Synchronization

**Spinlock (`ring_lock`):**
- **Protects:** Ring buffer indices and data
- **Held by:** URB callback (interrupt context), `read()` (process context)
- **Type:** `spin_lock_irqsave()` / `spin_unlock_irqrestore()`

**Mutex (`tx_mutex`):**
- **Protects:** Rumble transmission
- **Held by:** `ioctl()` (process context only)
- **Type:** `mutex_lock()` / `mutex_unlock()`

**Wait Queue (`read_wq`):**
- **Purpose:** Block readers when ring empty
- **Woken by:** URB callback, `disconnect()`
- **Type:** `wait_event_interruptible()`

**Atomic (`disconnected`):**
- **Purpose:** Signal device removal
- **Set by:** `disconnect()`
- **Checked by:** `open()`, `read()`, `ioctl()`

### 7. Context Boundaries

**Interrupt Context (URB callback):**
- **Can:** Parse packets, update ring buffer, wake waiters
- **Cannot:** Sleep, allocate memory (GFP_KERNEL), take mutexes
- **Must:** Use `GFP_ATOMIC`, spinlocks, be fast

**Process Context (syscalls):**
- **Can:** Sleep, allocate memory, take mutexes, copy to/from user
- **Cannot:** Access user pointers without `copy_to_user()`
- **Must:** Check `disconnected` flag, handle signals

---

## Data Flow

### Read Path

```
User: read(fd, buf, sizeof(struct rumble_input))
       ↓
Kernel: rumble_read()
       ↓
Check buffer size
       ↓
Loop:
  ├─ spin_lock_irqsave(&ring_lock)
  ├─ ring_get(&inp)
  ├─ spin_unlock_irqrestore(&ring_lock)
  ├─ If got packet: break
  ├─ If disconnected: return -ENODEV
  ├─ If O_NONBLOCK: return -EAGAIN
  └─ wait_event_interruptible(read_wq, !ring_empty || disconnected)
       ↓
copy_to_user(buf, &inp, sizeof(inp))
       ↓
Return sizeof(inp)
```

### Write Path (Rumble)

```
User: ioctl(fd, RUMBLE_SET_MOTORS, &motors)
       ↓
Kernel: rumble_ioctl()
       ↓
Check disconnected
       ↓
copy_from_user(&motors, arg, sizeof(motors))
       ↓
Clamp values to 0-100
       ↓
Build 13-byte GIP packet
       ↓
mutex_lock(&tx_mutex)
       ↓
usb_interrupt_msg() [synchronous, sleeps]
       ↓
mutex_unlock(&tx_mutex)
       ↓
Return 0 or error
```

### Poll Path

```
User: poll(fd, POLLIN, timeout)
       ↓
Kernel: rumble_poll()
       ↓
poll_wait(filp, &read_wq, wait)
       ↓
Check: !ring_empty || disconnected
       ↓
Return: POLLIN | POLLRDNORM (if data available)
        POLLHUP (if disconnected)
```

---

## Memory Layout

### struct rumble_dev

```
Offset  Size  Field
------  ----  -----
0       8     udev (pointer)
8       8     intf (pointer)
16      4     kref
20      88    cdev
108     8     dev_node (pointer)
116     4     devno
120     1408  ring[64] (64 × 22 bytes)
1528    4     r_head
1532    4     r_tail
1536    4     ring_lock
1540    80    read_wq
1620    8     in_urb (pointer)
1628    8     in_buf (pointer)
1636    8     in_dma
1644    1     ep_out_addr
1645    4     disconnected (atomic)
1649    1     rumble_seq
1650    32    tx_mutex
```

Total: ~1682 bytes (approximate, architecture-dependent)

### struct rumble_input (ABI)

```
Offset  Size  Field
------  ----  -----
0       2     buttons
2       1     lt
3       1     rt
4       2     lx
6       2     ly
8       2     rx
10      2     ry
12      2     _pad
14      8     timestamp_us
```

Total: 22 bytes (packed, stable ABI)

---

## Error Handling

### Probe Failures

| Error | Cause | Recovery |
|-------|-------|----------|
| `-ENODEV` | Wrong interface, no endpoints | Reject device |
| `-ENOMEM` | Allocation failure | Clean up, return error |
| `-EBUSY` | Second controller | Reject (single controller only) |

### Runtime Errors

| Error | Cause | Handling |
|-------|-------|----------|
| `-ENODEV` | Device disconnected | Return to user, wake waiters |
| `-EAGAIN` | Non-blocking, no data | Return immediately |
| `-EINTR` | Signal during wait | Return to user (restartable) |
| `-EFAULT` | Bad user pointer | Return error |

### URB Errors

| Status | Meaning | Action |
|--------|---------|--------|
| `0` | Success | Process packet |
| `-ECONNRESET` | URB killed | Don't resubmit |
| `-ENOENT` | URB unlinked | Don't resubmit |
| `-ESHUTDOWN` | Device removed | Don't resubmit |
| `-EOVERFLOW` | Buffer too small | Resubmit |
| Other | Transient error | Resubmit |

---

## Performance Characteristics

### Latency
- **USB polling:** ~8ms (125 Hz)
- **Kernel processing:** <100μs (interrupt handler)
- **User read:** <1ms (ring buffer dequeue)
- **Total:** ~8-9ms (USB-limited)

### Throughput
- **Packet rate:** 125 Hz
- **Packet size:** 22 bytes
- **Bandwidth:** 2.75 KB/s
- **Ring capacity:** 64 packets (~512ms buffer)

### CPU Usage
- **Interrupt handler:** ~0.1% (125 Hz × 100μs)
- **User tools:** <1% (poll-based)

---

## Security Considerations

### Kernel Space
- **Input validation:** All user pointers checked with `copy_to/from_user()`
- **Buffer bounds:** Ring buffer indices masked, no overflow
- **Reference counting:** kref prevents use-after-free
- **Disconnect safety:** Atomic flag, wake waiters, kill URB

### User Space
- **Permissions:** `/dev/rumble0` requires `input` group
- **ioctl validation:** Motor values clamped to 0-100
- **No privilege escalation:** Standard character device semantics

---

## Testing Strategy

### Unit Tests
- Ring buffer full/empty conditions
- Index wrapping
- Concurrent access (simulated)

### Integration Tests
- Open/close stress test
- Concurrent readers
- Hotplug during read
- Rumble during disconnect

### System Tests
- Long-running stability (hours)
- Packet loss measurement
- Latency profiling

---

## Future Enhancements (Out of Scope)

- Multiple controller support (minor 1, 2, 3...)
- Wireless dongle support
- Trigger motor control
- LED control
- Battery status reporting
- Force-feedback API integration

---

## References

### Linux Kernel
- [USB Driver API](https://www.kernel.org/doc/html/latest/driver-api/usb/index.html)
- [Character Device Drivers](https://www.kernel.org/doc/html/latest/driver-api/infrastructure.html)
- [Kernel Reference Counting](https://www.kernel.org/doc/html/latest/core-api/kref.html)

### Xbox Controller
- [xpad driver](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c)
- [xone driver](https://github.com/medusalix/xone)
- [GIP Protocol](https://github.com/medusalix/xone/blob/master/docs/gip.md)

### Systems Programming
- [Linux Device Drivers (LDD3)](https://lwn.net/Kernel/LDD3/)
- [The Linux Programming Interface](https://man7.org/tlpi/)

# Pre-Demo Checklist

Quick checklist for preparing the rumble driver demo/viva.

---

## Build Verification

- [ ] Setup script runs successfully
  ```bash
  sudo ./setup.sh
  ```

- [ ] Module loaded
  ```bash
  lsmod | grep rumble
  ```

- [ ] Udev rule installed
  ```bash
  ls -l /etc/udev/rules.d/99-rumble.rules
  ```

- [ ] All tools built
  ```bash
  ls tools/rumble_read tools/rumble_monitor tools/rumble_mouse
  ```

---

## Hardware Test

- [ ] Controller detected
  ```bash
  lsusb | grep 045e:02dd
  ```

- [ ] Device node exists
  ```bash
  ls -l /dev/rumble0
  ```

- [ ] Driver bound to controller
  ```bash
  ls -l /sys/bus/usb/drivers/rumble/
  ```

---

## Functional Test

- [ ] rumble_read works
  ```bash
  cd tools && sudo ./rumble_read
  # Press buttons, verify output
  # Press Enter, verify rumble
  # Ctrl+C to exit
  ```

- [ ] rumble_monitor works
  ```bash
  cd tools && sudo ./rumble_monitor
  # Move sticks, verify visualization
  # Press buttons, verify display
  # Press 'r', verify rumble
  # Press 'q' to exit
  ```

- [ ] rumble_mouse works
  ```bash
  cd tools && sudo ./rumble_mouse
  # Move left stick, verify cursor moves
  # Press LB/RB, verify clicks
  # Move right stick, verify scrolling
  # Ctrl+C to exit
  ```

- [ ] Hotplug works
  ```bash
  # Unplug controller while rumble_monitor running
  # Verify graceful disconnect
  # Replug controller
  # Verify auto-rebind
  ```

---

## Code Review

- [ ] Driver code clean and commented
  ```bash
  wc -l driver/rumble.c driver/rumble.h
  # Should be ~800 lines total
  ```

- [ ] No debug printks left in
  ```bash
  grep -n "pr_debug\|printk.*DEBUG" driver/rumble.c
  # Should be empty
  ```

- [ ] ABI stable and documented
  ```bash
  grep "struct rumble_input" driver/rumble.h
  # Verify __attribute__((packed))
  ```

---

## Documentation Review

- [ ] README.md complete
  - Quick start section
  - Architecture diagram
  - Build instructions
  - Usage examples

- [ ] ARCHITECTURE.md complete
  - Component details
  - Data flow diagrams
  - Memory layout
  - Error handling

- [ ] SUMMARY.md complete
  - Project overview
  - Educational value
  - Key takeaways

- [ ] All READMEs in subdirectories
  ```bash
  ls tools/README.md scripts/README.md
  ```

---

## Demo Preparation

- [ ] Presentation slides ready (if required)

- [ ] Key talking points prepared:
  - Why bypass Input Subsystem?
  - How does ring buffer work?
  - What is interrupt vs process context?
  - How does kref prevent use-after-free?
  - How does driver_override work?

- [ ] Code sections bookmarked:
  - `rumble_urb_complete()` - URB callback
  - `rumble_read()` - blocking read
  - `rumble_poll()` - poll implementation
  - `rumble_ioctl()` - rumble control
  - Ring buffer helpers

- [ ] Diagrams ready:
  - System architecture
  - Data flow (read path)
  - Data flow (rumble path)
  - Ring buffer visualization

---

## Viva Questions Preparation

### Expected Questions

**Q: Why did you bypass the Input Subsystem?**
A: To demonstrate custom character device implementation and direct USB handling. Educational value in showing the full stack without framework abstraction.

**Q: How do you handle interrupt vs process context?**
A: URB callback runs in interrupt context (can't sleep, uses spinlocks). Syscalls run in process context (can sleep, uses mutexes). Ring buffer bridges the two.

**Q: What happens if the ring buffer fills up?**
A: Producer wins - oldest packet is dropped. This prevents stalling the URB pipeline. Userspace should read fast enough to avoid this.

**Q: How do you prevent use-after-free on disconnect?**
A: kref reference counting. Each open() takes a ref, each close() drops a ref. Memory freed only when last ref drops.

**Q: How does driver_override work?**
A: Write "rumble" to `/sys/bus/usb/devices/<dev>/driver_override`, then unbind from current driver and bind to rumble. Kernel respects the override.

**Q: What is GIP?**
A: Gaming Input Protocol - Microsoft's protocol for Xbox controllers. Uses USB interrupt transfers with structured packets (type 0x20 for input, 0x09 for rumble).

**Q: Why use a ring buffer instead of a single packet?**
A: Buffers bursts, allows userspace to fall behind temporarily without losing data. 64 slots = ~512ms buffer at 125 Hz.

**Q: How do you ensure ABI stability?**
A: `__attribute__((packed))` on struct, explicit padding field, fixed-size types (uint16_t, etc.). 22 bytes total.

---

## Cleanup After Demo

- [ ] Unload driver
  ```bash
  cd scripts && sudo ./teardown.sh
  ```

- [ ] Verify cleanup
  ```bash
  lsmod | grep rumble  # Should be empty
  ls /dev/rumble0      # Should not exist
  ```

- [ ] Restore xpad (if needed)
  ```bash
  # Unplug and replug controller
  # Should auto-bind to xpad
  ```

---

## Backup Plan

If something breaks during demo:

1. **Driver won't load**
   - Check dmesg for errors
   - Verify kernel headers match running kernel
   - Try rebuilding: `cd driver && make clean && make`

2. **Controller won't bind**
   - Check USB connection
   - Verify PID: `lsusb | grep 045e`
   - Try manual bind: `cd scripts && sudo ./bind.sh`

3. **Tools won't run**
   - Check permissions: `ls -l /dev/rumble0`
   - Try with sudo
   - Verify module loaded: `lsmod | grep rumble`

4. **Hotplug doesn't work**
   - Check udev rule: `cat /etc/udev/rules.d/99-rumble.rules`
   - Reload rules: `sudo udevadm control --reload-rules`
   - Use manual bind script instead

---

## Final Checks

- [ ] All code committed to git
- [ ] No build artifacts in repo
- [ ] .gitignore working
- [ ] Documentation up to date
- [ ] Demo script rehearsed
- [ ] Backup USB cable available
- [ ] Laptop charged / power adapter ready

---

**Ready for Demo!**

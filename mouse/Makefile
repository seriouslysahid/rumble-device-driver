# driver/Makefile — Kbuild-compatible build file for rumble.ko
#
# Usage:
#   make              — build rumble.ko against the running kernel
#   make KDIR=<path>  — build against an out-of-tree kernel source tree
#   make clean        — remove all build artefacts

obj-m += rumble.o

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

# tools/Makefile — builds user-space tools for the rumble driver
#
# Usage:
#   make          — compile all tools
#   make clean    — remove compiled binaries

CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -I../driver
LDFLAGS :=

TARGETS := rumble_read rumble_monitor rumble_mouse

.PHONY: all clean

all: $(TARGETS)

rumble_read: rumble_read.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

rumble_monitor: rumble_monitor.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -lncurses

rumble_mouse: rumble_mouse.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -lm

clean:
	rm -f $(TARGETS)

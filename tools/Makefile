# tools/Makefile — builds the user-space test program for the rumble driver
#
# Usage:
#   make          — compile test_read
#   make clean    — remove compiled binary

CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -I../driver
TARGET  := test_read
SRC     := test_read.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

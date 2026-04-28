# Top-level Makefile for rumble driver project

.PHONY: all driver tools clean help

all: driver tools

driver:
	$(MAKE) -C driver

tools:
	$(MAKE) -C tools

clean:
	$(MAKE) -C driver clean
	$(MAKE) -C tools clean

help:
	@echo "Rumble Driver Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all     - build driver and tools (default)"
	@echo "  driver  - build kernel module only"
	@echo "  tools   - build userspace tools only"
	@echo "  clean   - remove all build artifacts"
	@echo "  help    - show this message"
	@echo ""
	@echo "Quick start:"
	@echo "  sudo ./setup.sh                      # build + install"
	@echo "  cd tools && sudo ./rumble_monitor    # test"

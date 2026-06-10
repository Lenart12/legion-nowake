obj-m += legion_nowake.o

KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# one-shot build + load for testing (not persistent across reboots)
test: all
	sudo insmod ./legion_nowake.ko
	@echo "loaded; verify with: sudo dmesg | grep legion_nowake"

unload:
	sudo rmmod legion_nowake

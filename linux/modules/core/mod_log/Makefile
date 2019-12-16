obj-m := mod_log.o
mod_log-objs := log.o sign.o

KDIR := /lib/modules/$(shell uname -r)/build
#necisary for alpine chroot stuff
ifeq ($(wildcard $(KDIR)/),)
	KDIR := /lib/modules/$(shell ls /lib/modules/|head -n1)/build
endif
PWD := $(shell pwd)

EXTRA_CFLAGS=-ffunction-sections -fdata-sections

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

modules_install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

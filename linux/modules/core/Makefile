obj-y := mod_allocator/ mod_net/ mod_syscall/ mod_kl/ mod_log/ mod_integrity/ mod_interrupt/ mod_sensors/ mod_listener/ mod_config/ mod_utils/

KDIR := /lib/modules/$(shell uname -r)/build

#necisary for alpine chroot stuff
ifeq ($(wildcard $(KDIR)/),)
	KDIR := /lib/modules/$(shell ls /lib/modules/|head -n1)/build
endif

PWD := $(shell pwd)

all: modules

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

modules_install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

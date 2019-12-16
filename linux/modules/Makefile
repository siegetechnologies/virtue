obj-y := core/ tests/

KDIR := /lib/modules/$(shell uname -r)/build

#necisary for alpine chroot stuff
ifeq ($(wildcard $(KDIR)/),)
	KDIR := /lib/modules/$(shell ls /lib/modules/|head -n1)/build
endif

PWD := $(shell pwd)

BUILDSERVER_VAGRANT := $(PWD)/../../../../devops/vagrant/buildserver
TESTSERVER_VAGRANT := $(PWD)/../../../../devops/vagrant/testmachine
TESTSERVER_PROVIDER := aws
TESTSERVER_INVENTORY := $(TESTSERVER_VAGRANT)/.vagrant/provisioners/ansible/inventory/vagrant_ansible_inventory
PLAYBOOK_DIR := $(PWD)/../../../../devops/ansible/buildserver

ELF_MANGLER_DIR := ../userspace/elf-mangler
ELF_MANGLER_EXE := elf-mangler


all: clean modules modules_install

all-mangled: all shuffle_and_load

shuffle_and_load: modules_install load

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -rf bin

modules_install:
	sudo rm -rf /lib/modules/`uname -r`/extra
	sudo $(MAKE) -C $(KDIR) M=$(PWD) modules_install
	sudo depmod -a

shuffle: $(ELF_MANGLER_EXE)
	sudo ./mangle.sh

load: shuffle
	sudo modprobe mod_log uuid_in=$(shell uuidgen)
	sudo modprobe mod_listener

unload:
	sudo modprobe -r mod_listener

$(ELF_MANGLER_EXE):
	$(MAKE) -C $(ELF_MANGLER_DIR)

collect: modules
	mkdir -p bin
	find core -name '*.ko' -exec cp {} bin \;

# Create a test machine
testmachine:
	@echo Creating test machine
	(cd $(TESTSERVER_VAGRANT) && vagrant up --provider $(TESTSERVER_PROVIDER))

destroytestmachine:
	@echo Destroying test machine
	(cd $(TESTSERVER_VAGRANT) && vagrant destroy -f)

# Start the build target
deploy: testmachine collect
	@echo Deploying modules to target
	ansible-playbook -i $(TESTSERVER_INVENTORY) $(PLAYBOOK_DIR)/deploy.yml

reprovisiontestmachine:
	@echo Reprovisioning test machine
	(cd $(TESTSERVER_VAGRANT) && vagrant provision)

buildmachine:
	@echo Creating build machine
	(cd $(BUILDSERVER_VAGRANT) && vagrant up --provider $(TESTSERVER_PROVIDER))

destroybuildmachine:
	@echo Destroying test machine
	(cd $(BUILDSERVER_VAGRANT) && vagrant destroy -f)

reprovisionbuildmachine:
	@echo Reprovisioning build machine
	(cd $(BUILDSERVER_VAGRANT) && vagrant provision)


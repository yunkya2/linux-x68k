CROSS = m68k-xelf-
CC = $(CROSS)gcc
LD = $(CROSS)gcc

GIT_REPO_VERSION=$(shell git describe --tags --always)

CFLAGS = -Os -g
LDFLAGS = -s -specs=nano.specs

BUILDROOT := ./buildroot.sh
BUILDKERNEL := ./buildkernel.sh

##############################################################################

all: loader.x vmlinux.bin

%.x: %.o
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	-rm -f *.o *.x *.elf

release:
	zip -r linux-x68k-$(GIT_REPO_VERSION).zip loader.x vmlinux.bin

everything:
	$(MAKE) buildroot-config
	$(MAKE) buildroot
	$(MAKE) linux-config
	$(MAKE) linux
	$(MAKE) all

##############################################################################

linux vmlinux.bin:
	$(BUILDKERNEL) -j$(shell nproc) all
	buildroot/output/host/bin/m68k-linux-objcopy -O binary linux/build/vmlinux vmlinux.bin

linux-config: linux/build/.config

linux-clean linux-distclean linux-menuconfig:
	$(BUILDKERNEL) $(subst linux-,,$@)

linux-savedefconfig:
	$(BUILDKERNEL) savedefconfig
	cp linux/build/defconfig linux/arch/m68k/configs/x68k_defconfig

linux/build/.config:
	$(BUILDKERNEL) x68k_defconfig

##############################################################################

buildroot:
	$(BUILDROOT)

buildroot-help buildroot-clean buildroot-distclean:
	$(BUILDROOT) $(subst buildroot-,,$@)

buildroot-toolchain buildroot-menuconfig buildroot-savedefconfig: buildroot-config
	$(BUILDROOT) $(subst buildroot-,,$@)

busybox busybox-menuconfig busybox-rebuild busybox-update-config: buildroot-toolchain
	$(BUILDROOT) $@

buildroot-config: buildroot/.config

buildroot/output/host/bin: buildroot-config
	$(BUILDROOT) toolchain

buildroot/.config:
	$(BUILDROOT) x68k_defconfig

##############################################################################

.PHONY: help all clean everything release
.PHONY: linux buildroot

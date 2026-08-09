CROSS = m68k-xelf-
CC = $(CROSS)gcc
LD = $(CROSS)gcc

GIT_REPO_VERSION=$(shell git describe --tags --always)

CFLAGS = -Os -g
LDFLAGS = -s -specs=nano.specs

BUILDROOT := ./buildroot.sh
BUILDKERNEL := ./buildkernel.sh
XDFTOOL ?= xdftool.py
XDF := linux-x68k.xdf

##############################################################################

all: loader.x vmlinux.bin vmlinux.gz

loader.x: loader.o puff.o

%.x: %.o
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	-rm -f *.o *.x *.elf $(XDF) AUTOEXEC.BAT

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

vmlinux.gz: vmlinux.bin
	gzip -c vmlinux.bin > vmlinux.gz

xdf: $(XDF)

$(XDF): HUMAN.SYS COMMAND.X AUTOEXEC.BAT loader.x vmlinux.gz
	$(XDFTOOL) c $@ $^

AUTOEXEC.BAT:
	printf 'loader.x vmlinux.gz\r\n' > $@

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

.PHONY: help all clean everything release xdf
.PHONY: linux buildroot

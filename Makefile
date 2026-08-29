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
HDF := linux-x68k.hdf

##############################################################################

all: linux.x linux.sys

loader.x: loader.o puff.o

%.x: %.o
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	-rm -f *.o *.x *.elf linux.sys $(XDF) $(HDF) AUTOEXEC.BAT

release: hdf
	zip -r linux-x68k-$(GIT_REPO_VERSION).zip linux.x linux.sys

everything:
	$(MAKE) buildroot-config
	$(MAKE) buildroot
	$(MAKE) linux-config
	$(MAKE) linux
	$(MAKE) all

##############################################################################

linux:
	$(BUILDKERNEL) -j$(shell nproc) all

vmlinux.bin: linux
	buildroot/output/host/bin/m68k-linux-objcopy -O binary linux/build/vmlinux vmlinux.bin

linux.sys: linux
	./elf2x68k.py --force-reloc-symbol jiffies -o $@ linux/build/vmlinux

vmlinux.gz: vmlinux.bin
	gzip -c vmlinux.bin > vmlinux.gz

xdf: $(XDF)

hdf: $(HDF)

$(XDF): HUMAN.SYS COMMAND.X loader.x vmlinux.gz
	printf 'loader.x vmlinux.gz\r\n' > AUTOEXEC.BAT
	$(XDFTOOL) c $@ $^ AUTOEXEC.BAT
	rm -f AUTOEXEC.BAT

$(HDF): HUMAN.SYS COMMAND.X linux.x linux.sys
	printf 'linux.x\r\n' > AUTOEXEC.BAT
	$(XDFTOOL) c /h10 $@ $^ AUTOEXEC.BAT
	rm -f AUTOEXEC.BAT

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

.PHONY: help all clean everything release xdf hdf
.PHONY: linux buildroot

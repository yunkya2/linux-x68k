CROSS = m68k-xelf-
CC = $(CROSS)gcc
LD = $(CROSS)gcc

GIT_REPO_VERSION=$(shell git describe --tags --always)

CFLAGS = -Os -g
LDFLAGS = -s -specs=nano.specs

BUILDROOT := ./buildroot.sh
BUILDKERNEL := ./buildkernel.sh
TOOLCHAIN := toolchain
SDKNAME := m68k-buildroot-uclinux-uclibc_sdk-buildroot
SDK := $(TOOLCHAIN)/$(SDKNAME)
INITROOT := $(TOOLCHAIN)/initroot.cpio
XDFTOOL ?= xdftool.py
HDF := linux-x68k.hdf

##############################################################################

all: linux.x linux.sys linuxroot.img

%.x: %.o
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	-rm -f *.o *.x *.elf linux.sys $(HDF) AUTOEXEC.BAT

everything: | $(SDK)
	$(MAKE) buildroot-config
	$(MAKE) buildroot
	$(MAKE) linux-config
	$(MAKE) linux
	$(MAKE) all

hdf: linux.x linux.sys linuxroot.img | $(SDK)
	-rm -rf hdf
	-mkdir -p hdf
	(cd hdf; unlha.py x ../HUMAN302.LZH)
#	printf 'linux.x\r\n' > hdf/AUTOEXEC.BAT
	cp $^ hdf/
	(cd hdf; $(XDFTOOL) c /h10 ../$(HDF) *)

release: hdf
	zip -r linux-x68k-$(GIT_REPO_VERSION).zip linux.x linux.sys linuxroot.img

.PHONY: help all clean everything release hdf

##############################################################################

linux:
	$(BUILDKERNEL) -j$(shell nproc) all
	./elf2x68k.py --force-reloc-symbol jiffies -o linux.sys linux/build/vmlinux

vmlinux.bin: linux
	buildroot/output/host/bin/m68k-linux-objcopy -O binary linux/build/vmlinux vmlinux.bin

vmlinux.gz: vmlinux.bin
	gzip -c vmlinux.bin > vmlinux.gz

linux.sys: | $(INITROOT)
	$(MAKE) linux

linux-clean linux-distclean linux-menuconfig:
	$(BUILDKERNEL) $(subst linux-,,$@)

linux-savedefconfig:
	$(BUILDKERNEL) savedefconfig
	cp linux/build/defconfig linux/arch/m68k/configs/x68k_defconfig

linux-config:
	$(BUILDKERNEL) x68k_defconfig

.PHONY: linux hdf

##############################################################################

linuxroot.img: buildroot/output/images/rootfs.ext2 | $(SDK)
	cp buildroot/output/images/rootfs.ext2 $@

buildroot:
	$(BUILDROOT)

buildroot/output/images/rootfs.ext2: | $(SDK)
	$(BUILDROOT)

buildroot-help buildroot-clean buildroot-distclean:
	$(BUILDROOT) $(subst buildroot-,,$@)

buildroot-toolchain buildroot-menuconfig buildroot-savedefconfig:
	$(BUILDROOT) $(subst buildroot-,,$@)

busybox busybox-menuconfig busybox-rebuild busybox-update-config:
	$(BUILDROOT) $@

buildroot-config:
	$(BUILDROOT) x68k_defconfig

.PHONY: buildroot

##############################################################################

sdk:
	rm -rf $(SDK)
	rm -f $(TOOLCHAIN)/$(SDKNAME).tar.gz
	$(MAKE) $(SDK)

$(SDK):
	$(MAKE) $(TOOLCHAIN)/$(SDKNAME).tar.gz
	tar -xzf $(TOOLCHAIN)/$(SDKNAME).tar.gz -C $(TOOLCHAIN)

$(TOOLCHAIN)/$(SDKNAME).tar.gz: | $(TOOLCHAIN)
	$(BUILDROOT) clean
	$(BUILDROOT) x68k_sdk_defconfig
	$(BUILDROOT) sdk
	mv buildroot/output/images/$(SDKNAME).tar.gz $@
	$(BUILDROOT) clean
	$(BUILDROOT) x68k_defconfig
	
$(TOOLCHAIN):
	mkdir -p $(TOOLCHAIN)

.PHONY: sdk

##############################################################################

initroot:
	rm -f $(INITROOT)
	$(MAKE) $(INITROOT)

$(INITROOT): $(SDK)
	$(BUILDROOT) clean
	$(BUILDROOT) x68k_init_defconfig
	$(BUILDROOT)
	mv buildroot/output/images/rootfs.cpio $@
	$(BUILDROOT) clean
	$(BUILDROOT) x68k_defconfig

.PHONY: initroot

##############################################################################

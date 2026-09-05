.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
# Buildroot configuration and maintenance targets share an output directory.
# Serialize top-level goals; the kernel submake still builds in parallel.
.NOTPARALLEL:

CROSS = m68k-xelf-
CC = $(CROSS)gcc
LD = $(CROSS)gcc

GIT_REPO_VERSION = $(shell git describe --tags --always)
CFLAGS = -Os -g
LDFLAGS = -s -specs=nano.specs

BUILDROOT := ./buildroot.sh
BUILDKERNEL := ./buildkernel.sh
TOOLCHAIN := toolchain
SDKNAME := m68k-buildroot-uclinux-uclibc_sdk-buildroot
SDK := $(TOOLCHAIN)/$(SDKNAME)
SDK_ARCHIVE := $(TOOLCHAIN)/$(SDKNAME).tar.gz
SDK_OUTPUT := $(abspath buildroot/output-sdk)
INIT_OUTPUT := $(abspath buildroot/output-init)
INITROOT := $(TOOLCHAIN)/initroot.cpio
ROOTFS := buildroot/output/images/rootfs.ext2
VMLINUX := linux/build/vmlinux
OBJCOPY := $(SDK)/bin/m68k-linux-objcopy
JOBS ?= $(shell nproc)
XDFTOOL ?= xdftool.py
HDF := linux-x68k.hdf
BINARIES := linux.x linux.sys linuxroot.img

##############################################################################

all: $(BINARIES)

# Keep these steps ordered even when invoked with make -j.
everything:
	$(MAKE) sdk
	$(MAKE) initroot
	$(MAKE) buildroot-config
	$(MAKE) linux-config
	$(MAKE) all

%.x: %.o
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o *.x *.elf linux.sys linuxroot.img vmlinux.bin vmlinux.gz $(HDF)

hdf: $(BINARIES)
	rm -rf hdf
	mkdir -p hdf
	(cd hdf && unlha.py x ../HUMAN302.LZH)
	cp HIOCS.X hdf/SYS
	sed -i 's/IOCS\.X/HIOCS.X/' hdf/CONFIG.SYS
	echo -n "linux.x\r\n" > hdf/AUTOEXEC.BAT
	cp $(BINARIES) hdf/
	(cd hdf && $(XDFTOOL) c /h10 ../$(HDF) *)

release: hdf
	zip -r linux-x68k-$(GIT_REPO_VERSION).zip $(BINARIES)

help:
	@echo 'everything  Build SDK, initramfs, rootfs, kernel and loader'
	@echo 'all         Update rootfs, kernel and loader using existing SDK/initramfs'
	@echo 'sdk         Rebuild and install the toolchain SDK'
	@echo 'initroot    Rebuild the bootstrap initramfs using the existing SDK'
	@echo 'linux       Update the kernel and linux.sys'
	@echo 'buildroot   Update the root filesystem in buildroot/output'
	@echo 'hdf/release Create a Human68k disk image / release archive'
	@echo 'clean       Remove top-level build products (keep SDK/initramfs)'

##############################################################################

# These checks never build prerequisites implicitly.
check-sdk:
	@test -x "$(SDK)/bin/m68k-linux-gcc" || { \
		echo 'SDK missing: run make sdk or make everything first.' >&2; exit 1; }

check-initroot:
	@test -f "$(INITROOT)" || { \
		echo 'Initramfs missing: run make initroot or make everything first.' >&2; exit 1; }

linux: linux.sys

# Always let Kbuild inspect its own dependencies. Only regenerate the Human68k
# executable when the linked kernel or converter has changed.
linux-build: check-sdk check-initroot linux/build/.config
	+$(BUILDKERNEL) -j$(JOBS) all

linux.sys: linux-build elf2x68k.py
	@if test ! -f $@ || test $(VMLINUX) -nt $@ || test elf2x68k.py -nt $@; then \
		./elf2x68k.py --force-reloc-symbol jiffies -o $@ $(VMLINUX); \
	fi

vmlinux.bin: linux-build
	@if test ! -f $@ || test $(VMLINUX) -nt $@ || test $(OBJCOPY) -nt $@; then \
		$(OBJCOPY) -O binary $(VMLINUX) $@; \
	fi

vmlinux.gz: vmlinux.bin
	gzip -c $< > $@

linux-clean linux-distclean:
	+$(BUILDKERNEL) $(subst linux-,,$@)

linux-menuconfig: check-sdk linux/build/.config
	+$(BUILDKERNEL) menuconfig

linux-savedefconfig: check-sdk linux/build/.config
	+$(BUILDKERNEL) savedefconfig
	cp linux/build/defconfig linux/arch/m68k/configs/x68k_defconfig

linux-config: check-sdk
	+$(BUILDKERNEL) x68k_defconfig

linux/build/.config: | check-sdk
	+$(BUILDKERNEL) x68k_defconfig

##############################################################################

# Buildroot owns package/image dependencies. Avoid touching the exported image
# when the newly generated filesystem is byte-for-byte identical.
$(ROOTFS): buildroot
	@test -f $@

linuxroot.img: $(ROOTFS)
	@cmp -s $(ROOTFS) $@ || cp $(ROOTFS) $@

buildroot: check-sdk buildroot/.config
	+$(BUILDROOT)

buildroot-help buildroot-clean buildroot-distclean:
	+$(BUILDROOT) $(subst buildroot-,,$@)

buildroot-toolchain: check-sdk buildroot/.config
	+$(BUILDROOT) toolchain

buildroot-menuconfig buildroot-savedefconfig: buildroot/.config
	+$(BUILDROOT) $(subst buildroot-,,$@)

busybox busybox-rebuild: check-sdk buildroot/.config
	+$(BUILDROOT) $@

busybox-menuconfig busybox-update-config: buildroot-toolchain
	+$(BUILDROOT) $@

buildroot-config:
	+$(BUILDROOT) x68k_defconfig

buildroot/.config:
	+$(BUILDROOT) x68k_defconfig

##############################################################################

# Separate outputs keep explicit SDK/initramfs builds from cleaning or
# reconfiguring the normal root filesystem build.
sdk: | $(TOOLCHAIN)
	+$(BUILDROOT) O=$(SDK_OUTPUT) x68k_sdk_defconfig
	+$(BUILDROOT) O=$(SDK_OUTPUT) clean
	+$(BUILDROOT) O=$(SDK_OUTPUT) sdk
	cp $(SDK_OUTPUT)/images/$(SDKNAME).tar.gz $(SDK_ARCHIVE)
	rm -rf $(SDK)
	tar -xzf $(SDK_ARCHIVE) -C $(TOOLCHAIN)
	$(SDK)/relocate-sdk.sh

initroot: check-sdk | $(TOOLCHAIN)
	+$(BUILDROOT) O=$(INIT_OUTPUT) x68k_init_defconfig
	+$(BUILDROOT) O=$(INIT_OUTPUT) clean
	+$(BUILDROOT) O=$(INIT_OUTPUT)
	cp $(INIT_OUTPUT)/images/rootfs.cpio $(INITROOT)

$(TOOLCHAIN):
	mkdir -p $@

.PHONY: all everything clean hdf release help check-sdk check-initroot
.PHONY: linux linux-build linux-clean linux-distclean linux-menuconfig
.PHONY: linux-savedefconfig linux-config
.PHONY: buildroot buildroot-help buildroot-clean buildroot-distclean
.PHONY: buildroot-toolchain buildroot-menuconfig buildroot-savedefconfig buildroot-config
.PHONY: busybox busybox-rebuild busybox-menuconfig busybox-update-config sdk initroot

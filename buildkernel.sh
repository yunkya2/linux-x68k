#!/bin/sh
export PATH=$(pwd)/toolchain/m68k-buildroot-uclinux-uclibc_sdk-buildroot/bin:${PATH}
cd linux && make O=build ARCH=m68k CROSS_COMPILE=m68k-linux- $*

#!/bin/sh
export PATH=$(pwd)/buildroot/output/host/bin:${PATH}
cd linux && make O=build ARCH=m68k CROSS_COMPILE=m68k-linux- $*

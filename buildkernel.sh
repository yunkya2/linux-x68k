#!/bin/sh
export PATH=$(pwd)/buildroot/output/host/bin:${PATH}
make -C linux O=build ARCH=m68k CROSS_COMPILE=m68k-linux- $*

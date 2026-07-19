#!/bin/sh
export PATH=$(echo $PATH|sed 's/\/[^:]* [^:]*://g')
make -C buildroot $*

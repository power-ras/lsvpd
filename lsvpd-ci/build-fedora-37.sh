#!/bin/bash

set -uo pipefail
set -e
set -vx
MAKE_J=$(grep -c processor /proc/cpuinfo)

./bootstrap.sh
./configure
make -j $MAKE_J
make -j $MAKE_J check
mkdir -p /root/rpmbuild/SOURCES/
make dist-gzip
cp *.tar.gz /root/rpmbuild/SOURCES/
rpmbuild -ba lsvpd.spec

#!/bin/bash

# stage all files to be packed into the disk image
# mkdir / copy / create what is needed into build/rootfs

source ../config.sh  # get build variables
R=./build/rootfs
SFS=./sfs/sfs



mkdir -p \
    $R/bin \
    $R/etc \
    $R/usr \
    $R/usr/src \
    $R/usr/include \
    $R/usr/lib \
    $R/tmp

cat > $R/etc/initrc <<EOF
# commands executed by init, at system boot

# /bin/gui
# /bin/window_manager
# /bin/shell
# ... etc
EOF

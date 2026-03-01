#!/bin/bash
set -e

# stage all files to be packed into the disk image
# mkdir / copy / create what is needed into build/rootfs

source ./config.sh  # get build variables
R=./build/rootfs


mkdir -p \
    $R/bin \
    $R/etc \
    $R/usr \
    $R/usr/src \
    $R/usr/include \
    $R/usr/lib \
    $R/tmp

cp ./userapps/init/init   $R/bin
cp ./userapps/shell/shell $R/bin
cp ./userapps/edit/edit   $R/bin

cat > $R/etc/initrc <<EOF
# commands executed by init, at system boot

# /bin/gui
# /bin/window_manager
# /bin/shell
# ... etc
EOF

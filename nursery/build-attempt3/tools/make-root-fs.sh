#!/bin/bash
set -e

# stage all files to be packed into the disk image
# mkdir / copy / create what is needed into build/rootfs

source ./config.inc.sh  # get build variables
R=./build/rootfs


# Ensure the destination exists
mkdir -p ./build/rootfs

# 1. Recreate the directory structure
# We find all directories, excluding the root '.' itself, then mkdir them
find ./rootfs -type d -not -path "./rootfs" | sed 's|^\./rootfs/||' | xargs -I {} mkdir -p ./build/rootfs/{}

# 2. Copy the files, filtering out .gitkeep
# -not -name matches filenames exactly
find ./rootfs -type f -not -name ".gitkeep" | while read -r file; do
    dest="./build/rootfs/${file#./rootfs/}"
    cp "$file" "$dest"
done


# copy libc so that programs can compile..?
cp ./libc/libc.a $R/usr/lib
cp -r ./libc/include/* $R/usr/include



# these should be put there by their respective makefiles, upon "make install"
cp ./userapps/init/init   $R/bin
cp ./userapps/sash/sash   $R/bin
cp ./userapps/shell/shell $R/bin
cp ./userapps/edit/edit   $R/bin



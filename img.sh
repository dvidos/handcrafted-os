#!/bin/sh

# no spaces before/after the equal sign
IMG_FILE="hcos.img"
FILE_SECTORS=20480   # 20 K sectors -> 10 MB file size
LOOPBACK_DEV=/dev/loop101   # loop device name must have the same number as minor dev no
PARTIRION_DEV=${LOOPBACK_DEV}p1
MOUNT_DIR=/mnt/hcos
DIR_TO_COPY=./sysroot
CMD=$1


check_prerequisites() {
    if [ ! -b $LOOPBACK_DEV ]; then
        echo Creating Loopback device $LOOPBACK_DEV
        sudo mknod $LOOPBACK_DEV b 7 101
    fi
    if [ ! -d $MOUNT_DIR ]; then
        echo Creating mount directory $MOUNT_DIR
        sudo mkdir -p $MOUNT_DIR
        sudo chown dimitris $MOUNT_DIR
    fi
    if ! grep -q $MOUNT_DIR /etc/fstab; then
        echo "/etc/fstab has no entry for $MOUNT_DIR. Create so that mount is rw"
        exit 1
    fi
}

setup_image_file() {

    echo Creating image file $IMG_FILE, $FILE_SECTORS sectors
    dd if=/dev/zero of=$IMG_FILE count=$FILE_SECTORS

    echo Mounting raw image to loopback device $LOOPBACK_DEV
    sudo losetup -P $LOOPBACK_DEV $IMG_FILE

    echo Creating partitions on device
    # fsisk /dev/loop101
    # o -- create dos partition table
    # n -- create new partition
    # p -- primary (as opposed to e=extended)
    # 1 -- partition number (e.g. 1-4)
    # <enter> -- first sector, 1 is proposed
    # <enter> -- last sector, last is proposed
    # a   toggle a bootable flag
    # w -- write (but it failed)
    (echo o; 
     echo n; echo p; echo 1; echo ""; echo ""; 
     echo "a"; 
     echo "p"; echo "w"
    ) | sudo fdisk $LOOPBACK_DEV > /dev/null
    sudo fdisk -l $LOOPBACK_DEV
    # echo -e "o\n d\n n\n p\n1\n\n\na\np\nw\n" | sudo fdisk $LOOPBACK_DEV
    
    echo Re-mounting raw image to detect partitions
    sudo losetup -d $LOOPBACK_DEV
    sudo losetup -P $LOOPBACK_DEV $IMG_FILE 

    echo Creating file system on partition $PARTIRION_DEV
    sudo mkfs.fat -v -F 16 ${PARTIRION_DEV}
    
    # specify only the mount point, to force fstab policies
    echo Mounting partition $MOUNT_DIR
    mount -v $MOUNT_DIR

    echo Copying files to destination
    cp -prv $DIR_TO_COPY/* $MOUNT_DIR 
}

show_status() {
    if [ ! -f $IMG_FILE ]; then
        echo "Image file not found"
    else
        echo "Image file information      " `ls -lh $IMG_FILE | awk '{ print $9 ", " $5 ", " $6, $7, $8 }'`
        echo "Loop back device setup      " `losetup | grep $LOOPBACK_DEV | awk '{ print $6 " --> " $1 }'`
        echo "Mount information           " `mount | grep $MOUNT_DIR | awk '{ print $1 " --> " $3 }'`
        echo "Current directory contents  " `ls -F $MOUNT_DIR`
    fi
}

teardown_image() {
    sudo umount $MOUNT_DIR
    sudo losetup --detach $LOOPBACK_DEV
    rm $IMG_FILE
}

show_help() {
    echo "Disk image manipulation script"
    echo 
    echo "Syntax:"
    echo "   img setup     creates the image file from scratch"
    echo "   img status    shows status about image, loopback, mount etc"
    echo "   img teardown  removes the image file and temporary files"
    echo 
    echo "Notes:"
    echo "   Image file name will be \"$IMG_FILE\""
    echo "   Device \"$LOOPBACK_DEV\" will be used to loop file into a device"
    echo "   Partition 1 will be mounted on \"$MOUNT_DIR\""
}



if [ $CMD = "setup" ]; then
    set -e  # exit immediately if a command fails
    check_prerequisites
    setup_image_file
elif [ $CMD = "status" ]; then
    show_status
elif [ $CMD = "teardown" ]; then
    set +e  # don't exit if a command fails
    teardown_image
else
    show_help
fi

#!/usr/bin/env bash

function check_exit_status() {
	if [ $? -eq 0 ]
	then
		echo "$1 OK!"
	else
		echo "$1 FAILD!"
		exit
	fi
}

# test if the first para is null
if [ -n "$1" ]
then
    MOUNT_POINT=$1
else
    echo "No mount point specified, /mnt/tinyfs will be used"
    MOUNT_POINT=/mnt/tinyfs
fi

if [ ! -d $MOUNT_POINT ]
then
    mkdir $MOUNT_POINT
    check_exit_status "mkdir $MOUNT_POINT"
fi

make
check_exit_status make

TINYFS_FLAG=`lsmod | grep tinyfs`

if [ "$TINYFS_FLAG" != "" ]
then
	echo "rmmod tinyfs..."
	if [ "mount | grep tinyfs" != "" ]
	then
		umount /mnt/tinyfs
	fi
	rmmod tinyfs
	check_exit_status rmmod
else
	echo "tinyfs is not insert to kernel"
fi

echo "insmod tinyfs..."
insmod ./tinyfs.ko
check_exit_status insmod

mount -t tinyfs none /mnt/tinyfs
check_exit_status mount

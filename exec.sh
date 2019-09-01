function check_exit_status() {
	if [ $? -eq 0 ]
	then
		echo "$1 OK!"
	else
		echo "$1 FAILD!"
		exit
	fi
}

cd /root/tinyfs

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
insmod /root/tinyfs/tinyfs.ko
check_exit_status insmod

mount -t tinyfs none /mnt/tinyfs
check_exit_status mount

cd /mnt/tinyfs

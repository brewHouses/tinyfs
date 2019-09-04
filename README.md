# tinyfs

A very simple file system

The initial code is from https://mp.weixin.qq.com/s/Sidfn8CZn4KxKh6xMH2uJQ

And the project has been transplanted kernel 4.12.14

This module has been tested in kernel 4.12.14(open suse), 4.4.0(ubuntu), 5.0.15(ubuntu) and 3.1.0(centos), but not pass in 3.1.0, so the 4.0 or upper is suggested.

And openSUSE-Leap-15.1 is highly recommended.

## Usage

```shell
git clone https://github.com/brewHouses/tinyfs.git
cd tinyfs
./exec.sh
cd /mnt/tinyfs
# Then you can test the tinyfs here
```

## TODO
- Split the file `tinyfs.c`
- Add . and .. when create a new directory

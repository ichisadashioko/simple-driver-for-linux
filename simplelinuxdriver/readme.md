# compiling

```sh
make all
```

# installing and running

```sh
sudo insmod ./simplekernelmodule.ko
cat /proc/devices | grep simplekernelmodule
# take a note of the device major number (e.g. 509)

# sudo mknod /dev/simplekernelmodule c <device-major-number> 0 # e.g.
sudo mknod /dev/simplekernelmodule c 509 0
cat /dev/simplekernelmodule
```

# uninstalling

```sh
sudo rm /dev/simplekernelmodule
sudo rmmod simplekernelmodule
```

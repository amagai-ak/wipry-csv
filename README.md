# wipry-csv
Outputs Oscium WiPry spectrum analysis data in CSV/Influx Line Protocol format.

This repository is forked from https://github.com/bryanward-net/wipry-lp.

## Running on x86 PC

Since libWiPryClarity.a is an ARM binary and its source code is not public, QEMU is required to use it on the x86 architecture.

### Setting up emulated environment of ARM debian 

Create emulation environment on current directory.

(It takes loooong time.)

```shell
sudo apt install qemu-user-static debootstrap binfmt-support
sudo debootstrap --arch=arm64 --foreign jammy ./arm64-root http://ports.ubuntu.com/
sudo cp /usr/bin/qemu-aarch64-static ./arm64-root/usr/bin/
sudo chroot ./arm64-root /debootstrap/debootstrap --second-stage
sudo chroot ./arm64-root apt update
sudo chroot ./arm64-root apt install build-essential usbutils libstdc++6 libssl3 libcurl4
sudo chroot ./arm64-root apt install libusb-1.0-0-dev libpcap-dev libpcap0.8-dev libpcap0.8 libusb-0.1-4 libusb-1.0-0-dev libusb-1.0-0 libusb-dev
```

### Mount system directories on emulated environment

```shell
sudo mount --bind /dev ./arm64-root/dev
sudo mount --bind /sys ./arm64-root/sys
```

### Check WiPry is visible

```shell
$ sudo chroot ./arm64-root
# lsusb
Bus 003 Device 004: ID 26ae:000c Oscium WiPry Clarity
# exit
```

### Copy source codes and build

```shell
sudo mkdir -p ./arm64-root/root/wipry-csv
sudo cp main.cpp WiPryClarity.h libWiPryClarity.a Makefile ./arm64-root/root/wipry-csv/.
sudo chroot ./arm64-root
cd /root/wipry-csv/
make
exit
```

### RUN!

```shell
sudo chroot ./arm64-root /root/wipry-csv/wipry-csv -c -2
```

### Note

When deleting the emulation directories, *don't forget* to unmont /dev and /sys first.
